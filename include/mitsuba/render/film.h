#pragma once

#include <mitsuba/mitsuba.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/object.h>
#include <mitsuba/core/rfilter.h>
#include <mitsuba/core/vector.h>
#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

class MTS_EXPORT_RENDER Film : public Object {
public:
    Vector2i size() const { return m_size; }
    Vector2i crop_size() const { return m_crop_size; }
    Point2i crop_offset() const { return m_crop_offset; }

    virtual std::string to_string() const override {
        std::ostringstream oss;
        oss << "Film[not implemented]" << std::endl;
        return oss.str();
    }

    MTS_DECLARE_CLASS()

protected:
    Film(const Properties &props);

    ~Film() { }

private:
    void configure();

protected:
    Vector2i m_size, m_crop_size;
    Point2i m_crop_offset;
    bool m_high_quality_edges;
    ref<ReconstructionFilter> m_filter;
};

NAMESPACE_END(mitsuba)
