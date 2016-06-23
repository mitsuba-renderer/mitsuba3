#pragma once

#include <mitsuba/core/vector.h>

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
 * TBoundingBox<Vector3i> integerBBox(Point3i(0, 1, 3), Point3i(4, 5, 6));
 * TBoundingBox<Vector2d> doubleBBox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));
 * \endcode
 *
 * \tparam T The underlying point data type (e.g. \c Point2d)
 * \ingroup libcore
 */
template <typename _PointType> struct TBoundingBox {
    enum {
        Dimension = _PointType::Dimension
    };

    typedef _PointType                             PointType;
    typedef typename PointType::Scalar             Scalar;
    typedef typename PointType::VectorType         VectorType;

    /**
     * \brief Create a new invalid bounding box
     *
     * Initializes the components of the minimum
     * and maximum position to \f$\infty\f$ and \f$-\infty\f$,
     * respectively.
     */
    TBoundingBox() {
        reset();
    }

	/// Unserialize a bounding box from a binary data stream
	//TBoundingBox(Stream *stream) { // XXX
		//min = PointType(stream);
		//max = PointType(stream);
	//}

    /// Create a collapsed bounding box from a single point
    TBoundingBox(const PointType &p)
        : min(p), max(p) { }

    /// Create a bounding box from two positions
    TBoundingBox(const PointType &min, const PointType &max)
        : min(min), max(max) { }

    /// Test for equality against another bounding box
    bool operator==(const TBoundingBox &bbox) const {
        return all(cwiseEqual(min, bbox.min) & cwiseEqual(max, bbox.max));
    }

    /// Test for inequality against another bounding box
    bool operator!=(const TBoundingBox &bbox) const {
        return any(cwiseNotEqual(min, bbox.min) | cwiseNotEqual(max, bbox.max));
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

    /// Check whether this bounding box has collapsed to a single point
    bool collapsed() const {
        return min == max;
    }

    /// Return the dimension index with the largest associated side length
    int majorAxis() const {
        VectorType d = max - min;
        int largest = 0;
        for (int i = 1; i < Dimension; ++i)
            if (d[i] > d[largest])
                largest = i;
        return largest;
    }

    /// Return the dimension index with the shortest associated side length
    int minorAxis() const {
        VectorType d = max - min;
        int shortest = 0;
        for (int i = 1; i < Dimension; ++i)
            if (d[i] < d[shortest])
                shortest = i;
        return shortest;
    }

    /// Return the center point
    PointType center() const {
        return (max + min) * Scalar(0.5);
    }

    /**
     * \brief Calculate the bounding box extents
     * \return max-min
     */
    VectorType extents() const {
        return max - min;
    }

    /// Return the position of a bounding box corner
    PointType corner(int index) const {
        PointType result;
        for (int i = 0; i < Dimension; ++i)
            result[i] = (index & (1 << i)) ? max[i] : min[i];
        return result;
    }

    /// Calculate the n-dimensional volume of the bounding box
    Scalar volume() const {
        return hprod(max - min);
    }

    /// Calculate the n-1 dimensional volume of the boundary
    template <typename T = PointType, typename std::enable_if<T::Dimension == 3, int>::type = 0>
    Scalar surfaceArea() const {
        /* Optimized implementation for Dimension == 3 */
        VectorType d = max - min;
        return hsum(d.template swizzle<1,2,0>() * d) * Scalar(2);
    }

    /// Calculate the n-1 dimensional volume of the boundary
    template <typename T = PointType, typename std::enable_if<T::Dimension != 3, int>::type = 0>
    Scalar surfaceArea() const {
        /* Generic implementation for Dimension != 3 */
        VectorType d = max - min;

        Scalar result = Scalar(0);
        for (int i = 0; i < Dimension; ++i) {
            Scalar term = Scalar(1);
            for (int j = 0; j < Dimension; ++j) {
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
    bool contains(const PointType &p) const {
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
    bool contains(const TBoundingBox &bbox) const {
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
    bool overlaps(const TBoundingBox &bbox) const {
        if (Strict)
            return all(bbox.min < max & bbox.max > min);
        else
            return all(bbox.min <= max & bbox.max >= min);
    }

    /**
     * \brief Calculate the shortest squared distance between
     * the axis-aligned bounding box and the point \c p.
     */
    Scalar squaredDistance(const PointType &p) const {
        auto d1 = select(p < min, min - p, PointType::Zero());
        auto d2 = select(p > max, p - max, PointType::Zero());
        auto d = d1 + d2;
        return hsum(d * d);
    }


    /**
     * \brief Calculate the shortest squared distance between
     * the axis-aligned bounding box and \c bbox.
     */
    Scalar squaredDistance(const TBoundingBox &bbox) const {
        auto d1 = select(bbox.max < min, min - bbox.max, PointType::Zero());
        auto d2 = select(bbox.min > max, bbox.min - max, PointType::Zero());
        auto d = d1 + d2;
        return hsum(d * d);
    }

    /**
     * \brief Calculate the shortest distance between
     * the axis-aligned bounding box and the point \c p.
     */
    Scalar distance(const PointType &p) const {
        return std::sqrt(squaredDistance(p));
    }

    /**
     * \brief Calculate the shortest distance between
     * the axis-aligned bounding box and \c bbox.
     */
    Scalar distance(const TBoundingBox &bbox) const {
        return std::sqrt(squaredDistance(bbox));
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
    void clip(const TBoundingBox &bbox) {
        using simd::min;
        using simd::max;

        this->min = max(this->min, bbox.min);
        this->max = min(this->max, bbox.max);
    }

    /// Expand the bounding box to contain another point
    void expand(const PointType &p) {
        using simd::min;
        using simd::max;

        this->min = min(this->min, p);
        this->max = max(this->max, p);
    }

    /// Expand the bounding box to contain another bounding box
    void expand(const TBoundingBox &bbox) {
        using simd::min;
        using simd::max;

        this->min = min(this->min, bbox.min);
        this->max = max(this->max, bbox.max);
    }

    /// Merge two bounding boxes
    static TBoundingBox merge(const TBoundingBox &bbox1, const TBoundingBox &bbox2) {
        using simd::min;
        using simd::max;

        return TBoundingBox(
            min(bbox1.min, bbox2.min),
            max(bbox1.max, bbox2.max)
        );
    }

#if 0
    /// Check if a ray intersects a bounding box
    bool rayIntersect(const Ray3f &ray) const {
        float nearT = -std::numeric_limits<float>::infinity();
        float farT = std::numeric_limits<float>::infinity();

        for (int i=0; i<3; i++) {
            float origin = ray.o[i];
            float minVal = min[i], maxVal = max[i];

            if (ray.d[i] == 0) {
                if (origin < minVal || origin > maxVal)
                    return false;
            } else {
                float t1 = (minVal - origin) * ray.dRcp[i];
                float t2 = (maxVal - origin) * ray.dRcp[i];

                if (t1 > t2)
                    std::swap(t1, t2);

                nearT = std::max(t1, nearT);
                farT = std::min(t2, farT);

                if (!(nearT <= farT))
                    return false;
            }
        }

        return ray.mint <= farT && nearT <= ray.maxt;
    }

    /// Return the overlapping region of the bounding box and an unbounded ray
    bool rayIntersect(const Ray3f &ray, float &nearT, float &farT) const {
        nearT = -std::numeric_limits<float>::infinity();
        farT = std::numeric_limits<float>::infinity();

        for (int i=0; i<3; i++) {
            float origin = ray.o[i];
            float minVal = min[i], maxVal = max[i];

            if (ray.d[i] == 0) {
                if (origin < minVal || origin > maxVal)
                    return false;
            } else {
                float t1 = (minVal - origin) * ray.dRcp[i];
                float t2 = (maxVal - origin) * ray.dRcp[i];

                if (t1 > t2)
                    std::swap(t1, t2);

                nearT = std::max(t1, nearT);
                farT = std::min(t2, farT);

                if (!(nearT <= farT))
                    return false;
            }
        }

        return true;
    }
#endif

    PointType min; ///< Component-wise minimum
    PointType max; ///< Component-wise maximum
};

/// Return a string representation of the bounding box
template <typename PointType> std::ostream& operator<<(std::ostream &os, const TBoundingBox<PointType>& bbox) {
    if (!bbox.valid())
        os << "BoundingBox[invalid]";
    else
        os << "BoundingBox[min = " << bbox.min << ", max = " << bbox.max << "]";
    return os;
}

NAMESPACE_END(mitsuba)
