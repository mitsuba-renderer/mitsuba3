""" Mitsuba Python extension library """

import sys as _sys
import os as _os
import drjit as _dr
import logging
import functools as _functools
import importlib as _importlib
import inspect as _inspect

if _sys.version_info < (3, 9):
    raise ImportError("Mitsuba requires Python 3.9 or greater.")

mi_dir = _os.path.dirname(_os.path.realpath(__file__))
drjit_expected_loc = _os.path.realpath(_os.path.join(mi_dir, "..", "drjit"))
drjit_loc = _os.path.realpath(_dr.__path__[0])
if _os.name != 'nt' and drjit_expected_loc != drjit_loc:
    logging.warning("The `mitsuba` package relies on `drjit` and needs it "
                    "to be installed at a specific location. Currently, "
                    "`drjit` is located at \"%s\" when it is expected to be "
                    "at \"%s\". This can happen when both packages are not "
                    "installed in the same Python environment. You will very "
                    "likely experience linking issues if you do not fix this."
                    % (drjit_loc, drjit_expected_loc))
del mi_dir, drjit_expected_loc, drjit_loc

from .config import DRJIT_VERSION_REQUIREMENT
if _dr.__version__ != DRJIT_VERSION_REQUIREMENT:
    raise ImportError("You are using an incompatible version of `drjit`. "
                      "Only version \"%s\" is guaranteed to be compatible with "
                      "your current Mitsuba installation. Please update your "
                      "Python packages for `drjit` and/or `mitsuba`."
                      % (DRJIT_VERSION_REQUIREMENT))
del DRJIT_VERSION_REQUIREMENT

# Import detail module before native extensions
from . import detail

with _dr.detail.scoped_rtld_deepbind():
    # Replaces 'mitsuba' in sys.modules with itself (mitsuba_alias)
    from . import mitsuba_alias

class _ScalarFloat(float):
    """Scalar variant ``mi.Float`` accepting vector-style constructor syntax."""

    _mitsuba_scalar_float = True

    def __new__(cls, *values):
        if len(values) == 0:
            value = 0.0
        elif len(values) == 1 and isinstance(values[0], (list, tuple)):
            value = values[0][0] if values[0] else 0.0
        else:
            value = values[0]
        return float.__new__(cls, value)


def _ScalarBool(*values):
    """Scalar variant ``mi.Bool`` accepting vector-style constructor syntax."""

    if len(values) == 0:
        return False
    if len(values) == 1 and isinstance(values[0], (list, tuple)):
        return bool(values[0][0]) if values[0] else False
    return bool(values[0])


_ScalarBool._mitsuba_scalar_bool = True


def _mitsuba_field_args_len(args):
    if args is None:
        return 0
    try:
        return len(args)
    except TypeError:
        return 1


def _mitsuba_wrap_field_method(fn, method_name):
    """Wrap Python field overrides with the same argument validation as C++."""

    if getattr(fn, "_mitsuba_args_checked", False):
        return fn

    try:
        parameters = _inspect.signature(fn).parameters
        accepts_field_args = "args" in parameters
    except (TypeError, ValueError):
        accepts_field_args = True

    @_functools.wraps(fn)
    def wrapper(self, *args, **kwargs):
        if not accepts_field_args:
            if method_name == "eval_n" and len(args) >= 4:
                field_args = args[2]
                if _mitsuba_field_args_len(field_args) != 0:
                    raise RuntimeError(
                        "Field argument args_dim mismatch "
                        f"(expected 0, got {_mitsuba_field_args_len(field_args)})."
                    )
                return fn(self, args[0], args[1], *args[3:], **kwargs)
            elif method_name != "eval_n" and len(args) >= 3:
                field_args = args[1]
                if _mitsuba_field_args_len(field_args) != 0:
                    raise RuntimeError(
                        "Field argument args_dim mismatch "
                        f"(expected 0, got {_mitsuba_field_args_len(field_args)})."
                    )
                return fn(self, args[0], *args[2:], **kwargs)
            return fn(self, *args, **kwargs)

        field_args = kwargs.get("args", None)
        if "args" not in kwargs:
            if method_name == "eval_n" and len(args) >= 3:
                field_args = args[2]
            elif method_name != "eval_n" and len(args) >= 2:
                field_args = args[1]

        expected = int(self.args_dim())
        actual = _mitsuba_field_args_len(field_args)
        if actual != expected:
            raise RuntimeError(
                f"Field argument args_dim mismatch "
                f"(expected {expected}, got {actual})."
            )
        return fn(self, *args, **kwargs)

    wrapper._mitsuba_args_checked = True
    return wrapper


def _mitsuba_patch_field_class(mi):
    """Install argument validation on future Python ``Field`` subclasses."""

    field_cls = getattr(mi, "Field", None)
    if field_cls is None or getattr(field_cls, "_mitsuba_args_patch", False):
        return

    eval_methods = (
        "eval", "eval_1", "eval_color3", "eval_array2", "eval_array3",
        "eval_spec", "eval_array6", "eval_n"
    )
    previous_init_subclass = field_cls.__dict__.get("__init_subclass__", None)

    @classmethod
    def __init_subclass__(cls, **kwargs):
        if previous_init_subclass is not None:
            previous_init_subclass.__get__(cls, cls)(**kwargs)
        else:
            super(field_cls, cls).__init_subclass__(**kwargs)

        for name in eval_methods:
            method = cls.__dict__.get(name, None)
            if method is not None:
                setattr(cls, name, _mitsuba_wrap_field_method(method, name))

    field_cls.__init_subclass__ = __init_subclass__
    field_cls._mitsuba_args_patch = True


def _mitsuba_register_python_fields(mi):
    if mi.variant() is None:
        return
    fields = _importlib.import_module("mitsuba.python.fields")
    fields._register_variant_fields(mi)


def _mitsuba_patch_variant_aliases(mi):
    """Reapply Python-side compatibility patches after each variant switch."""

    _mitsuba_patch_field_class(mi)


if not hasattr(mitsuba_alias, "_mitsuba_original_set_variant"):
    mitsuba_alias._mitsuba_original_set_variant = mitsuba_alias.set_variant

    def _mitsuba_set_variant(*args):
        mitsuba_alias._mitsuba_original_set_variant(*args)
        _mitsuba_patch_variant_aliases(mitsuba_alias)
        _mitsuba_register_python_fields(mitsuba_alias)

    mitsuba_alias.set_variant = _mitsuba_set_variant
    _mitsuba_patch_variant_aliases(mitsuba_alias)
    _mitsuba_register_python_fields(mitsuba_alias)

_ = mitsuba_alias # Removes unused variable warnings
