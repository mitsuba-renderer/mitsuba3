#include <mitsuba/core/distr_1d.h>
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/phase.h>

NAMESPACE_BEGIN(mitsuba)

/**!

.. _phase-tabphase_irregular:

Tabulated phase function (irregular angular grid) (:monosp:`tabphase_irregular`)
--------------------------------------------------------------------------------

.. pluginparameters::

 * - values
   - |string|
   - A comma-separated list of phase function values parameterized by the
     cosine of the scattering angle. Must have the same length as `nodes`.
   - |exposed|, |differentiable|, |discontinuous|

 * - nodes
   - |string|
   - A comma-separated list of :math:`\cos \theta` specifying the grid on which
     `values` are defined. Bounds must be [-1, 1] and values must be strictly
     increasing. Must have the same length as `values`.
   - |exposed|, |differentiable|, |discontinuous|

This plugin implements a generic phase function model for isotropic media
parametrized by a lookup table giving values of the phase function as a
function of the cosine of the scattering angle.

This plugin is a variant of the ``tabphase`` plugin and behaves similarly but
uses an irregular distribution internally. Consequently, ``tabphase`` performs
better for evaluation and sampling.
*/

template <typename Float, typename Spectrum>
class IrregularTabulatedPhaseFunction final
    : public PhaseFunction<Float, Spectrum> {
public:
    MI_IMPORT_BASE(PhaseFunction, m_flags, m_components)
    MI_IMPORT_TYPES(PhaseFunctionContext)

    IrregularTabulatedPhaseFunction(const Properties &props) : Base(props) {
        std::vector<ScalarFloat> values;
        std::vector<ScalarFloat> nodes;

        if (props.type("values") == Properties::Type::String) {
            std::vector<std::string> values_str =
                string::tokenize(props.string("values"), " ,");
            values.reserve(values_str.size());

            for (const auto &s : values_str) {
                try {
                    values.push_back((ScalarFloat) std::stod(s));
                } catch (...) {
                    Throw("Could not parse floating point value '%s'", s);
                }
            }
        } else {
            Throw("'values' must be a string");
        }

        if (props.type("nodes") == Properties::Type::String) {
            std::vector<std::string> nodes_str =
                string::tokenize(props.string("nodes"), " ,");
            nodes.reserve(nodes_str.size());

            for (const auto &s : nodes_str) {
                try {
                    nodes.push_back((ScalarFloat) std::stod(s));
                } catch (...) {
                    Throw("Could not parse floating point value '%s'", s);
                }
            }
        } else {
            Throw("'nodes' must be a string");
        }

        // Check that values and nodes have the same size
        if (values.size() != nodes.size()) {
            Throw("'nodes' and 'values' must have the same length");
        }

        // Check that nodes cover the [-1, 1] segment
        if (nodes[0] != -1.f || nodes[nodes.size() - 1] != 1.f) {
            Throw("'nodes' bounds must be [-1, 1], got [%s, %s]", nodes[0],
                  nodes[nodes.size() - 1]);
        }

        m_distr = IrregularContinuousDistribution<Float>(
            nodes.data(), values.data(), values.size());

        m_flags = +PhaseFunctionFlags::Anisotropic;
        dr::set_attr(this, "flags", m_flags);
        m_components.push_back(m_flags);
    }

    void traverse(TraversalCallback *callback) override {
        callback->put_parameter("values", m_distr.pdf(),
                                ParamFlags::Differentiable |
                                    ParamFlags::Discontinuous);
        callback->put_parameter("nodes", m_distr.nodes(),
                                ParamFlags::Differentiable |
                                    ParamFlags::Discontinuous);
    }

    void
    parameters_changed(const std::vector<std::string> & /*keys*/) override {
        m_distr.update();
    }

    std::pair<Vector3f, Float> sample(const PhaseFunctionContext & /* ctx */,
                                      const MediumInteraction3f &mi,
                                      Float /* sample1 */,
                                      const Point2f &sample2,
                                      Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionSample, active);

        // Sample a direction in physics convention.
        // We sample cos θ' = cos(π - θ) = -cos θ.
        Float cos_theta_prime = m_distr.sample(sample2.x());
        Float sin_theta_prime =
            dr::safe_sqrt(1.f - cos_theta_prime * cos_theta_prime);
        auto [sin_phi, cos_phi] =
            dr::sincos(2.f * dr::Pi<ScalarFloat> * sample2.y());
        Vector3f wo{ sin_theta_prime * cos_phi, sin_theta_prime * sin_phi,
                     cos_theta_prime };

        // Switch the sampled direction to graphics convention and transform the
        // computed direction to world coordinates
        wo = -mi.to_world(wo);

        // Retrieve the PDF value from the physics convention-sampled angle
        Float pdf = m_distr.eval_pdf_normalized(cos_theta_prime, active) *
                    dr::InvTwoPi<ScalarFloat>;

        return { wo, pdf };
    }

    Float eval(const PhaseFunctionContext & /* ctx */,
               const MediumInteraction3f &mi, const Vector3f &wo,
               Mask active) const override {
        MI_MASKED_FUNCTION(ProfilerPhase::PhaseFunctionEvaluate, active);

        // The data is laid out in physics convention
        // (with cos θ = 1 corresponding to forward scattering).
        // This parameterization differs from the convention used internally by
        // Mitsuba and is the reason for the minus sign below.
        Float cos_theta = -dot(wo, mi.wi);
        return m_distr.eval_pdf_normalized(cos_theta, active) *
               dr::InvTwoPi<ScalarFloat>;
    }

    std::string to_string() const override {
        std::ostringstream oss;
        oss << "IrregularTabulatedPhaseFunction[" << std::endl
            << "  distr = " << string::indent(m_distr) << std::endl
            << "]";
        return oss.str();
    }

    MI_DECLARE_CLASS()
private:
    IrregularContinuousDistribution<Float> m_distr;
};

MI_IMPLEMENT_CLASS_VARIANT(IrregularTabulatedPhaseFunction, PhaseFunction)
MI_EXPORT_PLUGIN(IrregularTabulatedPhaseFunction, "Tabulated phase function")
NAMESPACE_END(mitsuba)
