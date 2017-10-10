#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

// TODO: documentation
class IndependentSampler : public Sampler {
public:
    IndependentSampler(const Properties &props) : Sampler(props) {
        configure();
    }


    virtual std::string to_string() const override {
        std::ostringstream oss;
        oss << "IndependentSampler[not implemented]";
        return oss.str();
    }

    MTS_DECLARE_CLASS()

private:
    void configure() { }
};

MTS_IMPLEMENT_CLASS(IndependentSampler, Sampler);
MTS_EXPORT_PLUGIN(IndependentSampler, "Independent Sampler");
NAMESPACE_END(mitsuba)
