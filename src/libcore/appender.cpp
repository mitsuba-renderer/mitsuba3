#include <mitsuba/core/appender.h>
#include <mitsuba/core/logger.h>
#include <fstream>
#include <sstream>

NAMESPACE_BEGIN(mitsuba)

StreamAppender::StreamAppender(std::ostream *stream)
 : m_stream(stream), m_isFile(false) {
    m_lastMessageWasProgress = false;
}

StreamAppender::StreamAppender(const std::string &filename)
 : m_fileName(filename), m_isFile(true) {
    std::fstream *stream = new std::fstream();
    stream->open(filename.c_str(),
        std::fstream::in | std::fstream::out | std::fstream::trunc);
    m_stream = stream;
    m_lastMessageWasProgress = false;
}

void StreamAppender::readLog(std::string &target) {
    Assert(m_isFile);
    std::fstream &stream = * ((std::fstream *) m_stream);
    if (!stream.good()) {
        target = "";
        return;
    }
    stream.flush();
    stream.seekg(0, std::ios::end);
    std::streamoff size = stream.tellg();
    if (stream.fail() || size == 0) {
        target = "";
        return;
    }
    target.resize((size_t) size);
    stream.seekg(0, std::ios::beg);

    std::istreambuf_iterator<std::string::value_type> it(stream);
    std::istreambuf_iterator<std::string::value_type> it_eof;
    target.insert(target.begin(), it, it_eof);

    stream.seekg(0, std::ios::end);
    Assert(!stream.fail());
}

void StreamAppender::append(ELogLevel, const std::string &text) {
    /* Insert a newline if the last message was a progress message */
    if (m_lastMessageWasProgress && !m_isFile)
        (*m_stream) << std::endl;
    (*m_stream) << text << std::endl;
    m_lastMessageWasProgress = false;
}

void StreamAppender::logProgress(Float, const std::string &,
    const std::string &formatted, const std::string &, const void *) {
    if (!m_isFile) {
        (*m_stream) << formatted;
        m_stream->flush();
    }
    m_lastMessageWasProgress = true;
}

std::string StreamAppender::toString() const {
    std::ostringstream oss;

    oss << "StreamAppender[stream=";

    if (m_isFile)
        oss << "\"" << m_fileName << "\"";
    else
        oss << "<std::ostream>";

    oss << "]";

    return oss.str();
}

StreamAppender::~StreamAppender() {
    if (m_isFile) {
        ((std::fstream *) m_stream)->close();
        delete m_stream;
    }
}

MTS_IMPLEMENT_CLASS(Appender, Object)
MTS_IMPLEMENT_CLASS(StreamAppender, Appender)

NAMESPACE_END(mitsuba)
