""" Mitsuba Python extension library """

import typing
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
    _tls.cache = {}
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

# Known submodules that will be directly accessible from the mitsuba package
submodules = ['warp', 'math', 'spline', 'quad', 'mueller', 'util', 'filesystem']

# Inform the meta path finder of the python folder
__path__.append(__path__[0] + '/python')

class MitsubaVariantModule(types.ModuleType):
    '''
    This class extends Python's builtin 'module' type to dynamically resolve
    elements from multiple sources. We use this to stitch multiple separate
    Mitsuba modules (variants, non-templated parts, python parts) into one
    coherent user-facing module.
    '''
    def __init__(self, name, variant, submodule=None, doc=None):
        super().__init__(name=name, doc=doc)

        from importlib.machinery import ModuleSpec
        self.__spec__ = ModuleSpec(name, None)
        self.__package__ = name
        self.__path__ = __path__
        self.__file__ = __file__
        self._variant = variant
        self._modules = None
        self._submodule = submodule

    def __getattribute__(self, key):
        global _tls
        variant = super().__getattribute__('_variant')
        modules = super().__getattribute__('_modules')

        # print('get V', key, variant)

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

        submodule = super().__getattribute__('_submodule')
        sub_suffix = '' if submodule is None else f'.{submodule}'

        # Try C++ libraries (Python bindings)
        if key != '__dict__':
            # Walk through loaded extension modules
            for m in modules:
                if not submodule is None:
                    m = getattr(m, submodule, None)
                result = getattr(m, key, None)
                if result is not None:
                    return result
        else:
            # Stitch together a dictionary, needed for pydoc
            result = super().__getattribute__('__dict__')
            for m in modules:
                if not submodule is None:
                    m = getattr(m, submodule, None)
                if m is not None:
                    for k, v in getattr(m, '__dict__').items():
                        if k not in result:
                            result[k] = v

                # Search python modules as well
                try:
                    py_m = _import(f'mitsuba.python{sub_suffix}')
                    for k, v in getattr(py_m, '__dict__').items():
                        if not k.startswith('_') and k not in result:
                            result[k] = v
                except Exception:
                    pass

            return result

        # Try Python extension (objects)
        try:
            return getattr(_import(f'mitsuba.python{sub_suffix}'), key)
        except Exception:
            pass

        # Try Python extension (modules)
        if submodule is None:
            try:
                return _import(f'mitsuba.python.{key}')
            except Exception:
                pass

        raise AttributeError('Module \"%s\" has no attribute \"%s\"!' %
                             (super().__getattribute__('__name__'), key))

    def __setattr__(self, key, value):
        super().__setattr__(key, value)

    def variant(self) -> str:
        'Return currently enabled variant'
        return super().__getattribute__('_variant')


class MitsubaModule(types.ModuleType):
    '''
    This class extends Python's builtin 'module' type to dynamically route
    mitsuba imports to the virtual module corresponding to currently enabled
    variant.
    '''
    def __init__(self, name, submodule=None, doc=None):
        super().__init__(name=name, doc=doc)

        from importlib.machinery import ModuleSpec
        self.__spec__ = ModuleSpec(name, None)
        self.__package__ = name
        self.__path__ = __path__
        self.__file__ = __file__
        self._submodule = submodule

        global _tls
        _tls.cache = {}
        _tls.variant = None

    def __getattribute__(self, key):
        global _tls

        submodule = super().__getattribute__('_submodule')
        variant = getattr(_tls, 'variant', None)

        # Try lookup the cache first
        try:
            result = _tls.cache.get((variant, submodule, key))
            if result is not None:
                return result
        except Exception:
            pass

        # Try default lookup
        try:
            if not key == '__dict__':
                return super().__getattribute__(key)
        except Exception:
            pass

        # Check whether we are trying to directly import a variant
        from .config import MI_VARIANTS
        if key in MI_VARIANTS:
            return sys.modules[f'mitsuba.{key}']

        if not key == '__dict__' and variant is None:
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

        if not variant is None:
            # Check whether we are importing a known submodule
            if submodule is None and key in submodules:
                return sys.modules[f'mitsuba.{variant}.{key}']

            # Redirect all other imports to the currently enabled variant module.
            sub_suffix = '' if submodule is None else f'.{submodule}'
            module = sys.modules[f'mitsuba.{variant}{sub_suffix}']
            result = module.__getattribute__(key)

        # Add set_variant(), variant() and variant modules to the __dict__
        if submodule is None and key == '__dict__':
            if variant is None:
                result = super().__getattribute__(key)
            result['set_variant'] = super().__getattribute__('set_variant')
            result['variant']  = super().__getattribute__('variant')
            result['variants'] = super().__getattribute__('variants')
            for v in super().__getattribute__('variants')():
                result[v] = sys.modules[f'mitsuba.{v}']

        # Add this lookup to the cache
        _tls.cache[(variant, submodule, key)] = result

        return result

    def __setattr__(self, key, value):
        super().__setattr__(key, value)

    def variant(self) -> str:
        'Return currently enabled variant'
        global _tls
        return getattr(_tls, 'variant', None)

    def variants(self) -> typing.List[str]:
        'Return a list of all variants that have been compiled'
        from .config import MI_VARIANTS
        return MI_VARIANTS

    def set_variant(self, *args) -> None:
        '''
        Set the variant to be used by the `mitsuba` module. Multiple variant
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

        global _tls
        if getattr(_tls, 'variant', None) == value:
            return

        _tls.variant = value

        # Automatically load/reload and register Python integrators for AD variants
        if value.startswith(('llvm_', 'cuda_')):
            import sys
            if 'mitsuba.ad.integrators' in sys.modules:
                _reload(sys.modules['mitsuba.ad.integrators'])
            else:
                _import('mitsuba.ad.integrators')
            del sys

# Check whether we are reloading the mitsuba module
reload = f'mitsuba.{submodules[0]}' in sys.modules
if reload:
    print(
        "The Mitsuba module was reloaded (imported a second time). "
        "This can in some cases result in small but subtle issues "
        "and is discouraged."
    )

# Register the variant modules
from .config import MI_VARIANTS
for variant in MI_VARIANTS:
    name = f'mitsuba.{variant}'
    if reload:
        sys.modules[name].__init__(name, variant)
    else:
        sys.modules[name] = MitsubaVariantModule(name, variant)

# Register variant submodules
for variant in MI_VARIANTS:
    for submodule in submodules:
        name = f'mitsuba.{variant}.{submodule}'
        if reload:
            sys.modules[name].__init__(name, variant, submodule)
        else:
            sys.modules[name] = MitsubaVariantModule(name, variant, submodule)

# Register the virtual mitsuba module and submodules. This will overwrite the
# real mitsuba module in order to redirect future imports.
if reload:
    sys.modules['mitsuba'].__init__('mitsuba')
else:
    sys.modules['mitsuba'] = MitsubaModule('mitsuba')

for submodule in submodules:
    name = f'mitsuba.{submodule}'
    if reload:
        sys.modules[name].__init__(name, submodule)
    else:
        sys.modules[name] = MitsubaModule(name, submodule)


# Cleanup
del MitsubaModule
del MitsubaVariantModule
del typing, types
del threading
del os
del config
del MI_VARIANTS
del variant, submodule, name, reload
