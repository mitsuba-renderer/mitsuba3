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

Scalar.*:

# Type alias but specific to variant we used to generate stub
UnpolarizedSpectrum:
Spectrum:

# Specific to variant we used to generate stub
is_monochromatic:
is_polarized:
is_rgb:
is_spectral:

mitsuba.__prefix__:
    class Spectrum(drjit.ArrayBase):
        pass

    class UnpolarizedSpectrum(drjit.ArrayBase):
        pass


.*Cp:
_.*Cp:

.*Ptr.Domain:
.*Ptr.__getitem__:
.*Ptr.__setitem__:
.*Ptr.__delitem__:
