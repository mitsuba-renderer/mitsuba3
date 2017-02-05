#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/bbox.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Shape : public Object {
public:
    /// Use 32 bit indices to keep track of indices to conserve memory
    using Index = uint32_t;

    /// Use 32 bit indices to keep track of sizes to conserve memory
    using Size  = uint32_t;

    /**
     * \brief Return an axis aligned box that bounds all shape primitives
     * (including any transformations that may have been applied to them)
     */
    virtual BoundingBox3f bbox() const = 0;

    /**
     * \brief Return an axis aligned box that bounds a single shape primitive
     * (including any transformations that may have been applied to it)
     *
     * \remark The default implementation simply calls \ref bbox()
     */
    virtual BoundingBox3f bbox(Index Size) const;

    /**
     * \brief Return an axis aligned box that bounds a single shape primitive
     * after it has been clipped to another bounding box.
     *
     * This is extremely important to construct decent kd-trees. The default
     * implementation just takes the bounding box returned by \ref bbox(Index
     * index) and clips it to \a clip.
     */
    virtual BoundingBox3f bbox(Index index,
                               const BoundingBox3f &clip) const;

    /**
     * \brief Returns the number of sub-primitives that make up this shape
     * \remark The default implementation simply returns \c 1
     */
    virtual Size primitive_count() const;

    MTS_DECLARE_CLASS()

protected:
    virtual ~Shape();
};

NAMESPACE_END(mitsuba)
