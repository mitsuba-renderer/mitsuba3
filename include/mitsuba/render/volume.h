#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/volume_utils.h>

NAMESPACE_BEGIN(mitsuba)

/// Abstract base class for 3D volumes.
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB Volume : public JitObject<Volume<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES(Texture)

    // ======================================================================
    // Volume interface
    // ======================================================================

    /// Evaluate the volume at the given surface interaction, with color processing.
    virtual UnpolarizedSpectrum eval(const Interaction3f &it, Mask active = true) const;

    /// Evaluate this volume as a single-channel quantity.
    virtual Float eval_1(const Interaction3f &it, Mask active = true) const;

    /// Evaluate this volume as a three-channel quantity with no color processing (e.g. velocity field).
    virtual Vector3f eval_3(const Interaction3f &it, Mask active = true) const;

   /**
     * Evaluate this volume as a six-channel quantity with no color processing
     * This interface is specifically intended to encode the parameters of an SGGX phase function.
     */
    virtual dr::Array<Float, 6> eval_6(const Interaction3f &it, Mask active = true) const;

    /**
     * Evaluate this volume as a n-channel float quantity
     *
     * This interface is specifically intended to encode a variable number of parameters.
     * Pointer allocation/deallocation must be performed by the caller.
     */
    virtual void eval_n(const Interaction3f &it, Float *out, Mask active = true) const;

    /**
     * Evaluate the volume at the given surface interaction,
     * and compute the gradients of the linear interpolant as well.
     */
    virtual std::pair<UnpolarizedSpectrum, Vector3f> eval_gradient(const Interaction3f &it,
                                                                   Mask active = true) const;

    /// Returns the maximum value of the volume over all dimensions.
    virtual ScalarFloat max() const;

    /**
     * In the case of a multi-channel volume, this function returns
     * the maximum value for each channel.
     *
     * Pointer allocation/deallocation must be performed by the caller.
     */
    virtual void max_per_channel(ScalarFloat *out) const;

    /// Returns the minimum value of the volume over all dimensions.
    virtual ScalarFloat min() const;

    /**
     * \brief In the case of a multi-channel volume, this function returns
     * the minimum value for each channel.
     *
     * Pointer allocation/deallocation must be performed by the caller.
     */
    virtual void min_per_channel(ScalarFloat *out) const;

    /**
     * \brief Compute local extrema over a spatial region
     *
     * Returns the maximum value (majorant) and minimum value (minorant)
     * over the specified bounding box region.
     * Only fully implemented for grid-based volumes.
     *
     * \param bbox  Bounding box defining the query region in local space
     * \return (minorant, majorant) pair
     */
    virtual std::pair<Float, Float>
    extremum(BoundingBox3f bbox, Mask local = true) const;

    /**
     * \brief Compute local majorant (maximum) over a spatial region
     *
     * Convenience method that returns only the majorant.
     * The default implementation calls `extremum()` and returns
     * the second element.
     */
    virtual Float majorant(const BoundingBox3f& bbox, Mask local = true) const {
        return extremum(bbox, local).second;
    }

    /**
     * \brief Compute local minorant (minimum) over a spatial region
     *
     * Convenience method that returns only the minorant.
     * The default implementation calls `extremum()` and returns
     * the first element.
     */
    virtual Float minorant(const BoundingBox3f& bbox, Mask local = true) const {
        return extremum(bbox, local).first;
    }

    virtual VolumeParametrization<ScalarFloat> parametrization() const {
        VolumeParametrization<ScalarFloat> param;
        param.to_world = m_to_local.inverse();
        return param;
    }

    /// Returns the bounding box of the volume
    ScalarBoundingBox3f bbox() const { return m_bbox; }

    /**
     * Returns the resolution of the volume, assuming that it is based
     * on a discrete representation.
     *
     * The default implementation returns ``(1, 1, 1)``
     */
    virtual ScalarVector3i resolution() const;

    /**
     * Returns the number of channels stored in the volume
     *
     * When the channel count is zero, it indicates that the volume
     * does not support per-channel queries.
     */
    uint32_t channel_count() const { return m_channel_count; }

    // ======================================================================

    /// Returns a human-reable summary
    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Volume[" << std::endl
            << "  to_local = " << m_to_local << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_PLUGIN_BASE_CLASS(Volume)

    /**
     * \brief A Scoped Guard that pins the reference count of the volume.
     *
     * Use for bulk operations in scalar mode.
     */
    struct PinGuard {
        const Volume *volume;
        explicit PinGuard(const Volume* v) : volume(v) { volume->pin_ref_count(); }
        ~PinGuard() { volume->unpin_ref_count(); }

        PinGuard(const PinGuard&) = delete;
        PinGuard& operator=(const PinGuard&) = delete;
    };

    virtual PinGuard pin() const { return PinGuard(this); };

protected:
    Volume(const Properties &props);

    void update_bbox() {
        ScalarAffineTransform4f to_world = m_to_local.inverse();
        m_bbox = ScalarBoundingBox3f();
        m_bbox.expand(to_world * ScalarPoint3f(0.f, 0.f, 0.f));
        m_bbox.expand(to_world * ScalarPoint3f(0.f, 0.f, 1.f));
        m_bbox.expand(to_world * ScalarPoint3f(0.f, 1.f, 0.f));
        m_bbox.expand(to_world * ScalarPoint3f(0.f, 1.f, 1.f));
        m_bbox.expand(to_world * ScalarPoint3f(1.f, 0.f, 0.f));
        m_bbox.expand(to_world * ScalarPoint3f(1.f, 0.f, 1.f));
        m_bbox.expand(to_world * ScalarPoint3f(1.f, 1.f, 0.f));
        m_bbox.expand(to_world * ScalarPoint3f(1.f, 1.f, 1.f));
    }

    /// Pin the reference count of the data that constitutes the volume e.g. Texture/
    virtual void pin_ref_count() const {};
    /// Unpin the reference count.
    virtual void unpin_ref_count() const {};

protected:
    /// Used to bring points in world coordinates to local coordinates.
    ScalarAffineTransform4f m_to_local;
    /// Bounding box
    ScalarBoundingBox3f m_bbox;
    /// Number of channels stored in the volume
    uint32_t m_channel_count;

    MI_TRAVERSE_CB(Object)
};

MI_EXTERN_CLASS(Volume)
NAMESPACE_END(mitsuba)
