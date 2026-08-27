"""Mitsuba detail module - internal implementation details."""

import os
import typing

# Callables that ``mitsuba.set_variant()`` invokes after a variant change. This
# is a dict rather than a set so that callbacks run in registration order.
_variant_callbacks = dict()

# Modules provided by external plugin packages, or ``None`` before the search
_plugin_modules = None


def add_variant_callback(callback: typing.Callable[[typing.Optional[str], str], None]) -> None:
    """
    Register a callable that runs each time the Mitsuba variant changes. It
    receives the arguments ``old_variant: Optional[str], new_variant: str``.
    Registering the same callable several times has no further effect.
    """
    _variant_callbacks.setdefault(callback, None)


def remove_variant_callback(callback: typing.Callable[[typing.Optional[str], str], None]) -> None:
    """Remove the given callable from the set of variant change callbacks."""
    _variant_callbacks.pop(callback, None)


def clear_variant_callbacks() -> None:
    """Remove all variant change callbacks."""
    _variant_callbacks.clear()


def load_plugins() -> None:
    """
    Import or reload the modules of external Mitsuba plugin packages.

    Any installed distribution can extend Mitsuba by declaring entry points
    in the ``mitsuba`` group that name modules containing plugin code:

    .. code-block:: toml

        [project.entry-points.mitsuba]
        my_bsdf = "my_plugins.bsdf"

    Such a module defines its classes and calls ``mitsuba.register_bsdf()``
    and friends at module scope, just like a script would. Mitsuba imports it
    the first time a variant becomes active and reloads it after every
    subsequent variant change, since plugin registration applies to one
    variant at a time.

    Setting the environment variable ``MI_DISABLE_AUTOLOAD`` skips the search.
    """
    global _plugin_modules

    import importlib
    import traceback
    import warnings

    if _plugin_modules is not None:
        for module in _plugin_modules:
            try:
                importlib.reload(module)
            except Exception:
                warnings.warn('Could not reload the Mitsuba plugin module '
                              '"%s":\n%s' % (module.__name__,
                                             traceback.format_exc()),
                              stacklevel=2)
        return

    # Assign first: a plugin module that sets a variant must not recurse here
    _plugin_modules = []

    if os.environ.get('MI_DISABLE_AUTOLOAD'):
        return

    import importlib.metadata

    # Distributions are enumerated in filesystem order, sort for determinism
    for ep in sorted(importlib.metadata.entry_points(group='mitsuba'),
                     key=lambda ep: ep.name):
        try:
            _plugin_modules.append(importlib.import_module(ep.value))
        except Exception:
            warnings.warn('Could not load the Mitsuba plugin "%s" provided by '
                          'the module "%s":\n%s'
                          % (ep.name, ep.value, traceback.format_exc()),
                          stacklevel=2)


class TransformWrapper:
    '''
    Helper functor that wraps Transform3f/Transform4f methods so that the following two
    calling conventions are equivalent:

    - Transform4f().translate().scale()...
    - Transform4f.translate().scale()...
    '''
    def __init__(self, method_name, original_method):
        self.method_name = method_name
        self.original_method = original_method

    def __get__(self, obj, objtype=None):
        if obj is None:
            def wrapper(*args, **kwargs):
                instance = objtype()
                return self.original_method.__get__(instance, objtype)(*args, **kwargs)
            wrapper.__name__ = self.method_name
            return wrapper
        else:
            return self.original_method.__get__(obj, objtype)


def patch_transform(transform_cls):
    methods = ['translate', 'scale', 'rotate', 'perspective',
               'orthographic', 'look_at', 'from_frame', 'to_frame']

    for method_name in methods:
        if hasattr(transform_cls, method_name):
            original = getattr(transform_cls, method_name)
            setattr(transform_cls, method_name, TransformWrapper(method_name, original))
