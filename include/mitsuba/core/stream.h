#pragma once

#include <mitsuba/core/fwd.h>
#include <mitsuba/core/object.h>

NAMESPACE_BEGIN(mitsuba)

/** \brief Abstract seekable stream class
 *
 * Specifies all functions to be implemented by stream
 * subclasses and provides various convenience functions
 * layered on top of on them.
 *
 * All read<b>X</b>() and write<b>X</b>() methods support transparent
 * conversion based on the endianness of the underlying system and the
 * value passed to \ref setByteOrder(). Whenever \ref getHostByteOrder()
 * and \ref getByteOrder() disagree, the endianness is swapped.
 *
 * \sa FileStream, MemoryStream, DummyStream
 */
class MTS_EXPORT_CORE Stream : public Object {

public:
    Stream()
        : Object() { }

    Stream(Object const & o)
        : Object(o) {}
};

NAMESPACE_END(mitsuba)

