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

    /// Evaluate the texture at the given surface interaction as well as its gradients
    virtual std::pair<Spectrumf, Vector3f> eval_gradient(const Interaction3f &it) const;

    /**
     * \brief Returns the (possibly approximate) mean value of the texture over all
     * dimensions.
     *
     * Not guaranteed to be implemented. The default implementation throws an
     * exception.
     */
    virtual Float mean() const;

    /// Returns the maximum value of the texture over all dimensions.
    virtual Float max() const { NotImplementedError("max"); };

    /// Returns the bounding box of the 3d texture
    BoundingBox3f bbox() const { return m_bbox; };

    /// Returns the resolution of the texture, defaults to "1"
    virtual Vector3i resolution() const { return Vector3i(1, 1, 1); };

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

NAMESPACE_END(mitsuba)
