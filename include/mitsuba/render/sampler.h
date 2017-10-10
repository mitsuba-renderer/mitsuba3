#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

// TODO: port the Sampler interface
class MTS_EXPORT_RENDER Sampler : public Object {
public:

    virtual std::string to_string() const override {
        std::ostringstream oss;
        oss << "Sampler[not implemented]" << std::endl;
        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
    Sampler(const Properties &/*props*/) {
        configure();
    }

private:
    void configure() { };
};

NAMESPACE_END(mitsuba)
