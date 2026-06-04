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
    'diffuse',
    'neuralbsdf',
    'dielectric',
    'thindielectric',
    'roughdielectric',
    'conductor',
    'roughconductor',
    'hair',
    'measured',
    'measured_polarized',
    'plastic',
    'roughplastic',
    'bumpmap',
    'normalmap',
    'blendbsdf',
    'mask',
    'twosided',
    'null',
    'polarizer',
    'retarder',
    'circular',
    'pplastic'
]

EMITTER_ORDERING = [
    'area',
    'point',
    'constant',
    'envmap',
    'sunsky',
    'spot',
    'projector'
    'directional',
    'directionalarea',
]

SENSOR_ORDERING = [
    'orthographic',
    'perspective',
    'thinlens'
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
    'direct',
    'path',
    'aov',
    'volpath',
    'volpathmis',
    '../src/python/python/ad/integrators/prb.py',
    '../src/python/python/ad/integrators/prb_basic.py',
    '../src/python/python/ad/integrators/direct_projective.py',
    '../src/python/python/ad/integrators/prb_projective.py',
    '../src/python/python/ad/integrators/prbvolpath.py',
]

FILM_ORDERING = [
    'hdrfilm',
    'specfilm'
]

RFILTER_ORDERING = [
    'box',
    'tent',
    'gaussian',
    'mitchell',
    'catmullrom',
    'lanczos'
]

MEDIUM_ORDERING = [
    'homogeneous',
    'heterogeneous',
]

PHASE_ORDERING = [
    'isotropic',
    'hg',
    'sggx'
]

VOLUME_ORDERING = [
    'constvolume',
    'gridvolume'
]

FIELD_ORDERING = [
    'sinusoidal',
    '../src/python/python/fields/hashgrid.py',
    '../src/python/python/fields/permuto.py',
    '../src/python/python/fields/neural.py'
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
            line = match.group(1).replace('%', '\\%')
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
    indent = ''
    for line in f.readlines():
        if inheader and indent and line.startswith(indent):
            line = line[len(indent):]

        match_beg = re.match(r'^(\s*)r\"\"\"', line)
        match_end = re.match(r'\"\"\"',  line)
        if not inheader and match_beg is not None:
            print("Processing %s" % filename)
            indent = match_beg.group(1)
            inheader = True
            continue
        if inheader and match_end is not None:
            inheader = False
            indent = ''
            continue
        if not inheader:
            continue
        target.write(line)
    f.close()

# Traverse source directories and process any found plugin code

def process(path, target, ordering, include_files=None):
    def capture(file_list, dirname, files):
        suffix = os.path.split(dirname)[1]
        if 'lib' in suffix or suffix == 'tests' \
                or suffix == 'mitsuba' or suffix == 'utils' \
                or suffix == 'converter':
            return
        for filename in files:
            name, ext = os.path.splitext(filename)
            if ext == '.cpp' and (include_files is None or name in include_files):
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


def process_src(target, src_subdir, ordering=None, source_subdir=None,
                include_files=None):
    section = "section_" + src_subdir

    # Copy paste the contents of the appropriate section file
    with open('src/plugin_reference/' + section + '.rst', 'r', encoding='utf-8') as f:
        target.write(f.read())
    if source_subdir is None:
        source_subdir = src_subdir
    process('../src/{0}'.format(source_subdir), target, ordering, include_files)


def generate(build_dir):
    original_wd = os.getcwd()
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    sections = [
        ('shapes',      SHAPE_ORDERING,   None,     None),
        ('bsdfs',       BSDF_ORDERING,    None,     None),
        ('media',       MEDIUM_ORDERING,  None,     None),
        ('phase',       PHASE_ORDERING,   None,     None),
        ('emitters',    EMITTER_ORDERING, None,     None),
        ('sensors',     SENSOR_ORDERING,  None,     None),
        ('textures',    TEXTURE_ORDERING, 'fields', TEXTURE_ORDERING),
        ('fields',      FIELD_ORDERING,   'fields', FIELD_ORDERING),
        ('spectra',     SPECTRUM_ORDERING, None,    None),
        ('integrators', INTEGRATOR_ORDERING, None,  None),
        ('samplers',    SAMPLER_ORDERING, None,     None),
        ('films',       FILM_ORDERING,    None,     None),
        ('rfilters',    RFILTER_ORDERING, None,     None),
        ('volumes',     VOLUME_ORDERING,  'fields', VOLUME_ORDERING)
    ]

    for section, ordering, source_subdir, include_files in sections:
        with open(os.path.join(build_dir, f'plugins_{section}.rst'), 'w', encoding='utf-8') as f:
            process_src(f, section, ordering, source_subdir, include_files)

    os.chdir(original_wd)

if __name__ == "__main__":
    import sys
    generate(sys.argv[1] if len(sys.argv) > 1 else os.getcwd())
