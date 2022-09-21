#!/usr/bin/env python
"""
Usage: generate_stub_files.py {dest_dir}

This script generates stub files for Python type information for the `mitsuba`
package.  It recursively traverses the `mitsuba` package and writes all the
objects (classes, methods, functions, enums, etc.) it finds to the `dest_dir`
folder. The stub files contain both the signatures and the docstrings of the
objects.
"""

import os
import inspect
import re
import sys
import logging
import collections.abc

# ------------------------------------------------------------------------------

top_level_objects = {}
buffer = ''

def w(s):
    global buffer
    buffer += f'{s}\n'

# ------------------------------------------------------------------------------

def process_type_hint(s):
    sub = s
    type_hints = []
    offset = 0

    while True:
        match = re.search(r'(\(|,|^)\s*[_a-zA-Z][_a-zA-Z0-9]*:[^:\w]', sub)
        if match is None:
            break

        i = match.start() + len(match.group())
        match_next = re.search(r'(\(|,|^)\s*[_a-zA-Z][_a-zA-Z0-9]*:[^:\w]', sub[i:])

        if match_next is None:
            j = sub.index(')')
        else:
            j = i + match_next.start()

        type_hints.append((offset + i, offset + j, sub[i:j]))

        if match_next is None:
            break

        offset = offset + j
        sub = s[offset:]

    offset = 0
    result = ''
    for t in type_hints:
        result += s[offset:t[0]-2]
        offset = t[1]
        # if the type hint is valid, then add it as well
        if not ('::' in t[2]):
            result += f': {t[2]}'
    result += s[offset:]

    # Check is return type hint is not valid
    if '::' in result[result.index(' -> '):]:
        result = result[:result.index(' -> ')]

    # Remove the specific variant hint
    result = result.replace(f'.{mi.variant()}', '')

    # Fix parameters that have enums as default values
    default_enum_re = re.compile(r'<')
    enum_match = default_enum_re.search(result)
    while enum_match is not None:
        begin = enum_match.end()
        end = begin + result[begin:].index('>')

        new_default_value = result[begin:end]
        if ':' in new_default_value:
            new_default_value = new_default_value[:new_default_value.index(':')]
            result = result[:begin - 1] + new_default_value + result[end + 1:]

        enum_match = default_enum_re.search(result, begin)

    return result

# ------------------------------------------------------------------------------

def process_properties(name, p, indent=0):
    indent = ' ' * indent

    if not p is None:
        w(f'{indent}{name} = ...')
        if not p.__doc__ is None:
            doc = p.__doc__.splitlines()
            if len(doc) == 1:
                w(f'{indent}\"{doc[0]}\"')
            elif len(doc) > 1:
                w(f'{indent}\"\"\"')
                for l in doc:
                    w(f'{indent}{l}')
                w(f'{indent}\"\"\"')

# ------------------------------------------------------------------------------

def process_enums(name, e, indent=0):
    indent = ' ' * indent

    if not e is None:
        w(f'{indent}{name} = {int(e)}')

        if not e.__doc__ is None:
            doc = e.__doc__.splitlines()
            w(f'{indent}\"\"\"')
            for l in doc:
                if l.startswith(f'  {name}'):
                    w(f'{indent}{l}')
            w(f'{indent}\"\"\"')

# ------------------------------------------------------------------------------

def process_class(_, obj, indent=0):
    methods = []
    py_methods = []
    properties = []
    enums = []
    classes = []

    indent_str = ' ' * indent

    for k in dir(obj):
        # Skip private attributes
        if k.startswith('_'):
            continue

        if k.endswith('_'):
            continue

        v = getattr(obj, k)
        if type(v).__name__ == 'instancemethod':
            methods.append((k, v))
        elif type(v).__name__ == 'function' and v.__code__.co_varnames[0] == 'self':
            py_methods.append((k, v))
        elif type(v).__name__ == 'property':
            properties.append((k, v))
        elif str(v).endswith(k):
            enums.append((k, v))
        elif inspect.isclass(v):
            if (hasattr(v, '__module__') and (v.__module__.startswith('mitsuba'))):
                classes.append((k, v))

    base = obj.__bases__[0]
    base_module = base.__module__
    base_name = base.__name__
    has_base = not (base_module == 'builtins' or
                    base_name == 'object' or
                    base_name == 'pybind11_object' or
                    base_module.startswith('drjit'))
    base_name = base_name.replace(f'.{mi.variant()}', '')

    if has_base and not base.__module__.startswith('mitsuba'):
        w(f'{indent_str}from {base.__module__} import {base_name}')

    w(f'{indent_str}class {obj.__name__}{"(" + base_name + ")" if has_base else ""}:')
    if obj.__doc__ is not None:
        doc = obj.__doc__.splitlines()
        if len(doc) > 0:
            if doc[0].strip() == '':
                doc = doc[1:]
            if obj.__doc__:
                w(f'{indent_str}    \"\"\"')
                for l in doc:
                    w(f'{indent_str}    {l}')
                w(f'{indent_str}    \"\"\"')
                w(f'')

    process_function('__init__', obj.__init__, indent=indent + 4)
    process_function('__call__', obj.__call__, indent=indent + 4)

    for k, v in classes:
        process_class(k, v, indent=indent + 4)

    if len(properties) > 0:
        for k, v in properties:
            process_properties(k, v, indent=indent + 4)
        w(f'')

    if len(enums) > 0:
        for k, v in enums:
            process_enums(k, v, indent=indent + 4)
        w(f'')

    for k, v in methods:
        process_function(k, v, indent=indent + 4)

    for k, v in py_methods:
        process_py_function(k, v, indent=indent + 4)

    w(f'{indent_str}    ...')
    w('')

# ------------------------------------------------------------------------------

def process_function(_, obj, indent=0):
    indent = ' ' * indent
    if obj is None or obj.__doc__ is None:
        return

    overloads = []
    for l in obj.__doc__.splitlines():
        if ') -> ' in l:
            l = process_type_hint(l)
            overloads.append((l, []))
        else:
            if len(overloads) > 0:
                overloads[-1][1].append(l)

    for l, doc in overloads:
        has_doc = len(doc) > 1

        # Overload?
        overload_re = re.compile(r'\d+\.')
        overload_match = overload_re.match(l)
        if overload_match is not None:
            w(f"{indent}@overload")
            w(f"{indent}def {l[overload_match.end() + 1:]}:{'' if has_doc else ' ...'}")
        else:
            w(f"{indent}def {l}:{'' if has_doc else ' ...'}")

        if len(doc) > 1: # first line is always empty
            w(f'{indent}    \"\"\"')
            for l in doc[1:]:
                w(f'{indent}    {l}')
            w(f'{indent}    \"\"\"')
            w(f'{indent}    ...')
            w(f'')

# ------------------------------------------------------------------------------

def process_py_function(name, obj, indent=0):
    indent = ' ' * indent
    if obj is None:
        return

    has_doc = obj.__doc__ is not None

    signature = str(inspect.signature(obj))
    signature = signature.replace('mi.', 'mitsuba.')

    # Fix parameters that have enums as default values
    enum_match = re.search(r'\=<', signature)
    while enum_match is not None:
        begin = enum_match.start()
        end = begin + signature[begin:].index('>')

        new_default_value = signature[begin + 2:end]
        new_default_value = new_default_value[:new_default_value.index(':')]

        signature = signature[:begin + 1] + new_default_value + signature[end + 1:]
        enum_match = re.search(r'\=<', signature[begin:])

    w(f"{indent}def {name}{signature}:{'' if has_doc else ' ...'}")

    if has_doc:
        doc = obj.__doc__.splitlines()
        if len(doc) > 0: # first line is always empty
            w(f'{indent}    \"\"\"')
            for l in doc:
                w(f'{indent}    {l.strip()}')
            w(f'{indent}    \"\"\"')
            w(f'{indent}    ...')
            w(f'')

# ------------------------------------------------------------------------------

def process_builtin_type(_, name):
    w(f"class {name}: ...")
    w(f'')

# ------------------------------------------------------------------------------

def process_module(m, top_module=False):
    global buffer

    submodules = []
    buffer = ''

    w('from typing import Any, Callable, Iterable, Iterator, Tuple, List, TypeVar, Union, overload')
    w('import mitsuba')
    w('import mitsuba as mi')
    w('import drjit as dr')
    w('')

    # Ignore initialization errors of invalid variants on this system
    try:
        dir(m)
    except Exception:
        pass

    if m not in top_level_objects:
        top_level_objects[m] = set()

    for k in dir(m):
        v = getattr(m, k)

        # Already seen object
        if isinstance(v, collections.abc.Hashable) and v in top_level_objects[m]:
            continue

        if inspect.isclass(v):
            if (hasattr(v, '__module__') and
                not (v.__module__.startswith('mitsuba') or v.__module__.startswith('drjit'))):
                if v in [bool, int, float]:
                    process_builtin_type(v, k)
                continue
            process_class(k, v)
        elif type(v).__name__ in ['method', 'function']:
            process_py_function(k, v)
        elif type(v).__name__ == 'builtin_function_or_method':
            process_function(k, v)
        elif type(v) in [str, bool, int, float]:
            if k.startswith('_'):
                continue
            process_properties(k, v)
        elif type(v).__bases__[0].__name__ == 'module' or type(v).__name__ == 'module':
            if k in ['mi', 'mitsuba', 'dr']:
                continue
            if not (v.__name__.startswith('mitsuba') or v.__name__.startswith('drjit')):
                continue

            if k.startswith('_'):
                continue

            module_filename = v.__name__[len("mitsuba."):]
            module_filename = module_filename.replace('.', '_')

            w(f'')
            w(f'from {".stubs" if top_module else "."} import {module_filename} as {k}')
            w('')

            submodules.append((module_filename, v))

        if isinstance(v, collections.abc.Hashable):
            top_level_objects[m].add(v)

    # Adjust DrJIT type hints manually here
    buffer = re.sub(r'drjit\.(scalar|llvm|cuda).(ad\.)?' , 'mitsuba.', buffer)

    return buffer, submodules

# ------------------------------------------------------------------------------

if __name__ == '__main__':
    logging.info('Generating stub files for the mitsuba package.')

    if len(sys.argv) < 2:
        raise RuntimeError("Exactly one argument is expected: the output "
                           "directory of the generated stubs")
    stub_folder = sys.argv[1]

    import mitsuba as mi
    mi.set_variant('scalar_rgb')

    os.makedirs(os.path.join(stub_folder, 'stubs'), exist_ok=True)

    logging.debug(f'Processing mitsuba root module')
    buffer, submodules = process_module(mi, top_module=True)
    with open(os.path.join(stub_folder, '__init__.pyi'), 'w') as f:
        f.write(buffer)

    processed_submodules = set()
    while len(submodules) != 0:
        k, v = submodules[0]
        if v in processed_submodules:
            submodules = submodules[1:]
            continue

        logging.debug(f'Processing submodule: {v.__name__}')
        buffer, new_submodules = process_module(v)

        with open(f'{os.path.join(stub_folder, "stubs", k + ".pyi")}', 'w') as f:
            f.write(buffer)

        submodules = submodules[1:] + new_submodules
        processed_submodules.add(v)

    logging.info(f'Done -> stub files written to {os.path.abspath(stub_folder)}')
