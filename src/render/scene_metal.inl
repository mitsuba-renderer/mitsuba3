/*
    scene_metal.inl -- Metal GPU ray tracing via Metal Acceleration Structures

    Copyright (c) 2026 Wenzel Jakob / Sebastien Speierer

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE file.
*/

#include <drjit-core/metal.h>
#include <mitsuba/render/mesh.h>

NAMESPACE_BEGIN(mitsuba)

// Metal acceleration structure state, stored in Scene::m_accel
struct MetalAccelState {
    void *tlas = nullptr;  // MTL::InstanceAccelerationStructure*
    std::vector<void *> blas_list;  // per-shape BLAS handles
    std::vector<void *> resources;  // all buffers the TLAS references
};

MI_VARIANT void Scene<Float, Spectrum>::accel_init_metal(const Properties &/*props*/) {
    if constexpr (dr::is_metal_v<Float>) {
        auto *device = (MTL::Device *) jit_metal_context();
        auto *queue  = (MTL::CommandQueue *) jit_metal_command_queue();

        auto *state = new MetalAccelState();
        m_accel = state;

        // ----------------------------------------------------------------
        //  1. Build BLAS for each mesh
        // ----------------------------------------------------------------
        std::vector<MTL::AccelerationStructure *> blas_vec;
        std::vector<MTL::AccelerationStructureInstanceDescriptor> instances;

        uint32_t instance_id = 0;
        for (auto &shape : m_shapes) {
            auto *mesh = dynamic_cast<mitsuba::Mesh<Float, Spectrum> *>(shape.get());
            if (!mesh) {
                Log(Warn, "scene_metal: skipping non-mesh shape \"%s\"",
                    std::string(shape->id()).c_str());
                continue;
            }

            // Get vertex and index data (must be evaluated on device)
            dr::eval(mesh->vertex_positions_buffer(),
                     mesh->faces_buffer());
            dr::sync_thread();

            void *vertex_ptr = nullptr, *index_ptr = nullptr;
            size_t vertex_count = mesh->vertex_count();
            size_t face_count   = mesh->face_count();

            uint32_t vbuf_idx = jit_var_data(
                mesh->vertex_positions_buffer().index(), &vertex_ptr);
            uint32_t ibuf_idx = jit_var_data(
                mesh->faces_buffer().index(), &index_ptr);

            // Find the MTL::Buffer objects for vertex/index data
            size_t v_off = 0, i_off = 0;
            auto *v_buf = (MTL::Buffer *) jit_metal_lookup_buffer(
                vertex_ptr, &v_off);
            auto *i_buf = (MTL::Buffer *) jit_metal_lookup_buffer(
                index_ptr, &i_off);

            if (!v_buf || !i_buf) {
                Log(Warn, "scene_metal: could not find Metal buffers for mesh \"%s\"",
                    std::string(mesh->id()).c_str());
                continue;
            }

            // Create triangle geometry descriptor
            auto *geom_desc =
                MTL::AccelerationStructureTriangleGeometryDescriptor::alloc()->init();
            geom_desc->setVertexBuffer(v_buf);
            geom_desc->setVertexBufferOffset(v_off);
            geom_desc->setVertexStride(sizeof(float) * 3);
            geom_desc->setVertexFormat(MTL::AttributeFormatFloat3);
            geom_desc->setIndexBuffer(i_buf);
            geom_desc->setIndexBufferOffset(i_off);
            geom_desc->setIndexType(MTL::IndexTypeUInt32);
            geom_desc->setTriangleCount(face_count);

            NS::Array *geom_array = NS::Array::array(
                (const NS::Object *const *) &geom_desc, 1);

            // Create BLAS descriptor
            auto *blas_desc =
                MTL::PrimitiveAccelerationStructureDescriptor::alloc()->init();
            blas_desc->setGeometryDescriptors(geom_array);

            // Get BLAS sizes
            auto sizes = device->accelerationStructureSizes(blas_desc);

            // Allocate BLAS
            auto *blas = device->newAccelerationStructure(sizes.accelerationStructureSize);

            // Allocate scratch buffer
            auto *scratch = device->newBuffer(sizes.buildScratchBufferSize,
                                               MTL::ResourceStorageModePrivate);

            // Build BLAS
            auto *cb = queue->commandBuffer();
            auto *enc = cb->accelerationStructureCommandEncoder();
            enc->buildAccelerationStructure(blas, blas_desc, scratch, 0);
            enc->endEncoding();
            cb->commit();
            cb->waitUntilCompleted();

            scratch->release();
            geom_desc->release();
            blas_desc->release();

            blas_vec.push_back(blas);
            state->blas_list.push_back(blas);
            state->resources.push_back(v_buf);
            state->resources.push_back(i_buf);
            state->resources.push_back(blas);

            // Create instance descriptor
            MTL::AccelerationStructureInstanceDescriptor inst = {};
            // Identity transform
            inst.transformationMatrix.columns[0] = MTL::PackedFloat3{1,0,0};
            inst.transformationMatrix.columns[1] = MTL::PackedFloat3{0,1,0};
            inst.transformationMatrix.columns[2] = MTL::PackedFloat3{0,0,1};
            inst.transformationMatrix.columns[3] = MTL::PackedFloat3{0,0,0};
            inst.options = MTL::AccelerationStructureInstanceOptionOpaque;
            inst.mask = 0xFF;
            inst.intersectionFunctionTableOffset = 0;
            inst.accelerationStructureIndex = instance_id;
            instances.push_back(inst);

            instance_id++;
        }

        if (instances.empty()) {
            Log(Warn, "scene_metal: no meshes found, ray tracing will not work.");
            jit_metal_configure_rt(nullptr, nullptr, 0);
            return;
        }

        // ----------------------------------------------------------------
        //  2. Build TLAS (instance acceleration structure)
        // ----------------------------------------------------------------
        auto *inst_buf = device->newBuffer(
            instances.data(),
            instances.size() * sizeof(MTL::AccelerationStructureInstanceDescriptor),
            MTL::ResourceStorageModeShared);

        // Create BLAS reference array
        NS::Array *blas_array = NS::Array::array(
            (const NS::Object *const *) blas_vec.data(), blas_vec.size());

        auto *tlas_desc =
            MTL::InstanceAccelerationStructureDescriptor::alloc()->init();
        tlas_desc->setInstancedAccelerationStructures(blas_array);
        tlas_desc->setInstanceDescriptorBuffer(inst_buf);
        tlas_desc->setInstanceCount(instances.size());

        auto sizes = device->accelerationStructureSizes(tlas_desc);
        auto *tlas = device->newAccelerationStructure(sizes.accelerationStructureSize);
        auto *scratch = device->newBuffer(sizes.buildScratchBufferSize,
                                           MTL::ResourceStorageModePrivate);

        auto *cb = queue->commandBuffer();
        auto *enc = cb->accelerationStructureCommandEncoder();
        enc->buildAccelerationStructure(tlas, tlas_desc, scratch, 0);
        enc->endEncoding();
        cb->commit();
        cb->waitUntilCompleted();

        scratch->release();
        tlas_desc->release();
        inst_buf->release();

        state->tlas = tlas;
        state->resources.push_back(tlas);

        // ----------------------------------------------------------------
        //  3. Bind the TLAS to drjit for ray tracing
        // ----------------------------------------------------------------
        jit_metal_configure_rt(
            tlas,
            state->resources.data(),
            (uint32_t) state->resources.size());

        Log(Info, "scene_metal: built acceleration structure (%zu meshes, %zu instances)",
            blas_vec.size(), instances.size());
    }
}

MI_VARIANT void Scene<Float, Spectrum>::accel_release_metal() {
    if constexpr (dr::is_metal_v<Float>) {
        if (m_accel) {
            auto *state = (MetalAccelState *) m_accel;
            jit_metal_configure_rt(nullptr, nullptr, 0);

            // Release TLAS
            if (state->tlas)
                ((MTL::AccelerationStructure *) state->tlas)->release();

            // Release BLAS
            for (auto *blas : state->blas_list)
                ((MTL::AccelerationStructure *) blas)->release();

            delete state;
            m_accel = nullptr;
        }
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::PreliminaryIntersection3f
Scene<Float, Spectrum>::ray_intersect_preliminary_metal(const Ray3f &ray,
                                                        Mask active) const {
    if constexpr (dr::is_metal_v<Float>) {
        using Single = dr::float32_array_t<Float>;

        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_tmin(0.f), ray_tmax(ray.maxt);

        uint32_t args[8] = {
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_tmin.index(), ray_tmax.index()
        };

        uint32_t out[7];
        jit_metal_ray_trace(8, args, active.index(), out, 7);

        // out: [valid, distance, bary_u, bary_v, instance_id, primitive_id, geometry_id]
        PreliminaryIntersection3f pi;
        pi.t          = Float(Single::steal(out[1]));
        pi.prim_uv    = Point2f(Float(Single::steal(out[2])),
                                Float(Single::steal(out[3])));
        pi.prim_index = UInt32::steal(out[5]);
        pi.shape_index = UInt32::steal(out[4]); // instance_id maps to shape index
        pi.shape      = nullptr;  // will be resolved later

        Mask valid = Mask::steal(out[0]);
        dr::masked(pi.t, !valid) = dr::Infinity<Float>;

        return pi;
    } else {
        Throw("ray_intersect_preliminary_metal() should only be called in Metal mode.");
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::SurfaceInteraction3f
Scene<Float, Spectrum>::ray_intersect_metal(const Ray3f &ray, uint32_t ray_flags,
                                             Mask active) const {
    if constexpr (dr::is_metal_v<Float>) {
        PreliminaryIntersection3f pi = ray_intersect_preliminary_metal(ray, active);
        return pi.compute_surface_interaction(ray, ray_flags, active);
    } else {
        Throw("ray_intersect_metal() should only be called in Metal mode.");
    }
}

MI_VARIANT typename Scene<Float, Spectrum>::Mask
Scene<Float, Spectrum>::ray_test_metal(const Ray3f &ray, Mask active) const {
    if constexpr (dr::is_metal_v<Float>) {
        using Single = dr::float32_array_t<Float>;

        dr::Array<Single, 3> ray_o(ray.o), ray_d(ray.d);
        Single ray_tmin(0.f), ray_tmax(ray.maxt);

        uint32_t args[8] = {
            ray_o.x().index(), ray_o.y().index(), ray_o.z().index(),
            ray_d.x().index(), ray_d.y().index(), ray_d.z().index(),
            ray_tmin.index(), ray_tmax.index()
        };

        uint32_t out[7];
        jit_metal_ray_trace(8, args, active.index(), out, 7);

        Mask valid = Mask::steal(out[0]);

        // Release unused outputs
        for (int i = 1; i < 7; ++i)
            jit_var_dec_ref(out[i]);

        return valid;
    } else {
        Throw("ray_test_metal() should only be called in Metal mode.");
    }
}

// Stubs for acceleration parameter change (rebuild)
MI_VARIANT void Scene<Float, Spectrum>::accel_parameters_changed_metal() {
    if constexpr (dr::is_metal_v<Float>) {
        accel_release_metal();
        Properties props;
        accel_init_metal(props);
    }
}

MI_VARIANT void Scene<Float, Spectrum>::static_accel_initialization_metal() { }
MI_VARIANT void Scene<Float, Spectrum>::static_accel_shutdown_metal() { }

NAMESPACE_END(mitsuba)
