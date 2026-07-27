#!/usr/bin/env python
#
# This script walks through all plugin files and
# extracts documentation that should go into the
# reference manual

import os
import re
import importlib

SHAPE_ORDERING = [
    'obj',
    'ply',
    'serialized',
    'cube',
    'sphere',
    'rectangle',
    'disk',
    'cylinder',
    'bsplinecurve',
    'linearcurve',
    'sdfgrid',
    'shapegroup',
    'instance',
    'ellipsoids',
    'ellipsoidsmesh'
]

BSDF_ORDERING = [
    'acousticbsdf',
]

SENSOR_ORDERING = [
    'microphone',
]

TEXTURE_ORDERING = [
    'bitmap',
    'checkerboard',
    'mesh_attribute',
    'volume'
]

SPECTRUM_ORDERING = [
    'uniform',
    'regular',
    'irregular',
    'srgb',
    'd65',
    'blackbody'
    'rawconstant'
]

SAMPLER_ORDERING = [
    'independent',
    'stratified',
    'multijitter',
    'orthogonal',
    'ldsampler'
]

INTEGRATOR_ORDERING = [
    'acoustic_path',
]

FILM_ORDERING = [
    'tape',
]

RFILTER_ORDERING = [
    'box',
    'tent',
    'gaussian',
    'mitchell',
    'catmullrom',
    'lanczos'
]


def find_order_id(filename, ordering):
    f = os.path.split(filename)[-1].split('.')[0]
    if ordering and f in ordering:
        return ordering.index(f)
    elif filename in ordering:
        return ordering.index(filename)
    else:
        return 1000

def extract(target, filename):
    f = open(filename, encoding='utf-8')
    inheader = False
    for line in f.readlines():
        match = re.match(r'^/\*\*! ?(.*)$', line)
        if match is not None:
            print("Processing %s" % filename)
            line = match.group(1).replace('%', '\%')
            target.write(line + '\n')
            inheader = True
            continue
        if not inheader:
            continue
        if re.search(r'^[\s\*]*\*/$', line):
            inheader = False
            continue
        target.write(line)
    f.close()

def extract_python(target, filename):
    f = open(filename, encoding='utf-8')
    inheader = False
    for line in f.readlines():
        # Remove indentation
        if line.startswith('    '):
            line = line[4:]
        match_beg = re.match(r'r\"\"\"', line)
        match_end = re.match(r'\"\"\"',  line)
        if not inheader and match_beg is not None:
            print("Processing %s" % filename)
            inheader = True
            continue
        if inheader and match_end is not None:
            inheader = False
            continue
        if not inheader:
            continue
        target.write(line)
    f.close()

# Traverse source directories and process any found plugin code

def process(path, target, ordering, allowlist=None):
    def capture(file_list, dirname, files):
        suffix = os.path.split(dirname)[1]
        if 'lib' in suffix or suffix == 'tests' \
                or suffix == 'mitsuba' or suffix == 'utils' \
                or suffix == 'converter':
            return
        for filename in files:
            if '.cpp' == os.path.splitext(filename)[1]:
                if allowlist is not None and \
                        os.path.splitext(filename)[0] not in allowlist:
                    continue
                fname = os.path.join(dirname, filename)
                file_list += [fname]

    file_list = []
    for (dirname, _, files) in os.walk(path):
        capture(file_list, dirname, files)

    for o in ordering:
        if o.endswith('.py'):
            file_list.append(o)

    ordering = [(find_order_id(fname, ordering), fname) for fname in file_list]
    ordering = sorted(ordering, key=lambda entry: entry[0])

    for entry in ordering:
        if entry[1].endswith('.py'):
            extract_python(target, entry[1])
        else:
            extract(target, entry[1])


def process_src(target, src_subdir, ordering=None, allowlist=None):
    section = "section_" + src_subdir

    # Copy paste the contents of the appropriate section file
    with open('src/plugin_reference/' + section + '.rst', 'r', encoding='utf-8') as f:
        target.write(f.read())
    process('../src/{0}'.format(src_subdir), target, ordering, allowlist)


def generate(build_dir):
    original_wd = os.getcwd()
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    # Sections shared with upstream Mitsuba, kept in full.
    # Sections filtered down to misuka's acoustic plugins only (thin fork:
    # optical bsdfs/sensors/films/integrators are documented upstream).
    sections = [
        ('shapes',      SHAPE_ORDERING,     None),
        ('bsdfs',       BSDF_ORDERING,      BSDF_ORDERING),
        ('sensors',     SENSOR_ORDERING,    SENSOR_ORDERING),
        ('textures',    TEXTURE_ORDERING,   None),
        ('spectra',     SPECTRUM_ORDERING,  None),
        ('integrators', INTEGRATOR_ORDERING, INTEGRATOR_ORDERING),
        ('samplers',    SAMPLER_ORDERING,   None),
        ('films',       FILM_ORDERING,      FILM_ORDERING),
        ('rfilters',    RFILTER_ORDERING,   None),
    ]

    for section, ordering, allowlist in sections:
        with open(os.path.join(build_dir, f'plugins_{section}.rst'), 'w', encoding='utf-8') as f:
            process_src(f, section, ordering, allowlist)

    os.chdir(original_wd)

if __name__ == "__main__":
    generate()
