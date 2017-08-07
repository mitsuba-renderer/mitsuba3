#include <mitsuba/core/appender.h>
#include <mitsuba/core/logger.h>
#include <fstream>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

StreamAppender::StreamAppender(std::ostream *stream)
 : m_stream(stream), m_is_file(false) {
    m_last_message_was_progress = false;
}

StreamAppender::StreamAppender(const std::string &filename)
 : m_fileName(filename), m_is_file(true) {
    std::fstream *stream = new std::fstream();
    stream->open(filename.c_str(),
        std::fstream::in | std::fstream::out | std::fstream::trunc);
    m_stream = stream;
    m_last_message_was_progress = false;
}

std::string StreamAppender::read_log() {
    std::string result;

    Assert(m_is_file);
    std::fstream &stream = * ((std::fstream *) m_stream);
    if (!stream.good())
        return result;

    stream.flush();
    stream.seekg(0, std::ios::end);
    std::streamoff size = stream.tellg();
    if (stream.fail() || size == 0)
        return result;
    result.resize((size_t) size);
    stream.seekg(0, std::ios::beg);

    std::istreambuf_iterator<std::string::value_type> it(stream);
    std::istreambuf_iterator<std::string::value_type> it_eof;
    result.insert(result.begin(), it, it_eof);
    stream.seekg(0, std::ios::end);
    Assert(!stream.fail());
    return result;
}

void StreamAppender::append(ELogLevel level, const std::string &text) {
    /* Insert a newline if the last message was a progress message */
    if (m_last_message_was_progress && !m_is_file)
        (*m_stream) << std::endl;
#if !defined(__WINDOWS__)
    if (level == EWarn || level == EError)
        (*m_stream) << "\x1b[31m";
    else if (level == EDebug)
        (*m_stream) << "\x1b[38;5;245m";
#endif
    (*m_stream) << text << std::endl;
#if !defined(__WINDOWS)
    (*m_stream) << "\x1b[0m";
#endif
    m_last_message_was_progress = false;
}

void StreamAppender::log_progress(Float, const std::string &,
    const std::string &formatted, const std::string &, const void *) {
    if (!m_is_file) {
        (*m_stream) << formatted;
        m_stream->flush();
    }
    m_last_message_was_progress = true;
}

std::string StreamAppender::to_string() const {
    std::ostringstream oss;

    oss << "StreamAppender[stream=";

    if (m_is_file)
        oss << "\"" << m_fileName << "\"";
    else
        oss << "<std::ostream>";

    oss << "]";

    return oss.str();
}

StreamAppender::~StreamAppender() {
    if (m_is_file) {
        ((std::fstream *) m_stream)->close();
        delete m_stream;
    }
}

MTS_IMPLEMENT_CLASS(Appender, Object)
MTS_IMPLEMENT_CLASS(StreamAppender, Appender)

NAMESPACE_END(mitsuba)
