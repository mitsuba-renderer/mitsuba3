#!/usr/bin/env python3
"""Extract documentation comments from C++ headers.

Produces a C++ header of raw string literals that the nanobind bindings
reference through the ``DOC()`` macro. Mitsuba's comments are already written
in reStructuredText and are copied verbatim.

Derived from ``pybind11_mkdoc``.

Usage: mkdocs.py [-o <file>] [<clang flag> ..] <header> ..
"""

import argparse
import ctypes.util
import os
import platform
import re
import sys
import textwrap
from concurrent.futures import ThreadPoolExecutor
from glob import glob

from clang import cindex
from clang.cindex import CursorKind

RECURSE_LIST = [
    CursorKind.TRANSLATION_UNIT,
    CursorKind.NAMESPACE,
    CursorKind.CLASS_DECL,
    CursorKind.STRUCT_DECL,
    CursorKind.ENUM_DECL,
    CursorKind.CLASS_TEMPLATE
]

PRINT_LIST = [
    CursorKind.CLASS_DECL,
    CursorKind.STRUCT_DECL,
    CursorKind.ENUM_DECL,
    CursorKind.ENUM_CONSTANT_DECL,
    CursorKind.CLASS_TEMPLATE,
    CursorKind.FUNCTION_DECL,
    CursorKind.FUNCTION_TEMPLATE,
    CursorKind.CONVERSION_FUNCTION,
    CursorKind.CXX_METHOD,
    CursorKind.CONSTRUCTOR,
    CursorKind.FIELD_DECL
]

# Sorted longest first so that 'operator<<' isn't mistaken for 'operator<'
CPP_OPERATORS = dict(sorted({
    '<=': 'le', '>=': 'ge', '==': 'eq', '!=': 'ne', '[]': 'array',
    '+=': 'iadd', '-=': 'isub', '*=': 'imul', '/=': 'idiv', '%=':
    'imod', '&=': 'iand', '|=': 'ior', '^=': 'ixor', '<<=': 'ilshift',
    '>>=': 'irshift', '++': 'inc', '--': 'dec', '<<': 'lshift', '>>':
    'rshift', '&&': 'land', '||': 'lor', '!': 'lnot', '~': 'bnot',
    '&': 'band', '|': 'bor', '+': 'add', '-': 'sub', '*': 'mul', '/':
    'div', '%': 'mod', '<': 'lt', '>': 'gt', '=': 'assign', '()': 'call'
}.items(), key=lambda kv: -len(kv[0])))


def sanitize_name(name):
    """Turn a qualified C++ name into the identifier that DOC() expands to."""
    name = re.sub(r'type-parameter-0-([0-9]+)', r'T\1', name)
    for k, v in CPP_OPERATORS.items():
        name = name.replace('operator' + k, 'operator_' + v)
    name = re.sub('<.*>', '', name)
    return '__doc_' + re.sub(r'[^0-9a-zA-Z]+', '_', name).rstrip('_')


def process_comment(comment):
    """Remove C++ comment syntax and the common indent."""
    lines = []

    for s in comment.expandtabs(tabsize=4).splitlines():
        # Peel off the comment markers, including a '<' that marks a comment
        # trailing the declaration it documents
        s = s.strip()
        if s.startswith('/*'):
            s = s[2:].lstrip('*!<')
        elif s.startswith('//'):
            s = s[2:].lstrip('/!<')
        if s.endswith('*/'):
            s = s[:-2].rstrip(' *')
        if s.startswith('*'):
            s = s[1:]
        # reStructuredText indentation begins after the space that follows the
        # marker. Drop that space before dedent() measures the common indent.
        lines.append(s[1:] if s.startswith(' ') else s)

    text = textwrap.dedent('\n'.join(lines)).strip('\n')

    # A ')doc' sequence would terminate the raw string literal this ends up in
    if ')doc' in text:
        raise ValueError('process_comment(): comment contains the raw string '
                         'delimiter \')doc\':\n%s' % text)
    return text


def extract(filename, node, prefix, output):
    """Collect (name, file, comment) triples for the declarations in a subtree."""
    if node.location.file is not None and \
       not os.path.samefile(node.location.file.name, filename):
        return

    # Skip the translation unit, whose spelling is the file name
    name = prefix
    if node.kind != CursorKind.TRANSLATION_UNIT and node.spelling:
        name = prefix + '_' + node.spelling if prefix else node.spelling

    if node.kind in PRINT_LIST and node.spelling:
        output.append((sanitize_name(name), filename,
                       process_comment(node.raw_comment or '')))

    if node.kind in RECURSE_LIST:
        for child in node.get_children():
            extract(filename, child, name, output)


def extract_file(filename, parameters):
    print('Processing "%s" ..' % filename, file=sys.stderr)
    # Unlike Index.create(), this displays parse diagnostics on stderr
    index = cindex.Index(cindex.conf.lib.clang_createIndex(False, True))
    tu = index.parse(filename, parameters)
    output = []
    extract(filename, tu.cursor, '', output)
    return output


def configure_clang(args):
    """Locate libclang and return the system include flags."""
    parameters = []
    libclang = os.environ.get('LIBCLANG_PATH')

    if libclang:
        if not os.path.isfile(libclang):
            raise FileNotFoundError(
                'LIBCLANG_PATH does not name a file: %s' % libclang)
        cindex.Config.set_library_file(libclang)

    if platform.system() == 'Darwin':
        dev_path = '/Applications/Xcode.app/Contents/Developer/'
        lib_dir = dev_path + 'Toolchains/XcodeDefault.xctoolchain/usr/lib/'
        sdk_dir = dev_path + 'Platforms/MacOSX.platform/Developer/SDKs'

        if not libclang and os.path.isfile(lib_dir + 'libclang.dylib'):
            cindex.Config.set_library_path(lib_dir)

        if os.path.exists(sdk_dir):
            parameters += ['-isysroot',
                           os.path.join(sdk_dir, next(os.walk(sdk_dir))[1][0])]
    elif platform.system() == 'Windows':
        if not libclang:
            libclang = ctypes.util.find_library('libclang.dll')
            if libclang is not None:
                cindex.Config.set_library_file(libclang)
    elif platform.system() == 'Linux':
        def version(path):
            return [int(v) for v in re.findall(r'(?<!lib)(?<!\d)\d+', path)]

        def newest(pattern):
            """Highest-versioned match of a glob pattern, or None."""
            return max(glob(pattern), default=None, key=version)

        llvm_dir = os.environ.get('LLVM_DIR_PATH')
        if llvm_dir is None:
            llvm_dir = max((path
                            for libdir in ('lib64', 'lib', 'lib32')
                            for path in glob('/usr/%s/llvm-*' % libdir)
                            if os.path.exists(os.path.join(
                                path, 'lib', 'libclang.so.1'))),
                           default=None, key=version)
        if llvm_dir is None:
            raise FileNotFoundError(
                'Failed to find a LLVM installation providing the file '
                '/usr/lib{32,64}/llvm-{VER}/lib/libclang.so.1. Install the '
                'packages libclang1-{VER} and libc++-{VER}-dev, or set the '
                'LLVM_DIR_PATH / LIBCLANG_PATH environment variables.')

        if not libclang:
            cindex.Config.set_library_file(
                os.path.join(llvm_dir, 'lib', 'libclang.so.1'))

        if '-stdlib=libc++' in args:
            cpp_dirs = [os.path.join(llvm_dir, 'include', 'c++', 'v1')]
        else:
            cpp_dirs = [newest('/usr/include/c++/*'),
                        newest('/usr/include/%s-linux-gnu/c++/*'
                               % platform.machine())]

        cpp_dirs.append(os.environ.get('CLANG_INCLUDE_DIR') or
                        newest(os.path.join(llvm_dir, 'lib', 'clang', '*',
                                            'include')))
        cpp_dirs += ['/usr/include/%s-linux-gnu' % platform.machine(),
                     '/usr/include']
        cpp_dirs += [p for p in os.environ.get('CPP_INCLUDE_DIRS', '').split()
                     if os.path.exists(p)]

        for cpp_dir in cpp_dirs:
            if cpp_dir is not None:
                parameters += ['-isystem', cpp_dir]

    return parameters


def read_args(args):
    """Split a command line into Clang parameters and header file names."""
    parameters = []
    if '-x' not in args:
        parameters += ['-x', 'c++']
    if not any(it.startswith('-std=') for it in args):
        parameters.append('-std=c++17')
    parameters.append('-Wno-pragma-once-outside-header')
    parameters += configure_clang(args)
    parameters += [it for it in args if it.startswith('-')]

    filenames = [it for it in args if not it.startswith('-')]
    if not filenames:
        raise ValueError('No header files were specified.')

    return parameters, filenames


def allocate_doc_names(comments):
    """Yield sorted comments with globally unique C++ identifiers.

    Overloads normally receive a numerical suffix. That suffix can itself be
    the name of another declaration: the third overload of ``eval`` and a
    method literally named ``eval_3`` both start out as ``__doc_*_eval_3``.
    Track every emitted identifier and advance the suffix until an unused one
    is found.
    """
    used_names = set()
    next_suffix = {}

    # Sort documented entries first so DOC() resolves to one with a comment.
    for name, filename, comment in sorted(
            comments, key=lambda x: (x[0], not x[2], x[1])):
        suffix = next_suffix.get(name, 1)
        while True:
            unique_name = name if suffix == 1 else '%s_%i' % (name, suffix)
            suffix += 1
            if unique_name not in used_names:
                break

        next_suffix[name] = suffix
        used_names.add(unique_name)
        yield unique_name, filename, comment


def write_header(comments, out_file=sys.stdout):
    print('''/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by resources/mkdocs.py
 */

#define __EXPAND(x)                                      x
#define __COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...)  COUNT
#define __VA_SIZE(...)                                   __EXPAND(__COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1))
#define __CAT1(a, b)                                     a ## b
#define __CAT2(a, b)                                     __CAT1(a, b)
#define __DOC1(n1)                                       __doc_##n1
#define __DOC2(n1, n2)                                   __doc_##n1##_##n2
#define __DOC3(n1, n2, n3)                               __doc_##n1##_##n2##_##n3
#define __DOC4(n1, n2, n3, n4)                           __doc_##n1##_##n2##_##n3##_##n4
#define __DOC5(n1, n2, n3, n4, n5)                       __doc_##n1##_##n2##_##n3##_##n4##_##n5
#define __DOC6(n1, n2, n3, n4, n5, n6)                   __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define __DOC7(n1, n2, n3, n4, n5, n6, n7)               __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...)                                         __EXPAND(__EXPAND(__CAT2(__DOC, __VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
''', file=out_file)

    for name, _, comment in allocate_doc_names(comments):
        print('\nstatic const char *%s =%sR"doc(%s)doc";' %
              (name, '\n' if '\n' in comment else ' ', comment), file=out_file)

    print('''
#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif
''', file=out_file)


def mkdoc(args, output=None):
    parameters, filenames = read_args(args)

    with ThreadPoolExecutor() as pool:
        results = pool.map(lambda fn: extract_file(fn, parameters), filenames)
        comments = [entry for result in results for entry in result]

    if not output:
        write_header(comments)
        return

    try:
        os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
        with open(output, 'w') as out_file:
            write_header(comments, out_file)
    except BaseException:
        # Don't leave a partially written output file behind
        if os.path.exists(output):
            os.unlink(output)
        raise


def main():
    parser = argparse.ArgumentParser(
        prog='mkdocs.py',
        description='Extract C++ header comments for use in nanobind bindings.',
        epilog='Any other argument is forwarded to Clang as a compiler flag.',
        allow_abbrev=False)
    parser.add_argument('-o', '--output', metavar='<file>',
                        help='Write to the specified file (default: stdout).')
    parser.add_argument('header', nargs='+', metavar='<header>',
                        help='A header file to process.')

    parsed, unparsed = parser.parse_known_args()
    mkdoc(unparsed + parsed.header, parsed.output)


if __name__ == '__main__':
    main()
