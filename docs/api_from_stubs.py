#!/usr/bin/env python3
"""Generate the API reference from Mitsuba's type stubs.

The stubs (``mitsuba/*.pyi``) ship inside the wheel, so this runs without
importing Mitsuba, selecting a variant, or having a working LLVM/CUDA runtime.
They also carry cleaned-up signatures: generic ``drjit.auto`` types instead of
the backend a particular build happened to use, and structured ``@overload``
chains instead of nanobind's concatenated "Overloaded function." text.

Usage: python api_from_stubs.py [<package-dir>] <output-dir>

``<package-dir>`` is the 'mitsuba' directory holding the stubs. Without it,
the stubs of an installed Mitsuba are used, or those of a local build tree.
"""

from __future__ import annotations

import ast
import os
import re
import sys
from collections.abc import Iterable, Iterator
from pathlib import Path
import sphinx_autostub as sas

# Regular expressions that assign the contents of the 'mitsuba' module to
# documentation sections, following the split of the C++ library into
# 'mitsuba/core' and 'mitsuba/render'. Anything unmatched lands in a final
# 'Other' section.
#
# The tables are written so that no name matches patterns in two sections,
# which makes the outcome independent of the order in which the sections
# appear. 'generate' reports a violation of this, along with patterns that
# match nothing.
CORE_SECTIONS: dict[str, list[str]] = {
    'Types: scalars and arrays': ['Bool', r'Float(16|32|64)?(Storage)?',
                                  r'U?Int(32|64)?(Storage)?',
                                  r'TensorXf(16|32)?'],
    'Types: vectors and points': [r'(Scalar)?Vector\w+', r'(Scalar)?Point\w+',
                                  r'(Scalar)?Normal\w+'],
    'Types: transforms and geometry': [r'(Scalar)?Matrix\w+',
                                       r'\w*Transform\w+', r'Frame\w+',
                                       r'(Scalar)?Bounding\w+',
                                       r'(Scalar)?Ray[0-9]\w*',
                                       'RayDifferential3f'],
    'Types: colors and spectra': [r'(Scalar)?Color\w+', 'Spectrum',
                                  'SpectrumEntry', 'UnpolarizedSpectrum'],
    'Object model and plugins': [r'Object\w*', 'Properties', 'PluginManager',
                                 'TraversalCallback', 'ParamFlags',
                                 r'register_\w+'],
    'Files and streams': [r'\w*Stream', 'MemoryMappedFile', 'TensorFile',
                          'FileResolver', 'file_resolver', 'set_file_resolver',
                          r'Struct\w*'],
    'Bitmaps and image processing': [r'Bitmap\w*', 'Resampler',
                                     'FilterBoundaryCondition',
                                     r'\w*ReconstructionFilter'],
    'Logging': [r'Log\w*', 'log_level', 'logger', r'set_log\w+',
                r'\w*Appender', r'\w*Formatter'],
    'Variants and utilities': ['variant', 'variants', 'set_variant',
                               'scoped_set_variant', 'variant_context',
                               r'MI_(?!CIE_)\w+', 'Version', 'Timer',
                               'ArgParser', 'has_flag'],
    'Distributions': [r'(?!Microfacet)\w*Distribution\w*',
                      r'Hierarchical2D\d', r'Marginal\w+2D\d',
                      r'Conditional\w+1D'],
    'Sampling and math': [r'sample_tea_\w+', r'permute\w*', 'sobol_2',
                          'RadicalInverse', 'radical_inverse_2',
                          'coordinate_system', 'dir_to_sph', 'sph_to_dir'],
    'Spectra and color': [r'cie\w*', r'MI_CIE_\w+', 'linear_rgb_rec',
                          'luminance', r'\w+_rgb_spectrum', r'spectrum_\w+',
                          r'srgb_\w+', r'\w+_to_srgb', 'sample_shifted',
                          'depolarizer', 'unpolarized_spectrum'],
}

RENDER_SECTIONS: dict[str, list[str]] = {
    'Scenes': ['Scene', r'load_\w+', 'cornell_box', 'parse_fov', 'traverse',
               'SceneParameters'],
    'Shapes': [r'Shape\w*', r'Mesh\w*', 'Layout', 'DirectedEdge',
               'VertexFlags', 'DiscontinuityFlags'],
    'Interactions and sampling records': [r'\w*Interaction3f',
                                          'PreliminaryIntersection3f',
                                          'RayFlags', r'\w*Sample3f'],
    'BSDFs': [r'BSDF(Ptr|Context|Flags)?', 'TransportMode', r'Microfacet\w*',
              r'fresnel\w*', 'reflect', 'refract', 'eval_reflectance',
              'lookup_ior'],
    'Textures and volumes': [r'Texture\w*', r'Volume\w*'],
    'Participating media': [r'Medium(Ptr)?', r'PhaseFunction\w*',
                            'SGGXPhaseFunctionParams', r'sggx_\w+'],
    'Emitters and sensors': ['Endpoint', r'Emitter\w*', r'Sensor\w*',
                             'ProjectiveCamera', r'\w+_projection'],
    'Films and image blocks': [r'Film\w*', 'ImageBlock', 'Spiral',
                               'OptixDenoiser'],
    'Integrators': ['render', r'\w*Integrator\w*', r'Sampler\w*'],
}

# Names that are implementation detail rather than public API. 'Thread' is a
# stub that Mitsuba keeps for backward compatibility.
EXCLUDED: list[str] = [
    r'_.*', 'casters', 'cast_object', r'mitsuba_\w+', 'DRJIT_STRUCT',
    'get_property', 'set_property', 'float_dtype', 'Thread']

# Modules that serve the implementation rather than the user. They keep a page,
# since annotations refer to what they declare, but stay out of the visible
# navigation. 'mitsuba.detail' holds the 'TransformWrapper' that the Transform
# factory attributes (translate, scale, rotate, look_at, ...) are annotated
# with, along with the variant switching callbacks.
UNLISTED_MODULES: set[str] = {'mitsuba.detail'}


def strip_template_args(doc: str) -> str:
    """Drop the C++-specific ``Template Args:`` sections from a docstring."""
    return re.sub(r'^Template Args:.*?(?=^\S|\Z)', '', doc, flags=re.M | re.S)


def resolve_compat_aliases(annotation: str) -> str:
    """Rewrite Dr.Jit's ``_<T>Cp`` aliases as the type they convert to.

    Dr.Jit gives each array type an alias that lists everything implicitly
    convertible to it. A type checker needs that union, a reader needs the
    type itself.
    """
    return re.sub(r'\b_(\w+)Cp\b', r'\1', annotation)


def reexports(pkg_dir: str) -> tuple[set[str], dict[str, str]]:
    """Read the top-level stub's re-export declarations.

    Returns ``(submodules, members)``: the submodule names lifted out of
    'mitsuba.python', and a map from the public name of each member lifted out
    of 'mitsuba.python.util' to the name it carries there.
    """
    tree = ast.parse(Path(pkg_dir, '__init__.pyi').read_text('utf-8'))
    submodules: set[str] = set()
    members: dict[str, str] = {}
    for node in tree.body:
        if not isinstance(node, ast.ImportFrom) or not node.module:
            continue
        # stubgen writes these imports in absolute form, the pattern file in
        # 'src/python/stubs.pat' in relative form.
        module = re.sub(r'^mitsuba\.', '', node.module)
        if module == 'python':
            submodules |= {a.name for a in node.names}
        elif module == 'python.util':
            members.update({a.asname or a.name: a.name for a in node.names})
    return submodules, members


def alias_entry(name: str, target: str) -> list[str]:
    """Render a name that is only a second spelling of another entry."""
    return [name, '-' * len(name), '', 'Alias of :py:obj:`%s`.' % target, '']


def top_level_entries(renderer: sas.Renderer, pkg_dir: str,
                      members: dict[str, str]) -> dict[str, list[str]]:
    """Collect the entries of the 'mitsuba' namespace itself.

    Members that 'mitsuba' lifts out of 'mitsuba.python.util' (render,
    traverse, SceneParameters, ...) are headline API and are reached under
    their short names, so they are documented here rather than on the
    'mitsuba.util' page. One that is renamed on the way gets an alias entry
    pointing at the name it is documented under.
    """
    _, entries = renderer.collect(os.path.join(pkg_dir, '__init__.pyi'))
    util_path = os.path.join(pkg_dir, 'python', 'util.pyi')
    if members and os.path.exists(util_path):
        _, util_entries = renderer.collect(util_path)
        for public, original in members.items():
            if public in entries or original not in util_entries:
                continue
            entries[public] = (util_entries[original] if public == original
                               else alias_entry(public, original))
    return entries


def write_section_pages(renderer: sas.Renderer, pkg_dir: str,
                        members: dict[str, str],
                        out_dir: str) -> tuple[list[str], list[str]]:
    """Split the 'mitsuba' namespace into the thematic section pages.

    Returns the core and the rendering page slugs. Both tables partition the
    namespace in one pass, so a name that appears in both goes to the core
    section that claims it, and the 'Other' catch-all closes the second group.
    """
    entries = top_level_entries(renderer, pkg_dir, members)
    core: list[str] = []
    render: list[str] = []
    for title, names in sas.partition(entries,
                                      {**CORE_SECTIONS, **RENDER_SECTIONS},
                                      other='Other', warn=print):
        slug = 'mitsuba_' + re.sub(r'\W+', '_', title.lower())
        sas.write_page(out_dir, slug, title, [entries[n] for n in names],
                       module='mitsuba')
        (core if title in CORE_SECTIONS else render).append(slug)
    return core, render


def stub_modules(pkg_dir: str,
                 submodules: set[str]) -> Iterator[tuple[str, str]]:
    """Yield ``(public module name, stub path)`` for each submodule stub.

    A submodule re-exported at the top level is documented there:
    'mitsuba.python.tensor_io' is reached as 'mitsuba.tensor_io', and so is
    everything below it ('mitsuba.python.ad.largesteps').
    """
    for module, path in sas.walk_stubs(pkg_dir, 'mitsuba'):
        # The top-level stub is split across the section pages instead.
        if module == 'mitsuba':
            continue
        if module.startswith('mitsuba.python.'):
            tail = module[len('mitsuba.python.'):]
            if tail.partition('.')[0] in submodules:
                module = 'mitsuba.' + tail
        yield module, path


def write_module_pages(renderer: sas.Renderer, pkg_dir: str,
                       submodules: set[str], members: dict[str, str],
                       out_dir: str) -> list[str]:
    """Write one page per submodule, under its public name."""
    lifted = set(members.values())
    modules: list[str] = []
    for module, path in stub_modules(pkg_dir, submodules):
        intro, entries = renderer.collect(path)
        if module == 'mitsuba.util':
            # The lifted members are already on the top-level pages.
            entries = {n: v for n, v in entries.items() if n not in lifted}
        if not entries:
            continue
        modules.append(module)
        sas.write_page(out_dir, module.replace('.', '_'), module,
                       [entries[n] for n in sorted(entries, key=str.lower)],
                       module=module, intro=intro)
    return modules


def write_toctree_fragment(out_dir: str, core: list[str], render: list[str],
                           modules: list[str]) -> None:
    """Write the page groups as a fragment for 'api_reference.rst'.

    The 'mitsuba' namespace reads in a deliberate order, first the core
    functionality and then what builds a renderer out of it, while the
    submodules stay a flat alphabetical list. Interleaving the two sorts
    'Types' next to 'mitsuba.tensor_io', which tells a reader nothing. The
    '.txt' suffix keeps Sphinx from also picking the fragment up as a page in
    its own right.
    """
    prefix = '/generated/api/'
    slugs = [m.replace('.', '_') for m in sorted(modules)]
    unlisted = {m.replace('.', '_') for m in UNLISTED_MODULES}
    sas.write_text(
        os.path.join(out_dir, 'toctree.txt'),
        sas.toctree('Core', core, prefix=prefix) +
        sas.toctree('Rendering', render, prefix=prefix) +
        sas.toctree('Submodules', [s for s in slugs if s not in unlisted],
                    prefix=prefix) +
        sas.toctree(None, [s for s in slugs if s in unlisted], prefix=prefix,
                    hidden=True))


def prune_stale_pages(out_dir: str, slugs: Iterable[str]) -> list[str]:
    """Delete pages that a previous run wrote and the current layout has not.

    A renamed section would otherwise leave its old page behind, where Sphinx
    picks it up, warns that no toctree includes it, and copies it into the
    rendered documentation.
    """
    keep = {slug + '.rst' for slug in slugs}
    stale = sorted(p for p in os.listdir(out_dir)
                   if p.endswith('.rst') and p not in keep)
    for name in stale:
        os.remove(os.path.join(out_dir, name))
    return stale


def report_mismatches(renderer: sas.Renderer) -> None:
    """Print parameters that are documented but not accepted by the binding."""
    if not renderer.param_mismatches:
        return
    print('\n%d documented parameters are not accepted by the binding. These '
          'usually indicate a stale docstring in the binding source:' %
          len(renderer.param_mismatches))
    print('\n'.join(
        sas.format_param_mismatches(renderer.param_mismatches)))


def generate(pkg_dir: str, out_dir: str) -> None:
    """Render the reference for the stubs in ``pkg_dir`` into ``out_dir``."""
    # The C++ headers are written in the Google docstring style.
    renderer = sas.Renderer(exclude=EXCLUDED,
                            docstring_filter=strip_template_args,
                            annotation_filter=resolve_compat_aliases,
                            style='google')
    submodules, members = reexports(pkg_dir)
    core, render = write_section_pages(renderer, pkg_dir, members, out_dir)
    modules = write_module_pages(renderer, pkg_dir, submodules, members,
                                 out_dir)
    write_toctree_fragment(out_dir, core, render, modules)
    slugs = core + render + [m.replace('.', '_') for m in modules]
    print('Wrote %d pages to %s' % (len(slugs), out_dir))
    stale = prune_stale_pages(out_dir, slugs)
    if stale:
        print('Removed %d page(s) of an earlier layout: %s' %
              (len(stale), ', '.join(stale)))
    report_mismatches(renderer)


def find_stub_dir() -> str | None:
    """Locate the directory holding ``mitsuba/*.pyi``.

    Prefers a local build tree, so that a developer documents the code at
    hand, and falls back to an installed ``mitsuba`` for a documentation
    build that only has the wheel.
    """
    build = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         '..', 'build', 'python', 'mitsuba')
    if os.path.exists(os.path.join(build, '__init__.pyi')):
        return os.path.normpath(build)
    return sas.find_stub_dir('mitsuba')


if __name__ == '__main__':
    args = sys.argv[1:]
    if len(args) == 1:
        stub_dir = find_stub_dir()
        if stub_dir is None:
            sys.exit('No Mitsuba type stubs found, pass their directory as '
                     'the first argument.')
        args.insert(0, stub_dir)
    if len(args) != 2:
        sys.exit(__doc__)
    generate(args[0], args[1])
