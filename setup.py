# -*- coding: utf-8 -*-

from __future__ import print_function

import sys, re, os

try:
    from skbuild import setup
    import pybind11
except ImportError:
    print("The preferred way to invoke 'setup.py' is via pip, as in 'pip "
          "install .'. If you wish to run the setup script directly, you must "
          "first install the build dependencies listed in pyproject.toml!",
          file=sys.stderr)
    raise

VERSION_REGEX = re.compile(
    r"^\s*#\s*define\s+MTS_VERSION_([A-Z]+)\s+(.*)$", re.MULTILINE)

this_directory = os.path.abspath(os.path.dirname(__file__))

with open(os.path.join("include/mitsuba/mitsuba.h")) as f:
    matches = dict(VERSION_REGEX.findall(f.read()))
    mitsuba_version = "{MAJOR}.{MINOR}.{PATCH}".format(**matches)

with open(os.path.join(this_directory, 'README.md'), encoding='utf-8') as f:
    long_description = f.read()

long_description = long_description[long_description.find('## Introduction'):]

setup(
    name="mitsuba",
    version=mitsuba_version,
    author="Realistic Graphics Lab (RGL), EPFL",
    author_email="wenzel.jakob@epfl.ch",
    description="Mitsuba 2: A Retargetable Forward and Inverse Renderer",
    url="https://github.com/mitsuba-renderer/mitsuba2",
    license="BSD",
    long_description=long_description,
    long_description_content_type='text/markdown',
    cmake_args=[
        '-DCMAKE_INSTALL_LIBDIR=mitsuba',
        '-DCMAKE_INSTALL_BINDIR=mitsuba',
        '-DCMAKE_INSTALL_INCLUDEDIR=mitsuba/include',
        '-DCMAKE_INSTALL_DATAROOTDIR=mitsuba/data'
    ],
    scripts=['resources/mitsuba'],
    packages=['mitsuba']
)
