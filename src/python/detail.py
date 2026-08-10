"""Mitsuba detail module - internal implementation details."""

import typing

# Callables that ``mitsuba.set_variant()`` invokes after a variant change
_variant_callbacks = set()


def add_variant_callback(callback: typing.Callable[[typing.Optional[str], str], None]) -> None:
    """
    Register a callable that runs each time the Mitsuba variant changes. It
    receives the arguments ``old_variant: Optional[str], new_variant: str``.
    The callbacks form a set, registering the same callable several times has
    no further effect.
    """
    _variant_callbacks.add(callback)


def remove_variant_callback(callback: typing.Callable[[typing.Optional[str], str], None]) -> None:
    """Remove the given callable from the set of variant change callbacks."""
    _variant_callbacks.discard(callback)


def clear_variant_callbacks() -> None:
    """Remove all variant change callbacks."""
    _variant_callbacks.clear()


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
