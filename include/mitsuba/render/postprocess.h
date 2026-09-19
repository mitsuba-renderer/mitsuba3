#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/render/fwd.h>

NAMESPACE_BEGIN(mitsuba)

/**
 * \brief Abstract post-processing stage applied to developed images
 *
 * Post-processing stages are nested within a \ref Film and transform the image
 * that it develops, e.g. to apply film response functions, bloom filters, etc.
 * The first stage receives the linear radiance values measured by the film.
 *
 * The stages are part of the film's output: \ref Film::develop(), \ref
 * Film::bitmap() and \ref Film::write() run them in the order in which they
 * were specified, and so does \ref Integrator::render(). These functions take
 * a <tt>postprocess=false</tt> argument that skips the stages and yields the
 * linear image.
 *
 * In JIT variants, a stage must compute its output from the input using Dr.Jit
 * operations so that derivatives propagate through it when the film takes part
 * in differentiable rendering. The film raises an error when a stage severs
 * the AD graph, e.g. by converting the image to a NumPy array.
 */
template <typename Float, typename Spectrum>
class MI_EXPORT_LIB PostProcess : public JitObject<PostProcess<Float, Spectrum>> {
public:
    MI_IMPORT_TYPES()

    /**
     * \brief Transform a developed image
     *
     * \param image
     *     Tensor of shape <tt>(height, width, channels)</tt>
     *
     * \param channels
     *     Names of the channels along the last axis (e.g. <tt>R, G, B, A</tt>
     *     followed by AOV names). Stages typically transform the color
     *     channels and pass the others through unchanged.
     *
     * \return
     *     A tensor with the same shape as \c image
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
