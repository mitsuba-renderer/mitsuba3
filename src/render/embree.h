/*
    embree.h -- Embree user-geometry callbacks for Mitsuba custom shapes.

    Scalar variants intersect custom shapes by calling the shape's C++
    implementation from the callback. JIT variants instead let Dr.Jit compile
    the routine recorded by SceneIRBuilder (see jit_isect_bind()). Their
    geometry user pointer is a binding record, and the thunks below forward
    the callback to the code pointer stored in it.
*/

#pragma once

#include <embree3/rtcore.h>
#include <mitsuba/render/shape.h>
#include "accel_cpu_common.h"

NAMESPACE_BEGIN(mitsuba)

template <typename ScalarBoundingBox3f>
void embree_store_bounds(const RTCBoundsFunctionArguments *args,
                         const ScalarBoundingBox3f &bbox) {
    RTCBounds *bounds_o = args->bounds_o;
    bounds_o->lower_x = (float) bbox.min.x();
    bounds_o->lower_y = (float) bbox.min.y();
    bounds_o->lower_z = (float) bbox.min.z();
    bounds_o->upper_x = (float) bbox.max.x();
    bounds_o->upper_y = (float) bbox.max.y();
    bounds_o->upper_z = (float) bbox.max.z();
}

/// Bounds callback of scalar variants: the geometry user pointer is the shape
template <typename Float, typename Spectrum>
void embree_bbox(const RTCBoundsFunctionArguments *args) {
    MI_IMPORT_TYPES(Shape)
    const Shape *shape = (const Shape *) args->geometryUserPtr;
    embree_store_bounds(args, shape->bbox(args->primID));
}

/// Bounds callback of JIT variants: the geometry user pointer is the
/// intersection binding record, which stores the shape in its user field
template <typename Float, typename Spectrum>
void embree_bbox_jit(const RTCBoundsFunctionArguments *args) {
    MI_IMPORT_TYPES(Shape)
    const Shape *shape = (const Shape *)
        ((const JitIsectBinding *) args->geometryUserPtr)->user;
    embree_store_bounds(args, shape->bbox(args->primID));
}

/**
 * Intersect/occlusion callback for a single ray. Shadow rays with SkipNull pass
 * through shapes with null transmission and record the encounter in the
 * ray's flags word instead of being occluded.
 */
template <typename Float, typename Spectrum>
void embree_intersect_scalar(int* valid,
                             void* geometryUserPtr,
                             unsigned int geomID,
                             unsigned int instID,
                             unsigned int primID,
                             RTCRay* rtc_ray,
                             RTCHit* rtc_hit) {
    MI_IMPORT_TYPES(Shape)

    const Shape* shape = (const Shape*) geometryUserPtr;

    if (!valid[0])
        return;

    Ray3f ray = dr::zeros<Ray3f>();
    ray.o.x() = rtc_ray->org_x;
    ray.o.y() = rtc_ray->org_y;
    ray.o.z() = rtc_ray->org_z;
    ray.d.x() = rtc_ray->dir_x;
    ray.d.y() = rtc_ray->dir_y;
    ray.d.z() = rtc_ray->dir_z;
    ray.time  = rtc_ray->time;

    ray.o += ray.d * rtc_ray->tnear;
    ray.maxt = rtc_ray->tfar - rtc_ray->tnear;

    if (rtc_hit) {
        PreliminaryIntersection3f pi = shape->ray_intersect_preliminary(ray, primID, true);
        if (dr::all(pi.is_valid())) {
            rtc_ray->tfar      = (float) dr::slice(pi.t);
            rtc_hit->u         = (float) dr::slice(pi.prim_uv.x());
            rtc_hit->v         = (float) dr::slice(pi.prim_uv.y());
            rtc_hit->geomID    = geomID;
            rtc_hit->primID    = primID;
            rtc_hit->instID[0] = instID;
#if !defined(NDEBUG)
            rtc_hit->Ng_x      = 0.f;
            rtc_hit->Ng_y      = 0.f;
            rtc_hit->Ng_z      = 0.f;
#endif
        }
    } else {
        if (dr::all(shape->ray_test(ray, primID, true))) {
            if ((rtc_ray->flags & CPURayFlags::SkipNull) &&
                shape->has_null()) {
                rtc_ray->flags |= CPURayFlags::HasNull;
                return;
            }
            rtc_ray->tfar = -dr::Infinity<float>;
        }
    }
}

template <typename Float, typename Spectrum>
void embree_intersect(const RTCIntersectFunctionNArguments* args) {
    if (args->N != 1)
        Throw("embree_intersect(): unsupported packet size!");

    embree_intersect_scalar<Float, Spectrum>(
        args->valid, args->geometryUserPtr, args->geomID,
        args->context->instID[0], args->primID,
        &((RTCRayHit *) args->rayhit)->ray,
        &((RTCRayHit *) args->rayhit)->hit);
}

template <typename Float, typename Spectrum>
void embree_occluded(const RTCOccludedFunctionNArguments* args) {
    if (args->N != 1)
        Throw("embree_occluded(): unsupported packet size!");

    embree_intersect_scalar<Float, Spectrum>(
        args->valid, args->geometryUserPtr, args->geomID,
        args->context->instID[0], args->primID,
        (RTCRay *) args->ray,
        (RTCHit *) nullptr);
}

/**
 * JIT variants: forward the callback to the intersection function compiled by
 * Dr.Jit. The code pointer is resolved when a kernel that traces the scene is
 * launched, hence it is null only if Embree runs ahead of that (a frozen
 * function replayed against a scene with shapes it never recorded).
 */
inline void embree_isect_jit(const JitIsectBinding *binding, const void *args,
                             unsigned int N, int mode) {
#if !defined(NDEBUG)
    if (N != jit_llvm_vector_width())
        Throw("embree_isect_jit(): packet size %u does not match the "
              "Dr.Jit vector width %u!", N, jit_llvm_vector_width());
#else
    (void) N;
#endif
    if (unlikely(!binding->code))
        Throw("Embree invoked the intersection callback of a custom shape "
              "whose intersection routine has not been compiled for the "
              "running kernel. This can happen when a frozen function is "
              "replayed with a scene containing shape types that it did not "
              "contain during recording.");

    ((void (*)(const void *, int)) binding->code)(args, mode);
}

inline void embree_intersect_jit(const RTCIntersectFunctionNArguments *args) {
    embree_isect_jit((const JitIsectBinding *) args->geometryUserPtr, args,
                     args->N, 0);
}

/// The generated code blocks every hit by setting the ray's far distance to -inf
inline void embree_occluded_jit(const RTCOccludedFunctionNArguments *args) {
    embree_isect_jit((const JitIsectBinding *) args->geometryUserPtr, args,
                     args->N, 1);
}

/**
 * Occlusion callback of shapes with null transmission. Shadow rays with
 * SkipNull pass through them and record the encounter in the ray's flags word.
 */
inline void embree_occluded_null_jit(const RTCOccludedFunctionNArguments *args) {
    const JitIsectBinding *binding =
        (const JitIsectBinding *) args->geometryUserPtr;
    unsigned int N = args->N;

    float *tfar = &RTCRayN_tfar(args->ray, N, 0), tfar_prev[16];
    unsigned int *flags = &RTCRayN_flags(args->ray, N, 0);
    memcpy(tfar_prev, tfar, N * sizeof(float));

    embree_isect_jit(binding, args, N, 1);

    for (unsigned int i = 0; i < N; ++i) {
        if (tfar[i] == tfar_prev[i] || !(flags[i] & CPURayFlags::SkipNull))
            continue;
        tfar[i] = tfar_prev[i];
        flags[i] |= CPURayFlags::HasNull;
    }
}

NAMESPACE_END(mitsuba)
