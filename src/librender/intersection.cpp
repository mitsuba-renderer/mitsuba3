#include <mitsuba/render/intersection.h>

#include <mitsuba/render/scene.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Point3>
void Intersection<Point3>::compute_partials(const RayDifferential3 & /*ray*/,
                                            const Mask & /*mask*/) {
    // TODO: vectorized implementation
    Log(EError, "Not implemented yet");

    // // Compute the texture coordinates partials wrt. changes in
    // // the screen-space position. Based on PBRT.
    // if (has_uv_partials || !ray.has_differentials) {
    //     return;
    // }

    // has_uv_partials = true;

    // // Mask active = enoki::eq(dpdu, enoki::zero<Vector3>()) &&
    // //               enoki::eq(dpdv, enoki::zero<Vector3>());
    // Mask active = (dpdu == enoki::zero<Vector3>()) &&
    //               (dpdv == enoki::zero<Vector3>());
    // if (none(active)) {
    //     dudx = dvdx = dudy = dvdy = Value(0);
    //     return;
    // }

    // // Compute a few projections onto the surface normal
    // Value pp, pox, poy, prx, pry;
    // pp/*(active)*/  = enoki::dot(geo_frame.n/*(active)*/, Vector3(p)/*(active)*/);
    // pox/*(active)*/ = enoki::dot(geo_frame.n/*(active)*/, Vector3(ray.o_x)/*(active)*/);
    // poy/*(active)*/ = enoki::dot(geo_frame.n/*(active)*/, Vector3(ray.o_y)/*(active)*/);
    // prx/*(active)*/ = enoki::dot(geo_frame.n/*(active)*/, ray.d_x/*(active)*/);
    // pry/*(active)*/ = enoki::dot(geo_frame.n/*(active)*/, ray.d_y/*(active)*/);

    // active &= !(eq(prx, enoki::zero<Value>()) | eq(pry, enoki::zero<Value>()));
    // if (unlikely(none(active))) {
    //     dudx = dvdx = dudy = dvdy = Value(0);
    //     return;
    // }

    // // Compute ray-plane intersections against the offset rays
    // Value tx, ty;
    // tx/*(active)*/ = (pp/*(active)*/ - pox/*(active)*/) / prx/*(active)*/;
    // ty/*(active)*/ = (pp/*(active)*/ - poy/*(active)*/) / pry/*(active)*/;

    // // Calculate the U and V partials by solving two out
    // // of a set of 3 equations in an overconstrained system.
    // Value absX, absY, absZ;
    // // TODO: is this the correct access?
    // absX/*(active)*/ = enoki::abs(geo_frame.n/*(active)*/[0]);
    // absY/*(active)*/ = enoki::abs(geo_frame.n/*(active)*/[1]);
    // absZ/*(active)*/ = enoki::abs(geo_frame.n/*(active)*/[2]);

    // // TODO: we can probably use a better layout than this.
    // Value A[2][2], Bx[2], By[2], x[2];
    // like_t<Value, int> axes[2];

    // // TODO: vectorize this (masks, select)
    // // if (absX > absY && absX > absZ) {
    // //     axes[0] = 1; axes[1] = 2;
    // // } else if (absY > absZ) {
    // //     axes[0] = 0; axes[1] = 2;
    // // } else {
    // //     axes[0] = 0; axes[1] = 1;
    // // }
    // auto selector = absX > absY && absX > absZ;
    // axes[0] = select(active && selector, 1, 0);
    // axes[1] = select(active && (selector || absY > absZ), 2, 1);

    // // TODO: is this the correct access?
    // A[0][0]/*(active)*/ = gather<Value>(&dpdu[0], axes[0], active);
    // A[0][1]/*(active)*/ = gather<Value>(&dpdv[0], axes[0], active);
    // A[1][0]/*(active)*/ = gather<Value>(&dpdu[0], axes[1], active);
    // A[1][1]/*(active)*/ = gather<Value>(&dpdv[0], axes[1], active);

    // // Auxilary intersection point of the adjacent rays
    // Point3 px, py;
    // px/*(active)*/ = ray.o_x/*(active)*/ + ray.d_x/*(active)*/ * tx/*(active)*/;
    // py/*(active)*/ = ray.o_y/*(active)*/ + ray.d_y/*(active)*/ * ty/*(active)*/;

    // // Bx[0] = px[axes[0]] - p[axes[0]];
    // // Bx[1] = px[axes[1]] - p[axes[1]];
    // // By[0] = py[axes[0]] - p[axes[0]];
    // // By[1] = py[axes[1]] - p[axes[1]];
    // Bx[0]/*(active)*/ = gather<Value>(&px[0], axes[0], active) -
    //                     gather<Value>(&p[0], axes[0], active);
    // Bx[1]/*(active)*/ = gather<Value>(&px[0], axes[1], active) -
    //                     gather<Value>(&p[0], axes[1], active);
    // By[0]/*(active)*/ = gather<Value>(&py[0], axes[0], active) -
    //                     gather<Value>(&p[0], axes[0], active);
    // By[1]/*(active)*/ = gather<Value>(&py[0], axes[1], active) -
    //                     gather<Value>(&p[0], axes[1], active);

    // selector = active && math::solve_linear_2x2(A, Bx, x);
    // dudx = select(selector, x[0], Value(1));
    // dvdx = select(selector, x[1], Value(0));

    // selector = active && math::solve_linear_2x2(A, By, x);
    // dudy = select(selector, x[0], Value(0));
    // dvdy = select(selector, x[1], Value(1));
}

// -----------------------------------------------------------------------------

template <typename Point3>
std::ostream &operator<<(std::ostream &os, const Intersection<Point3> &it) {
    if (enoki::any(it.is_valid())) {
        os << "Intersection[invalid]";
        return os;
    }

    os << "Intersection[" << std::endl
       << "  p = " << it.p << "," << std::endl
       << "  wi = " << it.wi << "," << std::endl
       << "  t = " << it.t << "," << std::endl
       << "  geo_frame = " << it.geo_frame << "," << std::endl
       << "  sh_frame = " << it.sh_frame << "," << std::endl
       << "  uv = " << it.uv << "," << std::endl
       << "  has_uv_partials = " << it.has_uv_partials << "," << std::endl
       << "  dpdu = " << it.dpdu << "," << std::endl
       << "  dpdv = " << it.dpdv << "," << std::endl;
    if (enoki::any(it.has_uv_partials)) {
        os << "  dud[x,y] = [" << it.dudx << ", " << it.dudy << "]," << std::endl
           << "  dvd[x,y] = [" << it.dvdx << ", " << it.dvdy << "]," << std::endl;
    }
    os << "  time = " << it.time << "," << std::endl
       << "  shape = " << it.shape << std::endl
       << "]";
    return os;
}

// -----------------------------------------------------------------------------

// template struct MTS_EXPORT_RENDER Intersection<Point3f>;
template struct MTS_EXPORT_RENDER Intersection<Point3fP>;
// template struct MTS_EXPORT_RENDER Intersection<Point3fX>;

// template MTS_EXPORT_RENDER std::ostream &operator<<(
//     std::ostream &, const Intersection<Point3f> &);
// template MTS_EXPORT_RENDER std::ostream &operator<<(
//     std::ostream &, const Intersection<Point3fP> &);
// template MTS_EXPORT_RENDER std::ostream &operator<<(
//     std::ostream &, const Intersection<Point3fX> &);

NAMESPACE_END(mitsuba)
