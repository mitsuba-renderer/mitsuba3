import math
from dataclasses import dataclass, replace
from typing import Literal

import drjit as dr
import mitsuba as mi

from .common import ADIntegrator, mis_weight

# ----------------------------- Packing helpers -----------------------------


def oct_encode(n: mi.Vector3f) -> mi.UInt32:
    """Unit vector to two 16-bit octahedral coordinates in one word."""
    norm = dr.abs(n.x) + dr.abs(n.y) + dr.abs(n.z)
    p = dr.select(norm > 0, mi.Vector2f(n.x, n.y) / norm, mi.Vector2f(0))
    fold = (1.0 - dr.abs(mi.Vector2f(p.y, p.x))) * dr.select(p >= 0, 1.0, -1.0)
    p = dr.select(n.z < 0, fold, p)
    q = mi.Vector2u(dr.clip(p * 0.5 + 0.5, 0.0, 1.0) * 65535.0 + 0.5)
    return q.x | (q.y << 16)


def oct_decode(u: mi.UInt32) -> mi.Vector3f:
    p = mi.Vector2f(u & 0xFFFF, u >> 16) / 65535.0 * 2.0 - 1.0
    n = mi.Vector3f(p.x, p.y, 1.0 - dr.abs(p.x) - dr.abs(p.y))
    t = dr.maximum(-n.z, 0.0)
    n.x += dr.select(n.x >= 0, -t, t)
    n.y += dr.select(n.y >= 0, -t, t)
    return dr.normalize(n)


def half_bits(x: mi.Float) -> mi.UInt32:
    """IEEE half of ``x`` in the low 16 bits; round to nearest, denormals to 0."""
    b = dr.reinterpret_array(mi.UInt32, mi.Float(x))
    sign = (b >> 16) & 0x8000
    e = mi.Int32((b >> 23) & 0xFF) - 112
    m = b & 0x7FFFFF
    rounded = (m + 0xFFF + ((m >> 13) & 1)) >> 13
    h = (mi.UInt32(dr.maximum(e, 0)) << 10) + rounded
    h = dr.select(e <= 0, mi.UInt32(0), dr.select(e >= 31, mi.UInt32(0x7C00), h))
    return sign | (h & 0x7FFF)


def half_to_float(h: mi.UInt32) -> mi.Float:
    h = h & 0xFFFF
    sign = (h & 0x8000) << 16
    e = (h >> 10) & 0x1F
    m = h & 0x3FF
    normal = sign | ((e + 112) << 23) | (m << 13)
    b = dr.select(e == 0, sign, dr.select(e == 31, sign | 0x7F800000, normal))
    return dr.reinterpret_array(mi.Float, b)


def pack_rgbe(c: mi.Color3f) -> mi.UInt32:
    """Shared-exponent color: three 8-bit mantissas and a biased 8-bit exponent."""
    c = dr.maximum(c, 0.0)
    m = dr.max(c)
    e = dr.select(m > 0, dr.floor(dr.log2(m)) + 1.0, -128.0)
    e = dr.clip(e, -128.0, 127.0)
    scale = dr.exp2(8.0 - e)
    q = mi.Vector3u(dr.clip(c * scale + 0.5, 0.0, 255.0))
    return q.x | (q.y << 8) | (q.z << 16) | (mi.UInt32(mi.Int32(e) + 128) << 24)


def unpack_rgbe(w: mi.UInt32) -> mi.Color3f:
    e = mi.Float(mi.Int32(w >> 24) - 128)
    scale = dr.exp2(e - 8.0)
    return mi.Color3f(w & 0xFF, (w >> 8) & 0xFF, (w >> 16) & 0xFF) * scale


def pack_unorm2(p: mi.Point2f) -> mi.UInt32:
    q = mi.Vector2u(dr.clip(p, 0.0, 1.0) * 65535.0 + 0.5)
    return q.x | (q.y << 16)


def unpack_unorm2(u: mi.UInt32) -> mi.Point2f:
    return mi.Point2f(u & 0xFFFF, u >> 16) / 65535.0


def bits(x, lo: int, width: int) -> mi.UInt32:
    return (mi.UInt32(x) >> lo) & ((1 << width) - 1)


# ----------------------------- Random streams -----------------------------


class ReSTIRSampler(mi.Sampler):
    """Lane-invariant random stream used to sample and replay stored paths."""

    def __init__(self, stream: int = 0):
        super().__init__(mi.Properties())
        self.stream = stream
        self.rng = mi.PCG32()

    def seed(self, seed, wavefront_size=2**32 - 1):
        if wavefront_size == 2**32 - 1:
            wavefront_size = dr.width(seed)
        super().seed(seed, wavefront_size)
        v0, v1 = mi.sample_tea_32(mi.UInt32(seed), mi.UInt32(self.stream))
        self.rng.seed(mi.UInt64(v0), mi.UInt64(v1))

    def next_1d(self, active=True):
        return mi.Float(self.rng.next_float32(active))

    def next_2d(self, active=True):
        return mi.Point2f(self.next_1d(active), self.next_1d(active))

    def advance(self, n: mi.UInt32):
        """Skip the next ``n`` samples of the stream."""
        self.rng += mi.Int64(n)

    def schedule_state(self):
        super().schedule_state()
        dr.schedule(self.rng.state, self.rng.inc)

    def clone(self):
        sampler = ReSTIRSampler(self.stream)
        if self.wavefront_size() > 0:
            sampler.seed(mi.UInt32(0), self.wavefront_size())
            sampler.rng.state = dr.copy(self.rng.state)
            sampler.rng.inc = dr.copy(self.rng.inc)
        return sampler

    def fork(self):
        return ReSTIRSampler(self.stream)


# ------------------------- Samples and reservoirs -------------------------


class Reservoir:
    """Streaming weighted reservoir sampling over a payload ``z``."""

    def update(
        self,
        z,
        w: mi.Float,
        draw: mi.Float,
        active: mi.Bool,
    ) -> mi.Bool:
        """Add one candidate of weight ``w``; true where it was selected."""
        active = mi.Bool(active) & (w > 0) & dr.isfinite(w)
        self.w += dr.select(active, w, 0.0)
        selected = active & (draw * self.w < w)
        self.z = dr.select(selected, z, self.z)
        return selected

    def contribution_weight(self, phat: mi.Float, norm: mi.Float = 1.0) -> mi.Float:
        """Unbiased contribution weight ``w / (norm * phat)``."""
        return dr.select(phat * norm > 0, self.w / (phat * norm), 0.0)


@dataclass
class EmitterSample:
    """One emitter sample, the payload of RIS over lights (Sec. 6.1)."""

    ds: mi.DirectionSample3f
    em_weight: mi.Color3f
    f: mi.Color3f
    pdf_bsdf: mi.Float
    phat: mi.Float
    contribution: mi.Color3f  # f * em_weight * MIS, zero if occluded
    density: mi.Float  # Emitter sampling density in the connection measure
    index: mi.UInt32  # Number of the candidate in its RIS


@dataclass
class EmitterReservoir(Reservoir):
    """RIS over the emitter samples drawn at one vertex."""

    z: EmitterSample
    w: mi.Float


@dataclass
class Reconnection:
    """The reconnection vertex placed so far (``k = 0``: none yet)."""

    k: mi.UInt32
    hit: mi.PreliminaryIntersection3f
    pos: mi.Point3f
    dir: mi.Vector3f
    arrival_pdf: mi.Float
    bsdf_pdf: mi.Float
    postfix_throughput: mi.Spectrum  # Throughput from reconnection to emitter


@dataclass
class PathSample:
    """One path of the tree, as much of it as a shift needs."""

    seed: mi.UInt32  # Replays the prefix
    d: mi.UInt32  # Index of the light vertex (primary vertex = 1)
    k: mi.UInt32  # Index of x_k (0: replay the whole path)
    nee: mi.Bool  # Light vertex reached by emitter sampling
    delta: mi.Bool  # The light vertex is on a delta light
    emitter_next: mi.Bool  # No scattering vertex between x_k and the light
    # Primitive hit of an interior x_k; prim_uv stores ds.uv for an emitter x_k
    rc_hit: mi.PreliminaryIntersection3f
    rc_pos: mi.Point3f
    rc_dir: mi.Vector3f  # Direction leaving x_k, or of an infinite emitter
    L_rc: mi.Color3f  # Frozen contribution past what the shift rebuilds
    density: mi.Float  # Base path densities the Jacobian divides by
    emitter_pdf: mi.Float  # NEE density for the MIS weight at the light
    emitter: mi.EmitterPtr

    @staticmethod
    def from_emitter(
        seed: mi.UInt32,
        d: mi.UInt32,
        rc: Reconnection,
        nee: bool,
        ds: mi.DirectionSample3f,
        rc_n: mi.Normal3f,
        emitter_density: mi.Float,
        L: mi.Color3f,
        L_emitter: mi.Color3f,
        emitter_pdf: mi.Float,
        count: mi.Float,
        rc_src: mi.Bool = False,
        depth_prev: mi.UInt32 | None = None,
    ) -> "PathSample":
        """A path of ``d`` vertices ending on the emitter ``ds``.

        The path is split into a prefix `x_0` - `x_{k-1}` and suffix `x_k` -
        `x_{d}`. The `trace` method constructs the samples, and adds them to the reservoir.
        ``depth_prev`` is the last scattering vertex before the light
        (default ``d - 1``); null crossings after it are not path vertices.

        """
        z: PathSample = dr.zeros(PathSample)
        z.seed = seed
        z.d = d
        z.nee = mi.Bool(nee)
        z.delta = ds.delta
        z.emitter = ds.emitter
        z.emitter_pdf = emitter_pdf
        z.k = mi.UInt32(rc.k)
        z.k[(rc.k == 0) & (nee | rc_src)] = d
        emitter_next = (z.k > 0) & (z.k == (d - 1 if depth_prev is None else depth_prev))
        z.emitter_next = emitter_next & (z.k < d)
        z.rc_hit = dr.copy(rc.hit)
        z.rc_pos = mi.Point3f(rc.pos)
        z.rc_dir = dr.select(emitter_next & nee, ds.d, rc.dir)
        # The NEE density leaving x_k does not change under the shift
        z.density = rc.arrival_pdf * dr.select(emitter_next & nee, 1.0, rc.bsdf_pdf)
        z.L_rc = rc.postfix_throughput * dr.select(emitter_next, L_emitter, L)

        # The light is x_k
        rc_emitter = z.k == d
        infinite = mi.has_flag(ds.emitter.flags(), mi.EmitterFlags.Infinite)
        z.rc_hit.prim_uv = dr.select(rc_emitter, ds.uv, z.rc_hit.prim_uv)
        z.rc_pos[rc_emitter] = ds.p
        z.rc_dir[rc_emitter] = dr.select(infinite, ds.d, rc_n)
        z.density[rc_emitter] = emitter_density
        z.density[z.k == 0] = 1.0
        z.L_rc[rc_emitter | (z.k == 0)] = 1.0
        z.emitter_pdf[rc_emitter | (z.k == 0)] = count
        # A delta light keeps its selection probability, the shift's NEE density
        z.emitter_pdf[rc_emitter & ds.delta] = ds.pdf
        return z

    WORDS = 12

    def pack(self) -> mi.ArrayXu:
        """48 bytes per path."""
        flags = (
            (self.d & 0xF)
            | ((self.k & 0xF) << 4)
            | (mi.UInt32(self.nee) << 8)
            | (mi.UInt32(self.delta) << 9)
            | (mi.UInt32(self.emitter_next) << 10)
        )
        rc_emitter = self.k == self.d
        pos = dr.reinterpret_array(mi.ArrayXu, mi.ArrayXf(*self.rc_pos))
        uv = dr.reinterpret_array(mi.ArrayXu, mi.ArrayXf(*self.rc_hit.prim_uv))
        return mi.ArrayXu(
            self.seed,
            flags,
            dr.select(
                rc_emitter,
                dr.reinterpret_array(mi.UInt32, self.emitter),
                dr.reinterpret_array(mi.UInt32, self.rc_hit.shape),
            ),
            dr.select(rc_emitter, uv[0], self.rc_hit.prim_index),
            dr.select(rc_emitter, uv[1], pack_unorm2(self.rc_hit.prim_uv)),
            pos[0],
            pos[1],
            pos[2],
            oct_encode(self.rc_dir),
            pack_rgbe(self.L_rc),
            dr.reinterpret_array(mi.UInt32, self.density),
            dr.reinterpret_array(mi.UInt32, self.emitter_pdf),
        )

    @staticmethod
    def unpack(p) -> "PathSample":
        z: PathSample = dr.zeros(PathSample)
        z.seed = p[0]
        flags = p[1]
        z.d = bits(flags, 0, 4)
        z.k = bits(flags, 4, 4)
        z.nee = bits(flags, 8, 1) != 0
        z.delta = bits(flags, 9, 1) != 0
        z.emitter_next = bits(flags, 10, 1) != 0
        rc_emitter = z.k == z.d
        z.emitter = dr.reinterpret_array(
            mi.EmitterPtr, dr.select(rc_emitter, p[2], mi.UInt32(0))
        )
        z.rc_hit.shape = dr.reinterpret_array(
            mi.ShapePtr, dr.select(rc_emitter, mi.UInt32(0), p[2])
        )
        z.rc_hit.prim_index = dr.select(rc_emitter, mi.UInt32(0), p[3])
        terminal_uv = mi.Point2f(
            dr.reinterpret_array(mi.Float, p[3]),
            dr.reinterpret_array(mi.Float, p[4]),
        )
        z.rc_hit.prim_uv = dr.select(rc_emitter, terminal_uv, unpack_unorm2(p[4]))
        pos = dr.reinterpret_array(mi.ArrayXf, mi.ArrayXu(p[5], p[6], p[7]))
        z.rc_pos = mi.Point3f(pos[0], pos[1], pos[2])
        z.rc_dir = oct_decode(p[8])
        z.L_rc = unpack_rgbe(p[9])
        z.density = dr.reinterpret_array(mi.Float, p[10])
        z.emitter_pdf = dr.reinterpret_array(mi.Float, p[11])
        return z

    def emitter_sample(
        self,
        ref: mi.SurfaceInteraction3f,
        d: mi.Vector3f,
        dist: mi.Float,
        active: mi.Bool,
    ) -> mi.DirectionSample3f:
        """The emitter at x_k as seen from ``ref``, rebuilt as emitter sampling
        would have returned it."""
        infinite = mi.has_flag(self.emitter.flags(), mi.EmitterFlags.Infinite)
        ds = mi.DirectionSample3f(
            self.rc_pos,
            mi.Normal3f(self.rc_dir),
            self.rc_hit.prim_uv,
            ref.time,
            mi.Float(0.0),
            self.delta,
            d,
            dr.select(infinite, dr.inf, dist),
            self.emitter,
        )
        delta = active & self.delta
        sampled, _ = self.emitter.sample_direction(ref, mi.Point2f(0.5), delta)
        return dr.select(delta, sampled, ds)

    def shifted_hit(self, distance: mi.Float) -> mi.PreliminaryIntersection3f:
        """The stored primitive hit, reached at a shifted connection distance."""
        pi = dr.copy(self.rc_hit)
        pi.t = distance
        pi.valid = dr.isfinite(distance)
        return pi

    def needs_replay(self) -> mi.Bool:
        return (self.k == 0) | (self.k > 2)


@dataclass
class PathReservoir(Reservoir):
    """Reservoir holding one path, its integrand and its confidence."""

    z: mi.ArrayXu
    F: mi.Color3f  # Integrand at the owner pixel
    w: mi.Float  # Weight sum
    L: mi.Color3f  # Vector weight sum, used for shading (Sec. 6.3)
    W: mi.Float  # Contribution weight
    M: mi.Float  # Confidence

    WORDS = 16

    @staticmethod
    def empty(n: int = 1) -> "PathReservoir":
        """A reservoir without a path."""
        r: PathReservoir = dr.zeros(PathReservoir, n)
        r.z = dr.zeros(mi.ArrayXu, (PathSample.WORDS, n))
        return r

    def path(self) -> PathSample:
        """The path, unpacked."""
        return PathSample.unpack(self.z)

    def pack(self) -> mi.ArrayXu:
        """64 bytes per reservoir; M shares the path flags word."""
        F = dr.reinterpret_array(mi.ArrayXu, mi.ArrayXf(*self.F))
        return mi.ArrayXu(
            dr.reinterpret_array(mi.UInt32, self.W),
            F[0],
            F[1],
            F[2],
            self.z[0],
            self.z[1] | (half_bits(self.M) << 16),
            *self.z[2:],
        )

    @staticmethod
    def unpack(p: mi.ArrayXu) -> "PathReservoir":
        r: PathReservoir = dr.zeros(PathReservoir)
        r.W = dr.reinterpret_array(mi.Float, p[0])
        F = dr.reinterpret_array(mi.ArrayXf, mi.ArrayXu(p[1], p[2], p[3]))
        r.F = mi.Color3f(F[0], F[1], F[2])
        r.M = half_to_float(p[5] >> 16)
        r.z = mi.ArrayXu(p[4], p[5] & 0xFFFF, *p[6:])
        return r

    @staticmethod
    def gather(buf: mi.UInt32, idx: mi.UInt32, active) -> "PathReservoir":
        n = dr.width(buf) // PathReservoir.WORDS
        return PathReservoir.unpack(
            dr.gather(mi.ArrayXu, buf, idx, active, shape=(PathReservoir.WORDS, n))
        )

    def update(
        self,
        sampler: mi.Sampler,
        z: mi.ArrayXu,
        F: mi.Color3f,
        W: mi.Float,
        active: mi.Bool,
    ) -> mi.Bool:
        """Add the packed path ``z`` with integrand ``F`` and contribution
        weight ``W``; keeps ``F`` and the vector weight sum."""
        w, wvec = mi.luminance(F) * W, F * W
        active = mi.Bool(active) & (w > 0) & dr.isfinite(w) & dr.all(dr.isfinite(wvec))
        self.L += dr.select(active, wvec, mi.Color3f(0))
        selected = super().update(z, w, sampler.next_1d(active), active)
        self.F = dr.select(selected, F, self.F)
        return selected

    def merge(
        self,
        sampler: mi.Sampler,
        other: Reservoir,
        z: mi.ArrayXu,
        F: mi.Color3f,
        phat_src: mi.Float,
        norm: mi.Float,
        density: mi.Float,
        active: mi.Bool,
    ) -> mi.Bool:
        """Add the sample of an inner resampling ``other`` as the path ``z``."""
        W = other.contribution_weight(phat_src, norm) / density
        return self.update(sampler, z, F, W, active)

    def finalize(self, norm: mi.Float = 1.0):
        """Compute the contribution weight."""
        self.W = self.contribution_weight(mi.luminance(self.F), norm)
        self.L = dr.select(norm > 0, self.L / norm, mi.Color3f(0))
        # Seed 0 marks a reservoir without a path
        self.z[0] = dr.select(self.W > 0, self.z[0], mi.UInt32(0))


@dataclass
class TreeReservoir(PathReservoir):
    """Resampling over the paths of one tree, by their light vertex only.

    ``trace`` rebuilds the selected path afterwards; carrying it through the
    path loop instead costs registers.
    """

    z: mi.UInt32  # Light vertex of the selected path, see encode()

    @staticmethod
    def encode(
        d: mi.UInt32, nee: bool, nee_index: mi.UInt32 | None = None
    ) -> mi.UInt32:
        """The light vertex ``d`` of a path in one word: bits 0-3 hold ``d``,
        bit 4 whether emitter sampling reached it, and the upper bits which
        candidate of the emitter RIS it was."""
        z = d | 0x10 if nee else d
        if nee_index is not None:
            z |= nee_index << 5
        return z

    def decode(self) -> tuple[mi.UInt32, mi.Bool, mi.UInt32]:
        """The light vertex, whether emitter sampling reached it, and the
        index of its emitter candidate."""
        return self.z & 0xF, (self.z & 0x10) != 0, self.z >> 5


@dataclass
class PairShift:
    """A shifted integrand and its density at the destination pixel."""

    F: mi.Color3f
    density: mi.Float

    def jacobian(self, source: PathSample) -> mi.Float:
        """Shifted connection density divided by the source density."""
        return dr.select(source.density > 0, self.density / source.density, 0.0)


@dataclass
class Prefix:
    """A prefix replayed from another primary vertex up to ``y_{k-1}``."""

    weight: mi.Color3f  # Prefix weight up to y_{k-1}, or complete integrand for k = 0
    si: mi.SurfaceInteraction3f  # y_{k-1}
    rc_src: mi.Bool  # Forward criterion at y_{k-1}
    jacobian_prev: mi.Float  # Solid angle to area at y_{k-2}, seen from y_{k-1}
    consistent: mi.Bool  # No earlier vertex qualified as x_k


# --------------------------------- Helpers ---------------------------------


def solid_angle_to_area(n: mi.Vector3f, d: mi.Vector3f, dist: mi.Float) -> mi.Float:
    """Jacobian ``|cos| / dist^2`` from solid angle to area at a vertex with
    normal ``n``, reached along the direction ``d`` after the distance
    ``dist``. Unlike ``common.solid_angle_to_area_jacobian()``, it takes the
    segment rather than its end points, and is zero for ``dist = 0``."""
    return dr.select(dist > 0, dr.abs(dr.dot(n, d)) / dr.square(dist), 0.0)


def balance_heuristic(w: mi.Float, w_other: mi.Float) -> mi.Float:
    """MIS weight ``w / (w + w_other)`` of one of two strategies; zero where
    ``w`` is zero."""
    return dr.select(w > 0, w / (w + w_other), 0.0)


def footprint(pdf: mi.Float, g: mi.Float) -> mi.Float:
    """Reciprocal area density of a vertex reached with density ``pdf * g``."""
    return dr.select(pdf * g > 0, dr.rcp(pdf * g), dr.inf)


# ------------------------------- Integrator -------------------------------


class RestirPTIntegrator(ADIntegrator):
    r"""
    .. _integrator-restirpt:

    Enhanced ReSTIR Path Tracer (:monosp:`restirpt`)
    ------------------------------------------------

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth in the generated output image. A value
         of 2 will lead to single-bounce (direct-only) illumination, 3 adds one
         indirect bounce, and so on. Since path depths are stored in 4 bits, the
         value must lie between 2 and 15. (Default: 6)

     * - spatial_neighbors
       - |int|
       - Specifies the number of paired spatial neighbors that are merged per
         pixel and frame. A value of zero disables spatial reuse. The parameter
         :monosp:`spatial_rounds` is accepted as an alias. (Default: 3)

     * - search_radius
       - |float|
       - Specifies the target radius (in pixels) of spatial reuse, which
         determines the spread of the pairing textures. Must be positive.
         (Default: 30)

     * - pairing_sizes
       - |string|
       - Comma-separated list of side lengths of the tiled pairing textures. One
         entry is needed per spatial neighbor. (Default:
         :monosp:`254,230,210`)

     * - max_M_temporal
       - |float|
       - Upper limit on the confidence of a temporally reused reservoir. Larger
         values give more weight to the history, which reduces noise but makes
         the image slower to react to changes. (Default: 20)

     * - min_M_temporal
       - |float|
       - Lower limit on the temporal confidence, which is used in regions where
         many pixels hold copies of the same path (see
         :paramtype:`duplication_map`). (Default: 1)

     * - duplication_map
       - |bool|
       - If set to |true|, the temporal confidence limit is lowered in regions
         where many nearby pixels hold copies of the same path. This reduces
         correlation artifacts. (Default: |true|)

     * - duplication_radius
       - |int|
       - Radius (in pixels) of the square neighborhood that is used to measure
         path duplication. (Default: 8)

     * - duplication_alpha
       - |float|
       - Exponent controlling how strongly the duplication score lowers the
         temporal confidence limit. (Default: 0.1)

     * - footprint_c
       - |float|
       - Scale factor of the primary-ray footprint used by the reconnection
         criterion. Larger values make reconnections more restrictive.
         (Default: 0.02)

     * - roughness_threshold
       - |float|
       - Minimum roughness :math:`1/p^2` of the direction density at the
         source of a reconnection. (Default: 0.2)

     * - nee_candidates
       - |int|
       - Specifies the number of emitter samples that are resampled at the first
         path vertex. A vertex at depth :math:`d` uses :math:`\lfloor N / d^2
         \rfloor` candidates, but at least one. Only the selected candidate is
         tested for visibility. (Default: 1)

     * - dual_motion_vectors
       - |bool|
       - If set to |true|, temporal reprojection also tries the apparent motion
         of the occluder when the ordinary reprojection lands on an incompatible
         surface. (Default: |true|)

     * - russian_roulette
       - |bool|
       - If set to |true|, paths are terminated using the *russian roulette*
         criterion. (Default: |false|)

     * - rr_depth
       - |int|
       - Specifies the path depth, at which the implementation will begin to use
         the *russian roulette* path termination criterion. Only used when
         :paramtype:`russian_roulette` is enabled. (Default: 2)

     * - rr_luminance
       - |bool|
       - If set to |true|, the *russian roulette* survival probability is based
         on the luminance of the path throughput. Otherwise, the largest
         spectral component and the relative index of refraction are used.
         (Default: |false|)

     * - rr_max_survival
       - |float|
       - Upper limit on the *russian roulette* survival probability.
         (Default: 0.95)

     * - reorder
       - |bool|
       - If set to |true|, threads are reordered before the path replay to
         reduce divergence. This only affects performance, not the rendered
         image. (Default: |false|)

     * - reorder_path
       - |bool|
       - If set to |true|, threads are also reordered by the shape they hit
         during the path traversal, which makes the material evaluations of
         a warp coherent. This only affects performance, not the rendered
         image. (Default: |true|)

    This plugin implements the enhanced ReSTIR path tracer of Lin, Kettunen and
    Wyman (2026). It is intended for interactive rendering of successive frames
    with one sample per pixel, and reuses paths across neighboring pixels and
    previous frames to reduce noise. Each frame performs three steps:

    - **Initial resampling**: A path tree is traced from each pixel, and every
      complete path that ends on an emitter (found by BSDF sampling or emitter
      sampling) is a candidate. A reservoir selects one path per pixel.

    - **Temporal reuse**: The reservoir of the previous frame is reprojected and
      merged with the new one. Position and normal tests reject incompatible
      history, and a confidence limit bounds how long a path may persist.

    - **Spatial reuse**: Self-inverting pixel pairings select spatial neighbors.
      Their paths are shifted in both directions and merged using pairwise MIS.

    A stored path consists of a random seed, a *reconnection vertex* and the
    contribution of the path suffix beyond it. To reuse a path at another
    pixel, the camera-side prefix is replayed from that pixel with the same
    seed and then connected to the stored suffix. The shift is rejected if the
    connection is occluded or violates the footprint and roughness criteria,
    and its contribution is scaled by the Jacobian of the mapping.

    .. note::
        The integrator keeps reservoirs, primary intersections, duplication
        scores and the previous camera transform between calls to
        :meth:`render`. Rendering unrelated frames with the same instance will
        therefore reuse the previous frame, unless the film resolution changes.

    .. note::
        With pixel jitter, the primary hit point of a pixel changes in every
        frame, and reused paths must be shifted to it. This makes the result
        noisier, so we recommend setting the sensor's :monosp:`jitter`
        parameter to |false|, which sends every camera ray through the pixel
        center.

    .. warning::
        This integrator only renders one sample per pixel and does not support
        films with :monosp:`sample_border` enabled. It does not support
        differentiation, and all rendering happens with gradient tracking
        disabled.

    .. tabs::

        .. code-tab:: python

            'type': 'restirpt',
            'max_depth': 8,
            'spatial_neighbors': 3,
            'search_radius': 30.0
    """

    dist_threshold = 0.1
    angle_threshold = 25 * dr.pi / 180

    def __init__(self, props: mi.Properties):
        super().__init__(props)
        self.max_M_temporal = props.get("max_M_temporal", 20.0)
        self.min_M_temporal = props.get("min_M_temporal", 1.0)
        self.duplication_map = props.get("duplication_map", True)
        self.duplication_radius = props.get("duplication_radius", 8)
        self.duplication_alpha = props.get("duplication_alpha", 0.1)
        self.spatial_neighbors = props.get(
            "spatial_neighbors", props.get("spatial_rounds", 3)
        )
        self.search_radius = props.get("search_radius", 30.0)
        self.pairing_sizes = [
            int(v) for v in props.get("pairing_sizes", "254,230,210").split(",")
        ]
        self.footprint_c = props.get("footprint_c", 0.02)
        self.roughness_threshold = props.get("roughness_threshold", 0.2)
        self.nee_candidates = props.get("nee_candidates", 1)
        self.dual_motion_vectors = props.get("dual_motion_vectors", True)
        self.russian_roulette = props.get("russian_roulette", False)
        self.rr_depth = props.get("rr_depth", 2)
        self.rr_luminance = props.get("rr_luminance", False)
        self.rr_max_survival = props.get("rr_max_survival", 0.95)
        self.reorder = props.get("reorder", False)
        self.reorder_path = props.get("reorder_path", True)
        if not 2 <= self.max_depth <= 15:
            raise ValueError("max_depth must be between 2 and 15")
        if self.search_radius <= 0:
            raise ValueError("search_radius must be greater than 0")
        if self.spatial_neighbors < 0:
            raise ValueError("spatial_neighbors must be nonnegative")
        if self.spatial_neighbors > len(self.pairing_sizes):
            raise ValueError("pairing_sizes must list one size per spatial neighbor")
        if self.nee_candidates < 1:
            raise ValueError("nee_candidates must be at least 1")
        self.film_size: None | mi.ScalarVector2u = None

    # ---------------------- Reconnection policy ----------------------

    def diffuse_only(self, bsdf: mi.BSDF, active: mi.Bool) -> mi.Bool:
        """Materials whose sampling density does not depend on the incidence."""
        flags = bsdf.flags()
        other = mi.has_flag(flags, mi.BSDFFlags.Glossy)
        other |= mi.has_flag(flags, mi.BSDFFlags.Delta)
        return active & mi.has_flag(flags, mi.BSDFFlags.Diffuse) & ~other

    def rc_src_criterion(
        self,
        delta: mi.Bool,
        pdf: mi.Float,
        g: mi.Float,
        threshold: mi.Float,
    ) -> mi.Bool:
        """Whether this vertex can serve as the source vertex of a reconnection
        (Eq. 5, with the roughness safeguard of Supp. Eq. 26)."""
        rough = dr.square(pdf) * self.roughness_threshold <= 1.0
        return (pdf > 0) & rough & ~delta & (footprint(pdf, g) >= threshold)

    def rc_dst_criterion(
        self,
        bsdf: mi.BSDF,
        pdf: mi.Float,
        g: mi.Float,
        threshold: mi.Float,
        active: mi.Bool,
    ) -> mi.Bool:
        """Whether the vertex can serve as the destination of a reconnection (Eq. 5)."""
        return self.diffuse_only(bsdf, active) | (footprint(pdf, g) >= threshold)

    # ------------------------ Spatial pairing ------------------------

    def pairing_texture(self, slot: int) -> mi.Vector2i:
        """Self-inverting pixel offsets of pairing texture ``slot``, with a
        Gaussian spread set by the search radius (Sec. 3.1)."""
        size = self.pairing_layout(slot)[0]
        sigma = math.sqrt(8.0 / (9.0 * math.pi)) * self.search_radius
        seed = 0xC0FFEE + 7919 * slot
        iterations = round(
            sigma**2 / 2 + 1.46 / sigma + 1.76 / sigma**2 + 0.656 / sigma**3 + 0.5
        )
        n, half = size * size, size // 2
        texel = dr.arange(mi.UInt32, n)

        def shuffle(a: mi.TensorXu, offset: int, iteration: int) -> mi.TensorXu:
            blocks = dr.reshape(dr.roll(a, -offset, axis=(0, 1)), (half, 2, half, 2))
            blocks = dr.transpose(blocks, (0, 2, 1, 3))
            blocks = dr.reshape(blocks, (half, half, 4))
            keys = mi.TensorXf(
                mi.sample_tea_float32(texel, seed + 7919 * iteration), blocks.shape
            )
            order = dr.argsort(keys).array + (texel // 4) * 4
            a = mi.TensorXu(dr.gather(mi.UInt32, blocks.array, order), blocks.shape)
            a = dr.transpose(dr.reshape(a, (half, half, 2, 2)), (0, 2, 1, 3))
            return dr.roll(dr.reshape(a, (size, size)), offset, axis=(0, 1))

        samples = mi.TensorXu(texel, (size, size))
        for i in range(max(iterations, 1)):
            samples = shuffle(samples, i & 1, i)
            dr.eval(samples)
        position = dr.empty(mi.UInt32, n)
        dr.scatter(position, texel, samples.array)
        partner = dr.gather(mi.UInt32, position, samples.array ^ 1)
        dx = mi.Int32(partner % size) - mi.Int32(texel % size)
        dy = mi.Int32(partner // size) - mi.Int32(texel // size)
        wrap = lambda d: dr.select(
            d > half, d - size, dr.select(d < -half, d + size, d)
        )
        return mi.Vector2i(wrap(dx), wrap(dy))

    # -------------------------- Frame state --------------------------

    def reset_frame_state(self, film_size: mi.ScalarVector2u) -> None:
        """Reset frame state such as pairing textures and reservoirs."""
        self.film_size = mi.ScalarVector2u(film_size)
        n = film_size.x * film_size.y
        self.reservoir_buf = dr.zeros(mi.UInt32, n * PathReservoir.WORDS)
        self.si_buf: mi.SurfaceInteraction3f = dr.zeros(mi.SurfaceInteraction3f, n)
        if self.duplication_map:
            self.duplication_buf = dr.zeros(mi.Float, n)

        if self.spatial_neighbors:
            # One texture per slot with interleaved (dx, dy), concatenated for
            # lookup in a symbolic loop
            self.pairing = dr.concat(
                [dr.ravel(self.pairing_texture(r)) for r in range(self.spatial_neighbors)]
            )
            dr.eval(self.pairing)

    def footprint_threshold(self, x_v: mi.Vector3f, n_v: mi.Vector3f, eye: mi.Point3f):
        """The scene dependent footprint threshold (right hand side of Eq. 5)."""
        v = eye - x_v
        dist2 = dr.squared_norm(v)
        cos = dr.abs(dr.dot(n_v, v * dr.rsqrt(dr.maximum(dist2, 1e-20))))
        scale = self.footprint_c / 100.0 * 4.0 * dr.pi
        return dr.select(cos > 0, scale * dist2 / cos, dr.inf)

    def to_idx(self, pos: mi.Point2u) -> mi.UInt32:
        """Convert a screen-space position to an index into the flattened buffer."""
        assert self.film_size is not None
        pos = dr.clip(mi.Point2u(pos), mi.Point2u(0), self.film_size - 1)
        return pos.y * self.film_size.x + pos.x

    def similar(
        self, a: mi.SurfaceInteraction3f, b: mi.SurfaceInteraction3f
    ) -> mi.Bool:
        """Are two primary vertices close with similar normals?"""
        close = dr.norm(a.p - b.p) < self.dist_threshold
        facing = dr.dot(a.n, b.n) > dr.cos(self.angle_threshold)
        return a.is_valid() & b.is_valid() & close & facing

    def nee_count(self, depth: mi.UInt32) -> mi.Float:
        """Light candidates at bounce ``depth``: ``N / B^2``, at least one (Sec. 7)."""
        schedule = float(self.nee_candidates) / dr.square(mi.Float(depth))
        return dr.maximum(dr.floor(schedule), 1.0)

    # --------------------------- Rendering ---------------------------

    def render(
        self,
        scene: mi.Scene,
        sensor: int | mi.Sensor = 0,
        seed: int = 0,
        spp: int = 0,
        develop: bool = True,
        evaluate: bool = True,
        profile: bool = False,
    ) -> mi.TensorXf:
        """One frame: initial resampling, temporal reuse, paired spatial reuse."""
        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]

        film = sensor.film()
        film_size = film.crop_size()

        if film.sample_border():
            raise Exception("sample_border=True is not supported by this integrator")

        if spp > 1:
            raise ValueError("restirpt renders one path tree per pixel")

        if self.film_size is None or dr.any(self.film_size != film_size):
            self.reset_frame_state(film_size)
            self.prev_to_world = sensor.world_transform()

        n_pixels = film_size.x * film_size.y

        with dr.suspend_grad():
            sampler, spp = self.prepare(
                sensor=sensor, seed=seed, spp=1, aovs=self.aov_names(sensor.film())
            )

            ray, weight, image_pos = self.sample_rays(scene, sensor, sampler)
            eye = mi.Point3f(sensor.world_transform().translation())
            prev_eye = mi.Point3f(self.prev_to_world.translation())

            # ---------------- Initial resampling over the path tree -------
            si: mi.SurfaceInteraction3f = scene.ray_intersect(
                ray, mi.RayFlags.Default, coherent=True, ray_mask=mi.RayMask.Primary
            )
            threshold = self.footprint_threshold(si.p, si.n, eye)
            primary_ds = mi.DirectionSample3f(
                scene,
                si,
                dr.zeros(mi.SurfaceInteraction3f),
                ray_mask=mi.RayMask.Primary,
            )
            Le_dir = primary_ds.emitter.eval(si)
            fresh = self.sample_initial(scene, sampler, si, threshold)

            pixel = dr.arange(mi.UInt32, n_pixels)
            sampler.schedule_state()
            dr.eval(fresh, Le_dir, si, threshold, ray, weight, image_pos)

            # ---------------- Temporal reuse -------------------------------
            prev_idx, prev_ok = self.temporal_neighbor(sensor, si, pixel)
            prev = PathReservoir.gather(self.reservoir_buf, prev_idx, prev_ok)
            prev_si = dr.gather(mi.SurfaceInteraction3f, self.si_buf, prev_idx, prev_ok)
            prev.M = dr.minimum(prev.M, self.confidence_cap(prev_idx, prev_ok))

            if self.reorder:
                key = dr.select(prev_ok & prev.path().needs_replay(), 2, 0)
                key |= dr.select(fresh.path().needs_replay(), 1, 0)
                si.p, si.n = dr.reorder_threads(mi.UInt32(key), 2, (si.p, si.n))

            temporal = self.resample_temporal(
                scene, sampler, fresh, si, threshold, prev, prev_si, prev_ok, prev_eye
            )

            temporal_buf = dr.ravel(temporal.pack())
            sampler.schedule_state()
            dr.eval(temporal_buf, temporal, si, threshold, Le_dir, weight, image_pos)

            # ---------------- Paired spatial reuse -------------------------
            reservoir = temporal
            if self.spatial_neighbors:
                pairs = self.spatial_prepass(scene, temporal_buf, si, eye, seed)
                dr.eval(pairs)
                reservoir = self.resample_spatial(
                    sampler, temporal, si, temporal_buf, pairs, seed
                )

            reservoir_buf = dr.ravel(reservoir.pack())
            L = Le_dir + reservoir.L

            block = film.create_block()
            block.set_coalesce(block.coalesce() and spp >= 4)
            ADIntegrator._splat_to_block(
                block,
                film,
                image_pos,
                value=L * weight,
                weight=1.0,
                alpha=dr.select(si.is_valid(), mi.Float(1), mi.Float(0)),
                aovs=[],
                wavelengths=ray.wavelengths,
            )
            film.put_block(block)

            self.reservoir_buf = reservoir_buf
            self.si_buf = si
            self.prev_to_world = sensor.world_transform()
            dr.eval(self.reservoir_buf, self.si_buf)

            if self.duplication_map:
                self.duplication_buf = self.duplication_scores(self.reservoir_buf)
                dr.eval(self.duplication_buf)

            return film.develop() if develop else block.tensor()

    # ------------------------ Initial sampling ------------------------

    @dr.syntax
    def sample_initial(
        self,
        scene: mi.Scene,
        sampler: mi.Sampler,
        si: mi.SurfaceInteraction3f,
        threshold: mi.Float,
    ) -> PathReservoir:
        """Trace one path tree and add its paths into a new reservoir."""
        seed = (mi.UInt32(sampler.next_1d() * 65536.0) << 16) | mi.UInt32(
            sampler.next_1d() * 65536.0
        )
        seed = dr.maximum(seed, 1)
        tree = self.trace(scene, sampler, seed, si, threshold, "sample")
        sampler.schedule_state()
        dr.eval(tree)

        reservoir = PathReservoir.empty(dr.width(seed))
        d, nee, nee_index = tree.decode()
        reservoir.z, _ = self.trace(
            scene,
            None,
            seed,
            si,
            threshold,
            "rebuild",
            d=d,
            nee=nee,
            nee_index=nee_index,
            active=tree.w > 0,
        )
        reservoir.F = tree.F
        reservoir.w = tree.w
        reservoir.L = tree.L
        reservoir.M = mi.Float(1.0)
        reservoir.finalize(1.0)
        return reservoir

    @dr.syntax
    def emitter_ris(
        self,
        scene: mi.Scene,
        sampler: ReSTIRSampler,
        si: mi.SurfaceInteraction3f,
        bsdf: mi.BSDF,
        count: mi.Float,
        active: mi.Bool,
    ) -> EmitterReservoir:
        """RIS over unshadowed candidates, then visibility and MIS for the winner."""
        ctx = mi.BSDFContext()
        emitter_res: EmitterReservoir = dr.zeros(EmitterReservoir, dr.width(si))
        i = mi.UInt32(0)
        while dr.hint(
            active & (mi.Float(i) < count),
            max_iterations=self.nee_candidates,
            label="NEE RIS",
        ):
            sample = sampler.next_2d()
            ds, em_weight = scene.sample_emitter_direction(si, sample, False, active)
            take = active & (ds.pdf > 0)
            f, pdf = bsdf.eval_pdf(ctx, si, si.to_local(ds.d), take)
            phat = dr.select(take, mi.luminance(f * em_weight), 0.0)
            sample = EmitterSample(
                ds, em_weight, f, pdf, phat, mi.Color3f(0), mi.Float(0), mi.UInt32(i)
            )
            emitter_res.update(sample, phat, sampler.next_1d(), take)
            i += 1

        em = emitter_res.z
        visible = active & (emitter_res.w > 0)
        # Use zero-length rays for inactive lanes to keep the warp consistent.
        ray_em = si.spawn_ray_to(em.ds.p)
        ray_em.maxt[~visible] = 0
        if scene.has_null_shapes():
            # Null surfaces transmit; their transmittance becomes part of
            # the emitter weight, and hence of the path's suffix
            tr = scene.ray_test_tr(ray_em, active=visible)
            visible &= dr.any(mi.unpolarized_spectrum(tr) != 0)
            em.em_weight *= tr
        else:
            visible &= ~scene.ray_test(ray_em)
        # Preserve the unshadowed phat and w; merge() applies the RIS weight.
        self.emitter_contribution(em, count, visible)
        return emitter_res

    def emitter_candidate(
        self,
        scene: mi.Scene,
        sampler: ReSTIRSampler,
        si: mi.SurfaceInteraction3f,
        bsdf: mi.BSDF,
        count: mi.Float,
        index: mi.UInt32,
        active: mi.Bool,
    ) -> EmitterSample:
        """Candidate ``index`` of :py:meth:`emitter_ris`, drawn again from the
        same stream without the others, and unshadowed."""
        # Each candidate takes three draws: two for the emitter, one for the RIS
        sampler.advance(3 * index)
        ds, em_weight = scene.sample_emitter_direction(
            si, sampler.next_2d(), False, active
        )
        f, pdf = bsdf.eval_pdf(mi.BSDFContext(), si, si.to_local(ds.d), active)
        em = EmitterSample(
            ds, em_weight, f, pdf, mi.Float(0), mi.Color3f(0), mi.Float(0), index
        )
        if scene.has_null_shapes():
            ray_em = si.spawn_ray_to(ds.p)
            ray_em.maxt[~active] = 0
            em.em_weight *= scene.ray_test_tr(ray_em, active=active)
        self.emitter_contribution(em, count, active)
        return em

    def emitter_contribution(
        self, em: EmitterSample, count: mi.Float, visible: mi.Bool
    ) -> None:
        """Fill in the MIS-weighted contribution and the connection density."""
        mis_em = dr.select(
            em.ds.delta, 1.0, mis_weight(count * em.ds.pdf, em.pdf_bsdf)
        )
        em.contribution = dr.select(
            visible, em.f * em.em_weight * mis_em, mi.Color3f(0)
        )
        infinite = mi.has_flag(em.ds.emitter.flags(), mi.EmitterFlags.Infinite)
        G = dr.select(infinite, 1.0, solid_angle_to_area(em.ds.n, em.ds.d, em.ds.dist))
        em.density = dr.select(em.ds.delta, em.ds.pdf, em.ds.pdf * G)

    @dr.syntax
    def trace(
        self,
        scene: mi.Scene,
        sampler: mi.Sampler | None,
        seed: mi.UInt32,
        si: mi.SurfaceInteraction3f,
        threshold: mi.Float,
        mode: Literal["sample", "rebuild", "replay"],
        d: mi.UInt32 | int = 0,
        k: mi.UInt32 | int = 0,
        nee: mi.Bool | bool = False,
        nee_index: mi.UInt32 | int = 0,
        active: mi.Bool | bool = True,
    ):
        """Path tracer in three modes.

        ``"sample"`` traces the path tree and resamples its paths, but keeps
        only the light vertex of the selected one (a :py:class:`TreeReservoir`).
        ``"rebuild"`` traces that path again up to its last scattering
        vertex, tracks its reconnection vertex ``x_k``, and returns it packed
        together with its integrand. The path ends on the light vertex ``d``,
        which emitter sampling (``nee``, candidate ``nee_index``) or BSDF
        sampling reached.
        ``"replay"`` traces a prefix from its ``seed`` at another primary
        vertex up to ``y_{k-1}``, or the whole path if they could not be
        connected.
        """
        if mode not in ("sample", "rebuild", "replay"):
            raise ValueError(f"unknown trace mode {mode!r}")
        sampling = mode == "sample"
        rebuilding = mode == "rebuild"
        replaying = mode == "replay"
        k, d = mi.UInt32(k), mi.UInt32(d)
        nee, nee_index = mi.Bool(nee), mi.UInt32(nee_index)
        stop_at = dr.select(k > 0, k - 1, d)
        if rebuilding:
            stop_at[nee] = d - 1
        whole = k == 0

        # --------------------- Configure loop state ----------------------

        n = dr.width(seed)
        # Streams seeded by the path alone, not the lane, for replayed BSDF
        # samples. NEE uses one stream per vertex so that the rebuild can
        # redraw a vertex's candidates without the ones before.
        path_sampler = ReSTIRSampler(0)
        path_sampler.seed(seed, n)

        active = mi.Bool(active)
        weight = mi.Spectrum(1.0)
        eta = mi.Float(1.0)
        # Index j of the current vertex x_j, as in the paper: the primary
        # vertex is x_1 (prb.py instead counts the bounces from zero)
        depth = mi.UInt32(1)
        ray_mask = mi.UInt32(mi.RayMask.Primary)

        # The loop state of all modes must exist, whichever mode runs

        # Sample mode
        rr_pdf = mi.Float(1.0)
        reservoir: TreeReservoir = dr.zeros(TreeReservoir, n)

        # Rebuild mode (its paths may end on the environment, after the loop
        # deactivated them)
        selected = mi.Bool(active)
        rc: Reconnection = dr.zeros(Reconnection, n)
        rc.postfix_throughput = mi.Spectrum(1.0)

        # Replay mode
        prefix: Prefix = dr.zeros(Prefix, n)
        consistent = mi.Bool(True)
        bsdf_ctx = mi.BSDFContext()

        # The primary vertex is copied, since it becomes loop state
        si = dr.copy(si)
        pi: mi.PreliminaryIntersection3f = dr.zeros(mi.PreliminaryIntersection3f, n)
        valid = si.is_valid()
        rc_src = mi.Bool(False)
        arrival_pdf = mi.Float(1.0)
        # Solid angle to area at x_{j-1}, for the reverse density of x_j -> x_{j-1}
        jacobian_prev = mi.Float(0.0)
        # Variables caching information from the previous bounce. Null
        # crossings are not path vertices and leave them unchanged.
        p_prev = mi.Point3f(si.p)
        bsdf_pdf_prev = mi.Float(1.0)
        bsdf_delta_prev = mi.Bool(True)
        nee_count_prev = mi.Float(1.0)
        depth_prev = mi.UInt32(0)
        if not replaying:
            active &= valid
        else:
            consistent &= valid
        reached_end = (k > 0) & (depth == stop_at) & valid

        while dr.hint(
            active & (sampling or (depth < stop_at)),
            max_iterations=self.max_depth,
            label="Path tree",
        ):
            bsdf: mi.BSDF = si.bsdf()
            count = self.nee_count(depth)

            # ---------------------- BSDF sampling ----------------------

            bsdf_sample, bsdf_weight = bsdf.sample(
                bsdf_ctx,
                si,
                path_sampler.next_1d(),
                path_sampler.next_2d(),
            )
            bsdf_weight = si.to_world_mueller(bsdf_weight, -bsdf_sample.wo, si.wi)
            # A null crossing is, like a delta event, invisible to NEE
            bsdf_delta = mi.has_flag(
                bsdf_sample.sampled_type, mi.BSDFFlags.Delta | mi.BSDFFlags.Null
            )
            ray_mask[~bsdf_delta] = mi.UInt32(mi.RayMask.Secondary)
            wo_world = si.to_world(bsdf_sample.wo)

            # Does x_j qualify as reconnection vertex? (Eq. 5)
            rc_dst = self.rc_dst_criterion(
                bsdf, bsdf_sample.pdf, jacobian_prev, threshold, active
            )
            qualifies = rc_src & rc_dst & ~bsdf_delta

            active_next = active & (depth < self.max_depth)

            # -------------------- Reconnection vertex --------------------

            if dr.hint(rebuilding, mode="scalar"):
                # The first qualifying vertex becomes x_k
                proposed_rc = Reconnection(
                    k=mi.UInt32(depth),
                    hit=dr.copy(pi),
                    pos=mi.Point3f(si.p),
                    dir=mi.Vector3f(wo_world),
                    arrival_pdf=dr.maximum(arrival_pdf, 0.0),
                    bsdf_pdf=dr.maximum(bsdf_sample.pdf, 0.0),
                    postfix_throughput=mi.Spectrum(1.0),
                )
                rc = dr.select(active_next & (rc.k == 0) & qualifies, proposed_rc, rc)
            elif dr.hint(replaying, mode="scalar"):
                # No vertex before y_{k-1} may qualify
                consistent &= ~(qualifies & valid)

            # ---------------------- Emitter sampling ----------------------

            if dr.hint(sampling, mode="scalar"):
                active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

                nee_sampler = ReSTIRSampler(depth)
                nee_sampler.seed(seed, n)
                emitter_res = self.emitter_ris(
                    scene, nee_sampler, si, bsdf, count, active_em
                )

                em = emitter_res.z
                em_index = em.index if self.nee_candidates > 1 else None
                reservoir.merge(
                    sampler,
                    emitter_res,
                    TreeReservoir.encode(depth + 1, True, em_index),
                    weight * em.contribution,
                    em.phat,
                    count,
                    rr_pdf,
                    active_em & (dr.max(em.contribution) > 0),
                )

            # ------ Update loop variables based on current interaction ------

            weight *= bsdf_weight
            if dr.hint(rebuilding, mode="scalar"):
                rc.postfix_throughput[depth > rc.k] *= bsdf_weight
            # Information about the current vertex needed by the next
            # iteration (unchanged by null crossings)
            scattered = ~mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Null)
            if scattered:
                p_prev = mi.Point3f(si.p)
                bsdf_pdf_prev = mi.Float(bsdf_sample.pdf)
                bsdf_delta_prev = mi.Bool(bsdf_delta)
                nee_count_prev = mi.Float(count)
                depth_prev = mi.UInt32(depth)
            eta *= bsdf_sample.eta
            ray = si.spawn_ray(wo_world)
            src_n = mi.Normal3f(si.n)

            # -------------------- Stopping criterion ---------------------

            depth += 1
            weight_max = dr.max(mi.unpolarized_spectrum(weight))
            if dr.hint(sampling, mode="scalar"):
                survive = mi.Bool(True)
                if dr.hint(self.russian_roulette, mode="scalar"):
                    if dr.hint(self.rr_luminance, mode="scalar"):
                        rr_prob = mi.luminance(mi.unpolarized_spectrum(weight))
                    else:
                        rr_prob = weight_max * dr.square(eta)
                    rr_prob = dr.minimum(rr_prob, self.rr_max_survival)
                    rr_active = depth >= self.rr_depth
                    survive = ~rr_active | (sampler.next_1d() < rr_prob)
                    rr_pdf *= dr.select(rr_active, rr_prob, 1.0)
                active = active_next & survive & (weight_max > 0)
            else:
                active &= valid & (weight_max > 0)

            # ------------------------ Next vertex ------------------------

            pi = scene.ray_intersect_preliminary(
                ray,
                coherent=False,
                reorder=self.reorder_path,
                active=active,
                ray_mask=ray_mask,
            )
            si = scene.compute_surface_interaction(
                ray, pi, mi.RayFlags.Default, active=active
            )
            valid = si.is_valid()
            # The density with which x_{j+1} was reached, in the area measure.
            # Escaped rays keep the solid angle measure.
            jacobian_next = solid_angle_to_area(si.n, ray.d, si.t)
            jacobian_prev = solid_angle_to_area(src_n, ray.d, si.t)
            rc_src = self.rc_src_criterion(
                bsdf_delta, bsdf_sample.pdf, jacobian_next, threshold
            )
            arrival_pdf = bsdf_sample.pdf * dr.select(valid, jacobian_next, 1.0)

            # ---------------------- Direct emission ----------------------

            ref: mi.Interaction3f = dr.zeros(mi.Interaction3f, n)
            ref.p = p_prev
            ds = mi.DirectionSample3f(scene, si, ref, ray_mask=ray_mask)
            em_pdf = scene.pdf_emitter_direction(ref, ds, ~bsdf_delta_prev & active)
            mis_bsdf = mis_weight(bsdf_pdf_prev, nee_count_prev * em_pdf)

            if dr.hint(sampling, mode="scalar"):
                Le = ds.emitter.eval(si, active)
                F = weight * Le * mis_bsdf
                candidate = active & (dr.max(Le) > 0)
                reservoir.update(
                    sampler,
                    TreeReservoir.encode(depth, False),
                    F,
                    dr.rcp(rr_pdf),
                    candidate,
                )
                active &= valid
            elif dr.hint(rebuilding, mode="scalar"):
                active &= valid
            else:
                # A whole-path replay must end on the light the same way
                whole_end = active & whole & (depth == stop_at)
                Le = ds.emitter.eval(si, whole_end)
                weight = dr.select(whole_end, weight * Le * mis_bsdf, weight)
                consistent &= ~(whole_end & rc_src)
                consistent &= ~active | valid | whole_end
                reached_end |= active & (depth == stop_at)

        if dr.hint(sampling, mode="scalar"):
            return reservoir
        if dr.hint(rebuilding, mode="scalar"):
            # The light vertex is only evaluated now, after the loop, since
            # carrying its data through the loop would add to the loop state.

            # -------- NEE path: its light vertex, sampled from x_{d-1} --------
            count = self.nee_count(depth)
            nee_sampler = ReSTIRSampler(depth)
            nee_sampler.seed(seed, n)
            em = self.emitter_candidate(
                scene, nee_sampler, si, si.bsdf(), count, nee_index, selected & nee
            )
            # The reconnection vertex of an NEE path is x_{d-1} if no earlier
            # vertex qualified and x_{d-1} may be one; otherwise, the
            # light vertex is (Sec. 6.2.3)
            proposed_rc = Reconnection(
                k=mi.UInt32(depth),
                hit=dr.copy(pi),
                pos=mi.Point3f(si.p),
                dir=mi.Vector3f(0),
                arrival_pdf=dr.maximum(arrival_pdf, 0.0),
                bsdf_pdf=mi.Float(0.0),
                postfix_throughput=mi.Spectrum(1.0),
            )
            nee_rc = dr.select((rc.k == 0) & rc_src, proposed_rc, rc)
            z_nee = PathSample.from_emitter(
                seed=seed,
                d=depth + 1,
                rc=nee_rc,
                nee=True,
                ds=em.ds,
                rc_n=em.ds.n,
                emitter_density=em.density,
                L=em.contribution,
                L_emitter=em.em_weight,
                emitter_pdf=count * em.ds.pdf,
                count=count,
            )

            # ------ BSDF-sampled path: its light vertex x_d, just reached ------
            ref: mi.Interaction3f = dr.zeros(mi.Interaction3f, n)
            ref.p = p_prev
            ds = mi.DirectionSample3f(scene, si, ref, ray_mask=ray_mask)
            em_pdf = scene.pdf_emitter_direction(ref, ds, ~bsdf_delta_prev & ~nee)
            mis_bsdf = mis_weight(bsdf_pdf_prev, nee_count_prev * em_pdf)
            Le = ds.emitter.eval(si, selected & ~nee)
            # The shift recomputes the MIS weight of a light right after x_k
            z_hit = PathSample.from_emitter(
                seed=seed,
                d=depth,
                rc=rc,
                nee=False,
                ds=ds,
                rc_n=si.n,
                emitter_density=arrival_pdf,
                L=Le * mis_bsdf,
                L_emitter=Le,
                emitter_pdf=nee_count_prev * em_pdf,
                count=nee_count_prev,
                rc_src=rc_src,
                depth_prev=depth_prev,
            )
            z = dr.select(nee, z_nee, z_hit).pack()
            F = dr.select(nee, weight * em.contribution, weight * Le * mis_bsdf)
            return z, F
        prefix.weight = dr.select(whole & ~reached_end, mi.Color3f(0), weight)
        prefix.si = si
        prefix.rc_src = rc_src
        prefix.jacobian_prev = jacobian_prev
        prefix.consistent = (
            consistent & reached_end & dr.all(dr.isfinite(prefix.weight))
        )
        return prefix

    # ----------------------------- Shifts -----------------------------

    @dr.syntax
    def shift(
        self,
        scene: mi.Scene,
        z: PathSample,
        si_v: mi.SurfaceInteraction3f,
        threshold: mi.Float,
        active: mi.Bool,
    ) -> PairShift:
        """Hybrid shift of ``z`` to the primary vertex ``si_v``."""
        active = mi.Bool(active)
        bsdf_ctx = mi.BSDFContext()
        # Does the path end at x_k, or right after it?
        rc_emitter = z.d == z.k
        emitter_next = z.emitter_next
        # Area-measure vertices need a geometry factor. Terminal emitters use
        # their stored direction sample instead of reconstructing a hit.
        rc_area = ~rc_emitter | mi.has_flag(z.emitter.flags(), mi.EmitterFlags.Surface)
        rc_infinite = rc_emitter & mi.has_flag(
            z.emitter.flags(), mi.EmitterFlags.Infinite
        )

        replay = active & z.needs_replay()
        prefix = self.trace(
            scene, None, z.seed, si_v, threshold, "replay", d=z.d, k=z.k, active=replay
        )
        active &= prefix.consistent

        # y_{k-1}: the end of the replayed prefix
        src_si: mi.SurfaceInteraction3f = prefix.si
        rc = active & (z.k > 0) & src_si.is_valid()
        src_bsdf: mi.BSDF = src_si.bsdf()

        # ------------------------- x_k --------------------------
        d = z.rc_pos - src_si.p
        dist = dr.norm(d)
        d = dr.select(rc_infinite, z.rc_dir, d / dr.maximum(dist, 1e-20))
        rc &= rc_infinite | (dist > 0)
        rc_si: mi.SurfaceInteraction3f = scene.compute_surface_interaction(
            mi.Ray3f(mi.Point3f(src_si.p), d),
            z.shifted_hit(dist),
            ray_flags=mi.RayFlags.Default,
            active=rc & ~rc_emitter,
        )
        rc &= rc_emitter | rc_si.is_valid()
        rc_n = dr.select(rc_emitter, mi.Normal3f(z.rc_dir), mi.Normal3f(rc_si.n))
        rc_bsdf: mi.BSDF = rc_si.bsdf()

        F = mi.Color3f(0)
        J = mi.Float(0.0)
        density = mi.Float(0.0)
        valid = mi.Bool(False)

        if rc_infinite:
            vis_ray = src_si.spawn_ray(d)
        else:
            vis_ray = src_si.spawn_ray_to(z.rc_pos)
        # Null surfaces transmit, as they did for the path's emitter samples
        tr = mi.Spectrum(1.0)
        if dr.hint(scene.has_null_shapes(), mode="scalar"):
            tr = scene.ray_test_tr(vis_ray, active=rc)
            rc &= dr.any(mi.unpolarized_spectrum(tr) != 0)
        else:
            rc &= ~scene.ray_test(vis_ray, rc)

        src_f, src_pdf = src_bsdf.eval_pdf(bsdf_ctx, src_si, src_si.to_local(d), rc)
        rc &= (src_pdf > 0) & (dr.max(src_f) > 0)

        # ------------------ Reconnection criteria ------------------

        # y_{k-1} must not qualify as reconnection vertex itself
        src_qualifies = prefix.rc_src & self.rc_dst_criterion(
            src_bsdf, src_pdf, prefix.jacobian_prev, threshold, rc
        )
        rc &= ~src_qualifies

        # x_k must still qualify, seen from y_{k-1}. Infinite emitters have no
        # area measure; their footprint is unbounded.
        jacobian_rc = solid_angle_to_area(rc_n, d, dist)
        jacobian_src = solid_angle_to_area(src_si.n, d, dist)
        rc_f, rc_pdf = rc_bsdf.eval_pdf(
            bsdf_ctx, rc_si, rc_si.to_local(z.rc_dir), rc & ~rc_emitter
        )
        src_ok = rc & self.rc_src_criterion(
            mi.Bool(False),
            src_pdf,
            dr.select(rc_infinite, 0.0, jacobian_rc),
            threshold,
        )
        dst_ok = self.rc_dst_criterion(
            rc_bsdf,
            rc_pdf,
            dr.select(rc_infinite, 0.0, jacobian_src),
            threshold,
            rc,
        )

        if rc_emitter:
            if z.nee:
                rc &= ~prefix.rc_src
            else:
                rc &= src_ok
        elif emitter_next & z.nee:
            rc &= src_ok
        else:
            rc &= src_ok & dst_ok & (rc_pdf > 0)

        if rc:
            ds = z.emitter_sample(src_si, d, dist, rc & rc_emitter)

            # Infinite emitters (but delta lights) as the path evaluated them
            # on escaping; eval_direction() looks up ds.uv, which an escaped
            # hit lacks
            escaped = rc_infinite & ~z.delta
            Le = scene.eval_emitter_direction(src_si, ds, rc & rc_emitter & ~escaped)
            si_inf: mi.SurfaceInteraction3f = dr.zeros(mi.SurfaceInteraction3f)
            si_inf.wi = -d
            si_inf.t = dr.inf
            si_inf.time = src_si.time
            si_inf.wavelengths = src_si.wavelengths
            Le[escaped] = z.emitter.eval(si_inf, rc & escaped)
            em_pdf = scene.pdf_emitter_direction(src_si, ds, rc & rc_emitter & ~z.delta)
            em_pdf = dr.select(z.delta, z.emitter_pdf * ds.pdf, em_pdf)

            # Rebuild the path from y_{k-1} to x_{k+1}, collecting the
            # Jacobian's densities in ``density``
            F = prefix.weight * tr
            density = mi.Float(1.0)

            # ------------------ Segment y_{k-1} -> x_k ------------------
            nee = rc_emitter & z.nee
            pdf = dr.select(nee, em_pdf, src_pdf)
            F *= src_f / pdf
            density *= pdf
            if rc_area:
                density *= jacobian_rc
            if rc_emitter:
                w_bsdf = mis_weight(src_pdf, z.emitter_pdf * em_pdf)
                F *= Le * dr.select(z.delta, 1.0, dr.select(nee, 1.0 - w_bsdf, w_bsdf))
            else:
                # -------- Segment x_k -> x_{k+1}, as x_k scatters --------
                # Its emission and NEE density are frozen in L_rc and density
                nee = emitter_next & z.nee
                pdf = dr.select(nee, 1.0, rc_pdf)
                F *= rc_f / pdf
                density *= pdf
                if emitter_next:
                    w_bsdf = mis_weight(rc_pdf, z.emitter_pdf)
                    F *= dr.select(z.delta, 1.0, dr.select(nee, 1.0 - w_bsdf, w_bsdf))

            F *= z.L_rc
            J = dr.select(z.density > 0, density / z.density, 0.0)
            valid = rc & dr.all(dr.isfinite(F)) & dr.isfinite(J) & (J > 0)
            F = dr.select(valid, F, mi.Color3f(0))
            density = dr.select(valid, density, mi.Float(0.0))
        elif active & (z.k == 0):
            # Random replay: identity in primary sample space
            F = prefix.weight
            density = mi.Float(1.0)

        return PairShift(F, density)

    # ------------------------- Temporal reuse -------------------------

    def reproject(self, sensor: mi.Sensor, p: mi.Point3f) -> tuple[mi.Point2i, mi.Bool]:
        """Reproject the world-space point ``p`` into the previous frame's image."""
        # The previous camera is the current one moved back by the pose change
        it: mi.Interaction3f = dr.zeros(mi.Interaction3f)
        it.p = sensor.world_transform() @ (self.prev_to_world.inverse() @ p)
        ds, _ = sensor.sample_direction(it, mi.Point2f(0.0))
        pixel = mi.Point2i(ds.uv)
        size = mi.Point2i(self.film_size)
        return pixel, (ds.pdf > 0) & dr.all(pixel >= 0) & dr.all(pixel < size)

    def temporal_neighbor(
        self, sensor: mi.Sensor, si: mi.SurfaceInteraction3f, pixel: mi.UInt32
    ) -> tuple[mi.UInt32, mi.Bool]:
        """Previous-frame pixel, with dual motion vectors (Sec. 6.4)."""
        size = mi.Point2i(self.film_size)
        width = self.film_size.x
        own = mi.Point2i(mi.Int32(pixel % width), mi.Int32(pixel // width))
        prev, inside = self.reproject(sensor, si.p)
        inside &= si.is_valid()
        idx = self.to_idx(mi.Point2u(dr.maximum(prev, 0)))
        prev_si = dr.gather(mi.SurfaceInteraction3f, self.si_buf, idx, inside)
        ok = inside & self.similar(prev_si, si)

        if not self.dual_motion_vectors:
            return idx, ok

        ds_o, _ = sensor.sample_direction(prev_si, mi.Point2f(0.0))
        moved = mi.Point2i(ds_o.uv)
        dual = prev + (own - moved)
        try_dual = inside & ~ok & prev_si.is_valid() & (ds_o.pdf > 0)
        try_dual &= dr.all(dual >= 0) & dr.all(dual < size)
        idx_d = self.to_idx(mi.Point2u(dr.maximum(dual, 0)))
        prev_si_d = dr.gather(mi.SurfaceInteraction3f, self.si_buf, idx_d, try_dual)
        ok_d = try_dual & self.similar(prev_si_d, si)
        return dr.select(ok_d, idx_d, idx), ok | ok_d

    def confidence_cap(self, prev_idx: mi.UInt32, prev_ok: mi.Bool) -> mi.Float:
        """Temporal cap lowered by the duplication score of the previous-frame
        pixel (Sec. 5)."""
        if not self.duplication_map:
            return mi.Float(self.max_M_temporal)
        score = dr.gather(mi.Float, self.duplication_buf, prev_idx, prev_ok)
        t = dr.power(dr.clip(score, 0.0, 1.0), self.duplication_alpha)
        return dr.lerp(mi.Float(self.max_M_temporal), mi.Float(self.min_M_temporal), t)

    def resample_temporal(
        self,
        scene: mi.Scene,
        sampler: mi.Sampler,
        res: PathReservoir,
        si: mi.SurfaceInteraction3f,
        threshold: mi.Float,
        prev: PathReservoir,
        prev_si: mi.SurfaceInteraction3f,
        valid: mi.Bool,
        prev_eye: mi.Point3f,
    ) -> PathReservoir:
        """Combine the initial sample with the reprojected reservoir."""
        out = PathReservoir.empty()
        reuse = valid & (prev.W > 0)
        prev_z, res_z = prev.path(), res.path()

        # The previous path shifted here. Each shift runs in a kernel of its
        # own, since the state of two shifts in one kernel exceeds the
        # register budget.
        inbound = self.shift(scene, prev_z, si, threshold, reuse)
        dr.eval(inbound)
        # This pixel's path shifted to the previous frame
        threshold_p = self.footprint_threshold(prev_si.p, prev_si.n, prev_eye)
        outbound = self.shift(
            scene, res_z, prev_si, threshold_p, valid & (res.W > 0)
        )
        dr.eval(outbound)
        J_in = inbound.jacobian(prev_z)
        J_out = outbound.jacobian(res_z)

        # Pairwise MIS: each path is evaluated under the target functions of
        # both pixels, in the measure of the current pixel, and weighted by
        # their confidences
        phat_c = mi.luminance(res.F)
        phat_c_at_prev = dr.select(J_out > 0, mi.luminance(outbound.F) * J_out, 0.0)
        phat_t = dr.select(J_in > 0, mi.luminance(prev.F) / J_in, 0.0)
        phat_t_here = mi.luminance(inbound.F)
        m_c = balance_heuristic(res.M * phat_c, prev.M * phat_c_at_prev)
        m_c = dr.select(valid, m_c, 1.0)
        m_t = balance_heuristic(prev.M * phat_t, res.M * phat_t_here)

        out.update(sampler, res.z, res.F, m_c * res.W, res.W > 0)
        out.update(
            sampler,
            replace(prev_z, density=inbound.density).pack(),
            inbound.F,
            m_t * J_in * prev.W,
            reuse & (J_in > 0),
        )
        # A failed shift or empty reservoir still counts
        out.M = res.M + dr.select(valid, prev.M, 0.0)
        out.finalize(1.0)
        return out

    # ---------------------- Paired spatial reuse ----------------------

    def pairing_layout(self, slot) -> tuple:
        """Edge length and table offset of pairing texture ``slot``."""
        sizes = [max(2, s & ~1) for s in self.pairing_sizes[: self.spatial_neighbors]]
        offsets = [sum(s * s for s in sizes[:r]) for r in range(len(sizes))]
        if isinstance(slot, int):
            return sizes[slot], offsets[slot]
        size, offset = mi.UInt32(sizes[-1]), mi.UInt32(offsets[-1])
        for r in reversed(range(len(sizes) - 1)):
            size = dr.select(slot == r, mi.UInt32(sizes[r]), size)
            offset = dr.select(slot == r, mi.UInt32(offsets[r]), offset)
        return size, offset

    def partner(
        self, slot: mi.UInt32, pixel: mi.UInt32, seed: mi.UInt32
    ) -> tuple[mi.UInt32, mi.Bool]:
        """Partner pixel under pairing texture ``slot``, transformed randomly
        per frame by ``seed`` (Sec. 3.2)."""
        usize, offset = self.pairing_layout(slot)
        size = mi.Int32(usize)
        width, height = self.film_size.x, self.film_size.y
        h, _ = mi.sample_tea_32(seed, mi.UInt32(slot))
        flip_x = (h & 1) != 0
        flip_y = (h & 2) != 0
        transpose = (h & 4) != 0
        ox = mi.Int32((h >> 8) % usize)
        oy = mi.Int32((h >> 20) % usize)

        x = mi.Int32(pixel % width)
        y = mi.Int32(pixel // width)
        ux = (x + ox) % size
        uy = (y + oy) % size
        ux = dr.select(flip_x, size - 1 - ux, ux)
        uy = dr.select(flip_y, size - 1 - uy, uy)
        tx = dr.select(transpose, uy, ux)
        ty = dr.select(transpose, ux, uy)
        texel = offset + mi.UInt32(ty * size + tx)
        d = dr.gather(mi.Vector2i, self.pairing, texel)
        dx, dy = dr.select(transpose, d.y, d.x), dr.select(transpose, d.x, d.y)
        dx = dr.select(flip_x, -dx, dx)
        dy = dr.select(flip_y, -dy, dy)
        px = x + dx
        py = y + dy
        valid = (px >= 0) & (px < width) & (py >= 0) & (py < height)
        valid &= (dx != 0) | (dy != 0)
        px = dr.clip(px, 0, width - 1)
        py = dr.clip(py, 0, height - 1)
        return mi.UInt32(py * width + px), valid

    def spatial_prepass(
        self,
        scene: mi.Scene,
        reservoir_buf: mi.UInt32,
        si: mi.SurfaceInteraction3f,
        eye: mi.Point3f,
        seed: mi.UInt32,
    ) -> PairShift:
        """Shift every pixel's path to each of its partners (Sec. 3)."""
        n = dr.width(si.p)
        idx = dr.arange(mi.UInt32, n * self.spatial_neighbors)
        pixel, r = idx // self.spatial_neighbors, idx % self.spatial_neighbors
        partner, valid = self.partner(r, pixel, seed)
        res = PathReservoir.gather(reservoir_buf, pixel, mi.Bool(True))
        si_own = dr.gather(mi.SurfaceInteraction3f, si, pixel)
        si_q = dr.gather(mi.SurfaceInteraction3f, si, partner, valid)
        valid &= self.similar(si_q, si_own)
        threshold_q = self.footprint_threshold(si_q.p, si_q.n, eye)
        return self.shift(scene, res.path(), si_q, threshold_q, valid & (res.W > 0))

    @dr.syntax
    def resample_spatial(
        self,
        sampler: mi.Sampler,
        res: PathReservoir,
        si: mi.SurfaceInteraction3f,
        reservoir_buf: mi.UInt32,
        pairs: PairShift,
        seed: mi.UInt32,
    ) -> PathReservoir:
        """Pairwise MIS over the paired neighbors (Sec. 3.2)."""
        n = dr.width(si.p)
        lane = dr.arange(mi.UInt32, n)
        count = float(self.spatial_neighbors)
        out = PathReservoir.empty(n)
        res_z = res.path()
        phat_c = mi.luminance(res.F)
        canonical_weight = mi.Float(1.0)
        valid_count = mi.Float(0.0)
        r = mi.UInt32(0)
        while dr.hint(
            r < self.spatial_neighbors,
            max_iterations=self.spatial_neighbors,
            label="Spatial resampling",
        ):
            partner, valid = self.partner(r, lane, seed)
            si_q = dr.gather(mi.SurfaceInteraction3f, si, partner, valid)
            valid &= self.similar(si_q, si)
            neighbor = PathReservoir.gather(reservoir_buf, partner, valid)
            neighbor_z = neighbor.path()
            has_sample = valid & (neighbor.W > 0)
            # Neighbor's path shifted here
            slot = partner * self.spatial_neighbors + r
            inbound = dr.gather(PairShift, pairs, slot, has_sample)
            # This pixel's path shifted to the neighbor
            slot = lane * self.spatial_neighbors + r
            outbound = dr.gather(PairShift, pairs, slot, valid)
            J_in = inbound.jacobian(neighbor_z)
            J_out = outbound.jacobian(res_z)

            # Pairwise MIS as in resample_temporal(), with the confidence of
            # this pixel split evenly among its neighbors
            phat_n = dr.select(J_in > 0, mi.luminance(neighbor.F) / J_in, 0.0)
            phat_n_here = mi.luminance(inbound.F)
            phat_c_at_n = dr.select(J_out > 0, mi.luminance(outbound.F) * J_out, 0.0)
            m_n = balance_heuristic(neighbor.M * phat_n, res.M * phat_n_here / count)
            # This pixel's path keeps the share that the neighbor does not take
            canonical_weight += dr.select(
                valid,
                1.0 - balance_heuristic(neighbor.M * phat_c_at_n, res.M * phat_c / count),
                0.0,
            )
            valid_count += dr.select(valid, 1.0, 0.0)

            out.update(
                sampler,
                replace(neighbor_z, density=inbound.density).pack(),
                inbound.F,
                m_n * J_in * neighbor.W,
                has_sample & (J_in > 0),
            )
            out.M += dr.select(valid, neighbor.M, 0.0)
            r += 1

        out.update(sampler, res.z, res.F, canonical_weight * res.W, res.W > 0)
        out.M += res.M
        out.finalize(valid_count + 1.0)
        return out

    # ------------------------ Duplication map ------------------------

    @dr.syntax
    def duplication_scores(self, reservoir_buf: mi.UInt32) -> mi.Float:
        """Fraction of the 17x17 neighborhood with the same path seed (Sec. 5)."""
        n = self.film_size.x * self.film_size.y
        lane = dr.arange(mi.UInt32, n)
        width, height = self.film_size.x, self.film_size.y
        x, y = mi.Int32(lane % width), mi.Int32(lane // width)
        words = PathReservoir.WORDS
        # Seed 0 marks a pixel without a path
        seed = dr.gather(mi.UInt32, reservoir_buf, lane * words + 4)

        radius = self.duplication_radius
        side = 2 * radius + 1
        neighbors = side * side - 1
        count = mi.Float(0.0)
        i = mi.UInt32(0)
        while dr.hint(i < neighbors, max_iterations=neighbors):
            q = dr.select(i < radius * side + radius, i, i + 1)
            px = x + mi.Int32(q % side) - radius
            py = y + mi.Int32(q // side) - radius
            inside = (px >= 0) & (px < width) & (py >= 0) & (py < height)
            other = dr.select(inside, mi.UInt32(py * width + px), 0)
            other_seed = dr.gather(mi.UInt32, reservoir_buf, other * words + 4, inside)
            count += dr.select(inside & (other_seed == seed), 1.0, 0.0)
            i += 1
        return dr.select(seed != 0, count / max(neighbors, 1), 0.0)


mi.register_integrator("restirpt_rebuild", lambda props: RestirPTIntegrator(props))

