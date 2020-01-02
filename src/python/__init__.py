""" Mitsuba Python extension library """

import types
import sys

if sys.version_info < (3, 5):
    raise ImportError("Mitsuba requires Python 3.5 or greater.")


class MitsubaModule(types.ModuleType):
    '''
    This class extends Python's builtin 'module' type to dynamically resolve
    elements from multiple sources. We use this to stitch multiple separate
    Mitsuba modules (variants and non-templated parts) into one coherent
    user-facing module.
    '''
    def __init__(self, name, doc=None):
        super().__init__(name=name, doc=doc)

        from importlib.machinery import ModuleSpec
        import threading

        tls = threading.local()
        tls.modules = ()
        tls.variant = None
        self.__tls__ = tls
        self.__spec__ = ModuleSpec(name, None)
        self.__package__ = name

    def __getattribute__(self, key):
        tls = super().__getattribute__('__tls__')
        modules = tls.modules

        if key == '__dict__':
            result = super().__getattribute__('__dict__')
            for m in modules:
                result.update(getattr(m, '__dict__'))
            return result

        try:
            return super().__getattribute__(key)
        except Exception:
            pass

        for m in modules:
            if hasattr(m, key):
                return getattr(m, key)

        if tls.variant is None:
            raise AttributeError('Before importing any packages, you must '
                'specify the desired variant of Mitsuba using '
                '\"mitsuba.set_variant(..)\".\nThe following variants are '
                'available: %s.' % (", ".join(variants())))
        else:
            name = super().__getattribute__('__name__')
            raise AttributeError('Module \"%s\" has no attribute \"%s\"!' % (name, key))


    def __setattr__(self, key, value):
        super().__setattr__(key, value)

    def variant(self):
        return self.__tls__.variant

    def set_variant(self, variant):
        if self.__tls__.variant == variant:
            return
        import importlib
        modules = (
            importlib.import_module(self.__name__ + '_ext'),
            importlib.import_module(self.__name__ + '_' + variant + '_ext')
        )
        self.__tls__.modules = modules
        self.__tls__.variant = variant


# Register the modules
core = MitsubaModule('mitsuba.core')
render = MitsubaModule('mitsuba.render')

sys.modules['mitsuba.core'] = core
sys.modules['mitsuba.render'] = render


def set_variant(variant):
    '''
    Mitsuba 2 can be compiled to a great variety of different variants (e.g.
    'scalar_rgb', 'gpu_autodiff_spectral_polarized', etc.) that each have their
    own Python bindings in addition to generic/non-templated code that lives in
    yet another module.

    Writing various different prefixes many times in import statements such as

       from mitsuba.render_gpu_autodiff_spectral_polarized_ext import Integrator
       from mitsuba.core_ext import FileStream

    can get rather tiring. For this reason, Mitsuba uses /virtual/ Python
    modules that dynamically resolve import statements to the right
    destination. The desired Mitsuba variant should be specified via this
    function. The above example then simplifies to

        import mitsuba
        mitsuba.set_variant('gpu_autodiff_spectral_polarized')

        from mitsuba.render import Integrator
        from mitsuba.core import FileStream

    The variant name can be changed at any time and will only apply to future
    imports. The variant name is a per-thread property, hence multiple
    independent threads can execute code in separate variants.
    '''

    try:
        core.set_variant(variant)
        render.set_variant(variant)
    except ImportError as e:
        from .config import PYTHON_EXECUTABLE
        import sys

        extra_msg = ""

        if 'Symbol not found' in str(e):
            pass
        elif PYTHON_EXECUTABLE != sys.executable:
            extra_msg = ("You're likely trying to use Mitsuba within a Python "
            "binary (%s) that is different from the one for which the native "
            "module was compiled (%s).") % (sys.executable, PYTHON_EXECUTABLE)

        exc = ImportError("The 'mitsuba' native modules could not be "
                          "imported. %s" % extra_msg)
        exc.__cause__ = e

        raise exc


def variant():
    return core.variant()


def variants():
    import pkgutil
    import mitsuba

    variants = []
    for importer, modname, ispkg in pkgutil.iter_modules(mitsuba.__path__):
        if len(modname) > 8 and modname.startswith('core_') and \
           modname.endswith('_ext'):
            variants.append(modname[5:-4])

    return variants


# Cleanup
del sys
del MitsubaModule
del types
