#include <mitsuba/core/string.h>
#include <mitsuba/core/astream.h>
#include <mitsuba/core/util.h>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN()
/// Sentry used to determine whether a stream is indeed a compatible AnnotatedStream
static const std::string kSerializedHeaderId = "SER_V1";
static constexpr auto kSerializedHeaderIdLength = 6;
/// Protocol id string + total header size in bytes + number of elements
static constexpr auto kSerializedHeaderSize =
    kSerializedHeaderIdLength + sizeof(uint64_t) + sizeof(uint32_t);
NAMESPACE_END()

AnnotatedStream::AnnotatedStream(ref<Stream> &stream, bool throwOnMissing)
    : Object(), m_stream(stream), m_throwOnMissing(throwOnMissing) {

    m_prefixStack.push_back("");

    if (m_stream->canRead() && m_stream->getSize() > 0) {
        readTOC();
    }
    m_stream->seek(kSerializedHeaderSize);
}

AnnotatedStream::~AnnotatedStream() {
    if (canWrite())
        writeTOC();
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

bool AnnotatedStream::getBase(const std::string &name, const std::string &type_id) {
    if (!canRead()) {
        Log(EError, "Attempted to read from write-only stream: %s",
            m_stream->toString());
    }

    std::string fullName = m_prefixStack.back() + name;
    auto it = m_table.find(fullName);
    if (it == m_table.end()) {
        const auto logLevel = (m_throwOnMissing ? EError : EWarn);
        Log(logLevel, "Unable to find field named \"%s\". Available fields: %s",
                      fullName, keys());

        return false;
    }

    const auto &record = it->second;
    if (record.first != type_id) {
        Log(EError, "Field named \"%s\" has incompatible type: expected %s, found %s",
            fullName, type_id, record.first);
    }

    m_stream->seek(static_cast<size_t>(record.second));
    return true;
}

void AnnotatedStream::setBase(const std::string &name, const std::string &type_id) {
    if (!canWrite()) {
        Log(EError, "Attempted to write into read-only stream: %s",
            m_stream->toString());
    }

    std::string fullName = m_prefixStack.back() + name;
    auto it = m_table.find(fullName);
    if (it != m_table.end()) {
        Log(EError, "Field named \"%s\" was already set!", fullName);
    }

    const auto pos = static_cast<uint64_t>(m_stream->getPos());
    m_table[fullName] = std::make_pair(type_id, pos);
}

void AnnotatedStream::readTOC() {
    uint64_t trailerOffset;
    uint32_t nItems;
    std::string header;

    // Check that sentry is present at the beginning of the stream
    m_stream->seek(0);
    m_stream->readValue(header);
    if (header != kSerializedHeaderId) {
        Log(EError, "Error trying to read the table of contents, header mismatch"
                    " (expected %s, found %s). Underlying stream: %s",
            kSerializedHeaderId, header, m_stream->toString());
    }
    m_stream->readValue(trailerOffset);
    m_stream->readValue(nItems);

    // Read the table of contents (tucked at <tt>trailerOffset</tt>)
    m_stream->seek(trailerOffset);
    for (uint32_t i = 0; i < nItems; ++i) {
        std::string field_name, type_id;
        uint64_t offset;
        // TODO: make sure that field name, etc are written directly as values
        m_stream->readValue(field_name);
        m_stream->readValue(type_id);
        m_stream->readValue(offset);

        m_table[field_name] = std::make_pair(type_id, offset);
    }
}

void AnnotatedStream::writeTOC() {
    const auto trailer_offset = static_cast<uint64_t>(m_stream->getPos());
    const auto nItems = static_cast<uint32_t>(m_table.size());

    // Write sentry at the very beginning of the stream
    m_stream->seek(0);
    m_stream->writeValue(kSerializedHeaderId);
    m_stream->writeValue(trailer_offset);
    m_stream->writeValue(nItems);
    m_stream->flush();
    // Write table of contents at the end of the stream
    m_stream->seek(static_cast<size_t>(trailer_offset));
    for (const auto &item : m_table) {
        // For each field, write: field name, field type id, corresponding offset
        m_stream->writeValue(item.first);
        m_stream->writeValue(item.second.first);
        m_stream->writeValue(item.second.second);
    }
    m_stream->flush();
}

MTS_IMPLEMENT_CLASS(AnnotatedStream, Object)

NAMESPACE_END(mitsuba)

