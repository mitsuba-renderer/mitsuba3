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
template <typename Type, size_t Size>      struct TVector;
template <typename Type, size_t Size>      struct TPoint;
template <typename Type>                   struct TNormal;
template <typename Point, typename Vector> struct TRay;
template <typename Point>                  struct TBoundingBox;
template <typename Vector>                 struct TFrame;

using Vector1f = TVector<Float, 1>;
using Vector2f = TVector<Float, 2>;
using Vector3f = TVector<Float, 3>;
using Vector4f = TVector<Float, 4>;

using Vector1d = TVector<double, 1>;
using Vector2d = TVector<double, 2>;
using Vector3d = TVector<double, 3>;
using Vector4d = TVector<double, 4>;

using Vector1s = TVector<size_t, 1>;
using Vector2s = TVector<size_t, 2>;
using Vector3s = TVector<size_t, 3>;
using Vector4s = TVector<size_t, 4>;

using Point1f = TPoint<Float, 1>;
using Point2f = TPoint<Float, 2>;
using Point3f = TPoint<Float, 3>;
using Point4f = TPoint<Float, 4>;

using Point1d = TPoint<double, 1>;
using Point2d = TPoint<double, 2>;
using Point3d = TPoint<double, 3>;
using Point4d = TPoint<double, 4>;

using Normal3f = TNormal<Float>;
using Normal3d = TNormal<double>;

using BoundingBox1f = TBoundingBox<Point1f>;
using BoundingBox2f = TBoundingBox<Point2f>;
using BoundingBox3f = TBoundingBox<Point3f>;
using BoundingBox4f = TBoundingBox<Point4f>;

using Ray2f = TRay<Point2f, Vector2f>;
using Ray3f = TRay<Point3f, Vector3f>;

using Frame3f = TFrame<Vector3f>;

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
