""" Mitsuba Python extension library """

import sys
import types

#def set_variant(variant):
#    '''
#    Mitsuba 2 can be compiled to a great variety of different variants
#    (e.g. 'scalar_rgb', 'gpu_autodiff_spectral_polarized') that each
#    have their own Python bindings.
#
#    Writing this prefix many times in import statements such as
#
#       import mitsuba.gpu_autodiff_spectral_polarized.render import Integrator
#
#    can get rather tiring. This convenience function modifies Python's
#    import statement so that it automatically inserts the variant name
#    into any later import statements from the mitsuba package.
#
#        import mitsuba
#        mitsuba.set_variant('gpu_autodiff_spectral_polarized')
#
#        import mitsuba.render import Integrator
#
#    The variant name can be changed at any time and only applies
#    to the current thread.
#    '''
#
#    tls.variant = variant
#
#    __import__("mitsuba.core_")

class MitsubaModule(types.ModuleType):
    def __init__(self, name, doc=None):
        super().__init__(name=name, doc=doc)

        from importlib.machinery import ModuleSpec
        import threading

        tls = threading.local()
        tls.modules = ()
        self.__tls__ = tls
        self.__spec__ = ModuleSpec(name, None)
        self.__package__ = name

    def __getattribute__(self, key):
        modules = super().__getattribute__('__tls__').modules

        if key == '__dict__':
            result = super().__getattribute__('__dict__')
            for m in modules:
                result.update(getattr(m, '__dict__'))
            return result

        try:
            return super().__getattribute__(key)
        except:
            pass

        for m in modules:
            if hasattr(m, key):
                return getattr(m, key)

        raise AttributeError('module has no attribute %s' % key)

    def __setattr__(self, key, value):
        super().__setattr__(key, value)

    def set_variant(self, variant):
        import importlib
        modules = (
            importlib.import_module(self.__name__ + '_ext'),
            importlib.import_module(self.__name__ + '_' + variant + '_ext')
        )

        self.__tls__.modules = modules

core = MitsubaModule('mitsuba.core')
# render = MitsubaModule('mitsuba.render')

def set_variant(variant):
    core.set_variant(variant)
    # render.set_variant(variant)

sys.modules['mitsuba.core'] = core
# sys.modules['mitsuba.render'] = render

# Cleanup

del types
del sys
del MitsubaModule

try:
    pass
except ImportError as e:
    from .config import PYTHON_EXECUTABLE
    import sys

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
