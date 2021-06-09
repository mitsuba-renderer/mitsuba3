#!/usr/bin/env python
#
# This script walks through all plugin files and
# extracts documentation that should go into the
# reference manual

import os
import re

SHAPE_ORDERING = ['obj',
                  'ply',
                  'serialized',
                  'sphere',
                  'cylinder',
                  'disk',
                  'rectangle',
                  'shapegroup',
                  'instance']

BSDF_ORDERING = ['diffuse',
                 'dielectric',
                 'thindielectric',
                 'roughdielectric',
                 'conductor',
                 'roughconductor',
                 'plastic',
                 'roughplastic',
                 'measured',
                 'bumpmap',
                 'normalmap',
                 'blendbsdf',
                 'mask',
                 'twosided',
                 'null',
                 'polarizer',
                 'retarder',
                 'circular',
                 'measured_polarized',
                 'pplastic']

EMITTER_ORDERING = ['area',
                    'point',
                    'constant',
                    'envmap',
                    'spot',
                    'projector']

SENSOR_ORDERING = ['perspective',
                   'thinlens']

TEXTURE_ORDERING = ['bitmap',
                    'checkerboard']

SPECTRUM_ORDERING = ['uniform',
                     'regular',
                     'irregular',
                     'srgb',
                     'd65',
                     'srgb_d65',
                     'blackbody']

SAMPLER_ORDERING = ['independent',
                    'stratified',
                    'multijitter',
                    'orthogonal',
                    'ldsampler']

INTEGRATOR_ORDERING = ['direct',
                       'path',
                       'aov',
                       'volpath',
                       'volpathmis']

FILM_ORDERING = ['hdrfilm']

RFILTER_ORDERING = ['box',
                    'tent',
                    'gaussian',
                    'mitchell',
                    'catmullrom',
                    'lanczos']

MEDIUM_ORDERING = ['homogeneous',
                   'heterogeneous']

PHASE_ORDERING = ['isotropic',
                  'hg',
                  'sggx']

VOLUME_ORDERING = ['constant3d',
                   'grid3d']


def find_order_id(filename, ordering):
    f = os.path.split(filename)[-1].split('.')[0]
    if ordering and f in ordering:
        return ordering.index(f)
    else:
        return 1000

def extract(target, filename):
    f = open(filename)
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

# Traverse source directories and process any found plugin code


def process(path, target, ordering):
    def capture(fileList, dirname, files):
        suffix = os.path.split(dirname)[1]
        if 'lib' in suffix or suffix == 'tests' \
                or suffix == 'mitsuba' or suffix == 'utils' \
                or suffix == 'converter' or suffix == 'mtsgui':
            return
        for filename in files:
            if '.cpp' == os.path.splitext(filename)[1]:
                fname = os.path.join(dirname, filename)
                fileList += [fname]

    fileList = []
    for (dirname, _, files) in os.walk(path):
        capture(fileList, dirname, files)

    ordering = [(find_order_id(fname, ordering), fname) for fname in fileList]
    ordering = sorted(ordering, key=lambda entry: entry[0])

    for entry in ordering:
        extract(target, entry[1])


def process_src(target, src_subdir, ordering=None):
    section = "section_" + src_subdir

    # Copy paste the contents of the appropriate section file
    with open('src/plugin_reference/' + section + '.rst', 'r') as f:
        target.write(f.read())
    process('../src/{0}'.format(src_subdir), target, ordering)


def generate(build_dir):
    original_wd = os.getcwd()
    os.chdir(os.path.dirname(os.path.abspath(__file__)))

    sections = [('shapes',      SHAPE_ORDERING),
                ('bsdfs',       BSDF_ORDERING),
                ('media',       MEDIUM_ORDERING),
                ('phase',       PHASE_ORDERING),
                ('emitters',    EMITTER_ORDERING),
                ('sensors',     SENSOR_ORDERING),
                ('textures',    TEXTURE_ORDERING),
                ('spectra',     SPECTRUM_ORDERING),
                ('integrators', INTEGRATOR_ORDERING),
                ('samplers',    SAMPLER_ORDERING),
                ('films',       FILM_ORDERING),
                ('rfilters',    RFILTER_ORDERING),
                ('volumes',     VOLUME_ORDERING)]

    for section, ordering in sections:

        with open(os.path.join(build_dir, f'plugins_{section}.rst'), 'w') as f:
            process_src(f, section, ordering)

    os.chdir(original_wd)

if __name__ == "__main__":
    generate()
