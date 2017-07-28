#pragma once

#include <mitsuba/render/fwd.h>
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

    // =============================================================
    //! @{ \name Ray tracing routines
    // =============================================================

    /**
     * Adjust an intersection record to a different time value
     */
    template <typename Intersection>
    /*virtual*/ void adjust_time(Intersection &/*its*/, Float /*time*/) const {
        Throw("Not implemented yet");
    }

    /**
     * \brief Return the derivative of the normal vector with
     * respect to the UV parameterization
     *
     * This can be used to compute Gaussian and principal curvatures,
     * amongst other things.
     *
     * \param its
     *     Intersection record associated with the query
     * \param dndu
     *     Parameter used to store the partial derivative of the
     *     normal vector with respect to \c u
     * \param dndv
     *     Parameter used to store the partial derivative of the
     *     normal vector with respect to \c v
     * \param shading_frame
     *     Specifies whether to compute the derivative of the
     *     geometric normal \a or the shading normal of the surface
     */
    template <typename Intersection, typename Vector>
    /*virtual*/ std::pair<Vector, Vector> normal_derivative(const Intersection &/*its*/,
        bool /*shading_frame*/ = true) const {
        Throw("Not implemented yet");
    }

    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

    /// Does the surface of this shape mark a medium transition?
    bool is_medium_transition() const {
        Throw("Not implemented yet");
        // return m_interior_medium.get() || m_exterior_medium.get();
    }
    /// Return the medium that lies on the interior of this shape (\c nullptr == vacuum)
    Medium *interior_medium() {
        Throw("Not implemented yet");
        // return m_interior_medium;
    }
    /// Return the medium that lies on the interior of this shape (\c nullptr == vacuum, const version)
    const Medium *interior_medium() const {
        Throw("Not implemented yet");
        // return m_interior_medium.get();
    }
    /// Return the medium that lies on the exterior of this shape (\c nullptr == vacuum)
    Medium *exterior_medium() {
        Throw("Not implemented yet");
        // return m_exterior_medium;
    }
    /// Return the medium that lies on the exterior of this shape (\c nullptr == vacuum, const version)
    const Medium *exterior_medium() const {
        Throw("Not implemented yet");
        // return m_exterior_medium.get();
    }

    /// Does this shape have a sub-surface integrator?
    bool has_subsurface() const {
        Throw("Not implemented yet");
        // return m_subsurface.get() != nullptr;
    }
    /// Return the associated sub-surface integrator
    Subsurface *subsurface() {
        Throw("Not implemented yet");
        // return m_subsurface;
    }
    /// Return the associated sub-surface integrator
    const Subsurface *subsurface() const {
        Throw("Not implemented yet");
        // return m_subsurface.get();
    }

    /// Is this shape also an area emitter?
    bool is_emitter() const {
        Throw("Not implemented yet");
        // return m_emitter.get() != nullptr;
    }
    /// Return the associated emitter (if any)
    Emitter *emitter() {
        Throw("Not implemented yet");
        // return m_emitter;
    }
    /// Return the associated emitter (if any)
    const Emitter *emitter() const {
        Throw("Not implemented yet");
        // return m_emitter.get();
    }
    /// Set the emitter of this shape
    void set_emitter(Emitter * /*emitter*/) {
        Throw("Not implemented yet");
        // m_emitter = emitter;
    }

    /// Is this shape also an area sensor?
    bool is_sensor() const {
        Throw("Not implemented yet");
        // return m_sensor.get() != nullptr;
    }
    /// Return the associated sensor (if any)
    Sensor *sensor() {
        Throw("Not implemented yet");
        // return m_sensor;
    }
    /// Return the associated sensor (if any)
    const Sensor *sensor() const {
        Throw("Not implemented yet");
        // return m_sensor.get();
    }

    /// Does the shape have a BSDF?
    bool has_bsdf() const {
        Throw("Not implemented yet");
        // return m_bsdf.get() != nullptr;
    }
    /// Return the shape's BSDF
    const BSDF *bsdf() const {
        Throw("Not implemented yet");
        // return m_bsdf.get();
    }
    /// Return the shape's BSDF
    BSDF *bsdf() {
        Throw("Not implemented yet");
        // return m_bsdf.get();
    }
    /// Set the BSDF of this shape
    void set_bsdf(BSDF * /*bsdf*/) {
        Throw("Not implemented yet");
        // m_bsdf = bsdf;
    }

    /**
     * \brief Returns the number of sub-primitives that make up this shape
     * \remark The default implementation simply returns \c 1
     */
    virtual Size primitive_count() const;

    /**
     * \brief Return the number of primitives (triangles, hairs, ..)
     * contributed to the scene by this shape
     *
     * Includes instanced geometry.
     * The default implementation simply returns `primitive_count()`.
     */
    virtual size_t effective_primitive_count() const {
        return primitive_count();
    };

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:
    virtual ~Shape();

private:
    ref<BSDF> m_bsdf;
};

NAMESPACE_END(mitsuba)

// -----------------------------------------------------------------------
//! @{ \name Enoki support for packets of Shape pointers
// -----------------------------------------------------------------------

// Enable usage of array pointers for our types
ENOKI_CALL_SUPPORT_BEGIN(mitsuba::ShapeP)
ENOKI_CALL_SUPPORT_SCALAR(is_emitter)
ENOKI_CALL_SUPPORT_SCALAR(is_sensor)
ENOKI_CALL_SUPPORT_SCALAR(has_subsurface)
ENOKI_CALL_SUPPORT_SCALAR(is_medium_transition)
ENOKI_CALL_SUPPORT_SCALAR(exterior_medium)
ENOKI_CALL_SUPPORT_SCALAR(interior_medium)
ENOKI_CALL_SUPPORT_SCALAR(bsdf)
ENOKI_CALL_SUPPORT_END(mitsuba::ShapeP)

//! @}
// -----------------------------------------------------------------------
