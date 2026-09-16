"""Pack the bitmap textures of a Mitsuba scene into a ``.packed`` container.

This script rewrites a Mitsuba XML scene file and packs all of its textures
into a single compressed file that stores block-compressed textures (BC4, BC5,
or BC7 depending on the texture type). This greatly reduces the memory usage
of the scene on GPU backends, which sample these formats directly using the
hardware texture units, and it accelerates scene loading since the block data
can be uploaded to the GPU directly and decodes much faster than JPG or PNG
files. Uniform textures are replaced by constant colors in the rewritten scene.

Example usage:

  python -m mitsuba.pack_tex scene.xml                  # writes scene_packed.xml and textures.packed
  python -m mitsuba.pack_tex scene.xml --mip -j 4       # also store MIP chains, 4 textures at a time
  python -m mitsuba.pack_tex scene.xml -o out/scene.xml --container out/tex.packed

Every ``<texture type="bitmap">`` with a ``filename`` is compressed with
bc7enc_rdo at maximum quality. The format follows from how the scene uses the
texture:

  BC7  color data (e.g. ``base_color``), kept sRGB-encoded unless ``raw``
  BC5  normal maps (the ``normalmap`` BSDF and the ``normal_map`` texture)
  BC4  single-channel data such as roughness or opacity, reduced to the
       luminance that Mitsuba would compute from a color image

Files with identical contents that are used in the same way share one entry.
The rewritten scene references the container through the ``filename`` and
``name`` parameters of each bitmap texture. The "File formats" section of the
Mitsuba documentation describes the container. The encoder is available at
https://github.com/richgel999/bc7enc_rdo and is located through ``--bc7enc``.
"""
import argparse, hashlib, os, subprocess, struct, sys, tempfile, shutil, time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
from xml.etree import ElementTree as ET

import drjit as dr
import mitsuba as mi
from drjit.auto import Float, TensorXf, TensorXu8, UInt8, UInt32

try:
    from tqdm import tqdm
except ImportError:
    class tqdm:
        """Stand-in without a progress bar"""
        def __init__(self, **kwargs): pass
        def __enter__(self): return self
        def __exit__(self, *args): pass
        def write(self, s): print(s, flush=True)
        def update(self): pass

# Texture parameters that Mitsuba evaluates as a single channel (``eval_1``)
SCALAR_PARAMS = {
    'roughness', 'alpha', 'alpha_u', 'alpha_v', 'opacity', 'metallic',
    'specular', 'spec_trans', 'anisotropic', 'sheen', 'sheen_tint',
    'clearcoat', 'clearcoat_gloss', 'flatness', 'spec_tint', 'diff_trans',
    'weight', 'exponent', 'factor', 'fac', 'brightness', 'contrast', 'hue',
    'saturation', 'value', 'strength', 'displacement', 'height', 'transmittance'
}

# (parent element type, parameter name) pairs evaluated as a single channel
SCALAR_SLOTS = {('color_ramp', 'input'), ('blender_bumpmap', 'texture'), ('bump', 'texture')}

# (parent element type, parameter name) pairs that hold tangent-space normals
NORMAL_SLOTS = {('normalmap', 'normalmap'), ('normal_map', 'texture')}

FORMAT_INFO = {   # kind -> (format number, block bytes, bc7enc flags)
    'color':  (7, 16, []),
    'normal': (5, 16, ['-5', '-hr32']),
    'scalar': (4, 8,  ['-4', '-hr32']),
}

LUMINANCE = (0.212671, 0.715160, 0.072169)


def parse_args():
    p = argparse.ArgumentParser(prog='python -m mitsuba.pack_tex', description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('scene')
    p.add_argument('-o', '--output', help='rewritten scene file (default: <scene>_packed.xml)')
    p.add_argument('--container', help='container file (default: textures.packed next to the output)')
    p.add_argument('--bc7enc', default='bc7enc', help='path of the bc7enc_rdo executable')
    p.add_argument('--mip', action='store_true', help='store a complete MIP chain per texture (for trilinear/anisotropic filtering)')
    p.add_argument('-j', '--jobs', type=int, default=None, help='textures compressed concurrently (default: cores / 8)')
    p.add_argument('--tmp', default=None, help='parent directory for intermediate files (default: system temp directory)')
    return p.parse_args()


def quantize(v):
    """Round values in [0, 1] to 8-bit codes (8-bit input passes through)"""
    if isinstance(v, TensorXu8):
        return v
    return TensorXu8(dr.clip(v * 255 + 0.5, 0, 255))


def transfer(a, encode):
    """Apply the sRGB transfer function or its inverse. 8-bit codes go through
    a lookup table and remain 8-bit."""
    f = dr.linear_to_srgb if encode else dr.srgb_to_linear
    if not isinstance(a, TensorXu8):
        return f(a)
    table = quantize(f(TensorXf(dr.arange(Float, 256)) / 255))
    return TensorXu8(dr.gather(UInt8, table.array, UInt32(a.array)), a.shape)


def prepare(a, linear, kind, raw):
    """Convert an image to the 8-bit codes stored in the texture, i.e. what the
    renderer sees after hardware decoding. Returns (codes, srgb) with codes of
    shape (H, W, C)."""
    u8 = isinstance(a, TensorXu8)
    h, w, c = a.shape
    decode = not raw and not linear   # the plugin would decode the sRGB codes

    if kind == 'color':
        rgb = a[..., :3] if c >= 3 else dr.concat([a[..., :1]] * 3, axis=2)
        alpha = a[..., c - 1:c] if c in (2, 4) else dr.full(type(a), 255 if u8 else 1, (h, w, 1))
        if not raw and linear:
            rgb = transfer(rgb, encode=True)
        return quantize(dr.concat([rgb, alpha], axis=2)), not raw

    if kind == 'normal':
        if c < 2:
            raise ValueError('a normal map needs at least two channels')
        xy = a[..., :2]
        return quantize(transfer(xy, encode=False) if decode else xy), False

    # Scalar: luminance of the value that Mitsuba would evaluate
    if c < 3:
        v = a[..., :1]
        return quantize(transfer(v, encode=False) if decode else v), False
    rgb = TensorXf(a[..., :3]) / 255 if u8 else a[..., :3]
    if decode:
        rgb = dr.srgb_to_linear(rgb)
    return quantize(sum(rgb[..., i:i + 1] * LUMINANCE[i] for i in range(3))), False


def downsample(v, srgb):
    """Halve the resolution (rounding down, but never below one texel) with a
    2x2 box filter, averaging sRGB data in linear space as the renderer does"""
    h, w, c = v.shape
    nh, nw = max(h // 2, 1), max(w // 2, 1)
    if srgb:
        v = dr.concat([dr.srgb_to_linear(v[..., :3]), v[..., 3:]], axis=2)
    # Along an axis with a single texel, both taps read that texel
    y0, x0 = slice(0, 2 * nh, 2), slice(0, 2 * nw, 2)
    y1 = slice(1, 2 * nh, 2) if h > 1 else y0
    x1 = slice(1, 2 * nw, 2) if w > 1 else x0
    r = (v[y0, x0] + v[y1, x0] + v[y0, x1] + v[y1, x1]) * 0.25
    if srgb:
        r = dr.concat([dr.linear_to_srgb(r[..., :3]), r[..., 3:]], axis=2)
    return r


def read_dds(path, w, h, block_bytes):
    """Extract the block data of a DDS file written by bc7enc"""
    d = open(path, 'rb').read()
    if d[:4] != b'DDS ':
        raise RuntimeError(f'{path}: not a DDS file')
    dh, dw = struct.unpack_from('<II', d, 12)
    if (dw, dh) != (w, h):
        raise RuntimeError(f'{path}: unexpected size {dw}x{dh}, expected {w}x{h}')
    off = 128 + (20 if d[84:88] == b'DX10' else 0)
    n = ((w + 3) // 4) * ((h + 3) // 4) * block_bytes
    if len(d) < off + n:
        raise RuntimeError(f'{path}: truncated block data')
    return d[off:off + n]


def compress(key, name, tmp, args, omp_threads):
    """Prepare, MIP-map, block-compress, and LZ4-compress one texture, with
    intermediate files at ``tmp``. Returns the entry record, or just the
    constant color of a uniform texture."""
    path, kind, raw = key
    fmt, block_bytes, flags = FORMAT_INFO[kind]
    bitmap = mi.Bitmap(path)
    u8 = bitmap.component_format() == mi.Struct.Type.UInt8
    a = TensorXu8(bitmap) if u8 else TensorXf(bitmap.convert(component_format=mi.Struct.Type.Float32))
    linear = not bitmap.srgb_gamma()
    c = bitmap.channel_count()

    if dr.all(a == a[0:1, 0:1], axis=None):
        # An ``<rgb>`` tag replaces the texture. Scalar slots get the luminance
        # in all channels since the ``rgb`` texture answers scalar queries with
        # the channel mean, whereas a bitmap returns the luminance.
        px = [x / (255 if u8 else 1) for x in a[0, 0].array]
        rgb = px[:3] if c >= 3 else px[:1] * 3
        if not raw and not linear:
            rgb = [dr.srgb_to_linear(x) for x in rgb]
        if kind == 'scalar':
            rgb = [sum(x * l for x, l in zip(rgb, LUMINANCE))] * 3
        return dict(name=name, constant=rgb)

    v, srgb = prepare(a, linear, kind, raw)
    h, w = v.shape[:2]

    # MIP levels are averaged in floating point, starting from the stored codes
    # of the base level
    levels = [v]
    if args.mip:
        x = TensorXf(v) / 255
        while max(x.shape[:2]) > 1:
            x = downsample(x, srgb)
            levels.append(quantize(x))

    # bc7enc can read the base level straight from an 8-bit PNG whenever
    # prepare() only rearranged channels, since its loader expands gray and
    # RGB images to RGBA in the same way
    if kind == 'color':
        direct = raw or not linear
    else:
        direct = (raw or linear) and c in ((3, 4) if kind == 'normal' else (1, 2))
    direct = direct and u8 and path.lower().endswith('.png')

    env = dict(os.environ, OMP_NUM_THREADS=str(omp_threads))
    png, dds = tmp + '.png', tmp + '.dds'
    # The entry: an uncompressed header followed by one compressed array per
    # MIP level (see the "File formats" documentation)
    entry = bytearray(b'BTEX' + struct.pack('<I4BII', 1, fmt, srgb, len(levels), v.shape[2], w, h))
    size = 0
    for l, lv in enumerate(levels):
        if l == 0 and direct:
            src = path
        else:
            src = png
            if lv.shape[2] == 2:   # bc7enc reads RG from an RGB image
                lv = dr.concat([lv, dr.zeros(TensorXu8, lv.shape[:2] + (1,))], axis=2)
            mi.Bitmap(lv).write(png, quality=1)
        cmd = [args.bc7enc, '-q', '-g', '-f'] + flags + [src, dds]
        r = subprocess.run(cmd, env=env, capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f'{" ".join(cmd)} failed:\n{r.stdout}\n{r.stderr}')
        blocks = read_dds(dds, lv.shape[1], lv.shape[0], block_bytes)
        entry += mi.PackedFile.compress_array(blocks)
        size += len(blocks)
    for f in (png, dds):
        if os.path.exists(f):
            os.remove(f)

    return dict(name=name, width=w, height=h, format=fmt, srgb=srgb,
                n_levels=len(levels), size=size, packed=bytes(entry),
                src_bytes=os.path.getsize(path),
                texel_bytes=w * h * (1 if kind == 'scalar' else 4))


def main():
    args = parse_args()
    mi.set_variant('scalar_rgb')
    scene = os.path.abspath(args.scene)
    scene_dir = os.path.dirname(scene)
    output = os.path.abspath(args.output or os.path.splitext(scene)[0] + '_packed.xml')
    container = os.path.abspath(args.container or os.path.join(os.path.dirname(output), 'textures.packed'))
    if shutil.which(args.bc7enc) is None:
        sys.exit(f'bc7enc executable not found: {args.bc7enc} (use --bc7enc)')

    parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))
    tree = ET.parse(scene, parser=parser)
    root = tree.getroot()
    parent_of = {c: p for p in root.iter() for c in p}

    # Collect the bitmap textures and decide on a format for each usage
    uses = []      # (element, filename element, (content hash, kind, raw), absolute path)
    digests = {}   # absolute path -> content hash
    for tex in root.iter('texture'):
        fn = tex.find('string[@name="filename"]')
        if tex.get('type') != 'bitmap' or fn is None:
            continue
        r = tex.find('boolean[@name="raw"]')
        raw = r is not None and r.get('value') == 'true'
        path = os.path.normpath(os.path.join(scene_dir, fn.get('value')))
        if not os.path.exists(path):
            sys.exit(f'texture not found: {path}')
        slot = (parent_of[tex].get('type'), tex.get('name'))
        if slot in NORMAL_SLOTS:
            kind = 'normal'
        elif slot in SCALAR_SLOTS or slot[1] in SCALAR_PARAMS:
            kind = 'scalar'
        else:
            kind = 'color'
        if path not in digests:
            with open(path, 'rb') as f:
                digests[path] = hashlib.sha256(f.read()).hexdigest()
        uses.append((tex, fn, (digests[path], kind, raw), path))

    # One entry per (file contents, kind, raw), named after the first file with
    # these contents, plus a suffix when the contents are used in several ways
    keys = {}   # key -> (absolute path, filename as written in the scene)
    for tex, fn, key, path in uses:
        keys.setdefault(key, (path, fn.get('value')))
    per_file = Counter(digest for digest, _, _ in keys)
    names = {}
    for (digest, kind, raw), (path, fn) in keys.items():
        name = os.path.splitext(fn.replace('\\', '/'))[0]
        if per_file[digest] > 1:
            name += f'_{kind}' + ('_raw' if raw else '')
        names[(digest, kind, raw)] = name

    counts = Counter(kind for _, kind, _ in names)
    print(f'{len(uses)} bitmap textures, {len(names)} unique: '
          + ', '.join(f'{counts[k]} BC{FORMAT_INFO[k][0]} ({k})' for k in FORMAT_INFO), flush=True)

    ncpu = os.cpu_count() or 1
    jobs = args.jobs or max(1, ncpu // 8)
    omp_threads = max(1, ncpu // jobs)

    t0 = time.time()
    results = {}   # key -> entry record
    partial = container + '.partial'
    pf = mi.PackedFile(partial)
    with tempfile.TemporaryDirectory(prefix='pack_tex_', dir=args.tmp) as tmp, \
         ThreadPoolExecutor(max_workers=jobs) as pool, \
         tqdm(total=len(names), unit='texture', dynamic_ncols=True) as progress:
        futures = {pool.submit(compress, (keys[key][0],) + key[1:], name,
                               os.path.join(tmp, str(i)), args, omp_threads): key
                   for i, (key, name) in enumerate(names.items())}
        # Stream each encoded entry to the container as soon as it is ready
        for fut in as_completed(futures):
            e = results[futures[fut]] = fut.result()
            if 'constant' in e:
                info = 'constant ' + ' '.join(f'{v:.4g}' for v in e['constant'])
            else:
                pf.begin(e['name'])
                pf.stream().write(e.pop('packed'))
                info = (f'{e["width"]}x{e["height"]} BC{e["format"]}{" sRGB" if e["srgb"] else ""}, '
                        f'{e["n_levels"]} level(s), {e["size"] / 2**20:.1f} MiB')
            progress.write(f'{e["name"]}: {info}')
            progress.update()
    pf.close()
    os.replace(partial, container)

    # Rewrite the scene: uniform textures become plain color values, the
    # others reference the container
    rel = os.path.relpath(container, os.path.dirname(output)).replace('\\', '/')
    for tex, fn, key, _ in uses:
        e = results[key]
        parent = parent_of[tex]
        if 'constant' in e:
            value = ' '.join(f'{v:.8g}' for v in e['constant'])
            repl = ET.Element('rgb', name=tex.get('name'), value=value)
            repl.tail = tex.tail
            parent[list(parent).index(tex)] = repl
        else:
            fn.set('value', rel)
            i = list(tex).index(fn)
            name = ET.Element('string', name='name', value=e['name'])
            # ``name`` takes over the whitespace after ``filename``, which in
            # turn receives the indentation preceding it
            name.tail, fn.tail = fn.tail, tex.text if i == 0 else tex[i - 1].tail
            tex.insert(i + 1, name)
    tree.write(output, encoding='utf-8', xml_declaration=True)

    entries = [e for e in results.values() if 'constant' not in e]
    src, texels, blocks = (sum(e[k] for e in entries) for k in ('src_bytes', 'texel_bytes', 'size'))
    print(f'\nWrote {output}\n      {container} ({os.path.getsize(container) / 2**20:.1f} MiB, '
          f'{len(entries)} textures, {len(results) - len(entries)} uniform textures replaced by constants)\n'
          f'Source images: {src / 2**20:.1f} MiB on disk, {texels / 2**20:.1f} MiB as 8-bit texels; '
          f'block-compressed: {blocks / 2**20:.1f} MiB ({texels / max(blocks, 1):.1f}x smaller than 8-bit)\n'
          f'Time: {time.time() - t0:.0f}s')


if __name__ == '__main__':
    main()
