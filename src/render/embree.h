/*
    embree.h -- Embree user-geometry callbacks for Mitsuba custom shapes.
*/

#pragma once

#include <embree3/rtcore.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
void embree_bbox(const struct RTCBoundsFunctionArguments* args) {
    MI_IMPORT_TYPES(Shape)
    const Shape* shape = (const Shape*) args->geometryUserPtr;
    ScalarBoundingBox3f bbox = shape->bbox(args->primID);
    RTCBounds* bounds_o = args->bounds_o;
    bounds_o->lower_x = (float) bbox.min.x();
    bounds_o->lower_y = (float) bbox.min.y();
    bounds_o->lower_z = (float) bbox.min.z();
    bounds_o->upper_x = (float) bbox.max.x();
    bounds_o->upper_y = (float) bbox.max.y();
    bounds_o->upper_z = (float) bbox.max.z();
}

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
        if (dr::all(shape->ray_test(ray, primID, true)))
            rtc_ray->tfar = -dr::Infinity<float>;
    }
}

template <typename Float, typename Spectrum, size_t N, typename RTCRay_, typename RTCHit_>
static void embree_intersect_packet(int *valid, void *geometryUserPtr,
                                    unsigned int geomID,
                                    unsigned int instID,
                                    unsigned int primID,
                                    RTCRay_ *rtc_ray,
                                    RTCHit_ *rtc_hit) {
    MI_IMPORT_TYPES(Shape)

    using FloatP   = dr::Packet<dr::scalar_t<Float>, N>;
    using MaskP    = dr::mask_t<FloatP>;
    using Point2fP = Point<FloatP, 2>;
    using Point3fP = Point<FloatP, 3>;
    using Ray3fP   = Ray<Point<FloatP, 3>, Spectrum>;
    using UInt32P  = dr::uint32_array_t<FloatP>;
    using Float32P = dr::Packet<dr::scalar_t<Float32>, N>;

    const Shape* shape = (const Shape*) geometryUserPtr;

    MaskP active = dr::load_aligned<UInt32P>(valid) != 0;
    if (dr::none(active))
        return;

    Ray3fP ray;
    ray.o.x() = dr::load_aligned<Float32P>(rtc_ray->org_x);
    ray.o.y() = dr::load_aligned<Float32P>(rtc_ray->org_y);
    ray.o.z() = dr::load_aligned<Float32P>(rtc_ray->org_z);
    ray.d.x() = dr::load_aligned<Float32P>(rtc_ray->dir_x);
    ray.d.y() = dr::load_aligned<Float32P>(rtc_ray->dir_y);
    ray.d.z() = dr::load_aligned<Float32P>(rtc_ray->dir_z);
    ray.time  = dr::load_aligned<Float32P>(rtc_ray->time);

    Float32P tnear = dr::load_aligned<Float32P>(rtc_ray->tnear),
             tfar  = dr::load_aligned<Float32P>(rtc_ray->tfar);
    ray.o += ray.d * tnear;
    ray.maxt = tfar - tnear;

    if (rtc_hit) {
        auto [hit_mask, t, prim_uv, s_idx, p_idx] =
            shape->ray_intersect_preliminary_packet(ray, primID, active);
        active &= hit_mask;
        dr::store_aligned(rtc_ray->tfar,      Float32P(dr::select(active, t,           ray.maxt)));
        dr::store_aligned(rtc_hit->u,         Float32P(dr::select(active, prim_uv.x(), dr::load_aligned<Float32P>(rtc_hit->u))));
        dr::store_aligned(rtc_hit->v,         Float32P(dr::select(active, prim_uv.y(), dr::load_aligned<Float32P>(rtc_hit->v))));
        dr::store_aligned(rtc_hit->geomID,    dr::select(active, UInt32P(geomID), dr::load_aligned<UInt32P>(rtc_hit->geomID)));
        dr::store_aligned(rtc_hit->primID,    dr::select(active, UInt32P(primID), dr::load_aligned<UInt32P>(rtc_hit->primID)));
        dr::store_aligned(rtc_hit->instID[0], dr::select(active, UInt32P(instID), dr::load_aligned<UInt32P>(rtc_hit->instID[0])));
    } else {
        active &= shape->ray_test_packet(ray, primID, active);
        dr::store_aligned(rtc_ray->tfar, Float32P(dr::select(active, -dr::Infinity<Float>, tfar)));
    }
}

template <typename Float, typename Spectrum>
void embree_intersect(const RTCIntersectFunctionNArguments* args) {
    switch (args->N) {
        case 1:
            embree_intersect_scalar<Float, Spectrum>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                &((RTCRayHit *) args->rayhit)->ray,
                &((RTCRayHit *) args->rayhit)->hit);
            break;

        case 4:
            embree_intersect_packet<Float, Spectrum, 4>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                &((RTCRayHit4 *) args->rayhit)->ray,
                &((RTCRayHit4 *) args->rayhit)->hit);
            break;

        case 8:
            embree_intersect_packet<Float, Spectrum, 8>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                &((RTCRayHit8 *) args->rayhit)->ray,
                &((RTCRayHit8 *) args->rayhit)->hit);
            break;

        case 16:
            embree_intersect_packet<Float, Spectrum, 16>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                &((RTCRayHit16 *) args->rayhit)->ray,
                &((RTCRayHit16 *) args->rayhit)->hit);
            break;

        default:
            Throw("embree_intersect(): unsupported packet size!");
    }
}

template <typename Float, typename Spectrum>
void embree_occluded(const RTCOccludedFunctionNArguments* args) {
    switch (args->N) {
        case 1:
            embree_intersect_scalar<Float, Spectrum>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                (RTCRay *) args->ray,
                (RTCHit *) nullptr);
            break;

        case 4:
            embree_intersect_packet<Float, Spectrum, 4>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                (RTCRay4 *) args->ray,
                (RTCHit4 *) nullptr);
            break;

        case 8:
            embree_intersect_packet<Float, Spectrum, 8>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                (RTCRay8 *) args->ray,
                (RTCHit8 *) nullptr);
            break;

        case 16:
            embree_intersect_packet<Float, Spectrum, 16>(
                args->valid, args->geometryUserPtr, args->geomID,
                args->context->instID[0], args->primID,
                (RTCRay16 *) args->ray,
                (RTCHit16 *) nullptr);
            break;

        default:
            Throw("embree_occluded(): unsupported packet size!");
    }
}

NAMESPACE_END(mitsuba)
