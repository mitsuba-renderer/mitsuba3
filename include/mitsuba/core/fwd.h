#pragma once

#include <mitsuba/core/platform.h>
#include <enoki/fwd.h>
#include <cstddef>
#include <cstdint>

NAMESPACE_BEGIN(mitsuba)

/// Import the complete Enoki namespace
using namespace enoki;

// =============================================================
//! @{ \name Forward declarations of Mitsuba classes/structs
// =============================================================

class Object;
class Class;
template <typename> class ref;

class AnnotatedStream;
class AnimatedTransform;
class Appender;
class ArgParser;
class Bitmap;
class DefaultFormatter;
class DummyStream;
class FileResolver;
class FileStream;
class Formatter;
class Logger;
class MemoryStream;
class Mutex;
class PluginManager;
class Properties;
class ScopedThreadEnvironment;
class Stream;
class StreamAppender;
class Struct;
class StructConverter;
class Thread;
class ThreadLocalBase;
class ZStream;
enum ELogLevel : int;

template <typename, typename> class ThreadLocal;
template <typename> class AtomicFloat;

//! @}
// =============================================================

// =============================================================
//! @{ \name Common type aliases
// =============================================================

template <typename Value, size_t Size>       struct Vector;
template <typename Value, size_t Size>       struct Point;
template <typename Value, size_t Size>       struct Normal;
template <typename Value, size_t Size>       struct Color;
template <typename Value, size_t Size>       struct Spectrum;
template <typename Value, size_t Size>       struct Transform;
template <typename Point, typename Spectrum> struct Ray;
template <typename Point, typename Spectrum> struct RayDifferential;
template <typename Point>                    struct BoundingBox;
template <typename Point>                    struct BoundingSphere;
template <typename Vector>                   struct Frame;

using Vector1i = Vector<int32_t, 1>;
using Vector2i = Vector<int32_t, 2>;
using Vector3i = Vector<int32_t, 3>;
using Vector4i = Vector<int32_t, 4>;

using Vector1u = Vector<uint32_t, 1>;
using Vector2u = Vector<uint32_t, 2>;
using Vector3u = Vector<uint32_t, 3>;
using Vector4u = Vector<uint32_t, 4>;

//! @}
// =============================================================

NAMESPACE_BEGIN(filesystem)
class path;
NAMESPACE_END(filesystem)

namespace fs = filesystem;

NAMESPACE_END(mitsuba)

extern "C" {
#if defined(MTS_USE_EMBREE)
    // Forward declarations for Embree
    typedef struct RTCDeviceTy* RTCDevice;
    typedef struct RTCSceneTy* RTCScene;
    typedef struct RTCGeometryTy* RTCGeometry;
#endif

#if defined(MTS_USE_OPTIX)
    // Forward declarations for OptiX
    typedef struct RTcontext_api *RTcontext;
    typedef struct RTgeometrytriangles_api *RTgeometrytriangles;
    typedef struct RTbuffer_api *RTbuffer;
#endif
};
