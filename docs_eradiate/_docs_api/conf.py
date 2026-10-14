#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Eradiate Mitsuba API reference — Sphinx configuration
# Adapted from docs/docs_api/conf.py.
#
# Build this subsite separately before the main site:
#   sphinx-build -b html _docs_api _build/html_api
#
# This build generates docs_eradiate/generated/eradiate_mitsuba_api.rst, which is
# then included by the main site's api_reference/index.rst.

import enum
import re
from os.path import dirname, join, realpath

# -- General configuration ------------------------------------------------

source_suffix = ".rst"

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
.. |phase| replace:: :paramtype:`phase`
.. |point| replace:: :paramtype:`point`
.. |transform| replace:: :paramtype:`transform`
.. |volume| replace:: :paramtype:`volume`

.. |drjit| replace:: :monosp:`drjit`
.. |numpy| replace:: :monosp:`numpy`

.. |nbsp| unicode:: 0xA0
   :trim:

"""

master_doc = "list_api"

project = "eradiate-mitsuba"
copyright = "2024, Rayference"
author = "Rayference"

# -- Autodoc --------------------------------------------------------------

extensions = ["sphinx.ext.autodoc"]

autodoc_default_options = {
    "members": True,
    "special-members": "__call__",
}

autoclass_content = "both"
autodoc_member_order = "bysource"

# Set the Mitsuba variant used for autodoc introspection.
import mitsuba  # noqa: E402

mitsuba.set_variant("scalar_rgb")
variant_prefix = "mitsuba.scalar_rgb."
drjit_variant_alias = "drjit.scalar"

# -- Output file paths ----------------------------------------------------

docs_path = realpath(join(dirname(realpath(__file__)), ".."))
extracted_rst_filename = join(docs_path, "generated/extracted_rst_api.rst")
lib_api_filename = join(docs_path, "generated/eradiate_mitsuba_api.rst")
list_api_filename = join(dirname(realpath(__file__)), "list_api.rst")

# -- API structure --------------------------------------------------------

# Maps section names to lists of regex patterns matching class/function names.
# Only eradiate-specific additions are documented here; the base Mitsuba API
# is documented upstream at https://mitsuba.readthedocs.io.
api_doc_structure = {
    "Constants": [
        r"mitsuba.ERADIATE_MI_([\w]+)",
    ],
    "Medium": [
        r"mitsuba\.Medium",
    ],
    "Extremum": [
        r"mitsuba\.ExtremumSegment([\w]*)",
        r"mitsuba\.ExtremumStructure([\w]*)",
    ],
    "Phase": [
        r"mitsuba\.PhaseFunction",
    ],
}

# -- Docstring processing helpers -----------------------------------------

param_no_descr_str = "*no description available*"
active_descr_str = "Mask to specify active lanes."
parameters_to_skip = ["self"]

last_class_name = ""
cached_parameters = []
cached_signature = ""
extracted_rst = []
block_line_start = 0
last_block_name = None
rst_block_range = {}


def sanitize_types(s):
    s = re.sub(r"mitsuba::([a-zA-Z\_0-9]+)[a-zA-Z\_0-9<, :>]+>", r"mitsuba.\1", s)
    s = re.sub(rf"{variant_prefix}([a-zA-Z\_0-9]+)", r"mitsuba.\1", s)
    return s


def parse_signature_args(signature):
    signature = sanitize_types(signature)

    if signature == "(*args, **kwargs)":
        return "(overloaded)", []
    else:
        signature = signature[1:-1]
        is_method = signature.startswith("self,")
        items_tmp = re.split(r"([a-zA-Z\_0-9]+): ", signature)
        items_tmp.pop(0)

        parameters = []
        new_signature = ""

        if is_method:
            new_signature += "self"

        for i in range(len(items_tmp) // 2):
            if i == 0 and is_method:
                new_signature += ", "

            p_name = items_tmp[2 * i]
            p_type = items_tmp[2 * i + 1]

            if p_type[-2:] == ", ":
                p_type = p_type[:-2]

            result = p_type.split(" = ")
            p_default = None
            if len(result) == 2:
                p_type, p_default = result

            if p_default:
                new_signature += f"{p_name}={p_default}"
            else:
                new_signature += p_name

            if i < len(items_tmp) // 2 - 1:
                new_signature += ", "

            if p_name in parameters_to_skip:
                continue

            parameters.append([p_name, p_type, p_default])

        new_signature = f"({new_signature})"
        return new_signature, parameters


def parse_overload_signature(signature):
    signature = sanitize_types(signature)
    signature = signature[3:].strip()
    # Strip RST inline-code backticks that nanobind wraps overload signatures with
    if signature.startswith("``"):
        signature = signature[2:]
    if signature.endswith("``"):
        signature = signature[:-2]
    name, signature = signature.split("(", 1)

    return_type = None
    tmp = signature.split(") -> ")
    if len(tmp) == 2:
        signature, return_type = tmp

    new_signature, parameters = parse_signature_args(" %s " % signature)

    if return_type and not return_type == "None":
        parameters.append(["__return", return_type, None])

    return name, parameters, new_signature


def insert_params_and_return_docstring(lines, params, next_idx, indent=""):
    offset = 0
    for p_name, p_type, _ in params:
        is_return = p_name == "__return"

        if is_return:
            key = f"{indent}Returns:"
            new_line = f"{indent}Returns → {p_type}:"
        else:
            key = f"{indent}Parameter ``{p_name}``:"
            new_line = f"{indent}Parameter ``{p_name}`` ({p_type}):"

        found = False
        for i, l in enumerate(lines):
            if key in l:
                lines[i] = new_line
                found = True

        if not found:
            lines.insert(next_idx, new_line)
            lines.insert(
                next_idx + 1,
                f"    {indent}{active_descr_str if p_name == 'active' else param_no_descr_str}",
            )
            lines.insert(next_idx + 2, "")
            next_idx += 3
            offset += 3

    return offset


def process_overload_block(lines, what):
    if len(lines) == 0:
        return

    lines.pop(0)

    overload_indices = []
    for i, l in enumerate(lines):
        if len(l) > 1 and re.match(r"[0-9]+. ", l[:3]):
            overload_indices.append(i)

    offset = 0
    for i, idx in enumerate(overload_indices):
        idx += offset
        name, params, signature = parse_overload_signature(lines[idx])

        lines[idx] = f".. py:{what}:: {name}{signature}"

        if i == len(overload_indices) - 1:
            next_idx = len(lines)
        else:
            next_idx = overload_indices[i + 1] + offset

        for j in range(next_idx - idx - 1):
            if not lines[idx + j + 1] == "":
                lines[idx + j + 1] = "    " + lines[idx + j + 1]

        offset += insert_params_and_return_docstring(
            lines, params, next_idx, indent="    "
        )


def process_signature_callback(
    app, what, name, obj, options, signature, return_annotation
):
    global cached_parameters, cached_signature

    if signature and what == "class":
        cached_signature, cached_parameters = parse_signature_args(signature)
    elif signature and what in ["method", "function"]:
        cached_signature, cached_parameters = parse_signature_args(signature)
        if return_annotation:
            return_annotation = sanitize_types(return_annotation)
            cached_parameters.append(["__return", return_annotation, None])
    else:
        cached_signature = ""
        cached_parameters = []

    return signature, return_annotation


def process_docstring_callback(app, what, name, obj, options, lines):
    global cached_parameters, cached_signature, last_class_name
    global extracted_rst, rst_block_range, block_line_start, last_block_name

    is_python_doc = name.startswith("mitsuba.python.")

    if type(obj) in (int, float, bool, str):
        what = "data"

    if what == "class":
        if not last_class_name == name:
            if len(obj.__bases__) > 0:
                full_base_name = str(obj.__bases__[0])[8:-2]
                if full_base_name.startswith("mitsuba"):
                    lines.insert(0, f"Base class: {sanitize_types(full_base_name)}")
                    lines.insert(1, "")

            is_enum = issubclass(obj, enum.Enum)
            if is_enum:
                lines.append("Valid values are as follows:")
                lines.append("")
                for value in obj:
                    lines.append(f".. py:data:: {value.__name__}")
                    lines.append("")
                    lines.append(f"    {' '.join(value.__doc__.splitlines())}")
                    lines.append("")

            if len(lines) > 0 and "Overloaded function." in lines[0]:
                process_overload_block(lines, "method")

        else:
            if not cached_signature == "(overloaded)":
                for i, l in enumerate(lines):
                    lines[i] = "    " + l
                lines.insert(0, f".. py:method:: __init__{cached_signature}")
                lines.insert(1, "")
                lines.insert(len(lines) - 1, "")
                insert_params_and_return_docstring(
                    lines, cached_parameters, len(lines) - 1, indent="    "
                )
            else:
                process_overload_block(lines, "method")

    if what in ["method", "function"]:
        if cached_signature == "(overloaded)":
            process_overload_block(lines, what)
        else:
            next_idx = len(lines)
            for i, l in enumerate(lines):
                if "Returns:" in l:
                    next_idx = i
                    break
            insert_params_and_return_docstring(lines, cached_parameters, next_idx)

    in_code_block = False
    in_bullet_item_block = False
    for i, l in enumerate(lines):
        if re.match(r"([ ]+|)```", l):
            if not in_code_block:
                lines[i] = l[:-3] + ".. code-block:: c"
                lines.insert(i + 1, "")
                in_code_block = True
            else:
                lines[i] = ""
                in_code_block = False
        else:
            if in_code_block and not l == "":
                lines[i] = "    " + l

        if re.match(r"([ ]+|) \* ", lines[i]):
            in_bullet_item_block = True
        elif lines[i] == "":
            in_bullet_item_block = False
        elif in_bullet_item_block:
            lines[i] = "  " + lines[i]

        if not in_code_block:
            lines[i] = re.sub(
                r"(?<!`)(mitsuba(?:\.[a-zA-Z\_0-9]+)+)", r":py:obj:`\1`", lines[i]
            )

    local_class = what == "class" and re.fullmatch(rf"{last_block_name}\.[\w]+", name)

    if (
        what in ["function", "class", "module", "data"]
        and not local_class
        and not last_class_name == name
    ):
        if last_block_name:
            rst_block_range[last_block_name] = [
                block_line_start,
                len(extracted_rst) - 1,
            ]
        block_line_start = len(extracted_rst)
        last_block_name = name

    if what == "property" and lines and lines[0] == "(self: handle) -> str":
        return

    doc_indent = "    "
    directive_indent = ""
    if what in ["method", "attribute", "property"] or local_class:
        doc_indent += "    "
        directive_indent += "    "

    if not (what == "class" and last_class_name == name):
        directive_type = what
        directive = f"{directive_indent}.. py:{directive_type}:: {name}"

        if what in ["method", "function"] and cached_signature:
            directive += cached_signature

        if what == "class" and is_python_doc and cached_signature:
            directive += cached_signature

        extracted_rst.append(directive + "\n")

        if what == "data":
            extracted_rst.append(f"{doc_indent}:type: {str(type(obj))[8:-2]}\n")
            extracted_rst.append(f"{doc_indent}:value: {str(obj)}\n")

        extracted_rst.append("\n")

    if what not in ["module", "data"]:
        for l in lines:
            if l == "":
                extracted_rst.append("\n")
            else:
                extracted_rst.append(doc_indent + l + "\n")

    last_class_name = name


def write_rst_file_callback(app, exception):
    if last_block_name is None:
        return  # No sources were processed (incremental build with cached env)
    rst_block_range[last_block_name] = [block_line_start, len(extracted_rst)]

    def write_block(f, block_name):
        f.write(".. include:: /generated/extracted_rst_api.rst\n")
        f.write(f"  :start-line: {rst_block_range[block_name][0]}\n")
        f.write(f"  :end-line: {rst_block_range[block_name][1]}\n")
        f.write("\n")
        f.write("------------\n")
        f.write("\n")

    with open(extracted_rst_filename, "w", encoding="utf-8") as f:
        print(f"Extract API doc into: {extracted_rst_filename}")
        for l in extracted_rst:
            f.write(l)

    import io

    buf = io.StringIO()
    print(f"Generate API RST file: {lib_api_filename}")

    added_block = []

    for section_name in api_doc_structure.keys():
        buf.write(f"{section_name}\n")
        buf.write("-" * len(section_name) + "\n")
        buf.write("\n")

        for pattern in api_doc_structure[section_name]:
            for block_name in rst_block_range.keys():
                if re.fullmatch(pattern, block_name):
                    write_block(buf, block_name)
                    added_block.append(block_name)

    remaining = [b for b in rst_block_range if b not in added_block]
    if remaining:
        buf.write("Other\n")
        buf.write("-----\n")
        buf.write("\n")
        for block_name in remaining:
            write_block(buf, block_name)

    with open(lib_api_filename, "w", encoding="utf-8") as f:
        # Note: Delete any line separator at the end of the file (Sphinx warns
        # about it otherwise)
        f.write(buf.getvalue().rstrip("\n-") + "\n")


# -- Theme (minimal fallback for standalone HTML output) ------------------

html_theme = "alabaster"


# -- List API generation --------------------------------------------------


def generate_list_api_callback(app):
    """Generate list_api.rst from eradiate-specific mitsuba symbols.

    Walks the top-level ``mitsuba`` namespace and includes every symbol whose
    fully-qualified name matches a regex pattern defined in
    ``api_doc_structure``.  The autodoc directive is chosen based on the
    object's type:

    - ``autoclass`` for classes,
    - ``autofunction`` for other callables,
    - ``autodata`` for non-callable attributes (constants, etc.).
    """
    import importlib
    from inspect import isclass

    all_patterns = [p for patterns in api_doc_structure.values() for p in patterns]

    def is_eradiate_symbol(full_name):
        return any(re.fullmatch(p, full_name) for p in all_patterns)

    module = importlib.import_module("mitsuba")
    entries = {}

    for name in dir(module):
        if re.match(r"__[a-zA-Z_0-9]+__", name) or re.match(r"_[a-zA-Z_0-9]+", name):
            continue
        obj = getattr(module, name)
        full_name = f"mitsuba.{name}"
        if is_eradiate_symbol(full_name):
            if isclass(obj):
                specifier = "class"
            elif callable(obj):
                specifier = "function"
            else:
                specifier = "data"
            entries[full_name] = specifier

    print(f"Generate API list file: {list_api_filename}")
    with open(list_api_filename, "w", encoding="utf-8") as f:
        f.write("Eradiate Mitsuba API\n")
        f.write("====================\n")
        f.write("\n")
        for full_name in sorted(entries):
            f.write(f".. auto{entries[full_name]}:: {full_name}\n")
            f.write("\n")


# -- Event hooks ----------------------------------------------------------


def setup(app):
    import inspect

    import sphinx
    from sphinx.util import inspect as sphinx_inspect

    if sphinx.__version__ != "8.1.3":
        import warnings

        warnings.warn(
            f"Sphinx version {sphinx.__version__!r} differs from the tested "
            f"version 8.1.3. Results may vary.",
            RuntimeWarning,
        )

    # Monkey-patch Sphinx inspect functions to handle nanobind objects correctly.
    def mpatch_ismethod(object):
        if hasattr(object, "__name__") and type(object).__name__ == "nb_method":
            return True
        return inspect.ismethod(object)

    sphinx_inspect_isclassmethod = sphinx_inspect.isclassmethod

    def mpatch_isclassmethod(object, cls=None, name=None):
        if hasattr(object, "__name__") and type(object).__name__ == "nb_method":
            return False
        return sphinx_inspect_isclassmethod(object, cls, name)

    sphinx_inspect.ismethod = mpatch_ismethod
    sphinx_inspect.isclassmethod = mpatch_isclassmethod

    app.connect("builder-inited", generate_list_api_callback)
    app.connect("autodoc-process-docstring", process_docstring_callback)
    app.connect("autodoc-process-signature", process_signature_callback)
    app.connect("build-finished", write_rst_file_callback)
