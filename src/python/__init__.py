""" Mitsuba Python extension library """

import sys as _sys
import os as _os
import types as _types
import typing as _typing
import importlib as _importlib
import drjit as _dr

# Also search the 'python' directory, which makes 'mitsuba.foo' resolve to
# 'mitsuba/python/foo' for submodules without a variant-specific counterpart
__path__.append(_os.path.join(__path__[0], 'python'))

# The native extensions locate Dr.Jit's shared libraries via relative rpath
# entries, which assume that 'drjit' is installed right next to 'mitsuba'.
if _os.name != 'nt':
    _drjit_expected_loc = _os.path.realpath(_os.path.join(__path__[0], '..', 'drjit'))
    _drjit_loc = _os.path.realpath(_dr.__path__[0])
    if _drjit_expected_loc != _drjit_loc:
        import logging as _logging
        _logging.warning("The `mitsuba` package relies on `drjit` and needs it "
                         "to be installed at a specific location. Currently, "
                         "`drjit` is located at \"%s\" when it is expected to be "
                         "at \"%s\". This can happen when both packages are not "
                         "installed in the same Python environment. You will very "
                         "likely experience linking issues if you do not fix this."
                         % (_drjit_loc, _drjit_expected_loc))
    del _drjit_expected_loc, _drjit_loc

from . import config

if _dr.__version__ != config.DRJIT_VERSION_REQUIREMENT:
    raise ImportError("You are using an incompatible version of `drjit`. "
                      "Only version \"%s\" is guaranteed to be compatible with "
                      "your current Mitsuba installation. Please update your "
                      "Python packages for `drjit` and/or `mitsuba`."
                      % (config.DRJIT_VERSION_REQUIREMENT))

# Import the detail module before the native extensions
from . import detail

# Name of the currently active variant
_variant = None

# Maps variant names to the associated modules, or None before their import
_variant_modules = dict.fromkeys(config.MI_VARIANTS)


def variant() -> _typing.Optional[str]:
    """Return the name of the current variant, or None when none is active"""
    return _variant


def variants() -> _typing.List[str]:
    """Return the list of variants provided by this Mitsuba build"""
    return list(_variant_modules)


def _variant_module(name: str) -> _types.ModuleType:
    """Import the extension module implementing the variant ``name``"""
    module = _variant_modules[name]
    if module is None:
        with _dr.detail.scoped_rtld_deepbind():
            module = _importlib.import_module('mitsuba.' + name)
        _variant_modules[name] = module
    return module


def _import_symbols(module: _types.ModuleType) -> None:
    """
    Copy the public symbols of ``module`` into the mitsuba namespace and
    register contained submodules in ``sys.modules``, which makes them
    importable as ``mitsuba.<submodule>``.
    """
    g = globals()
    for k, v in module.__dict__.items():
        # Skip entries such as __name__, __file__, or __path__. They describe
        # the source module and would clobber this package's own metadata.
        if k.startswith('__') or k.endswith('__'):
            continue
        g[k] = v
        if isinstance(v, _types.ModuleType) and \
           v.__name__.startswith('mitsuba.'):
            _sys.modules[v.__name__] = v


def set_variant(*args: str) -> None:
    """
    Activate a variant. When several names are given, the function tries them
    in order and selects the first one that loads successfully.
    """
    global _variant

    valid = [name for name in args if name in _variant_modules]
    if not valid:
        raise ImportError('Requested an unsupported variant "%s". The '
                          'following variants are available: %s.'
                          % (', '.join(args), ', '.join(variants())))

    old_variant = _variant
    for name in valid:
        if name == old_variant:
            break

        try:
            module = _variant_module(name)
        except ImportError as e:
            # A variant can fail to load when its backend is unusable. Move to
            # the next candidate when there is one.
            if name == valid[-1]:
                raise
            # Log and LogLevel arrive via the mitsuba_ext import below
            Log(LogLevel.Debug,
                'The requested variant "%s" could not be loaded, attempting '
                'the next one. The exception was:\n%s\n' % (name, e))
            continue

        _import_symbols(module)
        _variant = name
        break

    if _variant != old_variant:
        # AD integrators and loaders subclass variant-specific types and must
        # be re-imported whenever a JIT variant becomes active
        if _variant.startswith(('llvm_', 'cuda_', 'metal_')):
            for module_name in ('mitsuba.python.ad.integrators',
                                'mitsuba.python.ad.loaders'):
                _importlib.reload(_importlib.import_module(module_name))

        # Invoke user-provided callbacks once the modules above have reloaded
        for callback in list(detail._variant_callbacks):
            callback(old_variant, _variant)


def __getattr__(name: str) -> _types.ModuleType:
    """Resolve variant-specific attributes such as ``mitsuba.scalar_rgb``"""
    if name in _variant_modules:
        return _variant_module(name)

    if _variant is None:
        raise AttributeError(
            "Cannot access 'mitsuba.%s' before setting a variant. Please "
            "call `mitsuba.set_variant('variant_name')` first. For example: "
            "mitsuba.set_variant('scalar_rgb') or "
            "mitsuba.set_variant('cuda_ad_rgb'). Use mitsuba.variants() to "
            "see all available variants." % name)

    raise AttributeError("module 'mitsuba' has no attribute '%s'" % name)


with _dr.detail.scoped_rtld_deepbind():
    from . import mitsuba_ext

__version__ = mitsuba_ext.__version__
_import_symbols(mitsuba_ext)

from . import python
_import_symbols(python)


if _os.environ.get('NB_STUBGEN'):
    # Score and set the variant with the largest API surface for stub generation
    _S = {'scalar': 1, 'llvm': 200, 'cuda': 300, 'mono': 10, 'rgb': 20,
          'spectral': 30, 'polarized': 100}
    set_variant(max(variants(),
                    key=lambda v: sum(s for f, s in _S.items() if f in v)))
