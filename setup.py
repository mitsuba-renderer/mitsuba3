# -*- coding: utf-8 -*-

from __future__ import print_function

import sys, re, os, pathlib

try:
    from skbuild import setup
    import pybind11
except ImportError:
    print("The preferred way to invoke 'setup.py' is via pip, as in 'pip "
          "install .'. If you wish to run the setup script directly, you must "
          "first install the build dependencies listed in pyproject.toml!",
          file=sys.stderr)
    raise

this_directory = os.path.abspath(os.path.dirname(__file__))

with open(os.path.join(this_directory, "include/mitsuba/mitsuba.h")) as f:
    mi_version_regex = re.compile(
        r"^\s*#\s*define\s+MI_VERSION_([A-Z]+)\s+(.*)$", re.MULTILINE)
    matches = dict(mi_version_regex.findall(f.read()))
    mitsuba_version = "{MAJOR}.{MINOR}.{PATCH}".format(**matches)

with open(os.path.join(this_directory, "ext/drjit/include/drjit/fwd.h")) as f:
    drjit_version_regex = re.compile(
        r"^\s*#\s*define\s+DRJIT_VERSION_([A-Z]+)\s+(.*)$", re.MULTILINE)
    matches = dict(drjit_version_regex.findall(f.read()))
    drjit_version = "{MAJOR}.{MINOR}.{PATCH}".format(**matches)

with open(os.path.join(this_directory, 'README.md'), encoding='utf-8') as f:
    long_description = f.read()

long_description = long_description[long_description.find('## Introduction'):]
mi_cmake_toolchain_file = os.environ.get("MI_CMAKE_TOOLCHAIN_FILE", "")
mi_drjit_cmake_dir = os.environ.get("MI_DRJIT_CMAKE_DIR", "")
mi_srgb_coeff_file = os.environ.get("MI_SRGB_COEFF_FILE", "")
mi_python_stubs_dir = os.environ.get("MI_PYTHON_STUBS_DIR", "")
pathlib.Path("./mitsuba").mkdir(exist_ok=True)

setup(
    name="mitsuba",
    version=mitsuba_version,
    author="Realistic Graphics Lab (RGL), EPFL",
    author_email="wenzel.jakob@epfl.ch",
    description="3: A Retargetable Forward and Inverse Renderer",
    url="https://github.com/mitsuba-renderer/mitsuba3",
    license="BSD",
    long_description=long_description,
    long_description_content_type='text/markdown',
    cmake_args=[
        '-DCMAKE_INSTALL_LIBDIR=mitsuba',
        '-DCMAKE_INSTALL_BINDIR=mitsuba',
        '-DCMAKE_INSTALL_INCLUDEDIR=mitsuba/include',
        '-DCMAKE_INSTALL_DATAROOTDIR=mitsuba/data',
        f'-DCMAKE_TOOLCHAIN_FILE={mi_cmake_toolchain_file}',
        f'-DMI_DRJIT_CMAKE_DIR:STRING={mi_drjit_cmake_dir}',
        f'-DMI_SRGB_COEFF_FILE:STRING={mi_srgb_coeff_file}',
        f'-DMI_PYTHON_STUBS_DIR:STRING={mi_python_stubs_dir}'
    ],
    install_requires=[f"drjit=={drjit_version}"],
    packages=['mitsuba'],
    entry_points={
        'console_scripts': [
            'mitsuba = mitsuba.cli:_main',
        ]
    },
    python_requires=">=3.8"
)
