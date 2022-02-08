#include <mitsuba/core/zstream.h>
#include <zlib.h>

NAMESPACE_BEGIN(mitsuba)

ZStream::ZStream(Stream *child_stream, EStreamType stream_type, int level)
    : m_child_stream(child_stream),
      m_deflate_stream(new z_stream()),
      m_inflate_stream(new z_stream()),
      m_did_write(false) {
    m_deflate_stream->zalloc = Z_NULL;
    m_deflate_stream->zfree = Z_NULL;
    m_deflate_stream->opaque = Z_NULL;

    int window_bits = 15 + (stream_type == EGZipStream ? 16 : 0);

    int retval = deflateInit2(m_deflate_stream.get(), level,
        Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY);

    if (retval != Z_OK)
        Throw("Could not initialize ZLIB: error code %i", retval);

    m_inflate_stream->zalloc = Z_NULL;
    m_inflate_stream->zfree = Z_NULL;
    m_inflate_stream->opaque = Z_NULL;
    m_inflate_stream->avail_in = 0;
    m_inflate_stream->next_in = Z_NULL;

    retval = inflateInit2(m_inflate_stream.get(), window_bits);
    if (retval != Z_OK)
        Throw("Could not initialize ZLIB: error code %i", retval);
}

void ZStream::write(const void *ptr, size_t size) {
    Assert(m_child_stream != nullptr);

    m_deflate_stream->avail_in = (uInt) size;
    m_deflate_stream->next_in = (uint8_t *) ptr;

    int output_size = 0;

    do {
        m_deflate_stream->avail_out = sizeof(m_deflate_buffer);
        m_deflate_stream->next_out = m_deflate_buffer;

        int retval = deflate(m_deflate_stream.get(), Z_NO_FLUSH);
        if (retval == Z_STREAM_ERROR)
            Throw("deflate(): stream error!");

        output_size = sizeof(m_deflate_buffer) - m_deflate_stream->avail_out;

        m_child_stream->write(m_deflate_buffer, output_size);
    } while (output_size != 0);

    Assert(m_deflate_stream->avail_in == 0);
    m_did_write = true;
}

void ZStream::read(void *ptr, size_t size) {
    Assert(m_child_stream != nullptr);

    uint8_t *targetPtr = (uint8_t *) ptr;
    while (size > 0) {
        if (m_inflate_stream->avail_in == 0) {
            size_t remaining = m_child_stream->size() - m_child_stream->tell();
            m_inflate_stream->next_in = m_inflate_buffer;
            m_inflate_stream->avail_in = (uInt) std::min(remaining, sizeof(m_inflate_buffer));
            if (m_inflate_stream->avail_in == 0)
                Throw("Read less data than expected (%i more bytes required)", size);
            m_child_stream->read(m_inflate_buffer, m_inflate_stream->avail_in);
        }

        m_inflate_stream->avail_out = (uInt) size;
        m_inflate_stream->next_out = targetPtr;

        int retval = inflate(m_inflate_stream.get(), Z_NO_FLUSH);
        switch (retval) {
            case Z_STREAM_ERROR:
                Throw("inflate(): stream error!");
                break;
            case Z_NEED_DICT:
                Throw("inflate(): need dictionary!");
                break;
            case Z_DATA_ERROR:
                Throw("inflate(): data error!");
                break;
            case Z_MEM_ERROR:
                Throw("inflate(): memory error!");
                break;
        };

        size_t output_size = size - (size_t) m_inflate_stream->avail_out;
        targetPtr += output_size;
        size -= output_size;

        if (size > 0 && retval == Z_STREAM_END)
            Throw("inflate(): attempting to read past the end of the stream!");
    }
}

void ZStream::flush() {
    Assert(m_child_stream != nullptr);

    if (m_did_write) {
        m_deflate_stream->avail_in = 0;
        m_deflate_stream->next_in = NULL;
        int output_size = 0;

        do {
            m_deflate_stream->avail_out = sizeof(m_deflate_buffer);
            m_deflate_stream->next_out = m_deflate_buffer;

            int retval = deflate(m_deflate_stream.get(), Z_FULL_FLUSH);
            if (retval == Z_STREAM_ERROR)
                Throw("deflate(): stream error!");

            output_size = sizeof(m_deflate_buffer) - m_deflate_stream->avail_out;

            m_child_stream->write(m_deflate_buffer, output_size);
        } while (output_size != 0);

        m_child_stream->flush();
    }
}

void ZStream::close() {
    if (!m_child_stream)
        return;

    if (m_did_write) {
        m_deflate_stream->avail_in = 0;
        m_deflate_stream->next_in = NULL;
        int output_size = 0;

        do {
            m_deflate_stream->avail_out = sizeof(m_deflate_buffer);
            m_deflate_stream->next_out = m_deflate_buffer;

            int retval = deflate(m_deflate_stream.get(), Z_FINISH);
            if (retval == Z_STREAM_ERROR)
                Throw("deflate(): stream error!");

            output_size = sizeof(m_deflate_buffer) - m_deflate_stream->avail_out;

            m_child_stream->write(m_deflate_buffer, output_size);
        } while (output_size != 0);
    }

    deflateEnd(m_deflate_stream.get());
    inflateEnd(m_inflate_stream.get());

    m_child_stream = nullptr;
}

ZStream::~ZStream() {
    close();
}

std::string ZStream::to_string() const {
    std::ostringstream oss;

    oss << class_()->name() << "[" << std::endl;
    if (is_closed()) {
        oss << "  closed" << std::endl;
    } else {
        oss << "  child_stream = \"" << string::indent(m_child_stream) << "\"" << "," << std::endl
            << "  host_byte_order = " << host_byte_order() << "," << std::endl
            << "  byte_order = " << byte_order() << "," << std::endl
            << "  can_read = " << can_read() << "," << std::endl
            << "  can_write = " << can_write() << "," << std::endl
            << "  pos = " << tell() << "," << std::endl
            << "  size = " << size() << std::endl;
    }

    oss << "]";

    return oss.str();
}

MTS_IMPLEMENT_CLASS(ZStream, Stream)

NAMESPACE_END(mitsuba)
