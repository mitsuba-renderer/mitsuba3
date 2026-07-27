#include <mitsuba/render/scene_ir.h>
#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/shapegroup.h>
#include <drjit-core/jit.h>
#include <drjit-core/hash.h>
#include <tsl/robin_map.h>

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
            group_geoms[i][j].data_slot = slot++;
        }
    }

    // Move same-kind geometry into one BLAS each (enumerator order), appending
    // them to sd.blases and reporting their indices. Instances have already been
    // routed away, so every geom here belongs to a geometry kind.
    auto bucket_into_blases = [&](std::vector<ShapeIR> &&geoms,
                                  std::vector<uint32_t> &out_indices) {
        std::vector<ShapeIR> buckets[NumGeometryKinds];
        for (ShapeIR &g : geoms)
            buckets[(size_t) g.kind].push_back(std::move(g));
        for (size_t k = 0; k < NumGeometryKinds; ++k) {
            if (buckets[k].empty())
                continue;
            out_indices.push_back((uint32_t) sd.blases.size());
            sd.blases.push_back(BlasEntry{ (ShapeIR::Kind) k, std::move(buckets[k]) });
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
    for (const ShapeIR &inst : inst_shapes) {
        uint32_t owner = jit_registry_id(inst.ctx);
        for (uint32_t bi : sd.group_blases[group_index.at(inst.group_id)]) {
            InstanceEntry e;
            e.blas_index = bi;
            e.owner_registry_id = owner;
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
