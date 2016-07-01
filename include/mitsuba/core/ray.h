#include <mitsuba/core/vector.h>
#include <mitsuba/core/math.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Simple n-dimensional ray segment data structure
 *
 * Along with the ray origin and direction, this data structure additionally
 * stores a ray segment [mint, maxt] (whose entries may include positive/negative
 * infinity), as well as the componentwise reciprocals of the ray direction.
 * That is just done for convenience, as these values are frequently required.
 *
 * \remark Important: be careful when changing the ray direction. You must
 * call \ref update() to compute the componentwise reciprocals as well, or Mitsuba's
 * ray-triangle intersection code will go haywire.
 */
template <typename _PointType, typename _VectorType> struct TRay {
    typedef _PointType                  PointType;
    typedef _VectorType                 VectorType;
    typedef typename PointType::Scalar  Scalar;

    PointType o;     ///< Ray origin
    VectorType d;    ///< Ray direction
    VectorType dRcp; ///< Componentwise reciprocals of the ray direction
    Scalar mint;     ///< Minimum position on the ray segment
    Scalar maxt;     ///< Maximum position on the ray segment

    /// Construct a new ray
    TRay() : mint(math::Epsilon), maxt(math::Infinity) { }

    /// Construct a new ray
    TRay(const PointType &o, const VectorType &d)
        : o(o), d(d), mint(math::Epsilon), maxt(math::Infinity) {
        update();
    }

    /// Construct a new ray
    TRay(const PointType &o, const VectorType &d, Scalar mint, Scalar maxt)
        : o(o), d(d), mint(mint), maxt(maxt) {
        update();
    }

    /// Copy constructor
    TRay(const TRay &ray)
        : o(ray.o), d(ray.d), dRcp(ray.dRcp), mint(ray.mint), maxt(ray.maxt) { }

    /// Copy a ray, but change the covered segment of the copy
    TRay(const TRay &ray, Scalar mint, Scalar maxt)
        : o(ray.o), d(ray.d), dRcp(ray.dRcp), mint(mint), maxt(maxt) { }

    /// Update the reciprocal ray directions after changing 'd'
    void update() { dRcp = Scalar(1.f) / d; }

    /// Return the position of a point along the ray
    PointType operator() (Scalar t) const { return o + t * d; }

    /// Return a ray that points into the opposite direction
    TRay reverse() const {
        TRay result;
        result.o = o; result.d = -d; result.dRcp = -dRcp;
        result.mint = mint; result.maxt = maxt;
        return result;
    }
};

/// Return a string representation of the bounding box
template <typename PointType, typename VectorType>
std::ostream &operator<<(std::ostream &os,
                         const TRay<PointType, VectorType> &r) {
    os << "Ray[\n"
       << "  o = " << r.o << ","
       << "  d = " << r.d << ","
       << "  mint = " << r.mint << ","
       << "  maxt = " << r.maxt
       << "]";
    return os;
}

NAMESPACE_END(mitsuba)
