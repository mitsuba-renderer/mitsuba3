#pragma once

#include <mitsuba/core/vector.h>
#include <mitsuba/core/ray.h>
#include <mitsuba/core/bsphere.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Generic n-dimensional bounding box data structure
 *
 * Maintains a minimum and maximum position along each dimension and provides
 * various convenience functions for querying and modifying them.
 *
 * This class is parameterized by the underlying point data structure,
 * which permits the use of different scalar types and dimensionalities, e.g.
 * \code
 * BoundingBox<Point3i> integer_bbox(Point3i(0, 1, 3), Point3i(4, 5, 6));
 * BoundingBox<Point2d> double_bbox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));
 * \endcode
 *
 * \tparam T The underlying point data type (e.g. \c Point2d)
 */
template <typename Point_> struct BoundingBox {
    static constexpr size_t Dimension = dr::array_size_v<Point_>;
    using Point  = Point_;
    using Value  = dr::value_t<Point>;
    using Scalar = dr::scalar_t<Point>;
    using Vector = typename Point::Vector;
    using UInt32 = dr::uint32_array_t<Value>;
    using Mask   = dr::mask_t<Value>;

    /**
     * \brief Create a new invalid bounding box
     *
     * Initializes the components of the minimum and maximum position to
     * \f$\infty\f$ and \f$-\infty\f$, respectively.
     */
    BoundingBox() { reset(); }

    /// Create a collapsed bounding box from a single point
    BoundingBox(const Point &p)
        : min(p), max(p) { }

    /// Create a bounding box from two positions
    BoundingBox(const Point &min, const Point &max)
        : min(min), max(max) { }

    /// Create a bounding box from a smaller type (e.g. vectorized from scalar).
    template <typename PointT>
    BoundingBox(const BoundingBox<PointT> &other)
        : min(other.min), max(other.max) { }

    /// Test for equality against another bounding box
    bool operator==(const BoundingBox &bbox) const {
        return dr::all_nested(dr::eq(min, bbox.min) && dr::eq(max, bbox.max));
    }

    /// Test for inequality against another bounding box
    bool operator!=(const BoundingBox &bbox) const {
        return dr::any_nested(dr::neq(min, bbox.min) || dr::neq(max, bbox.max));
    }

    /**
     * \brief Check whether this is a valid bounding box
     *
     * A bounding box \c bbox is considered to be valid when
     * \code
     * bbox.min[i] <= bbox.max[i]
     * \endcode
     * holds for each component \c i.
     */
    Mask valid() const {
        return dr::all(max >= min);
    }

    /// Check whether this bounding box has collapsed to a point, line, or plane
    Mask collapsed() const {
        return dr::any(dr::eq(min, max));
    }

    /// Return the dimension index with the index associated side length
    UInt32 major_axis() const {
        Vector d = max - min;
        UInt32 index = 0;
        Value value = d[0];

        for (uint32_t i = 1; i < Dimension; ++i) {
            auto mask = d[i] > value;
            dr::masked(index, mask) = i;
            dr::masked(value, mask) = d[i];
        }

        return index;
    }

    /// Return the dimension index with the shortest associated side length
    UInt32 minor_axis() const {
        Vector d = max - min;
        UInt32 index(0);
        Value value = d[0];

        for (uint32_t i = 1; i < Dimension; ++i) {
            Mask mask = d[i] < value;
            dr::masked(index, mask) = i;
            dr::masked(value, mask) = d[i];
        }

        return index;
    }

    /// Return the center point
    Point center() const {
        return (max + min) * Scalar(0.5f);
    }

    /**
     * \brief Calculate the bounding box extents
     * \return <tt>max - min</tt>
     */
    Vector extents() const { return max - min; }

    /// Return the position of a bounding box corner
    Point corner(size_t index) const {
        Point result;
        for (uint32_t i = 0; i < uint32_t(Dimension); ++i)
            result[i] = ((uint32_t) index & (1 << i)) ? max[i] : min[i];
        return result;
    }

    /// Calculate the n-dimensional volume of the bounding box
    Value volume() const { return dr::prod(max - min); }

    /// Calculate the 2-dimensional surface area of a 3D bounding box
    Value surface_area() const {
        if constexpr (Dimension == 3) {
            /// Fast path for n = 3
            Vector d = max - min;
            return dr::sum(dr::shuffle<1, 2, 0>(d) * d) * Scalar(2);
        } else {
            /// Generic case
            Vector d = max - min;

            Value result = Scalar(0);
            for (size_t i = 0; i < Dimension; ++i) {
                Value term = Scalar(1);
                for (size_t j = 0; j < Dimension; ++j) {
                    if (i == j)
                        continue;
                    term *= d[j];
                }
                result += term;
            }
            return result * Scalar(2);
        }
    }

    /**
     * \brief Check whether a point lies \a on or \a inside the bounding box
     *
     * \param p The point to be tested
     *
     * \tparam Strict Set this parameter to \c true if the bounding
     *                box boundary should be excluded in the test
     *
     * \remark In the Python bindings, the 'Strict' argument is a normal
     *         function parameter with default value \c False.
     */
    template <bool Strict = false, typename T, typename Result = dr::mask_t<dr::expr_t<T, Value>>>
    Result contains(const mitsuba::Point<T, Point::Size> &p) const {
        if constexpr (Strict)
            return dr::all((p > min) && (p < max));
        else
            return dr::all((p >= min) && (p <= max));
    }

    /**
     * \brief Check whether a specified bounding box lies \a on or \a within
     * the current bounding box
     *
     * Note that by definition, an 'invalid' bounding box (where min=\f$\infty\f$
     * and max=\f$-\infty\f$) does not cover any space. Hence, this method will always
     * return \a true when given such an argument.
     *
     * \tparam Strict Set this parameter to \c true if the bounding
     *                box boundary should be excluded in the test
     *
     * \remark In the Python bindings, the 'Strict' argument is a normal
     *         function parameter with default value \c False.
     */
    template <bool Strict = false, typename T, typename Result = dr::mask_t<dr::expr_t<T, Value>>>
    Result contains(const BoundingBox<mitsuba::Point<T, Point::Size>> &bbox) const {
        if constexpr (Strict)
            return dr::all((bbox.min > min) && (bbox.max < max));
        else
            return dr::all((bbox.min >= min) && (bbox.max <= max));
    }

    /**
     * \brief Check two axis-aligned bounding boxes for possible overlap.
     *
     * \param Strict Set this parameter to \c true if the bounding
     *               box boundary should be excluded in the test
     *
     * \remark In the Python bindings, the 'Strict' argument is a normal
     *         function parameter with default value \c False.
     *
     * \return \c true If overlap was detected.
     */
    template <bool Strict = false, typename T, typename Result = dr::mask_t<dr::expr_t<T, Value>>>
    Result overlaps(const BoundingBox<mitsuba::Point<T, Point::Size>> &bbox) const {
        if constexpr (Strict)
            return dr::all((bbox.min < max) && (bbox.max > min));
        else
            return dr::all((bbox.min <= max) && (bbox.max >= min));
    }

    /**
     * \brief Calculate the shortest squared distance between
     * the axis-aligned bounding box and the point \c p.
     */
    template <typename T, typename Result = dr::expr_t<T, Value>>
    Result squared_distance(const mitsuba::Point<T, Point::Size> &p) const {
        return dr::squared_norm(((min - p) & (p < min)) + ((p - max) & (p > max)));
    }

    /**
     * \brief Calculate the shortest squared distance between
     * the axis-aligned bounding box and \c bbox.
     */
    template <typename T, typename Result = dr::expr_t<T, Value>>
    Result squared_distance(const BoundingBox<mitsuba::Point<T, Point::Size>> &bbox) const {
        return dr::squared_norm(((min - bbox.max) & (bbox.max < min)) +
                                ((bbox.min - max) & (bbox.min > max)));
    }

    /**
     * \brief Calculate the shortest distance between
     * the axis-aligned bounding box and the point \c p.
     */
    template <typename T, typename Result = dr::expr_t<T, Value>>
    Result distance(const mitsuba::Point<T, Point::Size> &p) const {
        return dr::sqrt(squared_distance(p));
    }

    /**
     * \brief Calculate the shortest distance between
     * the axis-aligned bounding box and \c bbox.
     */
    template <typename T, typename Result = dr::expr_t<T, Value>>
    Result distance(const BoundingBox<mitsuba::Point<T, Point::Size>> &bbox) const {
        return dr::sqrt(squared_distance(bbox));
    }

    /**
     * \brief Mark the bounding box as invalid.
     *
     * This operation sets the components of the minimum
     * and maximum position to \f$\infty\f$ and \f$-\infty\f$,
     * respectively.
     */
    void reset() {
        min =  dr::Infinity<Value>;
        max = -dr::Infinity<Value>;
    }

    /// Clip this bounding box to another bounding box
    template <typename T>
    void clip(const BoundingBox<mitsuba::Point<T, Point::Size>> &bbox) {
        min = dr::maximum(min, bbox.min);
        max = dr::minimum(max, bbox.max);
    }

    /// Expand the bounding box to contain another point
    template <typename T>
    void expand(const mitsuba::Point<T, Point::Size> &p) {
        min = dr::minimum(min, p);
        max = dr::maximum(max, p);
    }

    /// Expand the bounding box to contain another bounding box
    template <typename T>
    void expand(const BoundingBox<mitsuba::Point<T, Point::Size>> &bbox) {
        min = dr::minimum(min, bbox.min);
        max = dr::maximum(max, bbox.max);
    }

    /// Merge two bounding boxes
    static BoundingBox merge(const BoundingBox &bbox1, const BoundingBox &bbox2) {
        return BoundingBox(
            dr::minimum(bbox1.min, bbox2.min),
            dr::maximum(bbox1.max, bbox2.max)
        );
    }

    /**
     * \brief Check if a ray intersects a bounding box
     *
     * Note that this function ignores the <tt>maxt</tt> value
     * associated with the ray.
     */
    template <typename Ray>
    MI_INLINE auto ray_intersect(const Ray &ray) const {
        using Float  = typename Ray::Float;
        using Vector = typename Ray::Vector;

        /* First, ensure that the ray either has a nonzero slope on each axis,
           or that its origin on a zero-valued axis is within the box bounds */
        auto active = dr::all(dr::neq(ray.d, dr::zeros<Vector>()) || ((ray.o > min) || (ray.o < max)));

        // Compute intersection intervals for each axis
        Vector d_rcp = dr::rcp(ray.d),
               t1 = (min - ray.o) * d_rcp,
               t2 = (max - ray.o) * d_rcp;

        // Ensure proper ordering
        Vector t1p = dr::minimum(t1, t2),
               t2p = dr::maximum(t1, t2);

        // Intersect intervals
        Float mint = dr::max(t1p),
              maxt = dr::min(t2p);

        active = active && maxt >= mint;

        return std::make_tuple(active, mint, maxt);
    }

    /// Create a bounding sphere, which contains the axis-aligned box
    BoundingSphere<Point> bounding_sphere() const {
        Point c = center();
        return { c, dr::norm(c - max) };
    }

    Point min; /// Component-wise minimum
    Point max; /// Component-wise maximum
};

/// Print a string representation of the bounding box
template <typename Point>
std::ostream &operator<<(std::ostream &os, const BoundingBox<Point> &bbox) {
    os << "BoundingBox" << type_suffix<Point>();
    if (dr::all(!bbox.valid()))
        os << "[invalid]";
    else
        os << "[" << std::endl
           << "  min = " << bbox.min << "," << std::endl
           << "  max = " << bbox.max << std::endl
           << "]";
    return os;
}

NAMESPACE_END(mitsuba)
