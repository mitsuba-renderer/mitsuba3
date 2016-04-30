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

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(filesystem)

/// Type of character used on the system
#if defined(_WIN32)
typedef wchar_t value_type;
#else
typedef char value_type;
#endif
/// Type of strings (built from system-specific characters)
typedef std::basic_string<value_type> string_type;

#if defined(_WIN32)
constexpr value_type preferred_separator = '\\';
#else
constexpr value_type preferred_separator = '/';
#endif

class MTS_EXPORT_CORE path {
 public:

    path() : m_absolute(false) { }

    path(const path &path)
        : m_path(path.m_path), m_absolute(path.m_absolute) {}

    path(path &&path)
        : m_path(std::move(path.m_path)), m_absolute(path.m_absolute) {}

    path(const char *string) { set(string); }

    path(const string_type &string) { set(string); }

    // Not part of the std::filesystem::path specification
    //size_t length() const { return m_path.size(); }

    void clear() {
      m_absolute = false;
      m_path.clear();
    }
    bool empty() const { return m_path.empty(); }

    bool is_absolute() const { return m_absolute; }
    bool is_relative() const { return !m_absolute; }

    /// Returns the path to the parent directory. Returns the empty path if it
    /// already empty or if it has only one element.
    path parent_path() const;
    /// Returns the extension of the filename component of the path (the
    /// substring starting at the rightmost period, including the period).
    /// Special paths '.' and '..' have an empty extension.
    string_type extension() const;
    /// Returns the filename component of the path, including the extension.
    string_type filename() const;

    // TODO: c_str (equivalent to p.native.c_str())
    // TODO: should be able to return a reference
    const string_type native() const noexcept {
      return str();
    }
    operator string_type() const noexcept {
      return str();
    }

    path operator/(const path &other) const;
    path & operator=(const path &path);
    path & operator=(path &&path);
    path & operator=(const string_type &str) { set(str); return *this; }
    friend std::ostream &operator<<(std::ostream &os, const path &path) {
      os << path.str();
      return os;
    }

    bool operator==(const path &p) const { return p.m_path == m_path; }
    bool operator!=(const path &p) const { return p.m_path != m_path; }

 protected:
    string_type str() const;

    void set(const string_type &str);

    static std::vector<std::string> tokenize(const string_type &string,
                                             const string_type &delim);

 protected:
    std::vector<string_type> m_path;
    bool m_absolute;
};

/// Returns the current working directory (equivalent to getcwd)
extern MTS_EXPORT_CORE path current_path();
// TODO: overload taking a base path as parameter

// TODO: take `filesystem::path base` parameter
extern MTS_EXPORT_CORE path make_absolute(const path& p);

extern MTS_EXPORT_CORE bool is_regular_file(const path& p) noexcept;
extern MTS_EXPORT_CORE bool is_directory(const path& p) noexcept;
extern MTS_EXPORT_CORE bool exists(const path& p) noexcept;

extern MTS_EXPORT_CORE size_t file_size(const path& p);
extern MTS_EXPORT_CORE bool create_directory(const path& p) noexcept;
extern MTS_EXPORT_CORE bool resize_file(const path& p, size_t target_length) noexcept;
/// Removes the file at the passed path
extern MTS_EXPORT_CORE bool remove(const path& p);
// TODO: remove_all to remove recursively

NAMESPACE_END(filesystem)

NAMESPACE_END(mitsuba)
