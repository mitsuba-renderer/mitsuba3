#pragma once

#include <mitsuba/core/object.h>
#include <mitsuba/core/stream.h>
#include <unordered_map>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

/** \brief An AnnotatedStream adds Table of Contents capabilities to an
 * underlying stream. A Stream instance must first be created and passed to
 * to the constructor.
 *
 * Table of Contents: objects and variables written to the stream are
 * prepended by a field name. Contents can then be queried by field name, as
 * if using a map.
 * A hierarchy can be created by <tt>push</tt>ing and <tt>pop</tt>ing prefixes.
 */
class MTS_EXPORT_CORE AnnotatedStream : public Object {
public:

    /** \brief Creates an AnnotatedStream based on the given Stream (decorator pattern).
     * Anything written to the AnnotatedStream is ultimately passed down to the
     * given Stream instance.
     * The given Stream instance should not be destructed before this.
     *
     * TODO: in order not to use unique_ptr, use move constructor or such?
     */
    AnnotatedStream(ref<Stream> &stream)
        : Object(), m_stream(stream) { }

    MTS_DECLARE_CLASS();

protected:
    /// Destructor.
    ~AnnotatedStream() { }

private:
    /// Underlying stream where the names and contents are written
    const ref<Stream> m_stream;
    /// Maintains the mapping: full field name -> (type, position in the stream)
    std::unordered_map<std::string, std::pair<std::string, uint64_t>> m_table;
    /// Stack of prefixes currently applied, constituting a path
    std::vector<std::string> m_prefixStack;
};

NAMESPACE_END(mitsuba)
