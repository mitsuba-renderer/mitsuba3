#pragma once

#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/profiler.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/field.h>
#include <mitsuba/render/interaction.h>
#include <mitsuba/render/shape.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/// Base class of fields that provide the volume role.
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB VolumeField : public Field<Float, Spectrum> {
public:
    MI_IMPORT_BASE(Field)
    MI_IMPORT_TYPES(Texture)

    using FloatStorage = typename Field<Float, Spectrum>::FloatStorage;
    using Args         = typename Field<Float, Spectrum>::Args;
    using Array2f      = typename Field<Float, Spectrum>::Array2f;
    using Array3f      = typename Field<Float, Spectrum>::Array3f;
    using Array6f      = typename Field<Float, Spectrum>::Array6f;

    // ======================================================================
    //! @{ \name Volume interface
    // ======================================================================

    /// Evaluate the volume at the given interaction, with color processing.
    UnpolarizedSpectrum eval(const Interaction3f &it, Mask active = true) const override;

    /// Evaluate this volume as a single-channel quantity.
    Float eval_1(const Interaction3f &it, Mask active = true) const override;

    /// Evaluate this volume as a three-channel quantity with no color
    /// processing (e.g. velocity field).
    Vector3f eval_3(const Interaction3f &it, Mask active = true) const override;

    /**
     * \brief Evaluate this volume as a six-channel quantity with no color
     * processing
     *
     * This interface is specifically intended to encode the parameters of an
     * SGGX phase function.
     */
    dr::Array<Float, 6> eval_6(const Interaction3f &it, Mask active = true) const override;

    /**
     * \brief Evaluate this volume as an n-channel float quantity
     *
     * This interface is specifically intended to encode a variable number of
     * parameters. Pointer allocation/deallocation must be performed by the
     * caller.
     */
    void eval_n(const Interaction3f &it, Float *out, Mask active = true) const override;

    /**
     * \brief Evaluate the volume at the given interaction and compute the
     * gradients of the linear interpolant as well.
     */
    std::pair<UnpolarizedSpectrum, Vector3f> eval_gradient(const Interaction3f &it,
                                                           Mask active = true) const override;

    /// Returns the maximum value of the volume over all dimensions.
    ScalarFloat max() const override;

    /**
     * \brief In the case of a multi-channel volume, this function returns
     * the maximum value for each channel.
     *
     * Pointer allocation/deallocation must be performed by the caller.
     */
    void max_per_channel(ScalarFloat *out) const override;

    /// Returns the bounding box of the volume.
    ScalarBoundingBox3f bbox() const override { return m_bbox; }

    /**
     * \brief Returns the resolution of the volume, assuming that it is based
     * on a discrete representation.
     *
     * The default implementation returns <tt>(1, 1, 1)</tt>
     */
    virtual ScalarVector3i resolution() const;

    ScalarVector3i resolution_3d() const override;

    /**
     * \brief Returns the number of channels stored in the volume
     *
     *  When the channel count is zero, it indicates that the volume
     *  does not support per-channel queries.
     */
    uint32_t channel_count() const override { return m_channel_count; }

    //! @}
    // ======================================================================

    // =============================================================
    //! @{ \name Field compatibility interface
    // =============================================================

    FieldValueType out_type() const override;
    FieldDomain domain() const override;
    uint32_t out_dim() const override;
    uint32_t args_dim() const override;
    bool supports_scalar() const override;
    bool supports_jit() const override;
    bool supports_surface_queries() const override;
    bool supports_interaction_queries() const override;

    FloatStorage eval(const SurfaceInteraction3f &si,
                      Args args,
                      Mask active) const override;
    FloatStorage eval(const Interaction3f &it,
                      Args args,
                      Mask active) const override;
    Float eval_1(const SurfaceInteraction3f &si,
                 Args args,
                 Mask active) const override;
    Float eval_1(const Interaction3f &it,
                 Args args,
                 Mask active) const override;
    Color3f eval_color3(const SurfaceInteraction3f &si,
                        Args args,
                        Mask active) const override;
    Color3f eval_color3(const Interaction3f &it,
                        Args args,
                        Mask active) const override;
    Array2f eval_array2(const SurfaceInteraction3f &si,
                        Args args,
                        Mask active) const override;
    Array2f eval_array2(const Interaction3f &it,
                        Args args,
                        Mask active) const override;
    Array3f eval_array3(const SurfaceInteraction3f &si,
                        Args args,
                        Mask active) const override;
    Array3f eval_array3(const Interaction3f &it,
                        Args args,
                        Mask active) const override;
    UnpolarizedSpectrum eval_spec(const SurfaceInteraction3f &si,
                                  Args args,
                                  Mask active) const override;
    UnpolarizedSpectrum eval_spec(const Interaction3f &it,
                                  Args args,
                                  Mask active) const override;
    Array6f eval_array6(const SurfaceInteraction3f &si,
                        Args args,
                        Mask active) const override;
    Array6f eval_array6(const Interaction3f &it,
                        Args args,
                        Mask active) const override;
    void eval_n(const SurfaceInteraction3f &si,
                Float *out,
                uint32_t count,
                Args args,
                Mask active) const override;
    void eval_n(const Interaction3f &it,
                Float *out,
                uint32_t count,
                Args args,
                Mask active) const override;

    //! @}
    // ======================================================================

    /// Returns a human-readable summary.
    std::string to_string() const override {
        std::ostringstream oss;
        oss << "Volume[" << std::endl
            << "  to_local = " << m_to_local << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS(VolumeField)
    static constexpr const char *Variant =
        detail::variant<Float, Spectrum>::name;
    static constexpr const char *Domain = "Field";
    static constexpr ObjectType Type = ObjectType::Field;
    std::string_view variant_name() const override { return Variant; }
    ObjectType type() const override { return Type; }

protected:
    VolumeField(const Properties &props);

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

MI_EXTERN_CLASS(VolumeField)
NAMESPACE_END(mitsuba)
