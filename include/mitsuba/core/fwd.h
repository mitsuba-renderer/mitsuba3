#pragma once

#include <mitsuba/core/platform.h>
#include <cstddef>

NAMESPACE_BEGIN(mitsuba)

class Object;
class Class;
template <typename> class ref;

class AnnotatedStraem;
class Appender;
class ArgParser;
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
class Stream;
class StreamAppender;
class Struct;
class StructConverter;
class Thread;
class ThreadLocalBase;
class ThreadLocalBase;
class ZStream;
enum ELogLevel : int;

template <typename, typename> class ThreadLocal;
template <typename> class AtomicFloat;

/* Forward declarations of various vector types */
template <typename Type, size_t Size>      struct Vector;
template <typename Type, size_t Size>      struct Point;
template <typename Type, size_t Size = 3>  struct Normal;
template <typename Point, typename Vector> struct TRay;
template <typename Point>                  struct TBoundingBox;
template <typename Vector>                 struct Frame;

using Vector1f = Vector<Float, 1>;
using Vector2f = Vector<Float, 2>;
using Vector3f = Vector<Float, 3>;
using Vector4f = Vector<Float, 4>;

using Vector1d = Vector<double, 1>;
using Vector2d = Vector<double, 2>;
using Vector3d = Vector<double, 3>;
using Vector4d = Vector<double, 4>;

using Vector1s = Vector<size_t, 1>;
using Vector2s = Vector<size_t, 2>;
using Vector3s = Vector<size_t, 3>;
using Vector4s = Vector<size_t, 4>;

using Point1f = Point<Float, 1>;
using Point2f = Point<Float, 2>;
using Point3f = Point<Float, 3>;
using Point4f = Point<Float, 4>;

using Point1d = Point<double, 1>;
using Point2d = Point<double, 2>;
using Point3d = Point<double, 3>;
using Point4d = Point<double, 4>;

using Normal3f = Normal<Float, 3>;
using Normal3d = Normal<double, 3>;

using BoundingBox1f = TBoundingBox<Point1f>;
using BoundingBox2f = TBoundingBox<Point2f>;
using BoundingBox3f = TBoundingBox<Point3f>;
using BoundingBox4f = TBoundingBox<Point4f>;

using Ray2f = TRay<Point2f, Vector2f>;
using Ray3f = TRay<Point3f, Vector3f>;

using Frame3f = Frame<Vector3f>;

NAMESPACE_BEGIN(filesystem)
class path;
NAMESPACE_END(filesystem)

namespace fs = filesystem;

NAMESPACE_END(mitsuba)

/* Forward declaration for Eigen types */
NAMESPACE_BEGIN(Eigen)
template<typename Scalar, int Rows, int Cols, int Options, int MaxRows, int MaxCols>
class Matrix;
template <typename Derived> class MatrixBase;
NAMESPACE_END(Eigen)
