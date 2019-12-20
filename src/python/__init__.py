""" Mitsuba Python extension library """

import sys

def set_variant(variant):
    imp_orig = __builtins__.__import__

    if hasattr(imp_orig, 'metadata'):
        imp_orig.metadata.variant = variant
        return

    def mitsuba_import(name, *args, **kwargs):
        prefix_old = 'mitsuba.'
        if name.startswith(prefix_old):
            prefix_new = prefix_old + mitsuba_import.metadata.variant + '.'
            if not name.startswith(prefix_new):
                name = prefix_new + name[len(prefix_old):]
        return imp_orig(name, *args, **kwargs)

    import threading
    mitsuba_import.metadata = threading.local()
    mitsuba_import.metadata.variant = variant

    __builtins__.__import__ = mitsuba_import

try:
    from . import core, render
except ImportError as e:
    from .config import PYTHON_EXECUTABLE

    if PYTHON_EXECUTABLE != sys.executable:
        extra_msg = ("You're likely trying to use Mitsuba within a Python "
        "binary (%s) that is different from the one for which the native "
        "module was compiled (%s).") % (sys.executable, PYTHON_EXECUTABLE)
    else:
        extra_msg = ""

    exc = ImportError("The 'mitsuba' native modules could not be "
                      "imported. %s" % extra_msg)
    exc.__cause__ = e

    raise exc
