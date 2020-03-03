#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Mitsuba 2 documentation build configuration file
#
# The documentation can be built by invoking "make mkdoc"
# from the build directory

import sys
import os
from os.path import join, realpath, dirname
import shlex
import subprocess
import guzzle_sphinx_theme

from pathlib import Path

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#sys.path.insert(0, os.path.abspath('.'))

# -- General configuration ------------------------------------------------

# If your documentation needs a minimal Sphinx version, state it here.
needs_sphinx = '2.4'


# Add any paths that contain templates here, relative to this directory.
# templates_path = ['_templates']

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
# source_suffix = ['.rst', '.md']
source_suffix = '.rst'

rst_prolog = r"""
.. role:: paramtype

.. role:: monosp

.. |spectrum| replace:: :paramtype:`spectrum`
.. |texture| replace:: :paramtype:`texture`
.. |float| replace:: :paramtype:`float`
.. |bool| replace:: :paramtype:`boolean`
.. |int| replace:: :paramtype:`integer`
.. |false| replace:: :monosp:`false`
.. |true| replace:: :monosp:`true`
.. |string| replace:: :paramtype:`string`
.. |bsdf| replace:: :paramtype:`bsdf`
.. |point| replace:: :paramtype:`point`
.. |transform| replace:: :paramtype:`transform`

.. |enoki| replace:: :monosp:`enoki`
.. |numpy| replace:: :monosp:`numpy`

.. |nbsp| unicode:: 0xA0
   :trim:

"""

# The encoding of source files.
#source_encoding = 'utf-8-sig'

# The master toctree document.
master_doc = 'list_api'

# General information about the project.
project = 'mitsuba2'
copyright = '2019, Realistic Graphics Lab (RGL), EPFL'
author = 'Realistic Graphics Lab, EPFL'


# -- Options for autodoc ----------------------------------------------

extensions = []
extensions.append('sphinx.ext.autodoc')

# extensions.append('sphinx.ext.autodoc.typehints')
# autodoc_typehints = 'description'

autodoc_default_options = {
    'members': True
}

autoclass_content = 'both'
autodoc_member_order = 'bysource'

# Set Mitsuba variant for autodoc
import mitsuba
mitsuba.set_variant('scalar_rgb')
import mitsuba.python

# -- Event callback for processing the docstring ----------------------------------------------

import re

# Default string for members with no description
param_no_descr_str = "*no description available*"
# Default string for 'active' function parameter
active_descr_str = "Mask to specify active lanes."
# List of function parameters to not add to the docstring (will still be in the signature)
parameters_to_skip = ['self']

# Used to differentiate first and second callback for classes
last_class_name = ""

# Cached signature and parameter list [[name, type, default], ...] in between
# the process_signature_callback() and process_docstring_callback callbacks()
cached_parameters = []
cached_signature = ""

# Extracted RST text (as a list of lines)
extracted_rst = []

# Keep track of the first line of the current block in the extracted_rst text list.
block_line_start = 0
# Keep track of the current block name
last_block_name = None

# Dictionary of {'block_name' : [start_line, end_line], ...} storeing the range of each block
# in the extracted RST text list.
rst_block_range = {}

"""
Define output file names:
- list_api.rst: RST file that will be processed when building the API documentation.
                This file gets generated before building the documentation and contains
                autodoc directives (e.g. autoclass::, autofunction::, ...) for
                every classes and functions of the Mitsuba python module. Those will
                then get parsed by the callbacks defined below and extracted to the
                `extracted_rst_api.rst` file.

- extracted_rst_api.rst: RST file into which the API documentation will be extracted.
"""
docs_path = realpath(join(dirname(realpath(__file__)), '..'))
list_api_filename = join(docs_path, 'docs_api/list_api.rst')
extracted_rst_filename = join(docs_path, 'generated/extracted_rst_api.rst')


# List of patterns defining element that shouldn't be added to the API documentation
excluded_api = [r'mitsuba.python.test.scenes.([\w]+)', 'mitsuba.python.autodiff.contextmanager']

# Define the structure of the generated reference pages for the different libraries.
api_doc_structure = {
    'core': {
        'Object': ['mitsuba.core.Object'],
        'Properties': ['mitsuba.core.Properties'],
        'Bitmap': ['mitsuba.core.Bitmap'],
        'XML': [r'mitsuba.core.xml.([\w]+)'],
        'Warp': [r'mitsuba.core.warp.([\w]+)'],
        'Distributions': [r'mitsuba.core.([\w]+)Distribution',
                          r'mitsuba.core.Hierarchical2D\d',
                          r'mitsuba.core.Marginal([\w]+)2D\d'],
        'Math': [r'mitsuba.core.math.([\w]+)', r'mitsuba.core.spline.([\w]+)'],
        'Log': [r'mitsuba.core.Log([\w]+)', 'mitsuba.core.Appender', ],
        'Types': [r'mitsuba.core.Scalar([\w]+)',
                  r'mitsuba.core.Vector([\w]+)',
                  r'mitsuba.core.Point([\w]+)',
                  r'mitsuba.core.Matrix([\w]+)',
                  r'mitsuba.core.Bounding([\w]+)',
                  r'mitsuba.core.Transform([\w]+)',
                  r'mitsuba.core.AnimatedTransform'],
    },
    'render': {
        'BSDF': [r'mitsuba.render.BSDF([\w]+|)', 'mitsuba.render.TransportMode',
                 r'mitsuba.render.Microfacet([\w]+|)'],
        'Endpoint': ['mitsuba.render.Endpoint'],
        'Emitter': [r'mitsuba.render.Emitter([\w]+|)'],
        'Sensor': ['mitsuba.render.Sensor', 'mitsuba.render.ProjectiveCamera'],
        'Medium': ['mitsuba.render.Medium', r'mitsuba.render.PhaseFunction([\w]+|)'],
        'Shape': ['mitsuba.render.Shape', 'mitsuba.render.Mesh'],
        'Texture': [],
        'Film': ['mitsuba.render.Film'],
        'Sampler': ['mitsuba.render.Sampler'],
        'Scene': ['mitsuba.render.Scene', 'mitsuba.render.ShapeKDTree'],
        'Record': ['mitsuba.render.PositionSample3f', 'mitsuba.render.DirectionSample3f',
                   r'mitsuba.render.([\w]+)Interaction3f'],
        'Polarization': [r'mitsuba.render.mueller.([\w]+)'],
    },
    'python': {
        'Chi2': [r'mitsuba.python.chi2.([\w]+)'],
        'Autodiff': [r'mitsuba.python.autodiff.([\w]+)'],
    }
}


# TODO this shouldn't be needed
def sanitize_cpp_types(s):
    """ Replace C++ type with python type in signature """
    # TODO it shouldn't always be 'render'
    return re.sub(r'mitsuba::([a-zA-Z\_0-9]+)[a-zA-Z\_0-9<, :>]+>', r'mitsuba.render.\1', s)


def parse_signature_args(signature):
    """This function parses a signature string of the form:
           "(arg0: type0, ..., arg4: type4=default4)"
       It creates a list of parameters [[name, type, default], ...] and
       constructs a new signature containing only the parameter
       names and the default types:
           "(arg0, ..., arg4=default4)"
    """
    signature = sanitize_cpp_types(signature)

    # Check for overloaded functions
    if signature == "(*args, **kwargs)":
        return '(overloaded)', []
    else:
        # Remove parenthesis
        signature = signature[1:-1]
        # Split the string based on 'arg: *'
        items_tmp = re.split(r'([a-zA-Z\_0-9]+): ', signature)
        # Remove first element as it is always ''
        items_tmp.pop(0)

        # Construct parameter list and new signature
        parameters = []
        new_signature = ''
        for i in range(len(items_tmp) // 2):
            p_name = items_tmp[2 * i]
            p_type = items_tmp[2 * i + 1]

            # Remove comma
            if p_type[-2:] == ', ':
                p_type = p_type[:-2]

            # Handle default value
            result = p_type.split(' = ')
            p_default = None
            if len(result) == 2:
                p_type, p_default = result

            # Update new signature
            if p_default:
                new_signature += '%s=%s' % (p_name, p_default)
            else:
                new_signature += p_name

            # If not the last parameter, add a comma
            if i < len(items_tmp) // 2 - 1:
                new_signature += ', '

            # Skip some parameters
            if p_name in parameters_to_skip:
                continue

            parameters.append([p_name, p_type, p_default])

        # Add surrounding parenthesis
        new_signature = "(%s)" % new_signature

        return new_signature, parameters


def parse_overload_signature(signature):
    """Parse the signature string of a overloaded function (string from the
       docstring and not from the process_signature_callback). The signature
       as the following form:
          "1. name(arg0: type0, ..., arg4: type4=default4) -> return_type"
       This function generate the same output as the parse_signature_args() function.
    """
    signature = sanitize_cpp_types(signature)

    # Remove enumeration
    signature = signature[3:]

    # Get function name
    name, signature = signature.split('(', 1)

    # Get return type (if any)
    return_type = None
    tmp = signature.split(') -> ')
    if len(tmp) == 2:
        signature, return_type = tmp

    # Parse signature arguments
    new_signature, parameters = parse_signature_args(' %s ' % signature)

    # Add return type to the parameter list (with name '__return')
    if return_type and not return_type == 'None':
        parameters.append(['__return', return_type, None])

    return name, parameters, new_signature


def insert_params_and_return_docstring(lines, params, next_idx, indent=''):
    """Add parameter descriptions to the docstring if neccessary (could
       already be added by the \param doxygen macro). Also add a line
       for the return value if any. It returns the number of lines added
       through this process.
    """
    offset = 0
    for p_name, p_type, p_default in params:
        is_return = (p_name == '__return')

        if is_return:
            key = '%sReturns:' % indent
            new_line = '%sReturns → %s:' % (indent, p_type)
        else:
            key = '%sParameter ``%s``:' % (indent, p_name)
            new_line = '%sParameter ``%s`` (%s):' % (indent, p_name, p_type)

        found = False
        for i, l in enumerate(lines):
            if key in l:
                lines[i] = new_line
                found = True

        if not found:
            lines.insert(next_idx, new_line)
            lines.insert(next_idx + 1, '    %s%s' % (indent,
                                                     active_descr_str if p_name == 'active'
                                                     else param_no_descr_str))
            lines.insert(next_idx + 2, '')
            next_idx += 3
            offset += 3

    return offset


def process_overload_block(lines, what):
    """When a function is overloaded, the different flavors of that function are
       enumerate in the docstring itself. This function parses that docstring to
       create as many `py:function::` directive in order to have a similar doc
       style as the other regular functions.
    """
    # Sanity check
    if len(lines) == 0:
        return

    # Remove first line (contains 'Overloaded function.')
    lines.pop(0)

    # Find all overload signature lines
    overload_indices = []
    for i, l in enumerate(lines):
        # Look for lines that start with an enum (e.g. '1. ')
        if len(l) > 1 and re.match(r'[0-9]+. ', l[:3]):
            overload_indices.append(i)

    # Process the text block
    offset = 0
    for i, idx in enumerate(overload_indices):
        idx += offset

        # Parse the overload signature
        name, params, signature = parse_overload_signature(lines[idx])

        # Replace overload signature with '.. py:method::' directives
        lines[idx] = '.. py:%s:: %s%s' % (what, name, signature)

        # Find where to the description of this overload ends
        if i == len(overload_indices) - 1:
            next_idx = len(lines)
        else:
            next_idx = overload_indices[i + 1] + offset

        # Increase indent of all the overload description lines
        for j in range(next_idx - idx - 1):
            if not lines[idx + j + 1] == '':
                lines[idx + j + 1] = '    ' + lines[idx + j + 1]

        # Insert parameter and return value description to the docstring
        offset += insert_params_and_return_docstring(
            lines, params, next_idx, indent='    ')


def process_signature_callback(app, what, name, obj, options, signature, return_annotation):
    """ This callback will emitted when autodoc has formatted a signature for an object.
        It parses and caches the signature arguments for function and classes so it can
        be used in the process_docstring_callback callback.
    """
    global cached_parameters
    global cached_signature

    if name.startswith('mitsuba.python.'):
        # Signatures from python scripts do not contain any argument types or
        # return type information. So we don't need to do anything.
        cached_signature = signature
        cached_parameters = []
    else:
        if signature and what == 'class':
            # For classes we don't display any signature in the class headers

            # Parse the signature string
            cached_signature, cached_parameters = parse_signature_args(signature)

        elif signature and what in ['method', 'function']:
            # For methods and functions, parameter types will be added to the docstring.

            # Parse the signature string
            cached_signature, cached_parameters = parse_signature_args(signature)

            # Return type (if any) will also be added to the docstring
            if return_annotation:
                return_annotation = sanitize_cpp_types(return_annotation)
                cached_parameters.append(['__return', return_annotation, None])
        else:
            cached_signature = ''
            cached_parameters = []

    return signature, None


def process_docstring_callback(app, what, name, obj, options, lines):
    """ This callback will emitted when autodoc has read and processed a docstring.
        Using the cached parameter list and signature, this function modifies the docstring
        and extract the resulting RST code into the extracted_rst text list.
    """
    global cached_parameters
    global cached_signature
    global last_class_name
    global extracted_rst
    global rst_block_range
    global block_line_start
    global last_block_name

    # True is the documentation wasn't generated with pybind11 (e.g. python script)
    is_python_doc = name.startswith('mitsuba.python.')

    if type(obj) in (int, float, bool, str):
        what = 'data'

    #----------------------------
    # Handle classes

    if what == 'class':
        # process_docstring callback is always called twice for a class:
        #   1. class description
        #   2. constructor(s) description. If the constructor is overloaded, the docstring
        #      will enumerate the constructor signature (similar to overloaded functions)

        if not last_class_name == name:  # First call (class description)
            # Add information about the base class if it is a Mitsuba type
            if len(obj.__bases__) > 0:
                full_base_name = str(obj.__bases__[0])[8:-2]
                if full_base_name.startswith('mitsuba'):
                    lines.insert(0, 'Base class: %s' % full_base_name)
                    lines.insert(1, '')

            # Handle enums properly
            # Search for 'Members:' line to define whether this class is an enum or not
            is_enum = False
            next_idx = 0
            for i, l in enumerate(lines):
                if 'Members:' in l:
                    is_enum = True
                    next_idx = i + 2
                    break

            if is_enum:
                # Iterate over the enum members
                while next_idx < len(lines) and not '__init__' in lines[next_idx]:
                    l = lines[next_idx]
                    # Skip empy lines
                    if not l == '':
                        # Check if this line defines a enum member
                        if re.match(r'  [a-zA-Z\_0-9]+ : .*', l):
                            e_name, e_desc = l[2:].split(' : ')
                            lines[next_idx] = '.. py:data:: %s' % (e_name)
                            next_idx += 1
                            lines.insert(next_idx, '')
                            next_idx += 1
                            lines.insert(next_idx, '    %s' % e_desc)
                        else:
                            # Add indentation
                            lines[next_idx] = '    %s' % l
                    next_idx += 1

            # Handle aliases (will only call the callback once)
            if len(lines) > 0 and 'Overloaded function.' in lines[0]:
                process_overload_block(lines, 'method')

        else:  # Second call (constructor(s) description)
            if not cached_signature == '(overloaded)':
                # Increase indent to all lines
                for i, l in enumerate(lines):
                    lines[i] = '    ' + l

                # Insert constructor signature
                lines.insert(0, '.. py:method:: %s%s' %
                             ('__init__', cached_signature))
                lines.insert(1, '')

                lines.insert(len(lines)-1, '')
                insert_params_and_return_docstring(
                    lines, cached_parameters, len(lines)-1, indent='    ')
            else:
                process_overload_block(lines, 'method')

    #----------------------------
    # Handle methods and functions

    if what in ['method', 'function']:
        if cached_signature == '(overloaded)':
            process_overload_block(lines, what)
        else:
            # Find where to insert next parameter if necessary
            # For this we look for the 'Returns:', otherwise we insert at the end
            next_idx = len(lines)
            for i, l in enumerate(lines):
                if 'Returns:' in l:
                    next_idx = i
                    break

            insert_params_and_return_docstring(
                lines, cached_parameters, next_idx)


    # Add code-block directive and cross reference for every mitsuba types
    in_code_block = False
    in_bullet_item_block = False
    for i, l in enumerate(lines):
        # Process code block
        if re.match(r'([ ]+|)```', l):
            if not in_code_block:
                lines[i] = l[:-3] + '.. code-block:: c'
                lines.insert(i+1, '')
                in_code_block = True
            else:
                lines[i] = ''
                in_code_block = False
        else:
            # Adjust the indentation
            if in_code_block and not l == '':
                lines[i] = '    ' + l

        # Adjust indentation for multi-line bullet list item
        if re.match(r'([ ]+|) \* ', lines[i]):
            in_bullet_item_block = True
        elif lines[i] == '':
            in_bullet_item_block = False
        elif in_bullet_item_block:
            lines[i] = '  ' + lines[i]

        # Add cross-reference
        if not in_code_block:
            lines[i] = re.sub(r'(?<!`)(mitsuba(?:\.[a-zA-Z\_0-9]+)+)',
                              r':py:obj:`\1`', lines[i])


    #----------------------------
    # Extract RST

    # Check whether this class is defined within another like (e.g. Bitmap.FileFormat),
    # in which case the indentation will be adjusted.
    local_class = what == 'class' and re.fullmatch(
        r'%s\.[\w]+' % last_block_name, name)

    # Check whether to start a new block
    if what in ['function', 'class', 'module', 'data'] and not local_class and not last_class_name == name:
        # Register previous block
        if last_block_name:
            rst_block_range[last_block_name] = [
                block_line_start, len(extracted_rst) - 1]

        # Start a new block
        block_line_start = len(extracted_rst)
        last_block_name = name

    # Get rid of default 'name' property in enums
    if what == 'property' and lines[0] == '(self: handle) -> str':
        return

    # Adjust the indentation
    doc_indent = '    '
    directive_indent = ''
    if what in ['method', 'attribute', 'property'] or local_class:
        doc_indent += '    '
        directive_indent += '    '

    # Don't write class directive twice
    if not (what == 'class' and last_class_name == name):
        directive_type = what
        if what == 'property':
            directive_type = 'method'

        # Add the corresponding RST directive
        directive = '%s.. py:%s:: %s' % (
            directive_indent, directive_type, name)

        # Display signature for methods and functions
        if what in ['method', 'function']:
            directive += cached_signature

        # Display signature for classes for python doc
        if what == 'class' and is_python_doc and cached_signature:
            directive += cached_signature

        extracted_rst.append(directive + '\n')

        # 'property' fields need an extra argument to the directive
        if what == 'property':
            extracted_rst.append(doc_indent + ':property:\n')

        # 'data' fields get extra arguments
        if what == 'data':
            extracted_rst.append(doc_indent + ':type: %s\n' % str(type(obj))[8:-2])
            extracted_rst.append(doc_indent + ':value: %s\n' % str(obj))

        extracted_rst.append('\n')

    # Extract the docstring (if not a module)
    if not what in ['module', 'data']:
        for l in lines:
            if l == '':
                extracted_rst.append('\n')
            else:
                extracted_rst.append(doc_indent + l + '\n')

    # Keep track of last class name (to distingush the two callbacks)
    last_class_name = name


def write_rst_file_callback(app, exception):
    """Write to a file the extracted RST and generate the RST reference
       pages for the different libraries"""

    # Register last block
    rst_block_range[last_block_name] = [block_line_start, len(extracted_rst)]

    # Given a class/fucntion "block" name, add an RST 'include' directive with the
    # corresponding start/end-line to the output file.
    def write_block(f, block_name):
        f.write('.. include:: extracted_rst_api.rst\n')
        f.write('  :start-line: %i\n' % rst_block_range[block_name][0])
        f.write('  :end-line: %i\n' % rst_block_range[block_name][1])
        # Add a horizontal separator line
        f.write('\n')
        f.write('------------\n')
        f.write('\n')

    # Write extracted RST to file
    with open(extracted_rst_filename, 'w') as f:
        print('Extract API doc into: %s' % extracted_rst_filename)
        for l in extracted_rst:
            f.write(l)

    # Generate API Reference RST according to the api_doc_structure description
    for lib in api_doc_structure.keys():
        lib_structure = api_doc_structure[lib]
        lib_api_filename = join(docs_path, 'generated/%s_api.rst' % lib)
        with open(lib_api_filename, 'w') as f:
            print('Generate %s API RST file: %s' % (lib, lib_api_filename))
            f.write('.. _sec-api-%s:\n\n' % lib)
            f.write('%s API Reference\n' % lib.title())
            f.write('=' * len(lib) + '==============\n')
            f.write('\n')

            # Keep track of the added block, so to add the rest in the 'Other' section
            added_block = []

            for section_name in lib_structure.keys():
                # Write section name
                f.write('%s\n' % section_name)
                f.write('-' * len(section_name) + '\n')
                f.write('\n')

                # Write all the blocks
                for pattern in lib_structure[section_name]:
                    for block_name in rst_block_range.keys():
                        if re.fullmatch(pattern, block_name):
                            write_block(f, block_name)
                            added_block.append(block_name)

            # Add the rest into the 'Other' section
            f.write('Other\n')
            f.write('-----\n')
            f.write('\n')

            for block_name in rst_block_range.keys():
                if block_name in added_block:
                    continue

                if not block_name.startswith('mitsuba.%s' % lib):
                    continue

                write_block(f, block_name)


def generate_list_api_callback(app):
    """Generate a RST file listing all the python members (classes or functions)
       to be parsed and extracted. This function will recursively explore submodules
       and packages."""

    import importlib
    from inspect import isclass, isfunction, ismodule, ismethod

    def process(f, obj, lib, name):
        if re.match(r'__[a-zA-Z\_0-9]+__', name):
            return

        if ismodule(obj):
            # 'python' is a package, so it needs to be treated differently
            if name == 'python':
                # Iterate recursively on all the python files
                for root, dirs, files in os.walk(obj.__path__[0]):
                    root = root.replace(obj.__path__[0], '').replace('/', '.')
                    if root.endswith('__pycache__'):
                        continue
                    for f_name in files:
                        if not f_name == '__init__.py' and f_name.endswith('.py'):
                            module = importlib.import_module(
                                'mitsuba.python%s.%s' % (root, f_name[:-3]))
                            for x in dir(module):
                                obj2 = getattr(module, x)
                                # Skip the imported modules (e.g. enoki)
                                if not ismodule(obj2):
                                    process(f, obj2, '.python%s.%s' %
                                            (root, f_name[:-3]), x)
            else:
                for x in dir(obj):
                    process(f, getattr(obj, x), '%s.%s' % (lib, name), x)
        else:

            full_name = 'mitsuba%s.%s' % (lib, name)

            # Check if this block should be excluded from the API documentation
            for pattern in excluded_api:
                if re.fullmatch(pattern, full_name):
                    return

            f.write('.. auto%s:: %s\n\n' %
                    ('class' if isclass(obj) else 'function', full_name))

    with open(list_api_filename, 'w') as f:
        print('Generate API list file: %s' % list_api_filename)
        for lib in ['core', 'render', 'python']:
            module = importlib.import_module('mitsuba.%s' % lib)
            process(f, module, '', lib)


# -- Register event callbacks ----------------------------------------------

def setup(app):
    # Texinfo
    app.connect("builder-inited", generate_list_api_callback)
    app.add_stylesheet('theme_overrides.css')

    # Autodoc
    app.connect('autodoc-process-docstring', process_docstring_callback)
    app.connect('autodoc-process-signature', process_signature_callback)
    app.connect("build-finished", write_rst_file_callback)
