#pragma once

/**
 * \brief filesystem helpers to manipulate paths on Linux/Windows/Mac OS
 *
 * Follows the C++17 fs interface (see https://en.cppreference.com/w/cpp/experimental/fs).
 * Based on implementations from https://github.com/wjakob/filesystem.
 *
 * This class is just a temporary workaround to avoid the heavy boost
 * dependency until C++17 compiler support picks-up.
 *
 * Copyright (c) 2017 Wenzel Jakob <wenzel.jakob@epfl.ch>
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE file.
 */

#include <mitsuba/core/fwd.h>
#include <iosfwd>
#include <string>

NAMESPACE_BEGIN(mitsuba)

NAMESPACE_BEGIN(filesystem)

/** Type of characters used on the system (in particular, type of characters
 * to use when calling system APIs).
 */
#if defined(_WIN32)
    using value_type = wchar_t;
    using string_type = std::wstring;
#else
    using value_type = char;
    using string_type = std::string;
#endif

/// System-specific separator used to write paths.
#if defined(_WIN32)
    constexpr value_type preferred_separator = L'\\';
#else
    constexpr value_type preferred_separator = '/';
#endif

/** \brief Represents a path to a filesystem resource.
 * On construction, the path is parsed and stored in a system-agnostic
 * representation. The path can be converted back to the system-specific string
 * using <tt>native()</tt> or <tt>string()</tt>.
 */
class MI_EXPORT_LIB path {
public:
    /// Default constructor. Constructs an empty path. An empty path is considered relative.
    path() : m_absolute(false) { }

    /// Copy constructor.
    path(const path &path)
        : m_path(path.m_path), m_absolute(path.m_absolute) {}

    /// Move constructor.
    path(path &&path)
        : m_path(std::move(path.m_path)), m_absolute(path.m_absolute) {}

    /** \brief Construct a path from a string with native type.
     * On Windows, the path can use both '/' or '\\' as a delimiter.
     */
    path(const string_type &string) { set(string); }

    /** \brief Construct a path from a string with native type.
     * On Windows, the path can use both '/' or '\\' as a delimiter.
     */
    path(const value_type *string) { set(string); }

#if defined(_WIN32)
    /** \brief Constructs a path from an std::string, even if it's not the
     * native string type. Assumes the string is UTF-8 encoded to carry
     * conversion to native type.
     */
    path(const std::string &string);

    /**
     * \brief Constructs a path from an char array, even if it's not the
     * native string type. Assumes the string is UTF-8 encoded to carry
     * conversion to native type.
     */
    path(const char *string) : path(std::string(string)) { }
#endif

    /// Makes the path an empty path. An empty path is considered relative.
    void clear() {
        m_absolute = false;
        m_path.clear();
    }

    /// Checks if the path is empty
    bool empty() const { return m_path.empty(); }

    /// Checks if the path is absolute.
    bool is_absolute() const { return m_absolute; }

    /// Checks if the path is relative.
    bool is_relative() const { return !m_absolute; }

    /** \brief Returns the path to the parent directory. Returns an empty path
     * if it is already empty or if it has only one element. */
    path parent_path() const;

    /** \brief Returns the extension of the filename component of the path (the
     * substring starting at the rightmost period, including the period).
     * Special paths '.' and '..' have an empty extension. */
    path extension() const;

    /** \brief Replaces the substring starting at the rightmost '.' symbol
     * by the provided string.
     *
     * A '.' symbol is automatically inserted if the replacement does not start
     * with a dot. Removes the extension altogether if the empty path is
     * passed. If there is no extension, appends a '.' followed by the
     * replacement. If the path is empty, '.' or '..', the method does nothing.
     *
     * Returns *this.
     */
    path& replace_extension(const path &replacement = path());

    /// Returns the filename component of the path, including the extension.
    path filename() const;

    /** \brief Returns the path in the form of a native string, so that it can
     * be passed directly to system APIs. The path is constructed using the
     * system's preferred separator and the native string type.
     */
    const string_type native() const noexcept { return str(); }

    /**
     * \brief Implicit conversion operator to the basic_string corresponding
     * to the system's character type. Equivalent to calling <tt>native()</tt>.
     */
    operator string_type() const noexcept { return native(); }

    /// Equivalent to native(), converted to the std::string type
    std::string string() const;

    /// Concatenates two paths with a directory separator.
    path operator/(const path &other) const;

    /// Assignment operator.
    path& operator=(const path &path);

    /// Move assignment operator.
    path& operator=(path &&path);

    /** \brief Assignment from the system's native string type. Acts similarly
     * to the string constructor.
     */
    path& operator=(const string_type &str) { set(str); return *this; }

#if defined(_WIN32)
    /** \brief Constructs a path from an std::string, even if it's not the
     * native string type. Assumes the string is UTF-8 encoded to carry
     * conversion to native type.
     */
    path& operator=(const std::string &str);
#endif

    /// Prints the path as it would be returned by <tt>string()</tt>.
    MI_EXPORT_LIB friend std::ostream& operator<<(std::ostream &os, const path &path);

    /** Equality operator. Warning: this only checks for lexicographic equivalence.
     * To check whether two paths point to the same filesystem resource,
     * use <tt>equivalent</tt>.
     */
    bool operator==(const path &p) const { return p.m_path == m_path; }

    /// Inequality operator.
    bool operator!=(const path &p) const { return p.m_path != m_path; }

protected:
    string_type str() const;

    /// Builds a path from the passed string.
    void set(const string_type &str);

    /** \brief Splits a string into tokens delimited by any of the characters
     * passed in <tt>delim</tt>. */
    static std::vector<string_type> tokenize(const string_type &string,
                                             const string_type &delim);

protected:
    std::vector<string_type> m_path;
    bool m_absolute;
};

/// Returns the current working directory (equivalent to getcwd)
extern MI_EXPORT_LIB path current_path();

/** \brief Returns an absolute path to the same location pointed by <tt>p</tt>,
 * relative to <tt>base</tt>.
 * \see http ://en.cppreference.com/w/cpp/experimental/fs/absolute)
 */
extern MI_EXPORT_LIB path absolute(const path& p);

/// Checks if <tt>p</tt> points to a regular file, as opposed to a directory or symlink.
extern MI_EXPORT_LIB bool is_regular_file(const path& p) noexcept;
/// Checks if <tt>p</tt> points to a directory.
extern MI_EXPORT_LIB bool is_directory(const path& p) noexcept;
/// Checks if <tt>p</tt> points to an existing filesystem object.
extern MI_EXPORT_LIB bool exists(const path& p) noexcept;
/** \brief Returns the size (in bytes) of a regular file at <tt>p</tt>.
 * Attempting to determine the size of a directory (as well as any other file
 * that is not a regular file or a symlink) is treated as an error.
 */
extern MI_EXPORT_LIB size_t file_size(const path& p);

/** \brief Checks whether two paths refer to the same file system object.
 * Both must refer to an existing file or directory.
 * Symlinks are followed to determine equivalence.
 */
extern MI_EXPORT_LIB bool equivalent(const path& p1, const path& p2);

/** \brief Creates a directory at <tt>p</tt> as if <tt>mkdir</tt> was used.
 * Returns true if directory creation was successful, false otherwise.
 * If <tt>p</tt> already exists and is already a directory, the function
 * does nothing (this condition is not treated as an error).
 */
extern MI_EXPORT_LIB bool create_directory(const path& p) noexcept;
/** \brief Changes the size of the regular file named by <tt>p</tt> as if
 * <tt>truncate</tt> was called. If the file was larger than <tt>target_length</tt>,
 * the remainder is discarded. The file must exist.
 */
extern MI_EXPORT_LIB bool resize_file(const path& p, size_t target_length) noexcept;

/** \brief Removes a file or empty directory. Returns true if removal was
 * successful, false if there was an error (e.g. the file did not exist).
 */
extern MI_EXPORT_LIB bool remove(const path& p);


/** \brief Renames a file or directory. Returns true if renaming was
 * successful, false if there was an error (e.g. the file did not exist).
 */
extern MI_EXPORT_LIB bool rename(const path& src, const path &dst);

NAMESPACE_END(filesystem)

NAMESPACE_END(mitsuba)
