#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/common.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Endpoint: an abstract interface to light sources and sensors
 *
 * This class implements an abstract interface to all sensors and light sources
 * emitting radiance and importance, respectively. Subclasses must implement
 * functions for evaluating and sampling the emission profile and furthermore
 * support querying the probability density of the provided sampling technique.
 *
 * Subclasses must also provide a specialized \a direct sampling method
 * (a generalization of direct illumination sampling to both emitters \a and
 * sensors). A direct sampling is given an arbitrary input position in the
 * scene and in turn returns a sampled emitter position and direction, which
 * has a nonzero contribution towards the provided position. The main idea is
 * that direct sampling reduces the underlying space from 4D to 2D, hence it
 * is often possible to use smarter sampling techniques than in the fully
 * general case.
 *
 * Since the emission profile is defined as function over both positions
 * and directions, there are functions to sample and query \a each of the
 * two components separately. Furthermore, there is a convenience function
 * to sample both at the same time, which is mainly used by unidirectional
 * rendering algorithms that do not need this level of flexibility.
 *
 * One underlying assumption of this interface is that position and
 * direction sampling will happen <em>in sequence</em>. This means that
 * the direction sampling step is allowed to statistically condition on
 * properties of the preceding position sampling step.
 *
 * When rendering scenes involving participating media, it is important
 * to know what medium surrounds the sensors and light sources. For
 * this reason, every emitter instance keeps a reference to a medium
 * (or \c nullptr when it is surrounded by vacuum).
 */
class MTS_EXPORT_RENDER Endpoint : public Object {
public:
    /**
     * \brief Flags used to classify the emission profile of
     * different types of emitters
     */
    enum EFlags {
        /// Emission profile contains a Dirac delta term with respect to position
        EDeltaPosition  = 0x01,

        /// Emission profile contains a Dirac delta term with respect to direction
        EDeltaDirection = 0x02,

        /// Is the emitter associated with a surface in the scene?
        EOnSurface      = 0x04
    };

// =============================================================
    //! @{ \name Sampling interface
    // =============================================================

    /**
     * \brief Importance sample the spatial component of the
     * emission profile.
     *
     * This function takes an uniformly distributed 2D vector
     * and maps it to a position on the surface of the emitter.
     *
     * Some implementations may choose to implement extra functionality
     * based on the value of \c extra: for instance, Sensors
     * (which are a subclass of \ref Endpoint) perform uniform
     * sampling over the entire image plane if <tt>extra == nullptr</tt>,
     * but other values, they will restrict sampling to a pixel-sized
     * rectangle with that offset.
     *
     * The default implementation throws an exception.
     *
     * \param p_rec
     *    A position record to be populated with the sampled
     *    position and related information
     *
     * \param sample
     *    A uniformly distributed 2D vector (or any value,
     *    when \ref needs_position_sample() == \c false)
     *
     * \param extra
     *    An additional 2D vector provided to the sampling
     *    routine -- its use is implementation-dependent.
     *
     * \return
     *    An importance weight associated with the sampled position.
     *    This accounts for the difference between the spatial part of the
     *    emission profile and the density function.
     */
    virtual Spectrumf sample_position(PositionSample3f &p_rec,
        const Point2f &sample, const Point2f *extra = nullptr) const;
    Spectrumf sample_position(PositionSample3f &p_rec,
        const Point2f &sample, const Point2f *extra, bool /*unused*/) const {
        return sample_position(p_rec, sample, extra);
    }
    /// Vectorized variant of \ref sample_position.
    virtual SpectrumfP sample_position(PositionSample3fP &p_rec,
        const Point2fP &sample, const Point2fP *extra = nullptr,
        const mask_t<FloatP> &active = true) const;

    /**
     * \brief Conditioned on the spatial component, importance
     * sample the directional part of the emission profile.
     *
     * Some implementations may choose to implement extra functionality
     * based on the value of \c extra: for instance, Sensors
     * (which are a subclass of \ref Endpoint) perform uniform
     * sampling over the entire image plane if <tt>extra == nullptr</tt>,
     * but other values, they will restrict sampling to a pixel-sized
     * rectangle with that offset.
     *
     * The default implementation throws an exception.
     *
     * \param d_rec
     *    A direction record to be populated with the sampled
     *    direction and related information
     *
     * \param p_rec
     *    A position record generated by a preceding call
     *    to \ref sample_position()
     *
     * \param sample
     *    A uniformly distributed 2D vector (or any value
     *    when \ref needs_direction_sample() == \c false)
     *
     * \return
     *    An importance weight associated with the sampled direction.
     *    This accounts for the difference between the directional part of the
     *    emission profile and the density function.
     */
    virtual Spectrumf sample_direction(
        DirectionSample3f &d_rec, PositionSample3f &p_rec,
        const Point2f &sample, const Point2f *extra = nullptr) const;
    Spectrumf sample_direction(
        DirectionSample3f &d_rec, PositionSample3f &p_rec,
        const Point2f &sample, const Point2f *extra, bool /*unused*/) const {
        return sample_direction(d_rec, p_rec, sample, extra);
    }
    virtual SpectrumfP sample_direction(
        DirectionSample3fP &d_rec, PositionSample3fP &p_rec,
        const Point2fP &sample, const Point2fP *extra = nullptr,
        const mask_t<FloatP> &active = true) const;

    /**
     * \brief \a Direct sampling: given a reference point in the
     * scene, sample an emitter position that contributes towards it.
     *
     * Given an arbitrary reference point in the scene, this method
     * samples a position on the emitter that has a nonzero contribution
     * towards that point.
     * This can be seen as a generalization of direct illumination sampling
     * so that it works on both luminaires and sensors.
     *
     * Ideally, the implementation should importance sample the product of
     * the emission profile and the geometry term between the reference point
     * and the position on the emitter.
     *
     * The default implementation throws an exception.
     *
     * \param d_rec
     *    A direct sampling record that specifies the reference point and
     *    a time value. After the function terminates, it will be
     *    populated with the position sample and related information
     *
     * \param sample
     *    A uniformly distributed 2D vector (or any value
     *    when \ref needs_direct_sample() == \c false)
     *
     * \return
     *    An importance weight associated with the sample. Includes
     *    any geometric terms between the emitter and the reference point.
     */
    virtual Spectrumf sample_direct(DirectSample3f &d_rec,
                                    const Point2f &sample) const;
    Spectrumf sample_direct(DirectSample3f &d_rec,
                                    const Point2f &sample,
                                    bool /*unused*/) const {
        return sample_direct(d_rec, sample);
    }
    virtual SpectrumfP sample_direct(DirectSample3fP &d_rec,
                                     const Point2fP &sample,
                                     const mask_t<FloatP> &active = true) const;

    //! @}
    // =============================================================

    // =============================================================
    //! @{ \name Query functions for the emission profile and
    //!          sampling density functions
    // =============================================================

    /**
     * \brief Evaluate the spatial component of the emission profile
     *
     * \param p_rec
     *    A position sampling record, which specifies the query location
     *
     * \return The component of the emission profile that depends on
     * the position (i.e. emitted power per unit area for luminaires and
     * sensor response, or inverse power per unit area for sensors)
     */
    virtual Spectrumf eval_position(const PositionSample3f &p_rec) const;
    Spectrumf eval_position(const PositionSample3f &p_rec,
                            bool /*unused*/) const {
        return eval_position(p_rec);
    }
    virtual SpectrumfP eval_position(const PositionSample3fP &p_rec,
                                     const mask_t<FloatP> &active = true) const;

    /**
     * \brief Evaluate the directional component of the emission profile
     *
     * When querying a smooth (i.e. non-degenerate) component, it already
     * multiplies the result by the cosine foreshortening factor with
     * respect to the outgoing direction.
     *
     * \param d_rec
     *    A direction sampling record, which specifies the query direction
     *
     * \param p_rec
     *    A position sampling record, which specifies the query position
     *
     * \return The component of the emission profile that depends on
     * the direction (having units of 1/steradian)
     */
    virtual Spectrumf eval_direction(const DirectionSample3f &d_rec,
                                     const PositionSample3f &p_rec) const;
    Spectrumf eval_direction(const DirectionSample3f &d_rec,
                             const PositionSample3f &p_rec,
                             bool /*unused*/) const {
        return eval_direction(d_rec, p_rec);
    }
    virtual SpectrumfP eval_direction(const DirectionSample3fP &d_rec,
                                      const PositionSample3fP &p_rec,
                                      const mask_t<FloatP> &active = true) const;

    /**
     * \brief Evaluate the spatial component of the sampling density
     * implemented by the \ref sample_position() method
     *
     * \param p_rec
     *    A position sampling record, which specifies the query location
     *
     * \return
     *    The area density at the supplied position
     */
    virtual Float pdf_position(const PositionSample3f &p_rec) const;
    Float pdf_position(const PositionSample3f &p_rec,
                       bool /*unused*/) const {
        return pdf_position(p_rec);
    }
    virtual FloatP pdf_position(const PositionSample3fP &p_rec,
                                const mask_t<FloatP> &active = true) const;

    /**
     * \brief Evaluate the directional component of the sampling density
     * implemented by the \ref sample_direction() method
     *
     * \param d_rec
     *    A direction sampling record, which specifies the query direction
     *
     * \param p_rec
     *    A position sampling record, which specifies the query position
     *
     * \return
     *    The directional density at the supplied position
     */
    virtual Float pdf_direction(const DirectionSample3f &d_rec,
                                const PositionSample3f &p_rec) const;
    Float pdf_direction(const DirectionSample3f &d_rec,
                        const PositionSample3f &p_rec,
                        bool /*unused*/) const {
        return pdf_direction(d_rec, p_rec);
    }
    virtual FloatP pdf_direction(const DirectionSample3fP &d_rec,
                                 const PositionSample3fP &p_rec,
                                 const mask_t<FloatP> &active = true) const;

    /**
     * \brief Evaluate the probability density of the \a direct sampling
     * method implemented by the \ref sample_direct() method.
     *
     * \param d_rec
     *    A direct sampling record, which specifies the query
     *    location. Note that this record need not be completely
     *    filled out. The important fields are \c p, \c n, \c ref,
     *    \c dist, \c d, \c measure, and \c uv.
     *
     * \return
     *    The density expressed with respect to the requested measure
     *    (usually \ref ESolidAngle)
     */
    virtual Float pdf_direct(const DirectSample3f &d_rec) const;
    Float pdf_direct(const DirectSample3f &d_rec,
                     bool /*unused*/) const {
        return pdf_direct(d_rec);
    }
    virtual FloatP pdf_direct(const DirectSample3fP &d_rec,
                              const mask_t<FloatP> &active = true) const;

    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Other query functions
    // =============================================================

    /**
     * \brief Return a listing of classification flags combined
     * using binary OR.
     *
     * \sa EFlags
     */
    uint32_t type(bool /*unused*/ = true) const { return m_type; }

    /// Return the local space to world space transformation
    const AnimatedTransform *world_transform(bool /*unused*/ = true) const;


    /// Set the local space to world space transformation
    void set_world_transform(const AnimatedTransform *trafo, bool /*unused*/ = true);

    /**
     * \brief Does the method \ref sample_position() require a uniformly
     * distributed sample for the spatial component?
     */
    bool needs_position_sample(bool /*unused*/ = true) const {
        return !(m_type & EDeltaPosition);
    }

    /**
     * \brief Does the method \ref sample_direction() require a uniformly
     * distributed sample for the direction component?
     */
    bool needs_direction_sample(bool /*unused*/ = true) const {
        return !(m_type & EDeltaDirection);
    }

    /**
     * \brief Does the emitter lie on some kind of surface?
     */
    bool is_on_surface(bool /*unused*/ = true) const {
        return m_type & EOnSurface;
    }

    /**
     * \brief Does the sensor have a degenerate directional or spatial
     * distribution?
     */
    bool is_degenerate(bool /*unused*/ = true) const {
        return m_type & (EDeltaPosition | EDeltaDirection);
    }

    /**
     * \brief Does the method \ref sample_direct() require a uniformly
     * distributed sample?
     *
     * Since sample_direct() essentially causes a 2D reduction of the
     * sampling domain, this is the case exactly when the original
     * domain was four-dimensionsional.
     */
    bool needs_direct_sample(bool /*unused*/ = true) const {
        return needs_position_sample() && needs_direction_sample();
    }

    /**
     * \brief Return the measure associated with the \ref sample_direct()
     * operation
     */
    EMeasure direct_measure(bool /*unused*/ = true) const {
        return needs_direct_sample() ? ESolidAngle : EDiscrete;
    }

    //! @}
    // =============================================================


    // =============================================================
    //! @{ \name Miscellaneous
    // =============================================================

    /// Return a pointer to the medium that surrounds the emitter
    Medium *medium(bool /*unused*/ = true) { return m_medium; }

    /// Return a pointer to the medium that surrounds the emitter (const version)
    const Medium *medium(bool /*unused*/ = true) const { return m_medium.get(); }

    /// Return the shape, to which the emitter is currently attached
    Shape *shape(bool /*unused*/ = true) { return m_shape; }

    /// Return the shape, to which the emitter is currently attached (const version)
    const Shape *shape(bool /*unused*/ = true) const { return m_shape; }

    /**
     * \brief Create a special shape that represents the emitter
     *
     * Some types of emitters are inherently associated with a surface, yet
     * this surface is not explicitly needed for many kinds of rendering
     * algorithms.
     *
     * An example would be an environment map, where the associated shape
     * is a sphere surrounding the scene. Another example would be a
     * perspective camera with depth of field, where the associated shape
     * is a disk representing the aperture (remember that this class
     * represents emitters in a generalized bidirectional sense, which
     * includes sensors).
     *
     * When this shape is in fact needed by the underlying rendering algorithm,
     * this function can be called to create it. The default implementation
     * simply returns \c nullptr.
     *
     * \param scene
     *     A pointer to the associated scene (the created shape is
     *     allowed to depend on it)
     */
    virtual ref<Shape> create_shape(const Scene *scene, bool /*unused*/ = true);

    /**
     * \brief Return an axis-aligned box bounding the spatial
     * extents of the emitter
     */
    virtual BoundingBox3f bbox(bool /*unused*/ = true) const = 0;

    /// Set the shape associated with this endpoint.
    virtual void set_shape(Shape *shape, bool /*unused*/ = true) {
        m_shape = shape;
    }

    /// Set the medium that surrounds the emitter.
    void set_medium(Medium *medium, bool /*unused*/ = true) { m_medium = medium; }

    /// Serialize this emitter to a binary data stream
    // virtual void serialize(Stream *stream, InstanceManager *manager) const;

    //! @}
    // =============================================================

    MTS_DECLARE_CLASS()

protected:
    Endpoint(const Properties &props);

    /// Unserialize a emitter instance from a binary data stream
    // Endpoint(Stream *stream, InstanceManager *manager);

    virtual ~Endpoint();

protected:
    Properties m_properties;

    ref<const AnimatedTransform> m_world_transform;
    ref<Medium> m_medium;
    Shape *m_shape = nullptr;
    uint32_t m_type;
};

NAMESPACE_END(mitsuba)
