#include <mitsuba/render/sampler.h>

NAMESPACE_BEGIN(mitsuba)

void Sampler::request_1d_array(size_t size) {
    m_requests_1d.push_back(size);
    m_samples_1d.push_back(new Float[m_sample_count * size]);
}

void Sampler::request_2d_array(size_t size) {
    m_requests_2d.push_back(size);
    m_samples_2d.push_back(new Point2f[m_sample_count * size]);
}

void Sampler::request_1d_array_p(size_t size) {
    m_requests_1d_p.push_back(size);
    m_samples_1d_p.push_back(new FloatP[m_sample_count * size]);
}

void Sampler::request_2d_array_p(size_t size) {
    m_requests_2d_p.push_back(size);
    m_samples_2d_p.push_back(new Point2fP[m_sample_count * size]);
}

Float *Sampler::next_1d_array(size_t size) {
    Assert(m_sample_index < m_sample_count);
    if (unlikely(m_dimension_1d_array >= m_requests_1d.size())) {
        Throw("Tried to retrieve a size-%i 1D sample array,"
              " which was not previously allocated.", size);
    }
    Assert(m_requests_1d[m_dimension_1d_array] == size);
    return m_samples_1d[m_dimension_1d_array++] + m_sample_index * size;
}

Point2f *Sampler::next_2d_array(size_t size) {
    Assert(m_sample_index < m_sample_count);
    if (unlikely(m_dimension_2d_array >= m_requests_2d.size())) {
        Throw("Tried to retrieve a size-%i 2D sample array,"
              " which was not previously allocated.", size);
    }
    Assert(m_requests_2d[m_dimension_2d_array] == size);
    return m_samples_2d[m_dimension_2d_array++] + m_sample_index * size;
}

FloatP *Sampler::next_1d_array_p(size_t size) {
    Assert(m_sample_index < m_sample_count);
    if (unlikely(m_dimension_1d_array_p >= m_requests_1d_p.size())) {
        Throw("Tried to retrieve a size-%i 1D sample array (packet),"
              " which was not previously allocated.", size);
    }
    Assert(m_requests_1d_p[m_dimension_1d_array_p] == size);
    return m_samples_1d_p[m_dimension_1d_array_p++] + m_sample_index * size;
}

Point2fP *Sampler::next_2d_array_p(size_t size) {
    Assert(m_sample_index < m_sample_count);
    if (unlikely(m_dimension_2d_array_p >= m_requests_2d_p.size())) {
        Throw("Tried to retrieve a size-%i 2D sample array (packet),"
              " which was not previously allocated.", size);
    }
    Assert(m_requests_2d_p[m_dimension_2d_array_p] == size);
    return m_samples_2d_p[m_dimension_2d_array_p++] + m_sample_index * size;
}

Sampler::~Sampler() {
    for (size_t i = 0; i < m_samples_1d.size(); i++) {
        if (m_samples_1d[i])
            delete[] m_samples_1d[i];
    }
    for (size_t i = 0; i < m_samples_2d.size(); i++) {
        if (m_samples_2d[i])
            delete[] m_samples_2d[i];
    }
}

MTS_IMPLEMENT_CLASS(Sampler, Object);
NAMESPACE_END(mitsuba)
