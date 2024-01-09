#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/fresnel.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/texture.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _bsdf-conductor:

Smooth conductor (:monosp:`conductor`)
-------------------------------------------

.. pluginparameters::

 * - material
   - |string|
   - Name of the material preset, see :num:`conductor-ior-list`. (Default: none)

 * - eta, k
   - |spectrum| or |texture|
   - Real and imaginary components of the material's index of refraction. (Default: based on the value of :paramtype:`material`)
   - |exposed|, |differentiable|, |discontinuous|

 * - specular_reflectance
   - |spectrum| or |texture|
   - Optional factor that can be used to modulate the specular reflection component.
     Note that for physical realism, this parameter should never be touched. (Default: 1.0)
   - |exposed|, |differentiable|

.. subfigstart::
.. subfigure:: ../../resources/data/docs/images/render/bsdf_conductor_gold.jpg
   :caption: Gold
.. subfigure:: ../../resources/data/docs/images/render/bsdf_conductor_aluminium.jpg
   :caption: Aluminium
.. subfigend::
    :label: fig-bsdf-conductor

This plugin implements a perfectly smooth interface to a conducting material,
such as a metal that is described using a Dirac delta distribution. For a
similar model that instead describes a rough surface microstructure, take a look
at the separately available :ref:`roughconductor <bsdf-roughconductor>` plugin.
In contrast to dielectric materials, conductors do not transmit
any light. Their index of refraction is complex-valued and tends to undergo
considerable changes throughout the visible color spectrum.

When using this plugin, you should ideally enable one of the :monosp:`spectral`
modes of the renderer to get the most accurate results. While it also works
in RGB mode, the computations will be more approximate in nature.
Also note that this material is one-sided---that is, observed from the
back side, it will be completely black. If this is undesirable,
consider using the :ref:`twosided <bsdf-twosided>` BRDF adapter plugin.

The following XML snippet describes a material definition for gold:

.. tabs::
    .. code-tab:: xml
        :name: lst-conductor-gold

        <bsdf type="conductor">
            <string name="material" value="Au"/>
        </bsdf>

    .. code-tab:: python

        'type': 'conductor',
        'material': 'Au'

It is also possible to load spectrally varying index of refraction data from
two external files containing the real and imaginary components,
respectively (see :ref:`Scene format <sec-file-format>` for details on the file format):

.. tabs::
    .. code-tab:: xml
        :name: lst-conductor-files

        <bsdf type="conductor">
            <spectrum name="eta" filename="conductorIOR.eta.spd"/>
            <spectrum name="k" filename="conductorIOR.k.spd"/>
        </bsdf>


    .. code-tab:: python

        'type': 'conductor',
        'eta':  {
            'type': 'spectrum',
            'filename': 'conductorIOR.eta.spd'
        },
        'k':  {
            'type': 'spectrum',
            'filename': 'conductorIOR.k.spd'
        }

In *polarized* rendering modes, the material automatically switches to a polarized
implementation of the underlying Fresnel equations.

To facilitate the tedious task of specifying spectrally-varying index of
refraction information, Mitsuba 3 ships with a set of measured data for several
materials, where visible-spectrum information was publicly
available:

.. figtable::
    :label: conductor-ior-list
    :caption: This table lists all supported materials that can be passed into the
       :ref:`conductor <bsdf-conductor>` and :ref:`roughconductor <bsdf-roughconductor>`
       plugins. Note that some of them are not actually conductors---this is not a
       problem, they can be used regardless (though only the reflection component
       and no transmission will be simulated). In most cases, there are multiple
       entries for each material, which represent measurements by different
       authors.
    :alt: List table

    .. list-table::
        :widths: 15 30 15 30
        :header-rows: 1

        * - Preset(s)
          - Description
          - Preset(s)
          - Description
        * - :paramtype:`a-C`
          - Amorphous carbon
          - :paramtype:`Na_palik`
          - Sodium
        * - :paramtype:`Ag`
          - Silver
          - :paramtype:`Nb`, :paramtype:`Nb_palik`
          - Niobium
        * - :paramtype:`Al`
          - Aluminium
          - :paramtype:`Ni_palik`
          - Nickel
        * - :paramtype:`AlAs`, :paramtype:`AlAs_palik`
          - Cubic aluminium arsenide
          - :paramtype:`Rh`, :paramtype:`Rh_palik`
          - Rhodium
        * - :paramtype:`AlSb`, :paramtype:`AlSb_palik`
          - Cubic aluminium antimonide
          - :paramtype:`Se`, :paramtype:`Se_palik`
          - Selenium
        * - :paramtype:`Au`
          - Gold
          - :paramtype:`SiC`, :paramtype:`SiC_palik`
          - Hexagonal silicon carbide
        * - :paramtype:`Be`, :paramtype:`Be_palik`
          - Polycrystalline beryllium
          - :paramtype:`SnTe`, :paramtype:`SnTe_palik`
          - Tin telluride
        * - :paramtype:`Cr`
          - Chromium
          - :paramtype:`Ta`, :paramtype:`Ta_palik`
          - Tantalum
        * - :paramtype:`CsI`, :paramtype:`CsI_palik`
          - Cubic caesium iodide
          - :paramtype:`Te`, :paramtype:`Te_palik`
          - Trigonal tellurium
        * - :paramtype:`Cu`, :paramtype:`Cu_palik`
          - Copper
          - :paramtype:`ThF4`, :paramtype:`ThF4_palik`
          - Polycryst. thorium (IV) fluoride
        * - :paramtype:`Cu2O`, :paramtype:`Cu2O_palik`
          - Copper (I) oxide
          - :paramtype:`TiC`, :paramtype:`TiC_palik`
          - Polycrystalline titanium carbide
        * - :paramtype:`CuO`, :paramtype:`CuO_palik`
          - Copper (II) oxide
          - :paramtype:`TiN`, :paramtype:`TiN_palik`
          - Titanium nitride
        * - :paramtype:`d-C`, :paramtype:`d-C_palik`
          - Cubic diamond
          - :paramtype:`TiO2`, :paramtype:`TiO2_palik`
          - Tetragonal titan. dioxide
        * - :paramtype:`Hg`, :paramtype:`Hg_palik`
          - Mercury
          - :paramtype:`VC`, :paramtype:`VC_palik`
          - Vanadium carbide
        * - :paramtype:`HgTe`, :paramtype:`HgTe_palik`
          - Mercury telluride
          - :paramtype:`V_palik`
          - Vanadium
        * - :paramtype:`Ir`, :paramtype:`Ir_palik`
          - Iridium
          - :paramtype:`VN`, :paramtype:`VN_palik`
          - Vanadium nitride
        * - :paramtype:`K`, :paramtype:`K_palik`
          - Polycrystalline potassium
          - :paramtype:`W`
          - Tungsten
        * - :paramtype:`Li`, :paramtype:`Li_palik`
          - Lithium
          -
          -
        * - :paramtype:`MgO`, :paramtype:`MgO_palik`
          - Magnesium oxide
          -
          -
        * - :paramtype:`Mo`, :paramtype:`Mo_palik`
          - Molybdenum
          - :paramtype:`none`
          - No mat. profile (100% reflecting mirror)

These index of refraction values are identical to the data distributed
with PBRT. They are originally from the `Luxpop database <http://www.luxpop.com>`_
and are based on data by Palik et al. :cite:`Palik1998Handbook` and measurements
of atomic scattering factors made by the Center For X-Ray Optics (CXRO) at
Berkeley and the Lawrence Livermore National Laboratory (LLNL).

There is also a special material profile named :paramtype:`none`, which disables
the computation of Fresnel reflectances and produces an idealized
100% reflecting mirror.

 */

template <typename Float, typename Spectrum>
class SmoothConductor final : public BSDF<Float, Spectrum> {
public:
    MI_IMPORT_BASE(BSDF, m_flags, m_components)
    MI_IMPORT_TYPES(Texture)

    SmoothConductor(const Properties &props) : Base(props) {
        m_flags = BSDFFlags::DeltaReflection | BSDFFlags::FrontSide;
        m_components.push_back(m_flags);

        m_specular_reflectance = props.texture<Texture>("specular_reflectance", 1.f);

        std::string material = props.string("material", "none");
        if (props.has_property("eta") || material == "none") {
            m_eta = props.texture<Texture>("eta", 0.f);
            m_k   = props.texture<Texture>("k",   1.f);
            if (material != "none")
                Throw("Should specify either (eta, k) or material, not both.");
        } else {
            std::tie(m_eta, m_k) = complex_ior_from_file<Spectrum, Texture>(props.string("material", "Cu"));
        }
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_object("eta",                  m_eta.get(),                  ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_object("k",                    m_k.get(),                    ParamFlags::Differentiable | ParamFlags::Discontinuous);
        callback->put_object("specular_reflectance", m_specular_reflectance.get(), +ParamFlags::Differentiable);
    }

    std::pair<BSDFSample3f, Spectrum> sample(const BSDFContext &ctx,
                                             const SurfaceInteraction3f &si,
                                             Float /* sample1 */,
                                             const Point2f &/* sample2 */,
                                             Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::BSDFSample, active);

        Float cos_theta_i = Frame3f::cos_theta(si.wi);
        active &= cos_theta_i > 0.f;

        BSDFSample3f bs = dr::zeros<BSDFSample3f>();
        Spectrum value(0.f);
        if (unlikely(dr::none_or<false>(active) || !ctx.is_enabled(BSDFFlags::DeltaReflection)))
            return { bs, value };

        bs.sampled_component = 0;
        bs.sampled_type = +BSDFFlags::DeltaReflection;
        bs.wo  = reflect(si.wi);
        bs.eta = 1.f;
        bs.pdf = 1.f;

        dr::Complex<UnpolarizedSpectrum> eta(m_eta->eval(si, active),
                                             m_k->eval(si, active));
        UnpolarizedSpectrum reflectance = m_specular_reflectance->eval(si, active);

        if constexpr (is_polarized_v<Spectrum>) {
            /* Due to the coordinate system rotations for polarization-aware
               pBSDFs below we need to know the propagation direction of light.
               In the following, light arrives along `-wo_hat` and leaves along
               `+wi_hat`. */
            Vector3f wo_hat = ctx.mode == TransportMode::Radiance ? bs.wo : si.wi,
                     wi_hat = ctx.mode == TransportMode::Radiance ? si.wi : bs.wo;

            // Mueller matrix for specular reflection.
            value = mueller::specular_reflection(UnpolarizedSpectrum(Frame3f::cos_theta(wo_hat)), eta);

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

            /* Rotate in/out reference vector of `value` s.t. it aligns with the
               implicit Stokes bases of -wo_hat & wi_hat. */
            value = mueller::rotate_mueller_basis(value,
                                                  -wo_hat, s_axis_in, mueller::stokes_basis(-wo_hat),
                                                   wi_hat, s_axis_out, mueller::stokes_basis(wi_hat));
            value *= mueller::absorber(reflectance);
        } else {
            value = reflectance * fresnel_conductor(UnpolarizedSpectrum(cos_theta_i), eta);
        }

        return { bs, value & active };
    }

    Spectrum eval(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
                  const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    Float pdf(const BSDFContext & /*ctx*/, const SurfaceInteraction3f & /*si*/,
              const Vector3f & /*wo*/, Mask /*active*/) const override {
        return 0.f;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "SmoothConductor[" << std::endl
            << "  eta = " << string::indent(m_eta) << "," << std::endl
            << "  k = "   << string::indent(m_k)   << "," << std::endl
            << "  specular_reflectance = " << string::indent(m_specular_reflectance) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    ref<Texture> m_specular_reflectance;
    ref<Texture> m_eta, m_k;
};

MI_IMPLEMENT_CLASS_VARIANT(SmoothConductor, BSDF)
MI_EXPORT_PLUGIN(SmoothConductor, "Smooth conductor")
NAMESPACE_END(mitsuba)
