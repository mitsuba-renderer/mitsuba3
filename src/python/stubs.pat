# This file contains declarations to patch Mitsuba's stub files. nanobind's
# stubgen automatically applies them during the build process
#
# The syntax of this file is described here:
#
# https://nanobind.readthedocs.io/en/latest/typing.html#pattern-files
#
# The design of type signatures and the use of generics and type variables is
# explained in the Dr.Jit documentation section entitled "Type Signatures".
#
# Whenever possible, it's preferable to specify signatures to C++ bindings
# using the nb::sig() override. The rules below are used in cases where that is
# not possible, or when the typing-specific overloads of a funciton deviate
# significantly from the overload chain implemented using nanobind.


# --------------------------- Core -----------------------------

MI_AUTHORS:
MI_ENABLE_CUDA:
MI_ENABLE_EMBREE:
MI_FILTER_RESOLUTION:
MI_VERSION:
MI_VERSION_MAJOR:
MI_VERSION_MINOR:
MI_VERSION_PATCH:
MI_YEAR:
DEBUG:

cast_object:
casters:


# ------------------------- Variant ----------------------------

.*\.DRJIT_STRUCT:

mitsuba.__prefix__:
    import typing
    from collections.abc import Sequence
    from .python import (
        ad as ad,
        chi2 as chi2,
        math_py as math_py,
        util as util,
    )

    def load_dict(dict: dict, parallel: bool = True, optimize: bool = True) -> typing.Any: ...
    def load_file(path: str, parallel: bool = True, optimize: bool = True, **kwargs) -> typing.Any: ...
    def load_string(value: str, parallel: bool = True, optimize: bool = True, **kwargs) -> typing.Any: ...

    def set_log_level(level: LogLevel) -> None: ...
    def log_level() -> LogLevel: ...

    _BoolCp: TypeAlias = Union[bool, 'Bool', 'drjit.auto.Bool', 'drjit.auto.ad.Bool', Annotated[NDArray[numpy.bool_], dict(shape=("*",), order='C', device='cpu')], Sequence[bool]]
    _IntCp: TypeAlias = Union[int, 'Int', 'drjit.auto.Int', 'drjit.auto.ad.Int', Annotated[NDArray[numpy.int32], dict(shape=("*",), order='C', device='cpu')], Sequence[int]]
    _Int32Cp: TypeAlias = Union[int, 'Int32', 'drjit.auto.Int32', 'drjit.auto.ad.Int32', Annotated[NDArray[numpy.int32], dict(shape=("*",), order='C', device='cpu')], Sequence[int]]
    _Int64Cp: TypeAlias = Union[int, 'Int64', 'drjit.auto.Int64', 'drjit.auto.ad.Int64', Annotated[NDArray[numpy.int64], dict(shape=("*",), order='C', device='cpu')], Sequence[int]]
    _UIntCp: TypeAlias = Union[int, 'UInt', 'drjit.auto.UInt', 'drjit.auto.ad.UInt', Annotated[NDArray[numpy.uint32], dict(shape=("*",), order='C', device='cpu')], Sequence[int]]
    _UInt32Cp: TypeAlias = Union[int, 'UInt32', 'drjit.auto.UInt32', 'drjit.auto.ad.UInt32', Annotated[NDArray[numpy.uint32], dict(shape=("*",), order='C', device='cpu')], Sequence[int]]
    _UInt64Cp: TypeAlias = Union[int, 'UInt64', 'drjit.auto.UInt64', 'drjit.auto.ad.UInt64', Annotated[NDArray[numpy.uint64], dict(shape=("*",), order='C', device='cpu')], Sequence[int]]
    _FloatCp: TypeAlias = Union[float, 'Float', 'drjit.auto.Float', 'drjit.auto.ad.Float', Annotated[NDArray[numpy.float32], dict(shape=("*",), order='C', device='cpu')], Sequence[float]]
    _Float16Cp: TypeAlias = Union[float, 'Float16', 'drjit.auto.Float16', 'drjit.auto.ad.Float16', Annotated[NDArray[numpy.float16], dict(shape=("*",), order='C', device='cpu')], Sequence[float]]
    _Float32Cp: TypeAlias = Union[float, 'Float32', 'drjit.auto.Float32', 'drjit.auto.ad.Float32', Annotated[NDArray[numpy.float32], dict(shape=("*",), order='C', device='cpu')], Sequence[float]]
    _Float64Cp: TypeAlias = Union[float, 'Float64', 'drjit.auto.Float64', 'drjit.auto.ad.Float64', Annotated[NDArray[numpy.float64], dict(shape=("*",), order='C', device='cpu')], Sequence[float]]

    _Matrix2fCp: TypeAlias = Union['Matrix2f', 'drjit.auto.ad.Matrix2f', 'drjit.scalar.Matrix2f', Annotated[NDArray[numpy.float32], dict(shape=(2, 2), order='C', device='cpu')], Sequence[Sequence[float]]]
    _Matrix3fCp: TypeAlias = Union['Matrix3f', 'drjit.auto.ad.Matrix3f', 'drjit.scalar.Matrix3f', Annotated[NDArray[numpy.float32], dict(shape=(3, 3), order='C', device='cpu')], Sequence[Sequence[float]]]
    _Matrix4fCp: TypeAlias = Union['Matrix4f', 'drjit.auto.ad.Matrix4f', 'drjit.scalar.Matrix4f', Annotated[NDArray[numpy.float32], dict(shape=(4, 4), order='C', device='cpu')], Sequence[Sequence[float]]]
    _Matrix2f64Cp: TypeAlias = Union['Matrix2f64', 'drjit.auto.ad.Matrix2f64', 'drjit.scalar.Matrix2f64', Annotated[NDArray[numpy.float64], dict(shape=(2, 2), order='C', device='cpu')], Sequence[Sequence[float]]]
    _Matrix3f64Cp: TypeAlias = Union['Matrix3f64', 'drjit.auto.ad.Matrix3f64', 'drjit.scalar.Matrix3f64', Annotated[NDArray[numpy.float64], dict(shape=(3, 3), order='C', device='cpu')], Sequence[Sequence[float]]]
    _Matrix4f64Cp: TypeAlias = Union['Matrix4f64', 'drjit.auto.ad.Matrix4f64', 'drjit.scalar.Matrix4f64', Annotated[NDArray[numpy.float64], dict(shape=(4, 4), order='C', device='cpu')], Sequence[Sequence[float]]]

    _UnpolarizedSpectrumCp: TypeAlias = Union['UnpolarizedSpectrum', 'drjit.auto.ad._FloatCp', 'drjit.scalar._ArrayXfCp', 'drjit.auto._ArrayXfCp', 'drjit.auto.ad._ArrayXf16Cp']
    _SpectrumEntryCp: TypeAlias = Union['SpectrumEntry', '_UnpolarizedSpectrumCp', 'drjit.scalar._ArrayXfCp', 'drjit.auto._ArrayXfCp', 'drjit.auto.ad._ArrayXf16Cp']
    _SpectrumCp: TypeAlias = Union['Spectrum', '_SpectrumEntryCp', 'drjit.scalar._ArrayXfCp', 'drjit.auto._ArrayXfCp', 'drjit.auto.ad._ArrayXf16Cp']

    _TensorXfCp: TypeAlias = Union['TensorXf', float, 'drjit.auto.ad.TensorXf', Annotated[NDArray[numpy.float32], dict(shape=("*",), order='C', device='cpu')], Sequence[float]]
    _TensorXf16Cp: TypeAlias = Union['TensorXf16', float, 'drjit.auto.ad.TensorXf16', Annotated[NDArray[numpy.float16], dict(shape=("*",), order='C', device='cpu')], Sequence[float]]
    _TensorXf32Cp: TypeAlias = Union['TensorXf32', float, 'drjit.auto.ad.TensorXf32', Annotated[NDArray[numpy.float32], dict(shape=("*",), order='C', device='cpu')], Sequence[float]]
    
    FloatStorage: TypeAlias = _FloatCp
    UInt32Storage: TypeAlias = _UInt32Cp
    Int32Storage: TypeAlias = _Int32Cp

    class Bool(drjit.ArrayBase['Bool', '_BoolCp', bool, bool, 'Bool', 'Bool', 'Bool']): pass
    class Int(drjit.ArrayBase['Int', '_IntCp', int, int, 'Int', 'Int', 'Bool']): pass
    class Int32(drjit.ArrayBase['Int32', '_Int32Cp', int, int, 'Int32', 'Int32', 'Bool']): pass
    class Int64(drjit.ArrayBase['Int64', '_Int64Cp', int, int, 'Int64', 'Int64', 'Bool']): pass
    class UInt(drjit.ArrayBase['UInt', '_UIntCp', int, int, 'UInt', 'UInt', 'Bool']): pass
    class UInt32(drjit.ArrayBase['UInt32', '_UInt32Cp', int, int, 'UInt32', 'UInt32', 'Bool']): pass
    class UInt64(drjit.ArrayBase['UInt64', '_UInt64Cp', int, int, 'UInt64', 'UInt64', 'Bool']): pass
    class Float(drjit.ArrayBase['Float', '_FloatCp', float, float, 'Float', 'Float', 'Bool']): pass
    class Float16(drjit.ArrayBase['Float16', '_Float16Cp', float, float, 'Float16', 'Float16', 'Bool']): pass
    class Float32(drjit.ArrayBase['Float32', '_Float32Cp', float, float, 'Float32', 'Float32', 'Bool']): pass
    class Float64(drjit.ArrayBase['Float64', '_Float64Cp', float, float, 'Float64', 'Float64', 'Bool']): pass

    class SpectrumEntry(drjit.ArrayBase['SpectrumEntry', '_SpectrumEntryCp', UnpolarizedSpectrum, '_UnpolarizedSpectrumCp', UnpolarizedSpectrum, 'SpectrumEntry', drjit.auto.ad.ArrayXb]): pass
    class Spectrum(drjit.ArrayBase['Spectrum', '_SpectrumCp', SpectrumEntry, '_SpectrumEntryCp', SpectrumEntry, drjit.auto.ad.ArrayXf, drjit.auto.ad.ArrayXb]): pass
    class UnpolarizedSpectrum(drjit.ArrayBase['UnpolarizedSpectrum', '_UnpolarizedSpectrumCp', drjit.auto.ad.Float, 'drjit.auto.ad._FloatCp', drjit.auto.ad.Float, 'UnpolarizedSpectrum', drjit.auto.ad.ArrayXb]): pass

    class Matrix2f(drjit.ArrayBase['Matrix2f', '_Matrix2fCp', float, float, 'Matrix2f', 'Matrix2f', 'Bool']): pass
    class Matrix3f(drjit.ArrayBase['Matrix3f', '_Matrix3fCp', float, float, 'Matrix3f', 'Matrix3f', 'Bool']): pass
    class Matrix4f(drjit.ArrayBase['Matrix4f', '_Matrix4fCp', float, float, 'Matrix4f', 'Matrix4f', 'Bool']): pass
    class Matrix2f64(drjit.ArrayBase['Matrix2f64', '_Matrix2f64Cp', float, float, 'Matrix2f64', 'Matrix2f64', 'Bool']): pass
    class Matrix3f64(drjit.ArrayBase['Matrix3f64', '_Matrix3f64Cp', float, float, 'Matrix3f64', 'Matrix3f64', 'Bool']): pass
    class Matrix4f64(drjit.ArrayBase['Matrix4f64', '_Matrix4f64Cp', float, float, 'Matrix4f64', 'Matrix4f64', 'Bool']): pass

    class TensorXf(drjit.ArrayBase['TensorXf', '_TensorXfCp', 'TensorXf', '_TensorXfCp', 'TensorXf', 'Float', 'Bool']):
        @overload
        def __init__(self) -> None: ...
        @overload
        def __init__(self, arg: _TensorXfCp, /) -> None: ...
        @overload
        def __init__(self, array: _FloatCp, shape: tuple[int, ...]) -> None: ...
        @property
        def shape(self) -> tuple[int, ...]: ...
        @property
        def array(self) -> Float: ...
        @property
        def ndim(self) -> int: ...

    class TensorXf16(drjit.ArrayBase['TensorXf16', '_TensorXf16Cp', 'TensorXf16', '_TensorXf16Cp', 'TensorXf16', 'Float16', 'Bool']):
        @overload
        def __init__(self) -> None: ...
        @overload
        def __init__(self, arg: _TensorXf16Cp, /) -> None: ...
        @overload
        def __init__(self, array: _Float16Cp, shape: tuple[int, ...]) -> None: ...
        @property
        def shape(self) -> tuple[int, ...]: ...
        @property
        def array(self) -> Float16: ...
        @property
        def ndim(self) -> int: ...

    class TensorXf32(drjit.ArrayBase['TensorXf32', '_TensorXf32Cp', 'TensorXf32', '_TensorXf32Cp', 'TensorXf32', 'Float32', 'Bool']):
        @overload
        def __init__(self) -> None: ...
        @overload
        def __init__(self, arg: _TensorXf32Cp, /) -> None: ...
        @overload
        def __init__(self, array: _Float32Cp, shape: tuple[int, ...]) -> None: ...
        @property
        def shape(self) -> tuple[int, ...]: ...
        @property
        def array(self) -> Float32: ...
        @property
        def ndim(self) -> int: ...

Bitmap.__eq__:
Bitmap.__ne__:

BoundingBox2f.__eq__:
BoundingBox2f.__ne__:

BoundingBox3f.__eq__:
BoundingBox3f.__ne__:

BoundingSphere3f.__eq__:
BoundingSphere3f.__ne__:

DiscreteDistribution.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: 'DiscreteDistribution', /) -> None:
        \doc
    @overload
    def __init__(self, pmf: FloatStorage, /) -> None:
        \doc

ContinuousDistribution.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: 'ContinuousDistribution', /) -> None:
        \doc
    @overload
    def __init__(self, range: _ScalarVector2fCp, pdf: FloatStorage, /) -> None:
        \doc

IrregularContinuousDistribution.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: 'IrregularContinuousDistribution', /) -> None:
        \doc
    @overload
    def __init__(self, nodes: FloatStorage, pdf: FloatStorage, /) -> None:
        \doc

Field.*.__eq__:
Field.*.__ne__:

Frame.*.__eq__:
Frame.*.__ne__:

Properties.__eq__:
Properties.__ne__:

Ray.*.__eq__:
Ray.*.__ne__:

ScalarTransform.*.__eq__:
ScalarTransform.*.__ne__:

Struct.__eq__:
Struct.__ne__:

SurfaceInteraction.*.__eq__:
SurfaceInteraction.*.__ne__:

ScalarPoint.*.__getitem__:
ScalarPoint.*.__delitem__:

ScalarColor.*.__getitem__:
ScalarColor.*.__delitem__:

ScalarNormal.*.__getitem__:
ScalarNormal.*.__delitem__:

Transform.*.__eq__:
Transform.*.__ne__:

Vector.*.__getitem__:
Vector.*.__delitem__:

.*Ptr.Domain:
.*Ptr.__getitem__:
.*Ptr.__delitem__:

# Specific to variant we used to generate stub
is_monochromatic:
is_polarized:
is_rgb:
is_spectral:
UnpolarizedSpectrum:
Spectrum:
_UnpolarizedSpectrumCp:
_SpectrumCp:
_SpectrumEntryCp:

.mueller.specular_reflection:
    @overload
    def specular_reflection(cos_theta_i: mitsuba.Float, eta: drjit.scalar.Complex2f) -> mitsuba.Spectrum:
        \doc
    @overload
    def specular_reflection(cos_theta_i: mitsuba.UnpolarizedSpectrum, eta: typing.Any) -> mitsuba.Spectrum:
        \doc

Stream\.(EBigEndian|ELittleEndian):
    \1: Stream.EByteOrder = ...

Bitmap\.(UInt8|Int8|UInt16|Int16|UInt32|Int32|UInt64|Int64|Float16|Float32|Float64|Invalid):
    \1: Struct.Type = ...

TensorFile.Field.shape:
    def shape(self) -> Sequence[int]: ...

FileStream\.(ERead|EReadWrite|ETruncReadWrite):
    \1: FileStream.EMode = ...

ZStream\.(EDeflateStream|EGZipStream):
    \1: ZStream.EStreamType = ...

Version.__init__:
    @overload
    def __init__(self, value: str) -> None: ...

Scene.sensors_dr:
    def sensors_dr(self) -> list['Sensor']:
        \doc

Scene.emitters_dr:
    def emitters_dr(self) -> list['Emitter']:
        \doc

Scene.shapes_dr:
    def shapes_dr(self) -> list['Shape']:
        \doc

Scene.integrator:
    def integrator(self) -> 'Integrator':
        \doc

.eval_reflectance:
    def eval_reflectance(type: MicrofacetType, alpha_u: float, alpha_v: float, wi: Vector3f, eta: float) -> Float:
        \doc

Mesh.merge:
    def merge(self, other: Mesh) -> Mesh:
        \doc

detail.TransformWrapper:
    class TransformWrapper:
        \doc
        def __init__(self, method_name: str, original_method: typing.Any) -> None: ...
        def __get__(self, obj: typing.Any, objtype: typing.Any = ...) -> typing.Any:  ...
        def __call__(self, *args, **kwargs) -> typing.Any: ...

AffineTransform3([fd]).extract:
    def extract(self) -> typing.Any:
        \doc

AffineTransform3f.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix3fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix3fCp, arg1: _Matrix3fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: AffineTransform3f) -> None:
        \doc
    @overload
    def __init__(self, arg: ProjectiveTransform3f, /) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform3f, /) -> None:
        \doc

ScalarAffineTransform3f.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix3fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix3fCp, arg1: _Matrix3fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform3f) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform3f, /) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc

ProjectiveTransform3f.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix3fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix3fCp, arg1: _Matrix3fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: AffineTransform3f, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ProjectiveTransform3f) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform3f, /) -> None:
        \doc

ScalarProjectiveTransform3f.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix3fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix3fCp, arg1: _Matrix3fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform3f, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform3f) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc

# --- 3d Transforms ---

AffineTransform3d.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix3f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix3f64Cp, arg1: _Matrix3f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: AffineTransform3d) -> None:
        \doc
    @overload
    def __init__(self, arg: ProjectiveTransform3d, /) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform3d, /) -> None:
        \doc

ScalarAffineTransform3d.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix3f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix3f64Cp, arg1: _Matrix3f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform3d) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform3d, /) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc

ProjectiveTransform3d.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix3f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix3f64Cp, arg1: _Matrix3f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: AffineTransform3d, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ProjectiveTransform3d) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform3d, /) -> None:
        \doc

ScalarProjectiveTransform3d.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix3f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix3f64Cp, arg1: _Matrix3f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform3d, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform3d) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc

# --- 4f Transforms ---

AffineTransform4f.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix4fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix4fCp, arg1: _Matrix4fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: AffineTransform4f) -> None:
        \doc
    @overload
    def __init__(self, arg: ProjectiveTransform4f, /) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform4f, /) -> None:
        \doc

ScalarAffineTransform4f.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix4fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix4fCp, arg1: _Matrix4fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform4f) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform4f, /) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc

ProjectiveTransform4f.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix4fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix4fCp, arg1: _Matrix4fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: AffineTransform4f, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ProjectiveTransform4f) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform4f, /) -> None:
        \doc

ScalarProjectiveTransform4f.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix4fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix4fCp, arg1: _Matrix4fCp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform4f, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform4f) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc

AffineTransform4d.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix4f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix4f64Cp, arg1: _Matrix4f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: AffineTransform4d) -> None:
        \doc
    @overload
    def __init__(self, arg: ProjectiveTransform4d, /) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform4d, /) -> None:
        \doc

ScalarAffineTransform4d.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix4f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix4f64Cp, arg1: _Matrix4f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform4d) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform4d, /) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc

ProjectiveTransform4d.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix4f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix4f64Cp, arg1: _Matrix4f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: AffineTransform4d, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ProjectiveTransform4d) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform4d, /) -> None:
        \doc

ScalarProjectiveTransform4d.__init__:
    @overload
    def __init__(self) -> None:
        \doc
    @overload
    def __init__(self, arg: _Matrix4f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg0: _Matrix4f64Cp, arg1: _Matrix4f64Cp, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarAffineTransform4d, /) -> None:
        \doc
    @overload
    def __init__(self, arg: ScalarProjectiveTransform4d) -> None:
        \doc
    @overload
    def __init__(self, arg: Sequence, /) -> None:
        \doc
