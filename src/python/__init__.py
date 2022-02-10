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

# Inform the meta path finder of the python folder
__path__.append(__path__[0] + '/python')

class MitsubaVariantModule(types.ModuleType):
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
                )
                super().__setattr__('_modules', modules)
            except ImportError as e:
                if str(e).startswith('No module named'):
                    raise ImportError('Mitsuba variant "%s" not found.' % variant)
                else:
                    raise

        # Try default lookup first
        try:
            if not key == '__dict__':
                return super().__getattribute__(key)
        except Exception:
            pass

        # Try C++ libraries (Python bindings)
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
            pass

        # Try Python extension
        try:
            return _import('mitsuba.python.' + key)
            print(f"m.__name__: {m.__name__}")
            return m
        except Exception:
            pass

        raise AttributeError('Module \"%s\" has no attribute \"%s\"!' %
                             (name, key))

    def __setattr__(self, key, value):
        super().__setattr__(key, value)

    def variant(self):
        return super().__getattribute__('_variant')


class MitsubaModule(types.ModuleType):
    '''
    This class extends Python's builtin 'module' type to dynamically route
    mitsuba imports to the virtual module corresponding to currently enabled
    variant.
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

        # Try default lookup first
        try:
            if not key == '__dict__':
                return super().__getattribute__(key)
        except Exception:
            pass

        if not hasattr(_tls, 'variant'):
            # The variant wasn't set explicitly, we first check if a default
            # variant is set in the config.py file.
            from .config import MI_DEFAULT_VARIANT
            if MI_DEFAULT_VARIANT != '':
                self.set_variant(MI_DEFAULT_VARIANT)
            else:
                raise ImportError('Before importing any packages, you must '
                                  'specify the desired variant of Mitsuba '
                                  'using \"mitsuba.set_variant(..)\".\nThe '
                                  'following variants are available: %s.' % (
                                  ", ".join(self.variants())))

        # Redirect all other imports to the currently enabled variant module.
        submodule = super().__getattribute__('_submodule')
        module = sys.modules['mitsuba.' + self.variant() + submodule]
        return module.__getattribute__(key)

    def __setattr__(self, key, value):
        super().__setattr__(key, value)

    def variant(self):
        return getattr(_tls, 'variant', None)

    def variants(self):
        'Returns a list of all variants that have been compiled'
        from .config import MI_VARIANTS
        return MI_VARIANTS

    def set_variant(self, *args):
        '''
        Set the variant to be used by the `mitsuba.current` module. Multiple variant
        names can be passed to this function and the first one that is supported
        will be set as current variant.
       '''
        value = None
        for v in args:
            if v in self.variants():
                value = v
                break

        if value is None:
            raise ImportError('Requested an unsupported variant "%s". The '
                              'following variants are available: %s.' % (
                                args if len(args) > 1 else args[0],
                                ", ".join(self.variants())))

        _tls.variant = value


submodules = ['', '.warp', '.math', '.spline', '.quad', '.mueller']

# Register the variant modules (and submodules)
from .config import MI_VARIANTS
for variant in MI_VARIANTS:
    for submodule in submodules:
        name = 'mitsuba.' + variant + submodule
        module = MitsubaVariantModule(name, variant, submodule[1:])
        sys.modules[name] = module
        locals()[variant + submodule] = module

# Register the virtual mitsuba module and submodules. This will overwrite the
# real mitsuba module in order to redirect future imports.
for submodule in submodules:
    name = 'mitsuba' + submodule
    module = MitsubaModule(name, submodule)
    sys.modules[name] = module


# Cleanup
del MitsubaModule
del MitsubaVariantModule
del types
del threading
del os
del submodules
