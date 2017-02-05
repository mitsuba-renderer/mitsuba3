#include <mitsuba/core/string.h>
#include <mitsuba/core/astream.h>
#include <mitsuba/core/util.h>

NAMESPACE_BEGIN(mitsuba)

namespace {
/// Sentry used to determine whether a stream is indeed a compatible AnnotatedStream
static const std::string kSerializedHeaderId = "SER_V1";
static constexpr auto kSerializedHeaderIdLength = 6;
/** Elements of the header =
 *    Protocol version string (= string length number followed by chars)
 *    + total header size in bytes
 *    + number of elements
 */
static constexpr auto kSerializedHeaderSize =
    sizeof(uint32_t) + kSerializedHeaderIdLength + sizeof(uint64_t) + sizeof(uint32_t);
}  // end anonymous namespace

AnnotatedStream::AnnotatedStream(Stream *stream, bool write_mode, bool throw_on_missing)
    : Object(), m_stream(stream), m_write_mode(write_mode)
    , m_throw_on_missing(throw_on_missing), m_is_closed(false) {
    if (!m_write_mode && !m_stream->can_read())
        Throw("Attempted to create a read-only AnnotatedStream from"
              " a stream without read capabilities: %s",
              m_stream->to_string());

    if (m_write_mode && !m_stream->can_write())
        Throw("Attempted to create a write-only AnnotatedStream from"
              " a stream without write capabilities: %s",
              m_stream->to_string());

    m_prefixStack.push_back("");

    if (m_stream->can_read() && m_stream->size() > 0)
        read_toc();

    // Even if the file was initially empty, we need to start any write
    // with an offset, so as to leave space for the header to be written on close.
    m_stream->seek(kSerializedHeaderSize);
}

AnnotatedStream::~AnnotatedStream() {
    close();
}

void AnnotatedStream::close() {
    if (m_stream->is_closed())
        return;

    if (can_write())
        write_toc();
    m_is_closed = true;
}

std::vector<std::string> AnnotatedStream::keys() const {
    const std::string &prefix = m_prefixStack.back();
    std::vector<std::string> result;
    for (auto const &kv : m_table) {
        if (kv.first.substr(0, prefix.length()) == prefix)
            result.push_back(kv.first.substr(prefix.length()));
    }
    return result;
}

void AnnotatedStream::push(const std::string &name) {
    m_prefixStack.push_back(m_prefixStack.back() + name + ".");
}

void AnnotatedStream::pop() {
    m_prefixStack.pop_back();
}

bool AnnotatedStream::get_base(const std::string &name, const std::string &type_id) {
    if (!can_read())
        Throw("Attempted to read from write-only stream: %s",
              m_stream->to_string());
    if (m_is_closed)
        Throw("Attempted to read from a closed annotated stream: %s",
              to_string());

    std::string full_name = m_prefixStack.back() + name;
    auto it = m_table.find(full_name);
    if (it == m_table.end()) {
        const auto logLevel = (m_throw_on_missing ? EError : EWarn);
        Log(logLevel, "Unable to find field named \"%s\". Available fields: %s",
                      full_name, keys());

        return false;
    }

    const auto &record = it->second;
    if (record.first != type_id)
        Throw("Field named \"%s\" has incompatible type: expected %s, found %s",
              full_name, type_id, record.first);

    m_stream->seek(static_cast<size_t>(record.second));
    return true;
}

void AnnotatedStream::setBase(const std::string &name, const std::string &type_id) {
    if (!can_write())
        Throw("Attempted to write into read-only stream: %s", m_stream->to_string());
    if (m_is_closed)
        Throw("Attempted to write to a closed annotated stream: %s", to_string());

    std::string full_name = m_prefixStack.back() + name;
    auto it = m_table.find(full_name);
    if (it != m_table.end())
        Throw("Field named \"%s\" was already set!", full_name);

    const auto pos = static_cast<uint64_t>(m_stream->tell());
    m_table[full_name] = std::make_pair(type_id, pos);
}

void AnnotatedStream::read_toc() {
    uint64_t trailer_offset;
    uint32_t item_count;
    std::string header;

    // Check that sentry is present at the beginning of the stream
    m_stream->seek(0);
    try {
        m_stream->read(header);
    } catch (const std::runtime_error &e) {
        Throw("Error trying to read the table of contents: %s", e.what());
    }
    if (header != kSerializedHeaderId)
        Throw("Error trying to read the table of contents, header mismatch"
              " (expected %s, found %s). Underlying stream: %s",
              kSerializedHeaderId, header, m_stream->to_string());

    m_stream->read(trailer_offset);
    m_stream->read(item_count);

    // Read the table of contents (located at offset 'trailer_offset')
    m_stream->seek(static_cast<size_t>(trailer_offset));
    for (uint32_t i = 0; i < item_count; ++i) {
        std::string field_name, type_id;
        uint64_t offset;
        m_stream->read(field_name);
        m_stream->read(type_id);
        m_stream->read(offset);

        m_table[field_name] = std::make_pair(type_id, offset);
    }
}

void AnnotatedStream::write_toc() {
    auto trailer_offset = uint64_t(m_stream->tell());
    auto item_count = uint32_t(m_table.size());

    // Write sentry at the very beginning of the stream
    m_stream->seek(0);
    m_stream->write(kSerializedHeaderId);
    m_stream->write(trailer_offset);
    m_stream->write(item_count);
    m_stream->flush();

    // Write table of contents at the end of the stream
    m_stream->seek(static_cast<size_t>(trailer_offset));
    for (const auto &item : m_table) {
        // For each field, write: field name, field type id, corresponding offset
        m_stream->write(item.first);
        m_stream->write(item.second.first);
        m_stream->write(item.second.second);
    }
    m_stream->flush();
}

MTS_IMPLEMENT_CLASS(AnnotatedStream, Object)

NAMESPACE_END(mitsuba)

