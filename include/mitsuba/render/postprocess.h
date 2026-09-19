#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * Abstract post-processing filter applied to developed images
 *
 * Instances of this class implement image post-processing functions such as
 * film response functions, bloom filters, or white balancing.
 *
 * The `Film` class holds an arbitrary number of `PostProcess` instances and
 * runs them sequentially in `Film::develop()`.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB PostProcess : public JitObject<PostProcess<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES()

    /**
     * Transform a developed image
     *
     * Args:
     *     image: Tensor of shape ``(height, width, channels)``.
     *
     *     channels: Names of the channels along the last axis (e.g. ``R``,
     *         ``G``, ``B``, ``A`` followed by AOV names). Filters typically
     *         transform the color channels and pass the others without change.
     *
     * Returns:
     *     A tensor with the same shape as ``image``.
     */
    virtual TensorXf eval(const TensorXf &image,
                          const std::vector<std::string> &channels) const = 0;

    std::string to_string() const override;

    /// Destructor
    ~PostProcess();

    MI_DECLARE_PLUGIN_BASE_CLASS(PostProcess)
protected:
    PostProcess(const Properties &props);
};

MI_EXTERN_CLASS(PostProcess)
NAMESPACE_END(mitsuba)
