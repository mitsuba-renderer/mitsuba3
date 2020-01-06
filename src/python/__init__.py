""" Mitsuba Python extension library """

import types
import sys
import threading
from importlib import import_module as _import_module

if sys.version_info < (3, 5):
    raise ImportError("Mitsuba requires Python 3.5 or greater.")

try:
    _import_module('mitsuba.core_ext')
    _import_module('mitsuba.render_ext')
    _tls = threading.local()
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
        self.__spec__ = ModuleSpec(name, None)
        self.__package__ = name
        self.__path__ = __path__

    def __getattribute__(self, key):
        global _tls
        # Try default lookup first
        try:
            if not key == '__dict__':
                return super().__getattribute__(key)
        except Exception:
            pass

        try:
            name = super().__getattribute__('__name__')
            modules = _tls.modules.get(name, ())

            if key != '__dict__':
                if modules:
                    # Walk through loaded extension modules
                    for m in modules:
                        result = getattr(m, key, None)
                        if result is not None:
                            return result
                else:
                    item = _import__('mitsuba')
                    for n in (name + '.' + key).split('.')[1:]:
                        item = getattr(item, n)
                    return item
            else:
                # Stitch together a dictionary, needed for pydoc
                result = super().__getattribute__('__dict__')
                for m in modules:
                    result.update(getattr(m, '__dict__'))
                return result
        except Exception as e:
            pass

        if not hasattr(_tls, 'variant'):
            raise AttributeError('Before importing any packages, you '
                'must specify the desired variant of Mitsuba using '
                '\"mitsuba.set_variant(..)\".\nThe following variants '
                'are available: %s.' % (", ".join(variants())))
        else:
            raise AttributeError('Module \"%s\" has no attribute \"%s\"!' %
                                 (name, key))

    def __setattr__(self, key, value):
        super().__setattr__(key, value)

    def variant(self):
        return self._tls.variant


# Register the modules
core   = MitsubaModule('mitsuba.core')
render = MitsubaModule('mitsuba.render')

sys.modules['mitsuba.core'] = core
sys.modules['mitsuba.render'] = render

# Submodules
sys.modules['mitsuba.core.xml'] = MitsubaModule('mitsuba.core.xml')
sys.modules['mitsuba.core.warp'] = MitsubaModule('mitsuba.core.warp')
sys.modules['mitsuba.core.math'] = MitsubaModule('mitsuba.core.math')
sys.modules['mitsuba.core.spline'] = MitsubaModule('mitsuba.core.spline')
sys.modules['mitsuba.render.mueller'] = MitsubaModule('mitsuba.render.mueller')


def set_variant(value):
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

    if variant() == value:
        return

    _tls.modules = {
        'mitsuba.core': (_import_module('mitsuba.core_ext'),
                         _import_module('mitsuba.core_' + value + '_ext')),
        'mitsuba.render': (_import_module('mitsuba.render_ext'),
                           _import_module('mitsuba.render_' + value + '_ext'))
    }
    _tls.variant = value


def variant():
    'Returns the currently active variant'
    return getattr(_tls, 'variant', None)


def variants():
    'Returns a list of all variants that have been compiled'
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
del threading
