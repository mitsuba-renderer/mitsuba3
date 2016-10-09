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
template <typename Scalar, int Dimension>  struct TVector;
template <typename Scalar, int Dimension>  struct TPoint;
template <typename Scalar>                 struct TNormal;
template <typename Point, typename Vector> struct TRay;
template <typename Point>                  struct TBoundingBox;

typedef TVector<Float, 1>       Vector1f;
typedef TVector<Float, 2>       Vector2f;
typedef TVector<Float, 3>       Vector3f;
typedef TVector<Float, 4>       Vector4f;

typedef TVector<double, 1>      Vector1d;
typedef TVector<double, 2>      Vector2d;
typedef TVector<double, 3>      Vector3d;
typedef TVector<double, 4>      Vector4d;

typedef TVector<size_t, 1>      Vector1s;
typedef TVector<size_t, 2>      Vector2s;
typedef TVector<size_t, 3>      Vector3s;
typedef TVector<size_t, 4>      Vector4s;

typedef TPoint<Float, 1>        Point1f;
typedef TPoint<Float, 2>        Point2f;
typedef TPoint<Float, 3>        Point3f;
typedef TPoint<Float, 4>        Point4f;

typedef TPoint<double, 1>       Point1d;
typedef TPoint<double, 2>       Point2d;
typedef TPoint<double, 3>       Point3d;
typedef TPoint<double, 4>       Point4d;

typedef TNormal<Float>          Normal3f;
typedef TNormal<double>         Normal3d;

typedef TBoundingBox<Point1f>   BoundingBox1f;
typedef TBoundingBox<Point2f>   BoundingBox2f;
typedef TBoundingBox<Point3f>   BoundingBox3f;
typedef TBoundingBox<Point4f>   BoundingBox4f;

typedef TRay<Point2f, Vector2f> Ray2f;
typedef TRay<Point3f, Vector3f> Ray3f;

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
