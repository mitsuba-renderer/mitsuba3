#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Mitsuba 2 documentation build configuration file
#
# The documentation can be built by invoking "make mkdoc"
# from the build directory

import sys
import os
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
#needs_sphinx = '1.0'


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

# -- Event callback for processing the docstring ----------------------------------------------

import re

param_no_descr_str = "*no description available*"
active_descr_str = "Mask to specify active lanes."
parameters_to_skip = ['self']

# Used to differentiate first and second callback for classes
last_name = ""

# TODO add comments here

# global variables
cached_parameters = []
cached_signature = ""

extracted_rst = []

block_line_start = 0
block_name = None
rst_block_range = {}

# NOTE this shouldn't be needed
def sanitize_cpp_types(s):
    # Replace C++ type from signature
    # TODO it shouldn't always be 'render'
    return re.sub(r'mitsuba::([a-zA-Z\_0-9]+)[a-zA-Z\_0-9<, :>]+>', r'mitsuba.render.\1', s)

def parse_signature(signature):
    """Return the new modified string signature as well as a list of
       parameters [name, type, default]"""

    signature = sanitize_cpp_types(signature)

    # Check for overloaded functions
    if signature == "(*args, **kwargs)":
        return '(overloaded)', []
    else:
        # TODO improve this
        items_tmp = re.split(r'([a-zA-Z\_0-9]+): ', signature[1:-1])
        items_tmp.pop(0)
        parameters = []
        new_signature = ''
        for i in range(len(items_tmp) // 2):
            # print(i)
            p_name = items_tmp[2*i]
            p_type = items_tmp[2*i+1]

            if p_type[-2:] == ', ':
                p_type = p_type[:-2]

            result = p_type.split(' = ')
            p_default = None
            if len(result) == 2:
                p_type, p_default = result

            if p_default:
                new_signature += '%s=%s, ' % (p_name, p_default)
            else:
                new_signature += '%s, ' % p_name

            # Skip some parameters
            if p_name in parameters_to_skip:
                continue

            parameters.append([p_name, p_type, p_default])

        # Add surrounding parenthesis
        new_signature = "(%s)" % new_signature

        return new_signature, parameters


def parse_overload_signature(signature):
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

    new_signature, parameters = parse_signature(' %s ' % signature)

    if return_type and not return_type == 'None':
        parameters.append(['__return', return_type, None])

    return name, parameters, new_signature


def insert_params_and_return_docstring(lines, params, next_idx, indent=''):
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


def process_overload_block(lines):
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
        lines[idx] = '.. py:method:: %s%s' % (name, signature)

        # Find where to the description of this overload ends
        if i == len(overload_indices) - 1:
            next_idx = len(lines)
        else:
            next_idx = overload_indices[i + 1] + offset

        # Increase indent of all the overload description lines
        for j in range(next_idx - idx - 1):
            lines[idx + j + 1] = '    ' + lines[idx + j + 1]

        offset += insert_params_and_return_docstring(
            lines, params, next_idx, indent='    ')


def process_signature_callback(app, what, name, obj, options, signature, return_annotation):
    global cached_parameters
    global cached_signature

    if signature and what == 'class':
        # For classes we don't display any signature in the class headers

        # Parse the signature string
        cached_signature, cached_parameters = parse_signature(signature)
        signature = ''

    elif signature and what == 'method':
        # For methods, parameter types will be add to the docstring

        # Parse the signature string
        cached_signature, cached_parameters = parse_signature(signature)
        # Display modified signature in the method header
        signature = cached_signature

        # Return type (if any) will also be added to the docstring
        if return_annotation:
            return_annotation = sanitize_cpp_types(return_annotation)
            cached_parameters.append(['__return', return_annotation, None])

    return signature, None


def process_docstring_callback(app, what, name, obj, options, lines):
    global cached_parameters
    global cached_signature
    global last_name
    global extracted_rst
    global rst_block_range
    global block_line_start
    global block_name

    #----------------------------
    # Handle classes

    if what == 'class':
        # process_docstring callback is always called twice for a class:
        #   1. class description
        #   2. constructor(s) description. If the constructor is overloaded, the docstring
        #      will enumerate the constructor signature (similar to overloaded functions)

        if not last_name == name:  # First call (class description)
            # Add information about the base class if it is a mitsuba type
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

        else:  # Second call (constructor(s) description)
            if not cached_signature == '(overloaded)':
                # Increase indent to all lines
                for i, l in enumerate(lines):
                    lines[i] = '        ' + l

                # Insert constructor signature
                lines.insert(0, '.. py:method:: %s%s' %
                             ('__init__', cached_signature))
                lines.insert(1, '')
            else:
                process_overload_block(lines)

    #----------------------------
    # Handle methods

    if what == 'method':
        if cached_signature == '(overloaded)':
            process_overload_block(lines)
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

    # Add cross-reference for every mitsuba types
    for i, l in enumerate(lines):
        lines[i] = re.sub(r'(mitsuba(?:\.[a-zA-Z\_0-9]+)+)',
                          r':py:class:`\1`', l)

    #----------------------------
    # Extract RST

    doc_indent = '    '
    directive_indent = ''

    if what in ['function', 'class'] and not last_name == name:
        # register previous block
        if block_name:
            rst_block_range[block_name] = [block_line_start, len(extracted_rst)-1]

        block_line_start = len(extracted_rst)
        block_name = name

    if what in ['method', 'attribute', 'property']:
        doc_indent += '    '
        directive_indent += '    '

    # Don't write class directive twice
    if not (what == 'class' and last_name == name):
        directive_type = what
        if what == 'property':
            directive_type = 'method'

        directive = '%s.. py:%s:: %s' % (directive_indent, directive_type, name)

        # Only display signature for methods
        if what == 'method':
            directive += cached_signature

        extracted_rst.append(directive + '\n')

        if what == 'property':
            extracted_rst.append(doc_indent + ':property:\n')

        extracted_rst.append('\n')

    for l in lines:
        extracted_rst.append(doc_indent + l + '\n')

    # Keep track of last class name (to distingush the two callbacks)
    last_name = name


# TODO add list of members to skip (like mitsuba.core.Vector2f)

# Define the structure of the generated reference pages for the different libraries.
api_structure = {
    'core' : {}, # TODO
    'render' : {
        'BSDF': ['mitsuba.render.BSDF', 'mitsuba.render.TransportMode', 'mitsuba.render.BSDFFlags',
                 'mitsuba.render.BSDFContext', 'mitsuba.render.BSDFSample3f',
                 'mitsuba.render.MicrofacetDistribution', 'mitsuba.render.MicrofacetType'],
        'Endpoint': ['mitsuba.render.Endpoint'],
        'Emitter': ['mitsuba.render.Emitter', 'mitsuba.render.EmitterFlags'],
        'Sensor': ['mitsuba.render.Sensor', 'mitsuba.render.ProjectiveCamera'],
        'Medium': ['mitsuba.render.Medium', 'mitsuba.render.PhaseFunction',
                   'mitsuba.render.PhaseFunctionContext', 'mitsuba.render.PhaseFunctionFlags'],
        'Shape': ['mitsuba.render.Shape', 'mitsuba.render.Mesh'],
        'Texture': [],
        'Film': ['mitsuba.render.Film'],
        'Sampler': ['mitsuba.render.Sampler'],
        'Scene': ['mitsuba.render.Scene', 'mitsuba.render.ShapeKDTree'],
        'Record': ['mitsuba.render.PositionSample3f', 'mitsuba.render.DirectionSample3f',
                   'mitsuba.render.Interaction3f', 'mitsuba.render.SurfaceInteraction3f',
                   'mitsuba.render.MediumInteraction3f']
    },
    'python' : {} # TODO
}

# Write to a file the extracted RST and generate the RST reference pages for the different libraries
def write_rst_file_callback(app, exception):
    # register last block
    rst_block_range[block_name] = [block_line_start, len(extracted_rst)]
    # print(rst_block_range)

    # Write extracted RST to file
    with open('../docs/generated/extracted_rst_api.rst', 'w') as outfile:
        for l in extracted_rst:
            outfile.write(l)

    # Generate API Reference RST according to the api_structure description
    for lib in api_structure.keys():
        lib_structure = api_structure[lib]

        with open('../docs/generated/%s_api.rst' % lib, 'w') as outfile:
            outfile.write('%s API Reference\n' % lib.title())
            outfile.write('=' * len(lib) + '==============\n')
            outfile.write('\n')

            added_block = []

            for header in lib_structure.keys():
                # Write header
                outfile.write('%s\n' % header)
                outfile.write('-' * len(header) + '\n')
                outfile.write('\n')

                for block in lib_structure[header]:
                    outfile.write('.. include:: extracted_rst_api.rst\n')
                    outfile.write('  :start-line: %i\n' % rst_block_range[block][0])
                    outfile.write('  :end-line: %i\n' % rst_block_range[block][1])
                    outfile.write('\n')
                    outfile.write('------------\n')
                    outfile.write('\n')


                    added_block.append(block)

            # Add the rest into the 'Other' section
            outfile.write('Other\n')
            outfile.write('-----\n')
            outfile.write('\n')

            for block in rst_block_range.keys():
                if block in added_block:
                    continue

                if not block.startswith('mitsuba.%s' % lib):
                    continue

                outfile.write('.. include:: extracted_rst_api.rst\n')
                outfile.write('  :start-line: %i\n' % rst_block_range[block][0])
                outfile.write('  :end-line: %i\n' % rst_block_range[block][1])
                outfile.write('\n')
                outfile.write('------------\n')
                outfile.write('\n')


# Generate a RST file that parses the documentation of all the python module of Mitsuba.
def generate_list_api_callback(app):

    import importlib
    from inspect import isclass, isfunction, ismodule, ismethod

    with open('../docs/docs_api/list_api.rst', 'w') as outfile:

        for lib in ['render', 'core']:
            module = importlib.import_module('mitsuba.%s' % lib)
            for x in dir(module):
                # Skip '__init__', '__doc__', ...
                if re.match(r'__[a-zA-Z\_0-9]+__', x):
                    continue

                if ismodule(getattr(module, x)):
                    outfile.write('.. automodule:: mitsuba.%s.%s\n\n' % (lib, x))
                elif isclass(getattr(module, x)):
                    outfile.write('.. autoclass:: mitsuba.%s.%s\n\n' % (lib, x))
                else:
                    outfile.write('.. autofunction:: mitsuba.%s.%s\n\n' % (lib, x))


# -- Register event callbacks ----------------------------------------------

def setup(app):
    # Texinfo
    app.connect("builder-inited", generate_list_api_callback)
    app.add_stylesheet('theme_overrides.css')

    # Autodoc
    app.connect('autodoc-process-docstring', process_docstring_callback)
    app.connect('autodoc-process-signature', process_signature_callback)
    app.connect("build-finished", write_rst_file_callback)

