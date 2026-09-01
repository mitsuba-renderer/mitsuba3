#include <mitsuba/render/scene_ir.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/shapegroup.h>
#include <drjit-core/jit.h>
#include <drjit-core/hash.h>
#include <tsl/robin_map.h>
#include <algorithm>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
SceneIR SceneIRBuilder<Float, Spectrum>::build(Scene<Float, Spectrum> *scene) {
    SceneIR sd;

    const auto &shapes = scene->shapes();        // top-level (incl. Instances)
    const auto &groups = scene->shapegroups();

    // Describe every top-level shape, stamping data slots. Route non-instance
    // shapes to the leading BLASes, Instances to the flattening pass below.
    std::vector<ShapeIR> top_noninst, inst_shapes;
    top_noninst.reserve(shapes.size());
    inst_shapes.reserve(shapes.size());

    uint32_t slot = 0;
    for (size_t i = 0; i < shapes.size(); ++i) {
        ShapeIR g;
        shapes[i]->describe(g);
        g.visibility_mask = shapes[i]->visibility_mask();
        g.data_slot = slot++;
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
            children[j]->describe(group_geoms[i][j]);
            group_geoms[i][j].visibility_mask = children[j]->visibility_mask();
            group_geoms[i][j].data_slot = slot++;
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
        for (uint32_t bi : sd.group_blases[group_index.at(inst.group_id)]) {
            InstanceEntry e;
            e.blas_index = bi;
            e.instance_index = instance_index;
            for (int k = 0; k < 12; ++k)
                e.to_world[k] = inst.to_world[k];
            sd.instances.push_back(e);
        }
    }

    return sd;
}

// Instantiated for every enabled variant (tracks mitsuba.conf, incl. CUDA).
MI_INSTANTIATE_STRUCT(SceneIRBuilder)

NAMESPACE_END(mitsuba)
