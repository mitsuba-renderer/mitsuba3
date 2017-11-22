
#include <mitsuba/core/string.h>
#include <mitsuba/core/fwd.h>
#include <mitsuba/render/bsdf.h>
#include <mitsuba/render/ior.h>
#include <mitsuba/render/microfacet.h>

NAMESPACE_BEGIN(mitsuba)

/*!\plugin{roughconductor}{Rough conductor material}
* \order{7}
* \icon{bsdf_roughconductor}
* \parameters{
*     \parameter{distribution}{\String}{
*          Specifies the type of microfacet normal distribution
*          used to model the surface roughness.
*          \vspace{-1mm}
*       \begin{enumerate}[(i)]
*           \item \code{beckmann}: Physically-based distribution derived from
*               Gaussian random surfaces. This is the default.\vspace{-1.5mm}
*           \item \code{ggx}: The GGX \cite{Walter07Microfacet} distribution (also known as
*               Trowbridge-Reitz \cite{Trowbridge19975Average} distribution)
*               was designed to better approximate the long tails observed in measurements
*               of ground surfaces, which are not modeled by the Beckmann distribution.
*           \vspace{-1.5mm}
*           \item \code{phong}: Anisotropic Phong distribution by
*              Ashikhmin and Shirley \cite{Ashikhmin2005Anisotropic}.
*              In most cases, the \code{ggx} and \code{beckmann} distributions
*              should be preferred, since they provide better importance sampling
*              and accurate shadowing/masking computations.
*              \vspace{-4mm}
*       \end{enumerate}
*     }
*     \parameter{alpha, alphaU, alphaV}{\Float\Or\Texture}{
*         Specifies the roughness of the unresolved surface micro-geometry
*         along the tangent and bitangent directions. When the Beckmann
*         distribution is used, this parameter is equal to the
*         \emph{root mean square} (RMS) slope of the microfacets.
*         \code{alpha} is a convenience parameter to initialize both
*         \code{alphaU} and \code{alphaV} to the same value. \default{0.1}.
*     }
*     \parameter{material}{\String}{Name of a material preset, see
*           \tblref{conductor-iors}.\!\default{\texttt{Cu} / copper}}
*     \parameter{eta, k}{\Spectrum}{Real and imaginary components of the material's index of
*             refraction \default{based on the value of \texttt{material}}}
*     \parameter{ext_eta}{\Float\Or\String}{
*           Real-valued index of refraction of the surrounding dielectric,
*           or a material name of a dielectric \default{\code{air}}
*     }
*     \parameter{sample_visible}{\Boolean}{
*         Enables a sampling technique proposed by Heitz and D'Eon~\cite{Heitz1014Importance},
*         which focuses computation on the visible parts of the microfacet normal
*         distribution, considerably reducing variance in some cases.
*         \default{\code{true}, i.e. use visible normal sampling}
*     }
*     \parameter{specular\showbreak Reflectance}{\Spectrum\Or\Texture}{Optional
*         factor that can be used to modulate the specular reflection component. Note
*         that for physical realism, this parameter should never be touched. \default{1.0}}
* }
* \vspace{3mm}
* This plugin implements a realistic microfacet scattering model for rendering
* rough conducting materials, such as metals. It can be interpreted as a fancy
* version of the Cook-Torrance model and should be preferred over
* heuristic models like \pluginref{phong} and \pluginref{ward} if possible.
* \renderings{
*     \rendering{Rough copper (Beckmann, $\alpha=0.1$)}
*         {bsdf_roughconductor_copper.jpg}
*     \rendering{Vertically brushed aluminium (Anisotropic Phong,
*         $\alpha_u=0.05,\ \alpha_v=0.3$), see
*         \lstref{roughconductor-aluminium}}
*         {bsdf_roughconductor_anisotropic_aluminium.jpg}
* }
*
* Microfacet theory describes rough surfaces as an arrangement of unresolved
* and ideally specular facets, whose normal directions are given by a
* specially chosen \emph{microfacet distribution}. By accounting for shadowing
* and masking effects between these facets, it is possible to reproduce the
* important off-specular reflections peaks observed in real-world measurements
* of such materials.
*
* This plugin is essentially the ``roughened'' equivalent of the (smooth) plugin
* \pluginref{conductor}. For very low values of $\alpha$, the two will
* be identical, though scenes using this plugin will take longer to render
* due to the additional computational burden of tracking surface roughness.
*
* The implementation is based on the paper ``Microfacet Models
* for Refraction through Rough Surfaces'' by Walter et al.
* \cite{Walter07Microfacet}. It supports three different types of microfacet
* distributions and has a texturable roughness parameter.
* To facilitate the tedious task of specifying spectrally-varying index of
* refraction information, this plugin can access a set of measured materials
* for which visible-spectrum information was publicly available
* (see \tblref{conductor-iors} for the full list).
* There is also a special material profile named \code{none}, which disables
* the computation of Fresnel reflectances and produces an idealized
* 100% reflecting mirror.
*
* When no parameters are given, the plugin activates the default settings,
* which describe copper with a medium amount of roughness modeled using a
* Beckmann distribution.
*
* To get an intuition about the effect of the surface roughness parameter
* $\alpha$, consider the following approximate classification: a value of
* $\alpha=0.001-0.01$ corresponds to a material with slight imperfections
* on an otherwise smooth surface finish, $\alpha=0.1$ is relatively rough,
* and $\alpha=0.3-0.7$ is \emph{extremely} rough (e.g. an etched or ground
* finish). Values significantly above that are probably not too realistic.
* \vspace{4mm}
* \begin{xml}[caption={A material definition for brushed aluminium}, label=lst:roughconductor-aluminium]
* <bsdf type="roughconductor">
*     <string name="material" value="Al"/>
*     <string name="distribution" value="phong"/>
*     <float name="alphaU" value="0.05"/>
*     <float name="alphaV" value="0.3"/>
* </bsdf>
* \end{xml}
*
* \subsubsection*{Technical details}
* All microfacet distributions allow the specification of two distinct
* roughness values along the tangent and bitangent directions. This can be
* used to provide a material with a ``brushed'' appearance. The alignment
* of the anisotropy will follow the UV parameterization of the underlying
* mesh. This means that such an anisotropic material cannot be applied to
* triangle meshes that are missing texture coordinates.
*
* \label{sec:visiblenormal-sampling}
* Since Mitsuba 0.5.1, this plugin uses a new importance sampling technique
* contributed by Eric Heitz and Eugene D'Eon, which restricts the sampling
* domain to the set of visible (unmasked) microfacet normals. The previous
* approach of sampling all normals is still available and can be enabled
* by setting \code{sample_visible} to \code{false}.
* Note that this new method is only available for the \code{beckmann} and
* \code{ggx} microfacet distributions. When the \code{phong} distribution
* is selected, the parameter has no effect.
*
* When rendering with the Phong microfacet distribution, a conversion is
* used to turn the specified Beckmann-equivalent $\alpha$ roughness value
* into the exponent parameter of this distribution. This is done in a way,
* such that the same value $\alpha$ will produce a similar appearance across
* different microfacet distributions.
*
* When using this plugin, you should ideally compile Mitsuba with support for
* spectral rendering to get the most accurate results. While it also works
* in RGB mode, the computations will be more approximate in nature.
* Also note that this material is one-sided---that is, observed from the
* back side, it will be completely black. If this is undesirable,
* consider using the \pluginref{twosided} BRDF adapter.
*/

namespace {
/// Helper function: reflect \c wi with respect to a given surface normal
template <typename Vector3, typename Normal3>
inline Vector3 reflect(const Vector3 &wi, const Normal3 &m) {
    return 2.0f * dot(wi, m) * Vector3(m) - wi;
}

template <typename Spectrum, typename Value = typename Spectrum::Scalar>
Spectrum fresnel_conductor_exact(Value cos_theta_i, Spectrum eta, Spectrum k) {
    // Modified from "Optics" by K.D. Moeller, University Science Books, 1988

    Value cos_theta_i_2 = cos_theta_i * cos_theta_i,
        sin_theta_i_2 = 1.0f - cos_theta_i_2,
        sin_theta_i_4 = sin_theta_i_2 * sin_theta_i_2;

    Spectrum temp_1 = eta * eta - k * k - Spectrum(sin_theta_i_2),
        a_2_pb_2 = safe_sqrt(temp_1*temp_1 + 4.0f * k*k*eta*eta),
        a = safe_sqrt(0.5f * (a_2_pb_2 + temp_1));

    Spectrum term_1 = a_2_pb_2 + Spectrum(cos_theta_i_2),
        term_2 = 2.0f * cos_theta_i * a;

    Spectrum rs_2 = (term_1 - term_2) / (term_1 + term_2);

    Spectrum term_3 = a_2_pb_2 * cos_theta_i_2 + Spectrum(sin_theta_i_4),
        term_4 = term_2 * sin_theta_i_2;

    Spectrum rp_2 = rs_2 * (term_3 - term_4) / (term_3 + term_4);

    return 0.5f * (rp_2 + rs_2);
}
}

class RoughConductor : public BSDF {
public:
    RoughConductor(const Properties &props) {
        m_flags = EGlossyReflection | EFrontSide;
        m_specular_reflectance = props.spectrumf("radiance", Spectrumf(1.0f));

        Spectrumf int_eta = Spectrumf(0.0f),
                  int_k   = Spectrumf(1.0f);

        Float ext_eta = lookup_IOR(props, "ext_eta", "air");

        m_eta = props.spectrumf("eta", int_eta) / ext_eta;
        m_k   = props.spectrumf("k", int_k) / ext_eta;

        MicrofacetDistribution<Float> distr(props);
        m_type = distr.type();
        m_sample_visible = distr.sample_visible();

        m_alpha_u = distr.alpha_u();
        if (distr.alpha_u() == distr.alpha_v())
            m_alpha_v = m_alpha_u;
        else
            m_alpha_v = distr.alpha_v();
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value,
              typename Spectrum = Spectrum<Value>>
    Spectrum eval_impl(const BSDFSample &bs, EMeasure measure,
                       const mask_t<Value> & active_) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        // Stop if this component was not requested
        if ((bs.component != -1 && bs.component != 0)
            || !(bs.type_mask & EGlossyReflection)
            || measure != ESolidAngle)
            return 0.0f;

        Mask active(active_);

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        Value n_dot_wo = Frame::cos_theta(bs.wo);

        active &= !((n_dot_wi <= 0.0f) & (n_dot_wo <= 0.0f));

        if (none(active))
            return 0.0f;

        // Calculate the reflection half-vector
        Vector3 H = normalize(bs.wo + bs.wi);

        // Construct the microfacet distribution matching the
        // roughness values at the current surface position.
        MicrofacetDistribution<Value> distr(
            m_type,
            enoki::mean(m_alpha_u),
            enoki::mean(m_alpha_v),
            m_sample_visible
        );

        // Evaluate the microfacet normal distribution
        const Value D = distr.eval(H, active);

        active &= !eq(D, 0.0f);

        if (none(active))
            return 0.0f;

        // Fresnel factor
        const Spectrum F = fresnel_conductor_exact(dot(bs.wi, H), Spectrum(m_eta), Spectrum(m_k))
                           * m_specular_reflectance;

        // Smith's shadow-masking function
        const Value G = distr.G(bs.wi, bs.wo, H, active);

        // Calculate the total amount of reflection
        Value model = D * G / (4.0f * Frame::cos_theta(bs.wi));

        Spectrum result(0.f);
        masked(result, active) = F * model;

        return result;
    }

    template <typename BSDFSample,
              typename Value = typename BSDFSample::Value>
    Value pdf_impl(const BSDFSample &bs, EMeasure measure,
                   const mask_t<Value> &active_) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Frame   = Frame<Vector3>;
        using Mask    = mask_t<Value>;

        Mask active(active_);

        // Stop if this component was not requested
        if ((bs.component != -1 && bs.component != 0)
            || !(bs.type_mask & EGlossyReflection)
            || measure != ESolidAngle)
            return 0.0f;

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        Value n_dot_wo = Frame::cos_theta(bs.wo);

        active &= !((n_dot_wi <= 0.0f) & (n_dot_wo <= 0.0f));

        if (none(active))
            return 0.0f;

        // Calculate the reflection half-vector
        Vector3 H = normalize(bs.wo + bs.wi);

        // Construct the microfacet distribution matching the
        // roughness values at the current surface position.
        MicrofacetDistribution<Value> distr(
            m_type,
            enoki::mean(m_alpha_u),
            enoki::mean(m_alpha_v),
            m_sample_visible
        );

        Value result(0.0f);
        if (m_sample_visible)
            masked(result, active) = distr.eval(H, active)
                                     * distr.smith_g1(bs.wi, H, active) / (4.0f * n_dot_wi);
        else
            masked(result, active) = distr.pdf(bs.wi, H, active) / (4 * abs_dot(bs.wo, H));

        return result;
    }

    template <typename BSDFSample,
              typename Value  = typename BSDFSample::Value,
              typename Point2 = typename BSDFSample::Point2,
              typename Spectrum = Spectrum<Value>>
    std::pair<Spectrum, Value> sample_impl(BSDFSample &bs,
                                           const Point2 &sample,
                                           const mask_t<Value> &active_) const {
        using Vector3 = typename BSDFSample::Vector3;
        using Normal3 = typename BSDFSample::Normal3;
        using Frame = Frame<Vector3>;
        using Mask = mask_t<Value>;

        if ((bs.component != -1 && bs.component != 0)
            || !(bs.type_mask & EGlossyReflection))
            return { 0.0f, 0.0f };

        Mask active(active_);

        Value n_dot_wi = Frame::cos_theta(bs.wi);
        active &= !(n_dot_wi <= 0.0f);

        if (none(active))
            return { 0.0f, 0.0f };

        // Construct the microfacet distribution matching the
        // roughness values at the current surface position.
        MicrofacetDistribution<Value> distr(
            m_type,
            enoki::mean(m_alpha_u),
            enoki::mean(m_alpha_v),
            m_sample_visible
        );

        // Sample M, the microfacet normal
        Value pdf;
        Normal3 m;
        std::tie(m, pdf) = distr.sample(bs.wi, sample, active);

        active &= !eq(pdf, 0.0f);

        // Perfect specular reflection based on the microfacet normal
        masked(bs.wo,  active) = reflect(bs.wi, m);
        masked(bs.eta, active) = 1.0f;
        masked(bs.sampled_component, active) = 0;
        masked(bs.sampled_type, active) = EGlossyReflection;

        active &= !(Frame::cos_theta(bs.wo) <= 0.0f);

        // Side check
        if (none(active))
            return { 0.0f, 0.0f };

        const Spectrum F = fresnel_conductor_exact(dot(bs.wi, m), Spectrum(m_eta), Spectrum(m_k))
                           * m_specular_reflectance;

        Value weight;
        if (m_sample_visible)
            weight = distr.smith_g1(bs.wo, m, active);
        else
            weight = distr.eval(m, active) * distr.G(bs.wi, bs.wo, m, active)
                     * dot(bs.wi, m) / (pdf * n_dot_wi);

        // Jacobian of the half-direction mapping
        pdf /= 4.0f * dot(bs.wo, m);

        masked(pdf,    ~active) = 0.0f;
        masked(weight, ~active) = 0.0f;

        return { F * weight, pdf };
    }

    MTS_IMPLEMENT_BSDF()

    template <typename SurfaceInteraction3,
              typename Value = typename SurfaceInteraction3::Value>
    Value roughness(const SurfaceInteraction3 &/*its*/, int /*component*/) const {
        return 0.5f * (enoki::mean(m_alpha_u) + enoki::mean(m_alpha_v));
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "RoughConductor[" << std::endl
            << "  distribution = " << MicrofacetDistribution<Float>::distribution_name(m_type) << "," << std::endl
            << "  sample_visible = " << m_sample_visible << "," << std::endl
            << "  alpha_u = " << string::indent(m_alpha_u) << "," << std::endl
            << "  alpha_v = " << string::indent(m_alpha_v) << "," << std::endl
            << "  specular_reflectance = " << string::indent(m_specular_reflectance) << "," << std::endl
            << "  eta = " << m_eta << "," << std::endl
            << "  k = " << m_k << std::endl
            << "]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()
private:
    EType m_type;
    Spectrumf m_specular_reflectance;
    Spectrumf m_alpha_u, m_alpha_v;
    bool m_sample_visible;
    Spectrumf m_eta, m_k;
};

MTS_IMPLEMENT_CLASS(RoughConductor, BSDF)
MTS_EXPORT_PLUGIN(RoughConductor, "Rough conductor BRDF");

NAMESPACE_END(mitsuba)
