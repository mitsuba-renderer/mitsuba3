#include <mitsuba/render/volumegrid.h>
#include <mitsuba/core/stream.h>
#include <mitsuba/core/logger.h>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/util.h>


NAMESPACE_BEGIN(mitsuba)

VolumeGrid::VolumeGrid(Stream *stream) {
    read(stream);
}

VolumeGrid::VolumeGrid(const fs::path &filename) {
    ref<FileStream> fs = new FileStream(filename);
    read(fs);
}

VolumeGrid::VolumeGrid(ScalarVector3i size, ScalarUInt32 channel_count)
    : m_size(size), m_channel_count(channel_count),
      m_bbox(BoundingBox3f(Point3f(0.f), Point3f(1.f))) {
    m_data = std::unique_ptr<ScalarFloat[]>(
        new ScalarFloat[ek::hprod(m_size) * m_channel_count]);
}

void VolumeGrid::read(Stream *stream) {
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
    m_size.x() = size_x;
    m_size.y() = size_y;
    m_size.z() = size_z;

    size_t size    = ek::hprod(m_size);
    if (size < 8)
        Throw("Invalid grid dimensions: %d x %d x %d < 8 (must have at "
              "least one value at each corner)",
              m_size.x(), m_size.y(), m_size.z());

    int32_t channel_count;
    stream->read(channel_count);
    m_channel_count = channel_count;

    // Transform specified in the volume file
    float dims[6];
    stream->read_array(dims, 6);
    m_bbox = ScalarBoundingBox3f(ScalarPoint3f(dims[0], dims[1], dims[2]),
                                 ScalarPoint3f(dims[3], dims[4], dims[5]));

    m_max = -ek::Infinity<ScalarFloat>;

    m_data = std::unique_ptr<ScalarFloat[]>(new ScalarFloat[size * m_channel_count]);
    size_t k = 0;
    for (size_t i = 0; i < size; ++i) {
        for (size_t j = 0; j < m_channel_count; ++j) {
            // auto val    = detail::read<float>(f);
            float val;

            // auto val    = detail::read<float>(f);
            stream->read(val);
            m_data[k] = val;
            m_max     = ek::max(m_max, val);
            ++k;
        }
    }
    Log(Debug, "Loaded grid volume data from file: dimensions %s, max value %f",
        m_size, m_max);
}

void VolumeGrid::write(const fs::path &path) const {
    ref<FileStream> fs = new FileStream(path, FileStream::ETruncReadWrite);
    write(fs);
}

void VolumeGrid::write(Stream *stream) const {
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
    stream->write_array(m_data.get(), ek::hprod(m_size) * m_channel_count);
}

std::string VolumeGrid::to_string() const {
    std::ostringstream oss;
    oss << "VolumeGrid[" << std::endl
        << "  size = " << m_size << "," << std::endl
        << "  channels = " << m_channel_count << "," << std::endl
        << "  max = " << m_max << "," << std::endl
        << "  data = [ " << util::mem_string(buffer_size())
        << " of volume data ]" << std::endl
        << "]";
    return oss.str();
}

MTS_IMPLEMENT_CLASS(VolumeGrid, Object)

NAMESPACE_END(mitsuba)
