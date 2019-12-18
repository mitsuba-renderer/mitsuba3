#include <mitsuba/core/appender.h>
#include <mitsuba/core/logger.h>
#include <fstream>
#include <sstream>

#if defined(__WINDOWS__)
#  include <windows.h>
#endif

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

void StreamAppender::append(LogLevel level, const std::string &text) {
#if defined(__WINDOWS__)
    HANDLE console = nullptr;
    CONSOLE_SCREEN_BUFFER_INFO console_info;
    memset(&console_info, 0, sizeof(CONSOLE_SCREEN_BUFFER_INFO));
#endif
    if (!m_is_file) {
        /* Insert a newline if the last message was a progress message */
        if (m_last_message_was_progress)
            (*m_stream) << std::endl;
        if (level == Debug || level == Warn || level == Error) {
#if defined(__WINDOWS__)
            console = GetStdHandle(STD_OUTPUT_HANDLE);
            GetConsoleScreenBufferInfo(console, &console_info);
            if (level == Warn || level == Error)
                SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_INTENSITY);
            else if (level == Debug)
                SetConsoleTextAttribute(console, FOREGROUND_INTENSITY);
#else
            if (level == Warn || level == Error)
                (*m_stream) << "\x1b[31m";
            else
                (*m_stream) << "\x1b[38;5;245m";
#endif
        }
    }
    (*m_stream) << text << std::endl;
    if (!m_is_file && (level == Debug || level == Warn || level == Error)) {
        // Reset text color
#if defined(__WINDOWS__)
        SetConsoleTextAttribute(console, console_info.wAttributes);
#else
        (*m_stream) << "\x1b[0m";
#endif
    }
    m_last_message_was_progress = false;
}

void StreamAppender::log_progress(float, const std::string &,
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
