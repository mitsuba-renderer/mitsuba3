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
    from .python import (
        ad as ad,
        chi2 as chi2,
        math_py as math_py,
        util as util,
        xml as xml
    )

    _UnpolarizedSpectrumCp: TypeAlias = Union['UnpolarizedSpectrum', 'drjit.auto.ad._FloatCp', 'drjit.scalar._ArrayXfCp', 'drjit.auto._ArrayXfCp', 'drjit.auto.ad._ArrayXf16Cp']

    _Spectrum_vtCp: TypeAlias = Union['Spectrum_vt', '_UnpolarizedSpectrumCp', 'drjit.scalar._ArrayXfCp', 'drjit.auto._ArrayXfCp', 'drjit.auto.ad._ArrayXf16Cp']

    _SpectrumCp: TypeAlias = Union['Spectrum', '_Spectrum_vtCp', 'drjit.scalar._ArrayXfCp', 'drjit.auto._ArrayXfCp', 'drjit.auto.ad._ArrayXf16Cp']

    class Spectrum_vt(drjit.ArrayBase['Spectrum_vt', '_Spectrum_vtCp', UnpolarizedSpectrum, '_UnpolarizedSpectrumCp', UnpolarizedSpectrum, 'Spectrum_vt', drjit.auto.ad.ArrayXb]):
        pass

    class Spectrum(drjit.ArrayBase['Spectrum', '_SpectrumCp', Spectrum_vt, '_Spectrum_vtCp', Spectrum_vt, drjit.auto.ad.ArrayXf, drjit.auto.ad.ArrayXb]):
        pass

    class UnpolarizedSpectrum(drjit.ArrayBase['UnpolarizedSpectrum', '_UnpolarizedSpectrumCp', drjit.auto.ad.Float, 'drjit.auto.ad._FloatCp', drjit.auto.ad.Float, 'UnpolarizedSpectrum', drjit.auto.ad.ArrayXb]):
        pass

Bitmap.__eq__:
Bitmap.__ne__:

BoundingBox2f.__eq__:
BoundingBox2f.__ne__:

BoundingBox3f.__eq__:
BoundingBox3f.__ne__:

BoundingSphere3f.__eq__:
BoundingSphere3f.__ne__:

DirectionSample3f.__setitem__:

Field.*.__eq__:
Field.*.__ne__:

Frame.*.__eq__:
Frame.*.__ne__:

Interaction3f.assign:
Interaction3f.__setitem__:

MediumInteraction.*.assign:

PositionSample3f.assign:
PositionSample3f.__setitem__:

Properties.__eq__:
Properties.__ne__:

Ray.*.__eq__:
Ray.*.__ne__:
Ray.*.assign:

RayDifferential.*.__setitem__:

ScalarTransform.*.__eq__:
ScalarTransform.*.__ne__:

Struct.__eq__:
Struct.__ne__:

SurfaceInteraction.*.__eq__:
SurfaceInteraction.*.__ne__:

ScalarPoint.*.__getitem__:
ScalarPoint.*.__setitem__:
ScalarPoint.*.__delitem__:

ScalarColor.*.__getitem__:
ScalarColor.*.__setitem__:
ScalarColor.*.__delitem__:

ScalarNormal.*.__getitem__:
ScalarNormal.*.__setitem__:
ScalarNormal.*.__delitem__:

Transform.*.__eq__:
Transform.*.__ne__:

Vector.*.__getitem__:
Vector.*.__setitem__:
Vector.*.__delitem__:

.*Ptr.Domain:
.*Ptr.__getitem__:
.*Ptr.__setitem__:
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
_Spectrum_vtCp: