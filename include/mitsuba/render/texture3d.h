#pragma once

#include <mitsuba/core/bbox.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/transform.h>
#include <mitsuba/render/autodiff.h>
#include <mitsuba/render/interaction.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Abstract base class for spatially varying 3D textures.
 */
class MTS_EXPORT_RENDER Texture3D : public DifferentiableObject {
public:
    explicit Texture3D(const Properties &props);

    // ======================================================================
    //! @{ \name 3D Texture interface
    // ======================================================================

    /// Evaluate the texture at the given surface interaction
    virtual Spectrumf eval(const Interaction3f &it) const;

    /// Vectorized version of \ref eval()
    virtual SpectrumfP eval(const Interaction3fP &it,
                            MaskP active = true) const;

    /// Wrapper for scalar \ref eval() with a mask (which will be ignored)
    Spectrumf eval(const Interaction3f &it, bool /*active*/) const {
        return eval(it);
    }

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref eval()
    virtual SpectrumfD eval(const Interaction3fD &it,
                            const MaskD &active = true) const;
#endif

    /// Evaluate the texture at the given surface interaction as well as its gradients
    virtual std::pair<Spectrumf, Vector3f> eval_gradient(const Interaction3f &it) const;

    /// Vectorized version of \ref eval_gradient()
    virtual std::pair<SpectrumfP, Vector3fP> eval_gradient(const Interaction3fP &it,
                                                           MaskP active = true) const;

    /// Wrapper for scalar \ref eval_gradient() with a mask (which will be ignored)
    std::pair<Spectrumf, Vector3f> eval_gradient(const Interaction3f &it, bool /*active*/) const {
        return eval_gradient(it);
    }

#if defined(MTS_ENABLE_AUTODIFF)
    /// Differentiable version of \ref eval_gradient()
    virtual std::pair<SpectrumfD, Vector3fD> eval_gradient(const Interaction3fD &it,
                                                           const MaskD &active = true) const;
#endif

    /**
     * Returns the (possibly approximate) mean value of the texture
     * over all dimensions.
     * May remain unimplemented, the default implementation throws
     * an exception.
     */
    virtual Float mean() const;

    /**
     * Returns the maximum value of the texture over all dimensions.
     */
    virtual Float max() const { NotImplementedError("max"); };

    /**
     * Returns the bounding box of the 3d texture
     */
    BoundingBox3f bbox() const { return m_bbox; };

    //! @}
    // ======================================================================

    /// Returns a human-reable summary
    std::string to_string() const override;

    MTS_DECLARE_CLASS()

protected:
    /// Used to bring points in world coordinates to local coordinates.
    Transform4f m_world_to_local;
    /// Bounding box
    BoundingBox3f m_bbox;

    virtual ~Texture3D();

    template <typename Interaction, typename Mask>
    Mask is_inside_impl(const Interaction &it, Mask active) const;

    void update_bbox();
};

/*
 * \brief These macros should be used in the definition of Texture3D
 * plugins to instantiate concrete versions of the interface.
 */

#if !defined(MTS_ENABLE_AUTODIFF)
#define MTS_IMPLEMENT_TEXTURE_3D_AUTODIFF()
#else
#define MTS_IMPLEMENT_TEXTURE_3D_AUTODIFF()                                                        \
    SpectrumfD eval(const Interaction3fD &it, const MaskD &active) const override {                \
        return eval_impl<false>(it, active);                                                       \
    }                                                                                              \
    std::pair<SpectrumfD, Vector3fD> eval_gradient(const Interaction3fD &it, const MaskD &active)  \
        const override {                                                                           \
        return eval_impl<true>(it, active);                                                        \
    }
#endif

#define MTS_IMPLEMENT_TEXTURE_3D()                                                                 \
    using Texture3D::eval;                                                                         \
    using Texture3D::eval_gradient;                                                                \
    Spectrumf eval(const Interaction3f &it) const override { return eval_impl<false>(it, true); }  \
    SpectrumfP eval(const Interaction3fP &it, MaskP active) const override {                       \
        return eval_impl<false>(it, active);                                                       \
    }                                                                                              \
    std::pair<Spectrumf, Vector3f> eval_gradient(const Interaction3f &it) const override {         \
        return eval_impl<true>(it, true);                                                          \
    }                                                                                              \
    std::pair<SpectrumfP, Vector3fP> eval_gradient(const Interaction3fP &it, MaskP active)         \
        const override {                                                                           \
        return eval_impl<true>(it, active);                                                        \
    }                                                                                              \
    MTS_IMPLEMENT_TEXTURE_3D_AUTODIFF()

NAMESPACE_END(mitsuba)
