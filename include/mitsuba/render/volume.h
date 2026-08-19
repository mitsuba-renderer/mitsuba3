#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>

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

    /**
     * \brief Returns conservative per-cell upper bounds ("local majorants")
     * of the volume, at a resolution coarsened by \c resolution_factor along
     * each axis relative to \ref resolution().
     *
     * The returned buffer stores one value per cell in x-fastest order, i.e.
     * <tt>index = (z * res.y() + y) * res.x() + x</tt>, where \c res is
     * written to \c res_out. Each value bounds the *interpolated* volume
     * anywhere inside the corresponding cell of the volume's local unit cube
     * (see \ref to_local()).
     *
     * The default implementation returns a single cell holding \ref max(),
     * i.e. a global majorant. Discretized volumes (e.g. \c gridvolume)
     * override this with exact per-cell maxima, which media use to build
     * majorant supergrids for delta tracking.
     *
     * The result is detached from the AD graph: majorants only shape the
     * sampling density and must not carry derivatives.
     */
    virtual DynamicBuffer<Float>
    local_majorants(uint32_t resolution_factor, ScalarVector3u &res_out) const;

    /// Returns the world-to-local transform used by this volume
    const ScalarAffineTransform4f &to_local() const { return m_to_local; }

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
