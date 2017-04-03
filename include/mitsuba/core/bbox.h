#pragma once

#include <mitsuba/core/vector.h>
#include <mitsuba/core/ray.h>

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
 * BoundingBox<Vector3i> integerBBox(Point3i(0, 1, 3), Point3i(4, 5, 6));
 * BoundingBox<Vector2d> doubleBBox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));
 * \endcode
 *
 * \tparam T The underlying point data type (e.g. \c Point2d)
 */
template <typename Point_> struct BoundingBox {
    static constexpr size_t Size = Point_::Size;

    using Point  = Point_;
    using Scalar = value_t<Point>;
    using Vector = typename Point::Vector;

    /**
     * \brief Create a new invalid bounding box
     *
     * Initializes the components of the minimum
     * and maximum position to \f$\infty\f$ and \f$-\infty\f$,
     * respectively.
     */
    BoundingBox() { reset(); }

    /// Create a collapsed bounding box from a single point
    BoundingBox(const Point &p)
        : min(p), max(p) { }

    /// Create a bounding box from two positions
    BoundingBox(const Point &min, const Point &max)
        : min(min), max(max) { }

    /// Test for equality against another bounding box
    bool operator==(const BoundingBox &bbox) const {
        return all(eq(min, bbox.min) & eq(max, bbox.max));
    }

    /// Test for inequality against another bounding box
    bool operator!=(const BoundingBox &bbox) const {
        return any(neq(min, bbox.min) | neq(max, bbox.max));
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
    bool valid() const {
        return all(max >= min);
    }

    /// Check whether this bounding box has collapsed to a point, line, or plane
    bool collapsed() const {
        return any(eq(min, max));
    }

    /// Return the dimension index with the largest associated side length
    int major_axis() const {
        Vector d = max - min;
        int largest = 0;
        for (int i = 1; i < int(Size); ++i)
            if (d[i] > d[largest])
                largest = i;
        return largest;
    }

    /// Return the dimension index with the shortest associated side length
    int minor_axis() const {
        Vector d = max - min;
        int shortest = 0;
        for (int i = 1; i < int(Size); ++i)
            if (d[i] < d[shortest])
                shortest = i;
        return shortest;
    }

    /// Return the center point
    Point center() const {
        return (max + min) * Scalar(0.5);
    }

    /**
     * \brief Calculate the bounding box extents
     * \return max-min
     */
    Vector extents() const {
        return max - min;
    }

    /// Return the position of a bounding box corner
    Point corner(size_t index) const {
        Point result;
        for (int i = 0; i < int(Size); ++i)
            result[i] = ((int) index & (1 << i)) ? max[i] : min[i];
        return result;
    }

    /// Calculate the n-dimensional volume of the bounding box
    Scalar volume() const { return hprod(max - min); }

    /// Calculate the n-1 dimensional volume of the boundary
    template <typename T = Point, typename std::enable_if<T::Size == 3, int>::type = 0>
    Scalar surface_area() const {
        /* Optimized implementation for Size == 3 */
        Vector d = max - min;
        return hsum(enoki::shuffle<1, 2, 0>(d) * d) * Scalar(2);
    }

    /// Calculate the n-1 dimensional volume of the boundary
    template <typename T = Point, typename std::enable_if<T::Size != 3, int>::type = 0>
    Scalar surface_area() const {
        /* Generic implementation for Size != 3 */
        Vector d = max - min;

        Scalar result = Scalar(0);
        for (size_t i = 0; i <  Size; ++i) {
            Scalar term = Scalar(1);
            for (size_t j = 0; j < Size; ++j) {
                if (i == j)
                    continue;
                term *= d[j];
            }
            result += term;
        }
        return Scalar(2) * result;
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
    template <bool Strict = false>
    bool contains(const Point &p) const {
        if (Strict)
            return all(p > min & p < max);
        else
            return all(p >= min & p <= max);
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
    template <bool Strict = false>
    bool contains(const BoundingBox &bbox) const {
        if (Strict)
            return all(bbox.min > min & bbox.max < max);
        else
            return all(bbox.min >= min & bbox.max <= max);
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
    template <bool Strict = false>
    bool overlaps(const BoundingBox &bbox) const {
        if (Strict)
            return all(bbox.min < max & bbox.max > min);
        else
            return all(bbox.min <= max & bbox.max >= min);
    }

    /**
     * \brief Calculate the shortest squared distance between
     * the axis-aligned bounding box and the point \c p.
     */
    Scalar squared_distance(const Point &p) const {
        return squared_norm(((p < min) & (min - p)) +
                            ((p > max) & (p - max)));
    }


    /**
     * \brief Calculate the shortest squared distance between
     * the axis-aligned bounding box and \c bbox.
     */
    Scalar squared_distance(const BoundingBox &bbox) const {
        return squared_norm(((bbox.max < min) & (min - bbox.max)) +
                            ((bbox.min > max) & (bbox.min - max)));
    }

    /**
     * \brief Calculate the shortest distance between
     * the axis-aligned bounding box and the point \c p.
     */
    Scalar distance(const Point &p) const {
        return std::sqrt(squared_distance(p));
    }

    /**
     * \brief Calculate the shortest distance between
     * the axis-aligned bounding box and \c bbox.
     */
    Scalar distance(const BoundingBox &bbox) const {
        return std::sqrt(squared_distance(bbox));
    }

    /**
     * \brief Mark the bounding box as invalid.
     *
     * This operation sets the components of the minimum
     * and maximum position to \f$\infty\f$ and \f$-\infty\f$,
     * respectively.
     */
    void reset() {
        min =  std::numeric_limits<Scalar>::infinity();
        max = -std::numeric_limits<Scalar>::infinity();
    }

    /// Clip this bounding box to another bounding box
    void clip(const BoundingBox &bbox) {
        this->min = enoki::max(this->min, bbox.min);
        this->max = enoki::min(this->max, bbox.max);
    }

    /// Expand the bounding box to contain another point
    void expand(const Point &p) {
        this->min = enoki::min(this->min, p);
        this->max = enoki::max(this->max, p);
    }

    /// Expand the bounding box to contain another bounding box
    void expand(const BoundingBox &bbox) {
        this->min = enoki::min(this->min, bbox.min);
        this->max = enoki::max(this->max, bbox.max);
    }

    /// Merge two bounding boxes
    static BoundingBox merge(const BoundingBox &bbox1, const BoundingBox &bbox2) {
        return BoundingBox(
            enoki::min(bbox1.min, bbox2.min),
            enoki::max(bbox1.max, bbox2.max)
        );
    }

    /// Check if a ray intersects a bounding box
    template <typename Ray>
    MTS_INLINE auto ray_intersect(const Ray &ray) const {
        using Vector = typename Ray::Vector;
        using Point = typename Ray::Point;
        using Scalar = value_t<Point>;
        using Mask = mask_t<Scalar>;

        /* First, ensure that the ray either has a nonzero slope on each axis,
           or that its origin on a zero-valued axis is within the box bounds */
        Mask active = all(neq(ray.d, zero<Vector>()) | (ray.o > min | ray.o < max));

        /* Compute intersection intervals for each axis */
        Vector t1 = (min - ray.o) * ray.d_rcp;
        Vector t2 = (max - ray.o) * ray.d_rcp;

        /* Ensure proper ordering */
        Vector t1p = enoki::min(t1, t2);
        Vector t2p = enoki::max(t1, t2);

        /* Intersect intervals */
        Scalar nearT = hmax(t1p);
        Scalar farT  = hmin(t2p);

        active &= ray.mint <= farT & nearT <= ray.maxt;

        return std::make_tuple(active, nearT, farT);
    }

    Point min; ///< Component-wise minimum
    Point max; ///< Component-wise maximum
};

/// Print a string representation of the bounding box
template <typename Point>
std::ostream &operator<<(std::ostream &os, const BoundingBox<Point> &bbox) {
    os << "BoundingBox" << type_suffix<Point>();
    if (!bbox.valid())
        os << "[invalid]";
    else
        os << "[min = " << bbox.min << ", max = " << bbox.max << "]";
    return os;
}

NAMESPACE_END(mitsuba)
