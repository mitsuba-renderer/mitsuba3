#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/stream.h>
#include <unordered_map>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/** \brief An AnnotatedStream adds Table of Contents capabilities to an
 * underlying stream. A Stream instance must first be created and passed to
 * to the constructor. The underlying stream should either be empty or a
 * stream that was previously written with an AnnotatedStream, so that it
 * contains a proper Table of Contents.
 *
 * Table of Contents: objects and variables written to the stream are
 * prepended by a field name. Contents can then be queried by field name, as
 * if using a map.
 * A hierarchy can be created by <tt>push</tt>ing and <tt>pop</tt>ing prefixes.
 * The root of this hierarchy is the empty prefix "".
 *
 * The table of contents is automatically read from the underlying stream on
 * creation and written back on destruction.
 */
class MTS_EXPORT_CORE AnnotatedStream : public Object {
public:

    /** \brief Creates an AnnotatedStream based on the given Stream (decorator pattern).
     * Anything written to the AnnotatedStream is ultimately passed down to the
     * given Stream instance.
     * The given Stream instance should not be destructed before this.
     *
     * Throws if <tt>write_mode</tt> is enabled (resp. disabled) but the underlying
     * stream does not have write (resp. read) capabilities.
     *
     * Throws if the underlying stream has read capabilities and is not empty
     * but does not correspond to a valid AnnotatedStream (i.e. it does not
     * start with the \ref kSerializedHeaderId sentry).
     *
     * @param write_mode Whether to use write mode. The stream is either read-only
     *                  or write-only.
     * @param throw_on_missing Whether an error should be thrown when \ref get is
     *                       called for a missing field.
     */
    AnnotatedStream(Stream *stream, bool write_mode, bool throw_on_missing = true);

    /** \brief Closes the annotated stream.
     * No further read or write operations are permitted.
     *
     * \note The underlying stream is not automatically closed by this function.
     * It may, however, call its own <tt>close</tt> function in its destructor.
     *
     * This function is idempotent and causes the ToC to be written out to the stream.
     * It is called automatically by the destructor.
     */
    void close();

    // =========================================================================
    //! @{ \name Table of Contents (TOC)
    // =========================================================================

    /** \brief Push a name prefix onto the stack (use this to isolate
     * identically-named data fields).
     */
    void push(const std::string &name);

    /// Pop a name prefix from the stack
    void pop();

    /** \brief Return all field names under the current name prefix.
     * Nested names are returned with the full path prepended, e.g.:
     *   level_1.level_2.my_name
     */
    std::vector<std::string> keys() const;

    /** \brief Retrieve a field from the serialized file (only valid in read mode)
     *
     * Throws if the field exists but has the wrong type.
     * Throws if the field is not found and <tt>throw_on_missing</tt> is true.
     */
    template <typename T> bool get(const std::string &name, T &value) {
        using helper = detail::serialization_helper<T>;
        if (!get_base(name, helper::type_id()))
            return false;
        if (!name.empty())
            push(name);
        m_stream->read(value);
        if (!name.empty())
            pop();
        return true;
    }

    /// Store a field in the serialized file (only valid in write mode)
    template <typename T> void set(const std::string &name, const T &value) {
        using helper = detail::serialization_helper<T>;
        set_base(name, helper::type_id());
        if (!name.empty())
            push(name);
        m_stream->write(value);
        if (!name.empty())
            pop();
    }

    /// Whether the stream won't throw when trying to get missing fields.
    bool compatibilityMode() const { return !m_throw_on_missing; }

    /// @}
    // =========================================================================

    // =========================================================================
    //! @{ \name Underlying stream
    // =========================================================================

    /// Returns the current size of the underlying stream
    size_t size() const { return m_stream->size(); }

    /// Whether the underlying stream has read capabilities and is not closed.
    bool can_read() const { return !m_write_mode && !is_closed(); }

    /// Whether the underlying stream has write capabilities and is not closed.
    bool can_write() const { return m_write_mode && !is_closed(); }

    /// Whether the annotated stream has been closed (no further read or writes permitted)
    bool is_closed() const { return m_is_closed; }

    /// @}
    // =========================================================================

    MTS_DECLARE_CLASS();

protected:

    /// Protected destructor
    virtual ~AnnotatedStream();

    /** \brief Attempts to seek to the position of the given field.
     * The active prefix (from previous \ref push operations) is prepended
     * to the given <tt>name</tt>.
     *
     * Throws if the field exists but has the wrong type.
     * Throws if the field is not found and <tt>m_throw_on_missing</tt> is true.
     */
    bool get_base(const std::string &name, const std::string &type_id);

    /** \brief Attempts to associate the current position of the stream to
     * the given field. The active prefix (from previous \ref push operations)
     * is prepended to the <tt>name</tt> of the field.
     *
     * Throws if a value was already set with that name (including prefix).
     */
    void set_base(const std::string &name, const std::string &type_id);

    /** \brief Read back the table of contents from the underlying stream and
     * update the in-memory <tt>m_table</tt> accordingly.
     * Should be called on construction.
     *
     * Throws if the underlying stream does not have read capabilities.
     * Throws if the underlying stream does not have start with the
     * AnnotatedStream sentry (\ref kSerializedHeaderId).
     */
    void read_toc();

    /** \brief Write back the table of contents to the underlying stream.
     * Should be called on destruction.
     */
    void write_toc();

private:

    /// Underlying stream where the names and contents are written
    ref<Stream> m_stream;

    /// Maintains the mapping: full field name -> (type, position in the stream)
    std::unordered_map<std::string, std::pair<std::string, uint64_t>> m_table;

    /** \brief Stack of accumulated prefixes,
     * i.e. <tt>m_prefixStack.back</tt> is the full prefix path currently applied.
     */
    std::vector<std::string> m_prefixStack;

    bool m_write_mode;

    bool m_throw_on_missing;

    /// Whether the annotated stream is closed (independent of the underlying stream).
    bool m_is_closed;
};

NAMESPACE_END(mitsuba)
