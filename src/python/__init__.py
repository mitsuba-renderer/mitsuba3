""" Mitsuba Python extension library """

import types
import sys
import threading
from importlib import import_module as _import, reload as _reload
import drjit as dr
import os

if sys.version_info < (3, 6):
    raise ImportError("Mitsuba requires Python 3.6 or greater.")

if os.name == 'nt':
    # Specify DLL search path for windows (no rpath on this platform..)
    d = __file__
    for i in range(3):
        d = os.path.dirname(d)
    try: # try to use Python 3.8's DLL handling
        os.add_dll_directory(d)
    except AttributeError:  # otherwise use PATH
        os.environ['PATH'] += os.pathsep + d
    del d, i

try:
    _import('mitsuba.mitsuba_ext')
    _tls = threading.local()
except (ImportError, ModuleNotFoundError) as e:
    from .config import PYTHON_EXECUTABLE

    extra_msg = ""

    if 'Symbol not found' in str(e):
        pass
    elif PYTHON_EXECUTABLE != sys.executable:
        extra_msg = ("You're likely trying to use Mitsuba within a Python "
                     "binary (%s) that is different from the one for which "
                     "the native module was compiled (%s).") % (
                         sys.executable, PYTHON_EXECUTABLE)

    exc = ImportError("The 'mitsuba' native modules could not be "
                      "imported. %s" % extra_msg)
    exc.__cause__ = e

    raise exc


class MitsubaModule(types.ModuleType):
    '''
    This class extends Python's builtin 'module' type to dynamically resolve
    elements from multiple sources. We use this to stitch multiple separate
    Mitsuba modules (variants, non-templated parts, python parts) into one
    coherent user-facing module.
    '''
    def __init__(self, name, variant, submodule, doc=None):
        super().__init__(name=name, doc=doc)

        from importlib.machinery import ModuleSpec
        self.__spec__ = ModuleSpec(name, None)
        self.__package__ = name
        self.__path__ = __path__
        self._variant = variant
        self._modules = None
        self._submodule = submodule

    def __getattribute__(self, key):
        global _tls
        variant = super().__getattribute__('_variant')
        modules = super().__getattribute__('_modules')

        if modules is None:
            try:
                modules = (
                    _import('mitsuba.mitsuba_ext'),
                    _import('mitsuba.mitsuba_' + variant + '_ext'),
                    _import('mitsuba.python')
                )
            except ImportError as e:
                if not str(e).startswith('No module named'):
                    raise
                pass

            if modules is None:
                raise ImportError('Mitsuba variant "%s" not found.' % variant)

        # Set the variant of this module as current if necessary
        if variant != getattr(_tls, 'variant', None):
            set_variant(variant)

        # Try default lookup first
        try:
            if not key == '__dict__':
                return super().__getattribute__(key)
        except Exception:
            pass

        try:
            name = super().__getattribute__('__name__')
            submodule = super().__getattribute__('_submodule')
            if key != '__dict__':
                # Walk through loaded extension modules
                for m in modules:
                    if submodule != '':
                        m = getattr(m, submodule, None)
                    result = getattr(m, key, None)
                    if result is not None:
                        return result
            else:
                # Stitch together a dictionary, needed for pydoc
                result = super().__getattribute__('__dict__')
                for m in modules:
                    result.update(getattr(m, '__dict__'))
                return result
        except Exception:
            raise AttributeError('Module \"%s\" has no attribute \"%s\"!' %
                                 (name, key))

    def __setattr__(self, key, value):
        super().__setattr__(key, value)


class MitsubaCurrentModule(types.ModuleType):
    '''
    This class extends Python's builtin 'module' type to dynamically route
    mitsuba imports to the current variant via a virtual mitsuba.current module.
    '''
    def __init__(self, name, submodule, doc=None):
        super().__init__(name=name, doc=doc)

        from importlib.machinery import ModuleSpec
        self.__spec__ = ModuleSpec(name, None)
        self.__package__ = name
        self.__path__ = __path__
        self._submodule = submodule

    def __getattribute__(self, key):
        global _tls
        submodule = super().__getattribute__('_submodule')
        module = sys.modules['mitsuba.' + _tls.variant + submodule]
        return module.__getattribute__(key)

    def __setattr__(self, key, value):
        super().__setattr__(key, value)


submodules = ['', '.warp', '.math', '.spline', '.quad', '.mueller']

# Register the modules
from .config import MI_VARIANTS
for variant in MI_VARIANTS:
    for submodule in submodules:
        name = 'mitsuba.' + variant + submodule
        module = MitsubaModule(name, variant, submodule[1:])
        sys.modules[name] = module
        locals()[variant + submodule] = module

# Register the virtual mitsuba.current modules
for submodule in submodules:
    name = 'mitsuba.current' + submodule
    module = MitsubaCurrentModule(name, submodule)
    sys.modules[name] = module
    locals()['current' + submodule] = module


def set_variant(value):
    '''
    Mitsuba 2 can be compiled to a great variety of different variants (e.g.
    'scalar_rgb', 'cuda_ad_spectral_polarized', etc.) that each have their
    own Python bindings in addition to generic/non-templated code that lives in
    yet another module.

    Writing various different prefixes many times in import statements such as

       from mitsuba.mitsuba_cuda_ad_spectral_ext import Integrator
       from mitsuba.mitsuba_ext import FileStream

    can get rather tiring. For this reason, Mitsuba uses /virtual/ Python
    modules that dynamically resolve import statements to the right
    destination. The desired Mitsuba variant can be specified via this
    function and then elements can be imported via the mitsuba.current virtual
    module. The above example then simplifies to

        import mitsuba
        mitsuba.set_variant('cuda_ad_spectral_polarized')
        ... = mitsuba.current.Integrator(...)
        ... = mitsuba.current.FileStream(...)

    The variant name can be changed at any time and will only apply to future
    imports. The variant name is a per-thread property, hence multiple
    independent threads can execute code in separate variants.
    '''

    if variant() == value:
        return

    if value not in variants():
        raise ImportError('Requested an unsupported variant "%s". The '
                          'following variants are available: %s.' % (
                              value, ", ".join(variants())))

    _tls.variant = value

    # Automatically load/reload and register Python integrators for AD variants
    if False: # TODO resume this when scripts are updated
        if value.startswith(('llvm_', 'cuda_')):
            import sys
            if 'mitsuba.python.ad.integrators' in sys.modules:
                _reload(sys.modules['mitsuba.python.ad.integrators'])
            else:
                _import('mitsuba.python.ad.integrators')
            del sys


def variant():
    'Returns the currently active variant'
    return getattr(_tls, 'variant', None)


def variants():
    'Returns a list of all variants that have been compiled'
    from .config import MI_VARIANTS
    return MI_VARIANTS


# Cleanup
del MitsubaModule
del MitsubaCurrentModule
del types
del threading
del os
del submodules
