#include <mitsuba/render/scene_ir.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/shapegroup.h>
#include <drjit-core/jit.h>
#include <drjit-core/hash.h>
#include <tsl/robin_map.h>
#include <algorithm>

NAMESPACE_BEGIN(mitsuba)

/**
 * Record the intersection routine of a custom shape (JIT variants only)
 *
 * Traces ``Shape::ray_intersect_preliminary()`` on symbolic placeholder
 * inputs and hands the resulting computation to Dr.Jit.
 */
template <typename Float, typename Spectrum>
static uint32_t record_intersection(const Shape<Float, Spectrum> *shape) {
    MI_IMPORT_TYPES()
    constexpr JitBackend backend = dr::backend_v<Float>;

    std::string name = std::string(shape->class_name()) + "::ray_intersect_preliminary";

    // Open a recording session
    uint32_t in[9];
    jit_isect_begin(backend, Float::Type, in);

    struct SessionGuard {
        const char *name;
        bool committed = false;
        ~SessionGuard() {
            if (!committed)
                jit_isect_end(backend, name, nullptr);
        }
    } guard { name.c_str() };

    Point3f o(Float::steal(in[0]), Float::steal(in[1]), Float::steal(in[2]));
    Vector3f d(Float::steal(in[3]), Float::steal(in[4]), Float::steal(in[5]));
    Float maxt = Float::steal(in[6]), time = Float::steal(in[7]);
    UInt32 prim_index = UInt32::steal(in[8]);

    Ray3f ray(o, d, maxt, time, dr::zeros<Wavelength>());

    // The preliminary intersection is detached by design
    PreliminaryIntersection3f pi;
    {
        dr::suspend_grad<Float> suspend;
        pi = shape->ray_intersect_preliminary(ray, prim_index, true);
    }

    // The attribute words hold single precision bit patterns
    using Single = dr::float32_array_t<Float>;
    UInt32 attr0 = dr::reinterpret_array<UInt32>(Single(pi.prim_uv.x())),
           attr1 = dr::reinterpret_array<UInt32>(Single(pi.prim_uv.y()));

    uint32_t out[4] = { pi.valid.index(), pi.t.index(), attr0.index(),
                        attr1.index() };

    guard.committed = true;
    return jit_isect_end(backend, name.c_str(), out);
}

template <typename Float, typename Spectrum>
SceneIR SceneIRBuilder<Float, Spectrum>::build(Scene<Float, Spectrum> *scene) {
    SceneIR sd;

    const auto &shapes = scene->shapes();        // top-level (incl. Instances)
    const auto &groups = scene->shapegroups();

    // Describe every top-level shape. Route non-instance shapes to the
    // leading BLASes, Instances to the flattening pass below.
    std::vector<ShapeIR> top_noninst, inst_shapes;
    top_noninst.reserve(shapes.size());
    inst_shapes.reserve(shapes.size());

    // JIT variants intersect custom shapes through their recorded routines
    auto describe = [&](const Shape<Float, Spectrum> *shape, ShapeIR &g) {
        shape->describe(g);
        if constexpr (dr::is_jit_v<Float>) {
            if (g.kind == ShapeIR::Kind::Custom) {
                g.isect_func = record_intersection(shape);
                sd.isect_funcs.push_back(g.isect_func);
            }
        }
    };

    for (size_t i = 0; i < shapes.size(); ++i) {
        ShapeIR g;
        describe(shapes[i].get(), g);
        g.visibility_mask =
            accel_mask(shapes[i]->visibility(), shapes[i]->has_null());
        if (g.kind == ShapeIR::Kind::Instance)
            inst_shapes.push_back(std::move(g));
        else
            top_noninst.push_back(std::move(g));
    }

    std::vector<std::vector<ShapeIR>> group_geoms(groups.size());
    for (size_t i = 0; i < groups.size(); ++i) {
        const auto &children = groups[i]->shapes();
        group_geoms[i].resize(children.size());
        for (size_t j = 0; j < children.size(); ++j) {
            describe(children[j].get(), group_geoms[i][j]);
            group_geoms[i][j].visibility_mask =
                accel_mask(children[j]->visibility(), children[j]->has_null());
        }
    }

    // Group the geometry into BLASes, appending them to sd.blases and
    // reporting their indices. Shapes share a BLAS when they agree in kind
    // and visibility mask. The mask split lets OptiX/Metal express per-shape
    // visibility via per-instance masks, and scenes without hidden emitters
    // still get one BLAS per kind. Instances were routed away earlier, so
    // every geom here has a geometry kind.
    auto bucket_into_blases = [&](std::vector<ShapeIR> &&geoms,
                                  std::vector<uint32_t> &out_indices) {
        std::vector<std::pair<uint32_t, std::vector<ShapeIR>>>
            buckets[NumGeometryKinds];
        for (ShapeIR &g : geoms) {
            auto &kb = buckets[(size_t) g.kind];
            auto it = std::find_if(kb.begin(), kb.end(), [&](const auto &p) {
                return p.first == g.visibility_mask;
            });
            if (it == kb.end())
                it = kb.emplace(kb.end(), g.visibility_mask,
                                std::vector<ShapeIR>());
            it->second.push_back(std::move(g));
        }
        for (size_t k = 0; k < NumGeometryKinds; ++k) {
            for (auto &[mask, bucket] : buckets[k]) {
                out_indices.push_back((uint32_t) sd.blases.size());
                sd.blases.push_back(
                    BlasEntry{ (ShapeIR::Kind) k, mask, std::move(bucket) });
            }
        }
    };

    bucket_into_blases(std::move(top_noninst), sd.top_blases);

    // Bucket each ShapeGroup's children, keeping a build-local map from the
    // group pointer to its position so the instance flattening can resolve it.
    tsl::robin_map<const void *, uint32_t, PointerHasher> group_index;
    sd.group_blases.resize(groups.size());
    for (size_t i = 0; i < groups.size(); ++i) {
        group_index[(const void *) groups[i].get()] = (uint32_t) i;
        bucket_into_blases(std::move(group_geoms[i]), sd.group_blases[i]);
    }

    // Flatten: one instance per top-level BLAS (no owner, identity transform)...
    for (uint32_t bi : sd.top_blases) {
        InstanceEntry e;
        e.blas_index = bi;
        sd.instances.push_back(e);
    }
    // ...then, per Instance shape, one instance per BLAS of its ShapeGroup.
    // ``inst_shapes`` preserves the order of appearance in the scene's shape
    // list, so the running index below matches the numbering used by
    // Scene::update_instance_transforms().
    uint32_t instance_index = 0;
    for (const ShapeIR &inst : inst_shapes) {
        ++instance_index;

        if (inst.keyframes.size() > 1) {
            float t0 = inst.keyframes.front().time,
                  t1 = inst.keyframes.back().time;
            sd.time_min = sd.has_motion ? std::min(sd.time_min, t0) : t0;
            sd.time_max = sd.has_motion ? std::max(sd.time_max, t1) : t1;
            sd.has_motion = true;
        }

        for (uint32_t bi : sd.group_blases[group_index.at(inst.group_id)]) {
            InstanceEntry e;
            e.blas_index = bi;
            e.instance_index = instance_index;
            for (int k = 0; k < 12; ++k)
                e.to_world[k] = inst.to_world[k];
            sd.instances.push_back(e);
        }
    }

    sd.instance_shapes = std::move(inst_shapes);
    return sd;
}

// Instantiated for every enabled variant (tracks mitsuba.conf, incl. CUDA).
MI_INSTANTIATE_STRUCT(SceneIRBuilder)

NAMESPACE_END(mitsuba)
