#include <mitsuba/core/properties.h>
#include <mitsuba/core/spectrum.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-circular:

Circular polarizer material (:monosp:`circular`)
------------------------------------------------

.. pluginparameters::

 * - transmittance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular transmission. (Default: 1.0)
   - |exposed|, |differentiable|

 * - left_handed
   - |bool|
   - Flag to switch between left and right circular polarization. (Default: |false|, i.e. right circular polarizer)

This material simulates an ideal circular polarizer useful to test polarization aware
light transport or to conduct virtual optical experiments. Unlike the
:ref:`linear retarder <bsdf-retarder>` or the :ref:`linear polarizer <bsdf-polarizer>`,
this filter is invariant to rotations and therefore does not provide a corresponding
``theta`` parameter.

The following XML snippet describes a left circular polarizer material:

.. tabs::
    .. code-tab:: xml
        :name: circular

        <bsdf type="circular">
            <boolean name="left_handed" value="true"/>
        </bsdf>

    .. code-tab:: python

        'type': 'circular',
        'left_handed': True

Apart from a change of polarization, light does not interact with this material
in any way and does not change its direction.
Internally, this is implemented as a forward-facing Dirac delta distribution.
Note that the standard :ref:`path tracer <integrator-path>` does not have a good
sampling strategy to deal with this, but the (:ref:`volumetric path tracer <integrator-volpath>`) does.

In *unpolarized* rendering modes, the behavior defaults to non-polarizing
transparent material similar to the :ref:`null <bsdf-null>` BSDF plugin.

*/
template <typename Float, typename Spectrum>
class CircularPolarizer final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    CircularPolarizer(const Properties &props) : Base(props) {
        m_transmittance = props.texture<Texture>("transmittance", 1.f);
        m_left_handed = props.get<bool>("left_handed", false);

        m_flags = BSDFFlags::FrontSide | BSDFFlags::BackSide | BSDFFlags::Null;
        m_components.push_back(m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("transmittance",   m_transmittance.get(),  +ParamFlags::Differentiable);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx, const SurfaceInteraction3f &si,
                                             Float /* sample1 */, const Point2f &/* sample2 */,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        bs.wo = -si.wi;
        bs.pdf = 1.f;
        bs.eta = 1.f;
        bs.sampled_type = +BSDFFlags::Null;
        bs.sampled_component = 0;

        UnpolarizedSpectrum transmittance = m_transmittance->eval(si, active);

        if constexpr (is_polarized_v<Spectrum>) {
            // Get standard Mueller matrix for the circular polarizer.
            Spectrum M = m_left_handed ? mueller::left_circular_polarizer<Float>()
                                       : mueller::right_circular_polarizer<Float>();

            /* The `forward` direction here is always along the direction that
               light travels. This is needed for the coordinate system rotation
               below. */
            Vector3f forward = ctx.mode == TransportMode::Radiance ? si.wi : -si.wi;

            // Rotate in/out basis of M s.t. it aligns with BSDF coordinate frame
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        Vector3f(1.f, 0.f, 0.f),
                                                        mueller::stokes_basis(forward));

            // Handle potential absorption if transmittance < 1.0
            M *= mueller::absorber(transmittance);

            return { bs, M };
        } else {
            return { bs, 0.5f*transmittance };
        }
    }

    Spectrum eval(const BSDFContext &/* ctx */, const SurfaceInteraction3f &/* si */,
                  const Vector3f &/* wo */, Mask /* active */) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext &/* ctx */, const SurfaceInteraction3f &/* si */,
              const Vector3f &/* wo */, Mask /* active */) const override {
        return 0.f;
    }

    Spectrum eval_null_transmission(const SurfaceInteraction3f &si, Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFEvaluate, active);

        UnpolarizedSpectrum transmittance = m_transmittance->eval(si, active);
        if constexpr (is_polarized_v<Spectrum>) {
            // Get standard Mueller matrix for the circular polarizer.
            Spectrum M = m_left_handed ? mueller::left_circular_polarizer<Float>()
                                       : mueller::right_circular_polarizer<Float>();

            /* The `forward` direction here is always along the direction that
               light travels. This is needed for the coordinate system rotation
               below. */
            Vector3f forward = si.wi;   // Note: Should be reversed for TransportMode::Importance.

            // Rotate in/out basis of M s.t. it alignes with BSDF coordinate frame
            M = mueller::rotate_mueller_basis_collinear(M, forward,
                                                        Vector3f(1.f, 0.f, 0.f),
                                                        mueller::stokes_basis(forward));

            // Handle potential absorption if transmittance < 1.0
            M *= mueller::absorber(transmittance);

            return M;
        } else {
            return 0.5f*transmittance;
        }
    }

    Spectrum eval_diffuse_reflectance(const SurfaceInteraction3f &si,
                                      Mask active) const override {
        return m_transmittance->eval(si, active);
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "CircularPolarizer[" << std::endl
            << "  transmittance = " << string::indent(m_transmittance) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ref<Texture> m_transmittance;
    bool m_left_handed;
};

MI_IMPLEMENT_CLASS_VARIANT(CircularPolarizer, BSDF)
MI_EXPORT_PLUGIN(CircularPolarizer, "Circular polarizer material")
NAMESPACE_END(mitsuba)
