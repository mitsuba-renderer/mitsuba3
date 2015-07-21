#include <mitsuba/core/path.h>
#if 0

#include "fwd.h"
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#if defined(_WIN32)
# include <windows.h>
#else
# include <unistd.h>
#endif
#include <sys/stat.h>

#if defined(__linux)
# include <linux/limits.h>
#endif
#endif

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(filesystem)

void path::set(const std::string &str, platform type) {
    m_platform = type;
    if (type == windows) {
        m_path = tokenize(str, "/\\");
        m_absolute = (str.size() >= 1 && str[0] == '\\') ||
                     (str.size() >= 2 && (std::isalpha(str[0]) && str[1] == ':'));
    } else {
        m_path = tokenize(str, "/");
        m_absolute = !str.empty() && str[0] == '/';
    }
}

std::vector<std::string> path::tokenize(const std::string &string, const std::string &delim) {
    std::string::size_type lastPos = 0, pos = string.find_first_of(delim, lastPos);
    std::vector<std::string> tokens;

    while (lastPos != std::string::npos) {
        if (pos != lastPos)
            tokens.push_back(string.substr(lastPos, pos - lastPos));
        lastPos = pos;
        if (lastPos == std::string::npos || lastPos + 1 == string.length())
            break;
        pos = string.find_first_of(delim, ++lastPos);
    }

    return tokens;
}


#if 0
std::string path::filename() const {
    if (empty())
        return "";
    const std::string &last = m_path[m_path.size()-1];
    return last;
}

std::string path::extension() const {
    const std::string &name = filename();
    size_t pos = name.find_last_of(".");
    if (pos == std::string::npos)
        return "";
    return name.substr(pos+1);
}

path path::parent_path() const {
    path result;
    result.m_absolute = m_absolute;

    if (m_path.empty()) {
        if (!m_absolute)
            result.m_path.push_back("..");
    } else {
        size_t until = m_path.size() - 1;
        for (size_t i = 0; i < until; ++i)
            result.m_path.push_back(m_path[i]);
    }
    return result;
}

path path::operator/(const path &other) const {
    if (other.m_absolute)
        throw std::runtime_error("path::operator/(): expected a relative path!");
    if (m_platform != other.m_platform)
        throw std::runtime_error("path::operator/(): expected a path of the same type!");

    path result(*this);

    for (size_t i=0; i<other.m_path.size(); ++i)
        result.m_path.push_back(other.m_path[i]);

    return result;
}

std::string path::str(platform type = native) const {
    std::ostringstream oss;

    if (m_platform == posix && m_absolute)
        oss << "/";

    for (size_t i=0; i<m_path.size(); ++i) {
        oss << m_path[i];
        if (i+1 < m_path.size()) {
            if (type == posix)
                oss << '/';
            else
                oss << '\\';
        }
    }

    return oss.str();
}

void path::set(const std::wstring &wstring, platform type) {
    std::string string;
    if (!wstring.empty()) {
        int size = WideCharToMultiByte(CP_UTF8, 0, &wstring[0], (int)wstring.size(),
                        NULL, 0, NULL, NULL);
        string.resize(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstring[0], (int)wstring.size(),
                            &string[0], size, NULL, NULL);
    }
    set(string, type);
}

std::wstring path::wstr(platform type) const {
    std::string temp = str(type);
    int size = MultiByteToWideChar(CP_UTF8, 0, &temp[0], (int)temp.size(), NULL, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, &temp[0], (int)temp.size(), &result[0], size);
    return result;
}

path absolute(const path &p) {
#if !defined(_WIN32)
    char temp[PATH_MAX];
    if (realpath(p.str().c_str(), temp) == NULL)
        throw std::runtime_error("Internal error in realpath(): " + std::string(strerror(errno)));
    return path(temp);
#else
    std::wstring value = p.wstr(), out(MAX_PATH, '\0');
    DWORD length = GetFullPathNameW(value.c_str(), MAX_PATH, &out[0], NULL);
    if (length == 0)
        throw std::runtime_error("Internal error in realpath(): " + std::to_string(GetLastError()));
    return path(out.substr(0, length));
#endif
}

bool exists(const path &p) const {
#if defined(_WIN32)
    return GetFileAttributesW(p.wstr().c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat sb;
    return stat(p.str().c_str(), &sb) == 0;
#endif
}

size_t file_size(const path &p) const {
#if defined(_WIN32)
    struct _stati64 sb;
    if (_wstati64(p.wstr().c_str(), &sb) != 0)
        throw std::runtime_error("path::file_size(): cannot stat file \"" + str() + "\"!");
#else
    struct stat sb;
    if (stat(p.str().c_str(), &sb) != 0)
        throw std::runtime_error("path::file_size(): cannot stat file \"" + str() + "\"!");
#endif
    return (size_t) sb.st_size;
}

bool create_directory(const path& p) {
#if defined(_WIN32)
    return CreateDirectoryW(p.wstr().c_str(), NULL) != 0;
#else
    return mkdir(p.str().c_str(), S_IRUSR | S_IWUSR | S_IXUSR) == 0;
#endif
}

path current_directory() {
#if !defined(_WIN32)
    char temp[PATH_MAX];
    if (::getcwd(temp, PATH_MAX) == NULL)
        throw std::runtime_error("Internal error in getcwd(): " + std::string(strerror(errno)));
    return path(temp);
#else
    std::wstring temp(MAX_PATH, '\0');
    if (!_wgetcwd(&temp[0], MAX_PATH))
        throw std::runtime_error("Internal error in getcwd(): " + std::to_string(GetLastError()));
    return path(temp.c_str());
#endif
}

bool resize_file(const path &p, size_t target_length) {
#if !defined(_WIN32)
    return ::truncate(p.str().c_str(), (off_t) target_length) == 0;
#else
    HANDLE handle = CreateFileW(p.wstr().c_str(), GENERIC_WRITE, 0, nullptr, 0,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER size;
    size.QuadPart = (LONGLONG) target_length;
    if (SetFilePointerEx(handle, size, NULL, FILE_BEGIN) == 0) {
        CloseHandle(handle);
        return false;
    }
    if (SetEndOfFile(handle) == 0) {
        CloseHandle(handle);
        return false;
    }
    CloseHandle(handle);
    return true;
#endif
}

bool remove(const path &p) {
#if !defined(_WIN32)
    return std::remove(p.str().c_str()) == 0;
#else
    return DeleteFileW(p.wstr().c_str()) != 0;
#endif
}

bool is_regular_file(const path &p) const {
#if defined(_WIN32)
    DWORD attr = GetFileAttributesW(p.wstr().c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
#else
    struct stat sb;
    if (stat(p.str().c_str(), &sb))
        return false;
    return S_ISREG(sb.st_mode);
#endif
}

bool is_directory(const path &p) const {
#if defined(_WIN32)
    DWORD result = GetFileAttributesW(p.wstr().c_str());
    if (result == INVALID_FILE_ATTRIBUTES)
        return false;
    return (result & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat sb;
    if (stat(p.str().c_str(), &sb))
        return false;
    return S_ISDIR(sb.st_mode);
#endif
}
#endif

NAMESPACE_END(filesystem)
NAMESPACE_END(mitsuba)
