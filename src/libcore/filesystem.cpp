#include <mitsuba/core/filesystem.h>

#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <cstring>

#if defined(__WINDOWS__)
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/stat.h>
#endif

#if defined(__LINUX__)
#  include <linux/limits.h>
#endif

/** Macro allowing to type hardocded character sequences
 * with the right type prefix (char_t: no prefix, wchar_t: 'L' prefix)
 */
#if defined(__WINDOWS__)
#  define NSTR(str) L##str
#else
#  define NSTR(str) str
#endif

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(filesystem)

path current_path() {
#if !defined(__WINDOWS__)
    char temp[PATH_MAX];
    if (::getcwd(temp, PATH_MAX) == NULL)
        throw std::runtime_error("Internal error in filesystem::current_path(): " + std::string(strerror(errno)));
    return path(temp);
#else
    std::wstring temp(MAX_PATH, '\0');
    if (!_wgetcwd(&temp[0], MAX_PATH))
        throw std::runtime_error("Internal error in filesystem::current_path(): " + std::to_string(GetLastError()));
    return path(temp.c_str());
#endif
}

path make_absolute(const path& p) {
#if !defined(__WINDOWS__)
    char temp[PATH_MAX];
    if (realpath(p.native().c_str(), temp) == NULL)
        throw std::runtime_error("Internal error in realpath(): " + std::string(strerror(errno)));
    return path(temp);
#else
    std::wstring value = p.native(), out(MAX_PATH, '\0');
    DWORD length = GetFullPathNameW(value.c_str(), MAX_PATH, &out[0], NULL);
    if (length == 0)
        throw std::runtime_error("Internal error in realpath(): " + std::to_string(GetLastError()));
    return path(out.substr(0, length));
#endif
}

bool is_regular_file(const path& p) noexcept {
#if defined(__WINDOWS__)
    DWORD attr = GetFileAttributesW(p.native().c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
#else
    struct stat sb;
    if (stat(p.native().c_str(), &sb))
        return false;
    return S_ISREG(sb.st_mode);
#endif
}

bool is_directory(const path& p) noexcept {
#if defined(__WINDOWS__)
    DWORD result = GetFileAttributesW(p.native().c_str());
    if (result == INVALID_FILE_ATTRIBUTES)
        return false;
    return (result & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat sb;
    if (stat(p.native().c_str(), &sb))
        return false;
    return S_ISDIR(sb.st_mode);
#endif
}

bool exists(const path& p) noexcept {
#if defined(__WINDOWS__)
    return GetFileAttributesW(p.native().c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat sb;
    return stat(p.native().c_str(), &sb) == 0;
#endif
}

size_t file_size(const path& p) {
#if defined(__WINDOWS__)
    struct _stati64 sb;
    if (_wstati64(p.native().c_str(), &sb) != 0)
        throw std::runtime_error("filesystem::file_size(): cannot stat file \"" + p.string() + "\"!");
#else
    struct stat sb;
    if (stat(p.native().c_str(), &sb) != 0)
        throw std::runtime_error("filesystem::file_size(): cannot stat file \"" + p.string() + "\"!");
#endif
    return (size_t) sb.st_size;
}

bool create_directory(const path& p) noexcept {
#if defined(__WINDOWS__)
    return CreateDirectoryW(p.native().c_str(), NULL) != 0;
#else
    return mkdir(p.native().c_str(), S_IRUSR | S_IWUSR | S_IXUSR) == 0;
#endif
}

bool resize_file(const path& p, size_t target_length) noexcept {
#if !defined(__WINDOWS__)
    return ::truncate(p.native().c_str(), (off_t) target_length) == 0;
#else
    HANDLE handle = CreateFileW(p.native().c_str(), GENERIC_WRITE, 0, nullptr, 0, FILE_ATTRIBUTE_NORMAL, nullptr);
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

bool remove(const path& p) {
#if !defined(__WINDOWS__)
    return std::remove(p.native().c_str()) == 0;
#else
    return DeleteFileW(p.native().c_str()) != 0;
#endif
}

// -----------------------------------------------------------------------------

string_type path::extension() const {
    if (empty() || m_path.back() == NSTR(".") || m_path.back() == NSTR(".."))
        return NSTR("");

    const string_type &name = filename();
    size_t pos = name.find_last_of(NSTR("."));
    if (pos == string_type::npos)
        return NSTR("");
    return name.substr(pos);  // Including the . character!
}

string_type path::filename() const {
    if (empty())
        return NSTR("");
    const string_type &last = m_path[m_path.size()-1];
    return last;
}

path path::parent_path() const {
    path result;
    result.m_absolute = m_absolute;

    if (m_path.empty()) {
        if (!m_absolute)
            result.m_path.push_back(NSTR(".."));
    } else {
        size_t until = m_path.size() - 1;
        for (size_t i = 0; i < until; ++i)
            result.m_path.push_back(m_path[i]);
    }
    return result;
}

// -----------------------------------------------------------------------------

std::string path::string() const {
#if !defined(__WINDOWS__)
    return str();
#else
    // Convert from wchar_t string to char_t string
    string_type wstring = str();
    std::string string;
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstring[0], (int)wstring.size(),
                                   NULL, 0, NULL, NULL);
    string.resize(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstring[0], (int)wstring.size(),
                        &string[0], size, NULL, NULL);
    return string;
#endif
}

// -----------------------------------------------------------------------------

path path::operator/(const path &other) const {
    if (other.m_absolute)
        throw std::runtime_error("path::operator/(): expected a relative path!");

    path result(*this);

    for (size_t i=0; i < other.m_path.size(); ++i)
        result.m_path.push_back(other.m_path[i]);

    return result;
}

path & path::operator=(const path &path) {
    m_path = path.m_path;
    m_absolute = path.m_absolute;
    return *this;
}

path & path::operator=(path &&path) {
    if (this != &path) {
        m_path = std::move(path.m_path);
        m_absolute = path.m_absolute;
    }
    return *this;
}

string_type path::str() const {
    std::basic_ostringstream<value_type> oss;

#if !defined(__WINDOWS__)
    if (m_absolute)
        oss << preferred_separator;
#endif

    for (size_t i = 0; i < m_path.size(); ++i) {
        oss << m_path[i];
        if (i + 1 < m_path.size()) {
            oss << preferred_separator;
        }
    }

    return oss.str();
}

void path::set(const string_type &str) {
    // TODO: is this  "/\\" correct for windows?
    // TODO: express just in terms of `preferred_path`
#if defined(__WINDOWS__)
    m_path = tokenize(str, NSTR("/\\"));
    m_absolute = str.size() >= 2 && std::isalpha(str[0]) && str[1] == NSTR(':');
#else
    m_path = tokenize(str, NSTR("/"));
    m_absolute = !str.empty() && str[0] == NSTR('/');
#endif
}

std::vector<string_type> path::tokenize(const string_type &string,
                                        const string_type &delim) {
    string_type::size_type lastPos = 0, pos = string.find_first_of(delim, lastPos);
    std::vector<string_type> tokens;

    while (lastPos != string_type::npos) {
        if (pos != lastPos)
            tokens.push_back(string.substr(lastPos, pos - lastPos));
        lastPos = pos;
        if (lastPos == string_type::npos || lastPos + 1 == string.length())
            break;
        pos = string.find_first_of(delim, ++lastPos);
    }

    return tokens;
}

NAMESPACE_END(filesystem)
NAMESPACE_END(mitsuba)
