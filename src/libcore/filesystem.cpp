#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#include <mitsuba/core/filesystem.h>
#include <mitsuba/core/string.h>

#include <cctype>
#include <cerrno>
#include <codecvt>
#include <cstdlib>
#include <cstring>
#include <locale>
#include <stdexcept>
#include <sstream>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/stat.h>
#endif

#if defined(__linux__)
#  include <linux/limits.h>
#endif

/** Macro allowing to type hardocded character sequences
 * with the right type prefix (char_t: no prefix, wchar_t: 'L' prefix)
 */
#if defined(_WIN32)
#  define NSTR(str) L##str
#else
#  define NSTR(str) str
#endif

NAMESPACE_BEGIN(mitsuba)
NAMESPACE_BEGIN(filesystem)

inline string_type to_native(const std::string &str) {
#if defined(_WIN32)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
#else
    return str;
#endif
}

inline std::string from_native(const string_type &str) {
#if defined(_WIN32)
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(str);
#else
    return str;
#endif
}

#if defined(_WIN32)
path::path(const std::string &string) { set(to_native(string)); }
#endif

path current_path() {
#if !defined(_WIN32)
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

path absolute(const path& p) {
#if !defined(_WIN32)
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
#if defined(_WIN32)
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
#if defined(_WIN32)
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
#if defined(_WIN32)
    return GetFileAttributesW(p.native().c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat sb;
    return stat(p.native().c_str(), &sb) == 0;
#endif
}

size_t file_size(const path& p) {
#if defined(_WIN32)
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

bool equivalent(const path& p1, const path& p2) {
#if defined(_WIN32)
    struct _stati64 sb1, sb2;
    if (_wstati64(p1.native().c_str(), &sb1) != 0)
        throw std::runtime_error("filesystem::equivalent(): cannot stat file \"" + p1.string() + "\"!");
    if (_wstati64(p2.native().c_str(), &sb2) != 0)
        throw std::runtime_error("filesystem::equivalent(): cannot stat file \"" + p2.string() + "\"!");
#else
    struct stat sb1, sb2;
    if (stat(p1.native().c_str(), &sb1) != 0)
        throw std::runtime_error("filesystem::equivalent(): cannot stat file \"" + p1.string() + "\"!");
    if (stat(p2.native().c_str(), &sb2) != 0)
        throw std::runtime_error("filesystem::equivalent(): cannot stat file \"" + p2.string() + "\"!");
#endif

    return (sb1.st_dev == sb2.st_dev) && (sb1.st_ino == sb2.st_ino);
}

bool create_directory(const path& p) noexcept {
    if (exists(p))
        return is_directory(p);

#if defined(_WIN32)
    return CreateDirectoryW(p.native().c_str(), NULL) != 0;
#else
    return mkdir(p.native().c_str(), S_IRUSR | S_IWUSR | S_IXUSR) == 0;
#endif
}

bool resize_file(const path& p, size_t target_length) noexcept {
#if !defined(_WIN32)
    return ::truncate(p.native().c_str(), (off_t) target_length) == 0;
#else
    HANDLE handle = CreateFileW(p.native().c_str(), GENERIC_WRITE, 0,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
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
#if !defined(_WIN32)
    return std::remove(p.native().c_str()) == 0;
#else
    if (is_directory(p))
        return RemoveDirectoryW(p.native().c_str()) != 0;
    else
        return DeleteFileW(p.native().c_str()) != 0;
#endif
}

bool rename(const path& src, const path &dst) {
#if !defined(_WIN32)
    return std::rename(src.native().c_str(), dst.native().c_str()) == 0;
#else
    return MoveFileW(src.native().c_str(), dst.native().c_str()) != 0;
#endif
}

// -----------------------------------------------------------------------------

fs::path path::extension() const {
    if (empty() || m_path.back() == NSTR(".") || m_path.back() == NSTR(".."))
        return NSTR("");

    const string_type &name = filename();
    size_t pos = name.find_last_of(NSTR("."));
    if (pos == string_type::npos)
        return "";
    return name.substr(pos);  // Including the . character!
}

path& path::replace_extension(const fs::path &replacement_) {
    if (empty() || m_path.back() == NSTR(".") || m_path.back() == NSTR(".."))
        return *this;

    string_type name = filename();
    size_t pos = name.find_last_of(NSTR("."));

    if (pos != string_type::npos)
        name = name.substr(0, pos);

    string_type replacement(replacement_);
    if (!replacement.empty()) {
        string_type period(NSTR("."));
        if (std::equal(period.begin(), period.end(), replacement.begin()))
            name += replacement;
        else
            name += period + replacement;
    }

    m_path.back() = name;
    return *this;
}

path path::filename() const {
    if (empty())
        return path(NSTR(""));
    return path(m_path.back());
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
    return from_native(str());
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

#if defined(_WIN32)
path & path::operator=(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    set(converter.from_bytes(str));
    return *this;
}
#endif

string_type path::str() const {
    std::basic_ostringstream<value_type> oss;

#if !defined(_WIN32)
    if (m_absolute)
        oss << preferred_separator;
#endif

    for (size_t i = 0; i < m_path.size(); ++i) {
        oss << m_path[i];
        if (i + 1 < m_path.size())
            oss << preferred_separator;
    }

    return oss.str();
}

void path::set(const string_type &str) {
    if (str.empty()) {
        clear();
        return;
    }

#if defined(_WIN32)
    m_path = tokenize(str, NSTR("/\\"));
    m_absolute = str.size() >= 2 && std::isalpha(str[0]) && str[1] == NSTR(':');
#else
    m_path = tokenize(str, NSTR("/"));
    m_absolute = !str.empty() && str[0] == NSTR('/');
#endif
}

std::vector<string_type> path::tokenize(const string_type &string,
                                        const string_type &delim) {
    string_type::size_type last_pos = 0,
                           pos = string.find_first_of(delim, last_pos);
    std::vector<string_type> tokens;

    while (last_pos != string_type::npos) {
        if (pos != last_pos)
            tokens.push_back(string.substr(last_pos, pos - last_pos));
        last_pos = pos;
        if (last_pos == string_type::npos || last_pos + 1 == string.length())
            break;
        pos = string.find_first_of(delim, ++last_pos);
    }

    return tokens;
}

std::ostream& operator<<(std::ostream &os, const path &path) {
    os << path.string();
    return os;
}

NAMESPACE_END(filesystem)
NAMESPACE_END(mitsuba)
