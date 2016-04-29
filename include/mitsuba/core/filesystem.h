#pragma once

/**
 * \brief filesystem helpers to manipulate paths on Linux/Windows/Mac OS
 *
 * Follows the C++17 fs interface (see http://en.cppreference.com/w/cpp/experimental/fs)
 * Uses implementations from https://github.com/wjakob/filesystem
 *
 * This class is just a temporary workaround to avoid the heavy boost
 * dependency until boost::filesystem is integrated into the standard template
 * library at some point in the future.
 *
 * Copyright (c) 2015-2016 Wenzel Jakob <wenzel@inf.ethz.ch>
 * All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE file.
 */

#include <mitsuba/core/fwd.h>

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


NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(filesystem)

// TODO: all interfaces matching the C++17 reference
class path;  // TODO: remove this


/// Returns the current working directory (equivalent to getcwd)
// TODO: overload taking a path as parameter
path current_path();

// TODO: take `filesystem::path base` parameter
path make_absolute(const path& p);

bool is_regular_file(const path& p) noexcept;
bool is_directory(const path& p) noexcept;
bool exists(const path& p) noexcept;

size_t file_size(const path& p);
inline bool create_directory(const path& p) noexcept;
bool resize_file(const path& p, size_t target_length) noexcept;
// TODO: remove_all to remove recursively
bool remove(const path& p);

class path {
public:
    enum path_type {
        windows_path = 0,
        posix_path = 1,
#if defined(_WIN32)
        native_path = windows_path
#else
        native_path = posix_path
#endif
    };

    // TODO: value_type and string_type typedefs?

    path() : m_type(native_path), m_absolute(false) { }

    path(const path &path)
        : m_type(path.m_type), m_path(path.m_path), m_absolute(path.m_absolute) {}

    path(path &&path)
        : m_type(path.m_type), m_path(std::move(path.m_path)),
          m_absolute(path.m_absolute) {}

    path(const char *string) { set(string); }

    path(const std::string &string) { set(string); }

#if defined(_WIN32)
    path(const std::wstring &wstring) { set(wstring); }
    path(const wchar_t *wstring) { set(wstring); }
#endif

    // Not part of the std::filesystem::path specification
    //size_t length() const { return m_path.size(); }

    bool empty() const { return m_path.empty(); }

    bool is_absolute() const { return m_absolute; }
    bool is_relative() const { return !m_absolute; }

    path parent_path() const;
    std::string extension() const;
    std::string filename() const;


    // TODO: should be able to return a reference
    const std::string native() const noexcept {
      return str(native_path);
    }
    operator std::string() const noexcept {
      return str(native_path);
    }

    path operator/(const path &other) const;
    path & operator=(const path &path);
    path & operator=(path &&path);
    friend std::ostream &operator<<(std::ostream &os, const path &path) {
      os << path.str();
      return os;
    }

    // TODO
#if defined(_WIN32)
    std::wstring wstr(path_type type = native_path) const {
        std::string temp = str(type);
        int size = MultiByteToWideChar(CP_UTF8, 0, &temp[0], (int)temp.size(), NULL, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, &temp[0], (int)temp.size(), &result[0], size);
        return result;
    }


    void set(const std::wstring &wstring, path_type type = native_path) {
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

    path &operator=(const std::wstring &str) { set(str); return *this; }
#endif

    bool operator==(const path &p) const { return p.m_path == m_path; }
    bool operator!=(const path &p) const { return p.m_path != m_path; }

    std::string str(path_type type = native_path) const;
protected:

    void set(const std::string &str, path_type type = native_path);

    static std::vector<std::string> tokenize(const std::string &string,
                                             const std::string &delim);

protected:
    path_type m_type;
    std::vector<std::string> m_path;
    bool m_absolute;
};

NAMESPACE_END(filesystem)

NAMESPACE_END(mitsuba)
