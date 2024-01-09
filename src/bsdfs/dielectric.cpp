#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/texture.h>
#include <mitsuba/render/ior.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-dielectric:

Smooth dielectric material (:monosp:`dielectric`)
-------------------------------------------------

.. pluginparameters::
 :extra-rows: 4

 * - int_ior
   - |float| or |string|
   - Interior index of refraction specified numerically or using a known material name. (Default: bk7 / 1.5046)

 * - ext_ior
   - |float| or |string|
   - Exterior index of refraction specified numerically or using a known material name.  (Default: air / 1.000277)

 * - specular_reflectance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular reflection component. Note that for physical realism, this parameter should never be touched. (Default: 1.0)
   - |exposed|, |differentiable|

 * - specular_transmittance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular transmission component. Note that for physical realism, this parameter should never be touched. (Default: 1.0)
   - |exposed|, |differentiable|

 * - eta
   - |float|
   - Relative index of refraction from the exterior to the interior
   - |exposed|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_dielectric_glass.jpg
    :caption: Air ↔ Water (IOR: 1.33) interface.
.. subfigure:: ../../resources/data/docs/images/render/bsdf_dielectric_diamond.jpg
    :caption: Air ↔ Diamond (IOR: 2.419)
.. subfigend::
    :label: fig-bsdf-dielectric

This plugin models an interface between two dielectric materials having mismatched
indices of refraction (for instance, water ↔ air). Exterior and interior IOR values
can be specified independently, where "exterior" refers to the side that contains
the surface normal. When no parameters are given, the plugin activates the defaults, which
describe a borosilicate glass (BK7) ↔ air interface.

In this model, the microscopic structure of the surface is assumed to be perfectly
smooth, resulting in a degenerate BSDF described by a Dirac delta distribution.
This means that for any given incoming ray of light, the model always scatters into
a discrete set of directions, as opposed to a continuum. For a similar model that
instead describes a rough surface microstructure, take a look at the
:ref:`roughdielectric <bsdf-roughdielectric>` plugin.

This snippet describes a simple air-to-water interface

.. tabs::
    .. code-tab:: xml
        :name: dielectric-water

        <shape type="...">
            <bsdf type="dielectric">
                <string name="int_ior" value="water"/>
                <string name="ext_ior" value="air"/>
            </bsdf>
        <shape>

    .. code-tab:: python

        'type': 'dielectric',
        'int_ior': 'water',
        'ext_ior': 'air'

When using this model, it is crucial that the scene contains
meaningful and mutually compatible indices of refraction changes---see the
section about :ref:`correctness considerations <bsdf-correctness>` for a
description of what this entails.

In many cases, we will want to additionally describe the *medium* within a
dielectric material. This requires the use of a rendering technique that is
aware of media (e.g. the :ref:`volumetric path tracer <integrator-volpath>`).
An example of how one might describe a slightly absorbing piece of glass is shown below:

.. tabs::
    .. code-tab:: xml
        :name: dielectric-glass

        <shape type="...">
            <bsdf type="dielectric">
                <float name="int_ior" value="1.504"/>
                <float name="ext_ior" value="1.0"/>
            </bsdf>

            <medium type="homogeneous" name="interior">
                <float name="scale" value="4"/>
                <rgb name="sigma_t" value="1, 1, 0.5"/>
                <rgb name="albedo" value="0.0, 0.0, 0.0"/>
            </medium>
        <shape>

    .. code-tab:: python

        'type': '...',
        'glass':  {
            'type': 'dielectric',
            'int_ior': 1.504,
            'ext_ior': 1.0
        },
        'interior': {
            'type': 'homogeneous',
            'scale': 4,
            'sigma_t': {
                'type': 'rgb',
                'value': [1, 1, 0.5]
            },
            'albedo': {
                'type': 'rgb',
                'value': [0.0, 0.0, 0.0]
            }
        }

In *polarized* rendering modes, the material automatically switches to a polarized
implementation of the underlying Fresnel equations that quantify the reflectance and
transmission.

Instead of specifying numerical values for the indices of refraction, Mitsuba 3
comes with a list of presets that can be specified with the :paramtype:`material`
parameter:

.. figtable::
    :label: ior-table-list
    :caption: This table lists all supported material names
       along with along with their associated index of refraction at standard conditions.
       These material names can be used with the plugins :ref:`dielectric <bsdf-dielectric>`,
       :ref:`roughdielectric <bsdf-roughdielectric>`, :ref:`plastic <bsdf-plastic>`
       , as well as :ref:`roughplastic <bsdf-roughplastic>`.
    :alt: List table

    .. list-table::
        :widths: 35 25 35 25
        :header-rows: 1

        * - Name
          - Value
          - Name
          - Value
        * - :paramtype:`vacuum`
          - 1.0
          - :paramtype:`acetone`
          - 1.36
        * - :paramtype:`bromine`
          - 1.661
          - :paramtype:`bk7`
          - 1.5046
        * - :paramtype:`helium`
          - 1.00004
          - :paramtype:`ethanol`
          - 1.361
        * - :paramtype:`water ice`
          - 1.31
          - :paramtype:`sodium chloride`
          - 1.544
        * - :paramtype:`hydrogen`
          - 1.00013
          - :paramtype:`carbon tetrachloride`
          - 1.461
        * - :paramtype:`fused quartz`
          - 1.458
          - :paramtype:`amber`
          - 1.55
        * - :paramtype:`air`
          - 1.00028
          - :paramtype:`glycerol`
          - 1.4729
        * - :paramtype:`pyrex`
          - 1.470
          - :paramtype:`pet`
          - 1.575
        * - :paramtype:`carbon dioxide`
          - 1.00045
          - :paramtype:`benzene`
          - 1.501
        * - :paramtype:`acrylic glass`
          - 1.49
          - :paramtype:`diamond`
          - 2.419
        * - :paramtype:`water`
          - 1.3330
          - :paramtype:`silicone oil`
          - 1.52045
        * - :paramtype:`polypropylene`
          - 1.49
          -
          -
 */

template <typename Float, typename Spectrum>
class SmoothDielectric final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    SmoothDielectric(const Properties &props) : Base(props) {

        // Specifies the internal index of refraction at the interface
        ScalarFloat int_ior = lookup_ior(props, "int_ior", "bk7");

        // Specifies the external index of refraction at the interface
        ScalarFloat ext_ior = lookup_ior(props, "ext_ior", "air");

        if (int_ior < 0 || ext_ior < 0)
            Throw("The interior and exterior indices of refraction must"
                  " be positive!");

        m_eta = int_ior / ext_ior;

        if (props.has_property("specular_reflectance"))
            m_specular_reflectance   = props.texture<Texture>("specular_reflectance", 1.f);
        if (props.has_property("specular_transmittance"))
            m_specular_transmittance = props.texture<Texture>("specular_transmittance", 1.f);

        m_components.push_back(BSDFFlags::DeltaReflection | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide);
        m_components.push_back(BSDFFlags::DeltaTransmission | BSDFFlags::FrontSide |
                               BSDFFlags::BackSide | BSDFFlags::NonSymmetric);

        m_flags = m_components[0] | m_components[1];
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("eta", m_eta, +ParamFlags::NonDifferentiable);
        if (m_specular_reflectance)
            callback->put_object("specular_reflectance",   m_specular_reflectance.get(),   +ParamFlags::Differentiable);
        if (m_specular_transmittance)
            callback->put_object("specular_transmittance", m_specular_transmittance.get(), +ParamFlags::Differentiable);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float sample1,
                                             const Point2f & /* sample2 */,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        bool has_reflection   = ctx.is_enabled(BSDFFlags::DeltaReflection, 0),
             has_transmission = ctx.is_enabled(BSDFFlags::DeltaTransmission, 1);

        // Evaluate the Fresnel equations for unpolarized illumination
        Float cos_theta_i = Frame3f::cos_theta(si.wi);

        auto [r_i, cos_theta_t, eta_it, eta_ti] = fresnel(cos_theta_i, Float(m_eta));
        Float t_i = 1.f - r_i;

        // Lobe selection
        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        Mask selected_r;
        if (likely(has_reflection && has_transmission)) {
            selected_r = sample1 <= r_i && active;
            bs.pdf = dr::detach(dr::select(selected_r, r_i, t_i));
        } else {
            if (has_reflection || has_transmission) {
                selected_r = Mask(has_reflection) && active;
                bs.pdf = 1.f;
            } else {
                return { bs, 0.f };
            }
        }
        Mask selected_t = !selected_r && active;

        bs.sampled_component = dr::select(selected_r, UInt32(0), UInt32(1));
        bs.sampled_type      = dr::select(selected_r, UInt32(+BSDFFlags::DeltaReflection),
                                                      UInt32(+BSDFFlags::DeltaTransmission));

        bs.wo = dr::select(selected_r,
                           reflect(si.wi),
                           refract(si.wi, cos_theta_t, eta_ti));

        bs.eta = dr::select(selected_r, Float(1.f), eta_it);

        UnpolarizedSpectrum reflectance = 1.f, transmittance = 1.f;
        if (m_specular_reflectance)
            reflectance = m_specular_reflectance->eval(si, selected_r);
        if (m_specular_transmittance)
            transmittance = m_specular_transmittance->eval(si, selected_t);

        Spectrum weight(0.f);
        if constexpr (is_polarized_v<Spectrum>) {
            /* Due to the coordinate system rotations for polarization-aware
               pBSDFs below we need to know the propagation direction of light.
               In the following, light arrives along `-wo_hat` and leaves along
               `+wi_hat`. */
            Vector3f wo_hat = ctx.mode == TransportMode::Radiance ? bs.wo : si.wi,
                     wi_hat = ctx.mode == TransportMode::Radiance ? si.wi : bs.wo;

            /* BSDF weights are Mueller matrices now. */
            Float cos_theta_o_hat = Frame3f::cos_theta(wo_hat);
            Spectrum R = mueller::specular_reflection(UnpolarizedSpectrum(cos_theta_o_hat), UnpolarizedSpectrum(m_eta)),
                     T = mueller::specular_transmission(UnpolarizedSpectrum(cos_theta_o_hat), UnpolarizedSpectrum(m_eta));

            if (likely(has_reflection && has_transmission)) {
                weight = dr::select(selected_r, R, T) / bs.pdf;
            } else if (has_reflection || has_transmission) {
                weight = has_reflection ? R : T;
                bs.pdf = 1.f;
            }

            /* The Stokes reference frame vector of this matrix lies perpendicular
               to the plane of reflection. */
            Vector3f n(0, 0, 1);
            Vector3f s_axis_in  = dr::cross(n, -wo_hat);
            Vector3f s_axis_out = dr::cross(n, wi_hat);

            // Singularity when the input & output are collinear with the normal
            Mask collinear = dr::all(s_axis_in == Vector3f(0));
            s_axis_in  = dr::select(collinear, Vector3f(1, 0, 0),
                                               dr::normalize(s_axis_in));
            s_axis_out = dr::select(collinear, Vector3f(1, 0, 0),
                                               dr::normalize(s_axis_out));

            /* Rotate in/out reference vector of `weight` s.t. it aligns with the
               implicit Stokes bases of -wo_hat & wi_hat. */
            weight = mueller::rotate_mueller_basis(weight,
                                                   -wo_hat, s_axis_in, mueller::stokes_basis(-wo_hat),
                                                    wi_hat, s_axis_out, mueller::stokes_basis(wi_hat));

            if (dr::any_or<true>(selected_r))
                weight[selected_r] *= mueller::absorber(reflectance);

            if (dr::any_or<true>(selected_t))
                weight[selected_t] *= mueller::absorber(transmittance);

        } else {
            if (likely(has_reflection && has_transmission)) {
                weight = 1.f;
                /* For differentiable variants, lobe choice has to be detached to avoid bias.
                    Sampling weights should be computed accordingly. */
                if constexpr (dr::is_diff_v<Float>) {
                    if (dr::grad_enabled(r_i)) {
                        Float r_diff = dr::replace_grad(Float(1.f), r_i / dr::detach(r_i));
                        Float t_diff = dr::replace_grad(Float(1.f), t_i / dr::detach(t_i));
                        weight = dr::select(selected_r, r_diff, t_diff);
                    }
                }
            } else if (has_reflection || has_transmission) {
                weight = has_reflection ? r_i : t_i;
            }

            if (dr::any_or<true>(selected_r))
                weight[selected_r] *= reflectance;

            if (dr::any_or<true>(selected_t))
                weight[selected_t] *= transmittance;
        }

        if (dr::any_or<true>(selected_t)) {
            /* For transmission, radiance must be scaled to account for the solid
               angle compression that occurs when crossing the interface. */
            Float factor = (ctx.mode == TransportMode::Radiance) ? eta_ti : Float(1.f);
            weight[selected_t] *= dr::square(factor);
        }

        return { bs, weight & active };
    }

    Spectrum eval(const BSDFContext & /* ctx */, const SurfaceInteraction3f & /* si */,
                  const Vector3f & /* wo */, Mask /* active */) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext & /* ctx */, const SurfaceInteraction3f & /* si */,
              const Vector3f & /* wo */, Mask /* active */) const override {
        return 0.f;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothDielectric[" << std::endl;
        if (m_specular_reflectance)
            oss << "  specular_reflectance = " << string::indent(m_specular_reflectance) << "," << std::endl;
        if (m_specular_transmittance)
            oss << "  specular_transmittance = " << string::indent(m_specular_transmittance) << ", " << std::endl;
        oss << "  eta = " << m_eta << "," << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ScalarFloat m_eta;
    ref<Texture> m_specular_reflectance;
    ref<Texture> m_specular_transmittance;
};

MI_IMPLEMENT_CLASS_VARIANT(SmoothDielectric, BSDF)
MI_EXPORT_PLUGIN(SmoothDielectric, "Smooth dielectric")
NAMESPACE_END(mitsuba)
