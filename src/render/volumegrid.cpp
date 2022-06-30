#include <mitsuba/render/volumegrid.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/util.h>

NAMESPACE_BEGIN(mitsuba)

MI_VARIANT
VolumeGrid<Float, Spectrum>::VolumeGrid(Stream *stream) { read(stream); }

MI_VARIANT
VolumeGrid<Float, Spectrum>::VolumeGrid(const fs::path &filename) {
    ref<FileStream> fs = new FileStream(filename);
    read(fs);
}

MI_VARIANT
VolumeGrid<Float, Spectrum>::VolumeGrid(ScalarVector3u size,
                                        ScalarUInt32 channel_count)
    : m_size(size), m_channel_count(channel_count),
      m_bbox(ScalarBoundingBox3f(ScalarPoint3f(0.f), ScalarPoint3f(1.f))),
      m_max_per_channel(channel_count, 0.f) {
    m_data = std::unique_ptr<ScalarFloat[]>(
        new ScalarFloat[dr::prod(m_size) * m_channel_count]);
}

MI_VARIANT
void VolumeGrid<Float, Spectrum>::read(Stream *stream) {
    char header[3];
    stream->read(header, 3);

    if (header[0] != 'V' || header[1] != 'O' || header[2] != 'L')
        Throw("Invalid volume file!");
    uint8_t version;
    stream->read(version);

    if (version != 3)
        Throw("Invalid version, currently only version 3 is supported (found %d)", version);

    int32_t data_type;
    stream->read(data_type);
    if (data_type != 1)
        Throw("Wrong type, currently only type == 1 (Float32) data is "
              "supported (found type = %d)", data_type);

    int32_t size_x, size_y, size_z;
    stream->read(size_x);
    stream->read(size_y);
    stream->read(size_z);
    m_size.x() = uint32_t(size_x);
    m_size.y() = uint32_t(size_y);
    m_size.z() = uint32_t(size_z);

    size_t size = dr::prod(m_size);
    int32_t channel_count;
    stream->read(channel_count);
    m_channel_count = channel_count;

    // Transform specified in the volume file
    float dims[6];
    stream->read_array(dims, 6);
    m_bbox = ScalarBoundingBox3f(ScalarPoint3f(dims[0], dims[1], dims[2]),
                                 ScalarPoint3f(dims[3], dims[4], dims[5]));

    m_max = -dr::Infinity<ScalarFloat>;
    m_max_per_channel.resize(m_channel_count, -dr::Infinity<ScalarFloat>);

    m_data = std::unique_ptr<ScalarFloat[]>(new ScalarFloat[size * m_channel_count]);
    size_t k = 0;
    for (size_t i = 0; i < size; ++i) {
        for (size_t j = 0; j < m_channel_count; ++j) {
            float val;
            stream->read(val);
            m_data[k] = val;
            m_max     = dr::maximum(m_max, val);
            m_max_per_channel[j] = dr::maximum(m_max_per_channel[j], val);
            ++k;
        }
    }
    Log(Debug, "Loaded grid volume data from file: dimensions %s, max value %f",
        m_size, m_max);
}

MI_VARIANT
void VolumeGrid<Float, Spectrum>::max_per_channel(ScalarFloat *out) const {
    for (size_t i=0; i<m_channel_count; ++i)
        out[i] = m_max_per_channel[i];
}

MI_VARIANT
void VolumeGrid<Float, Spectrum>::write(const fs::path &path) const {
    ref<FileStream> fs = new FileStream(path, FileStream::ETruncReadWrite);
    write(fs);
}

MI_VARIANT
void VolumeGrid<Float, Spectrum>::write(Stream *stream) const {
    stream->write("VOL", 3);
    stream->write(uint8_t(3)); // file format version
    stream->write(int32_t(1)); // data_type
    stream->write(int32_t(m_size.x()));
    stream->write(int32_t(m_size.y()));
    stream->write(int32_t(m_size.z()));
    stream->write(int32_t(m_channel_count));

    stream->write(float(m_bbox.min.x()));
    stream->write(float(m_bbox.min.y()));
    stream->write(float(m_bbox.min.z()));
    stream->write(float(m_bbox.max.x()));
    stream->write(float(m_bbox.max.y()));
    stream->write(float(m_bbox.max.z()));

    if constexpr (std::is_same<ScalarFloat, float>::value)
        stream->write_array(m_data.get(), dr::prod(m_size) * m_channel_count);
    else {
        // Need to convert data to single precision before writing to disk
        std::vector<float> output(dr::prod(m_size) * m_channel_count);
        for (size_t i = 0; i < dr::prod(m_size) * m_channel_count; ++i)
            output[i] = m_data[i];
        stream->write_array(output.data(), dr::prod(m_size) * m_channel_count);
    }
}

MI_VARIANT
std::string VolumeGrid<Float, Spectrum>::to_string() const {
    std::ostringstream oss;
    oss << "VolumeGrid[" << std::endl
        << "  size = " << m_size << "," << std::endl
        << "  channels = " << m_channel_count << "," << std::endl
        << "  max = " << m_max << "," << std::endl
        << "  max_channels = [" << std::endl << "    ";
    for (uint32_t i=0; i<m_max_per_channel.size(); ++i)
        oss << m_max_per_channel[i] << ", ";
    oss << std::endl;
    oss << "  ],"  << std::endl
        << "  data = [ " << util::mem_string(buffer_size())
        << " of volume data ]" << std::endl
        << "]";
    return oss.str();
}

MI_IMPLEMENT_CLASS_VARIANT(VolumeGrid, Object)
MI_INSTANTIATE_CLASS(VolumeGrid)

NAMESPACE_END(mitsuba)
