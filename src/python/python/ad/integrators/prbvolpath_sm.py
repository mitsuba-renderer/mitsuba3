from __future__ import annotations # Delayed parsing of type annotations

import struct

import drjit as dr
import mitsuba as mi

from .common import mis_weight
from .common import RBIntegrator


def index_spectrum(spec, idx):
    m = spec[0]
    if mi.is_rgb:
        m[idx == 1] = spec[1]
        m[idx == 2] = spec[2]
    return m


# Deferred records store spectra as one scalar buffer per component. The
# component count is variant-dependent -- three in RGB, but four in spectral
# variants, which carry a wavelength per component -- so the field names are
# generated rather than spelled out.
def spectrum_keys(prefix):
    """Record field names holding one spectrum, e.g. ('as0', 'as1', ...)."""
    return tuple(f'{prefix}{i}' for i in range(dr.size_v(mi.Spectrum)))


def spectrum_pack(prefix, value):
    """(name, component) pairs to scatter one spectrum into a record."""
    return tuple((f'{prefix}{i}', value[i])
                 for i in range(dr.size_v(mi.Spectrum)))


def spectrum_unpack(get, prefix):
    """Rebuild a spectrum from a record; `get` maps a field name to a Float."""
    return mi.Spectrum(*[get(f'{prefix}{i}')
                         for i in range(dr.size_v(mi.Spectrum))])


# A deferred record replays its segment in a *later* kernel, which rebuilds the
# medium interaction from scratch -- so the path's wavelengths have to travel
# with it, or the medium is evaluated at wavelength zero. Only spectral
# variants carry any: `Ray3f.wavelengths` is empty in RGB and monochromatic
# ones, where these three are no-ops.
def wavelength_keys():
    return spectrum_keys('wl') if mi.is_spectral else ()


def wavelength_pack(wavelengths):
    return spectrum_pack('wl', wavelengths) if mi.is_spectral else ()


def wavelength_restore(mei, get):
    if mi.is_spectral:
        mei.wavelengths = spectrum_unpack(get, 'wl')
    return mei




# TEA stream id for the probe-0 location of deferred records: derived from
# the record's ROW index only, so a second kernel can re-derive the exact
# same location (used by prbvolpath_sm_linear_mixed to collocate its
# deferred indirect suffix with the flush's probe 0).
_PROBE0_TEA = 0x8FB3D1A7


class _SuffixState:
    """
    Plain container used to re-enter :py:meth:`PRBVolpathSMIntegrator.sample`
    in primal mode, continuing a (detached) suffix path from a gradient probe
    location inside a medium.
    """
    def __init__(self, depth, medium, last_scatter_event,
                 last_scatter_direction_pdf, channel, active):
        self.depth = depth
        self.medium = medium
        self.last_scatter_event = last_scatter_event
        self.last_scatter_direction_pdf = last_scatter_direction_pdf
        self.channel = channel
        self.active = active


class PRBVolpathSMIntegrator(RBIntegrator):
    r"""
    .. _integrator-prbvolpath_sm:

    Sample-Matching PRB Volumetric Integrator (:monosp:`prbvolpath_sm`)
    ------------------------------------------------------------------

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth in the generated output image (where -1
         corresponds to :math:`\infty`). (Default: 6)

     * - rr_depth
       - |int|
       - Specifies the path depth, at which the implementation will begin to use
         the *russian roulette* path termination criterion. (Default: 5)

     * - hide_emitters
       - |bool|
       - Hide directly visible emitters. (Default: no, i.e. |false|)

     * - gradient_samples_per_segment
       - |int|
       - Number of gradient probe locations placed on each completed path
         segment inside a medium (the parameter :math:`\Lambda` in the paper).
         The first probe estimates direct *and* indirect in-scattered radiance
         (the latter via one recursive suffix path shared by the whole
         segment); additional probes only estimate direct lighting.
         (Default: 1; the paper uses 4)


     * - segment_slots
       - |int|
       - Record slots kept per lane, which is what selects between the two
         estimators:

         - :monosp:`max_depth + 1` (the default) gives every completed
           segment a slot of its own, so all of them are probed and nothing
           is reweighted. Adjoint storage and probe cost grow with path
           length.
         - :monosp:`1` keeps one weighted reservoir per lane: the path
           retains a single segment, which receives the whole probe workload
           scaled by the inverse selection probability. Cost is independent
           of path length. Also registered as
           :monosp:`prbvolpath_sm_linear`.

         Segment *j* goes to slot :monosp:`min(j, segment_slots - 1)`, so only
         the last slot pools more than one segment and only it runs a
         reservoir -- values in between are unbiased as well, but are neither
         documented behaviour nor covered by the tests.
         (Default: :monosp:`max_depth + 1`)

    This integrator extends the volumetric Path Replay Backpropagation
    integrator (:monosp:`prbvolpath`) with **sample matching** for extinction
    (:monosp:`sigma_t`) gradients:

    Differentiating volumetric transport with respect to the extinction
    coefficient yields two contributions with opposite signs — a *scattering*
    term (a density increase scatters more light toward the camera) and a
    *transmittance* term (a density increase attenuates all light passing
    through). Conventional estimators (e.g. differentiable delta tracking as
    used by :monosp:`prbvolpath`) evaluate the two terms at *different*
    locations, leaving their negative correlation unexploited. This integrator
    instead evaluates both terms at **shared, uniformly-sampled probe
    locations** on each path segment, activating the negative covariance and
    substantially reducing extinction-gradient variance without introducing
    bias.

    In primal mode this integrator behaves exactly like :monosp:`prbvolpath`.
    All sample-matching machinery only runs in the adjoint (backward) pass,
    which is organized in two stages: the path-replay loop consumes null
    collisions in a fused C++ walk (:py:meth:`Medium.sample_real_interaction`)
    and only *writes* one compact record per completed segment; the
    (ray-tracing heavy) probe estimation then runs in a small dedicated
    kernel over the compacted records, one lane per record rather than one
    per sample in flight. This keeps trace calls and their continuation
    state out of the large replay kernel, which matters under OptiX: state
    that survives a loop iteration is saved and restored around every trace
    call in it.

    Properties (inherited from :monosp:`prbvolpath`):

    - Emitter sampling (NEE), Russian roulette, surfaces + multiple media.
    - No projective sampling: geometric parameters (e.g. vertex positions)
      receive incorrect gradients.
    - Detached sampling: parameters of ideal specular objects cannot be
      optimized.
    - Forward-mode differentiation is not supported.

    See :cite:`Yu2026SampleMatching` for the sample-matching estimator,
    :cite:`NimierDavid2022Unbiased` for the differential-tracking (DRT)
    framework it builds on, and :cite:`Vicini2021` for the underlying PRB
    radiative-backpropagation framework. A linear-cost variant that
    subsamples the probe workload with an online reservoir is available as
    :monosp:`prbvolpath_sm_linear`.

    .. warning::
        This integrator is not supported in variants which track polarization
        states.

    .. tabs::

        .. code-tab:: python

            'type': 'prbvolpath_sm',
            'max_depth': 8,
            'gradient_samples_per_segment': 4
    """
    def __init__(self, props):
        super().__init__(props)
        self.use_nee = False
        self.nee_handle_homogeneous = False
        self.handle_null_scattering = False
        self.is_prepared = False
        self.gradient_samples_per_segment = props.get('gradient_samples_per_segment', 1)
        # Record slots per lane; see the plugin documentation. `max_depth`
        # gives every segment its own slot (no reweighting), 1 gives the
        # per-lane reservoir.
        # One more slot than the bounce budget, so the tail reservoir is a
        # pure overflow guard and the record layout matches the append-only
        # pool this replaces.
        self.segment_slots = int(props.get('segment_slots',
                                           self.max_depth + 1))

        if mi.is_polarized:
            raise Exception('PRBVolpathSMIntegrator does not support '
                            'polarized variants!')
        if self.gradient_samples_per_segment < 1:
            raise Exception('"gradient_samples_per_segment" must be >= 1')
        if self.segment_slots < 1:
            raise Exception('"segment_slots" must be >= 1')

    @dr.syntax
    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               δL: Optional[mi.Spectrum],
               state_in: Optional[mi.Spectrum],
               active: mi.Bool,
               path_state=None,
               **kwargs # Absorbs unused arguments
    ) -> Tuple[mi.Spectrum, mi.Bool, List[mi.Float], mi.Spectrum]:
        self.prepare_scene(scene)

        if mode == dr.ADMode.Forward:
            raise RuntimeError("PRBVolpathSMIntegrator doesn't support "
                               "forward-mode differentiation!")

        is_primal = mode == dr.ADMode.Primal

        ray = mi.Ray3f(ray)
        L = mi.Spectrum(0 if is_primal else state_in) # Radiance accumulator
        δL = mi.Spectrum(δL if δL is not None else 0) # Differential/adjoint radiance
        throughput = mi.Spectrum(1)                   # Path throughput weight
        η = mi.Float(1)                               # Index of refraction
        active = mi.Bool(active)

        si = dr.zeros(mi.SurfaceInteraction3f)
        needs_intersection = mi.Bool(True)
        valid_ray = mi.Bool(False)

        if path_state is not None:
            # Re-entry: continue a detached suffix path from a probe location
            # (only used by the adjoint pass; the recursion itself is primal).
            if not is_primal:
                raise RuntimeError('Recursive suffix rays must be traced in primal mode')
            depth = mi.UInt32(path_state.depth)
            medium = mi.MediumPtr(path_state.medium)
            last_scatter_event = mi.Interaction3f(path_state.last_scatter_event)
            last_scatter_direction_pdf = mi.Float(path_state.last_scatter_direction_pdf)
            channel = mi.UInt32(path_state.channel)
            specular_chain = mi.Bool(False)
        else:
            depth = mi.UInt32(0)
            last_scatter_event = dr.zeros(mi.Interaction3f)
            last_scatter_direction_pdf = mi.Float(1.0)
            # TODO: support sensors inside media
            medium = dr.zeros(mi.MediumPtr)
            specular_chain = mi.Bool(True)
            channel = 0
            if mi.is_rgb:
                # Sample a color channel to sample free-flight distances
                n_channels = dr.size_v(mi.Spectrum)
                channel = mi.UInt32(dr.minimum(n_channels * sampler.next_1d(active), n_channels - 1))

        # Secondary sampler driving all probe/suffix estimation in the adjoint.
        # One primary sample is consumed in *both* passes so that the primary
        # random number sequence stays aligned between primal and adjoint
        # (required by path replay).
        alt_seed_f = sampler.next_1d(active)
        alt_sampler = None
        if dr.hint(not is_primal, mode='scalar'):
            alt_seed = struct.unpack('!I', struct.pack('!f', alt_seed_f[0]))[0]
            alt_sampler = sampler.fork()
            alt_sampler.seed(mi.sample_tea_32(alt_seed, 1)[0],
                             sampler.wavefront_size())
        del alt_seed_f

        # Segment records live in a global buffer, `segment_slots` rows per
        # lane of the wavefront, written by a masked scatter at segment end so
        # the values die immediately.
        # Carrying a record as loop state instead (registers or local memory)
        # would make it part of the continuation OptiX saves around every
        # trace call of this megakernel: measured +2.5 KiB/thread spills and
        # +45% adjoint time. `res_wsum` is loop state, but it is one float.
        # It is initialized unconditionally because @dr.syntax requires loop
        # state to exist in every mode; only the top-level adjoint pass
        # records anything (recursive suffix rays and the primal pass never
        # reach the probe code).
        res_wsum = mi.Float(0.0)
        defer = (not is_primal) and (path_state is None)
        if dr.hint(defer, mode='scalar'):
            dfr_n = sampler.wavefront_size()   # one lane per sample in flight
            n_slots = self.segment_slots
            lane_idx = dr.arange(mi.UInt32, dfr_n)
            # Rows [0, n_append) are appended in allocation order, exactly as
            # the quadratic pool did; rows [n_append, n_append + dfr_n) are one
            # reserved, overwritable row per lane for the tail reservoir. At
            # the default the tail region stays empty and the layout is the
            # quadratic pool's; at segment_slots = 1 the append region has
            # length zero and the layout is one record per lane.
            n_append = dfr_n * (n_slots - 1)
            n_append_o = dr.opaque(mi.UInt32, n_append)
            dfr_ctr = dr.zeros(mi.UInt32, 1)
            # The reservoir compensation needs the weight sum over the WHOLE
            # path, which is only final at loop exit. Reading a loop output
            # after the flush's readback would force a loop replay, so mirror
            # it into a buffer of one entry per lane inside the loop and read
            # that instead.
            dfr_lane_wsum = dr.zeros(mi.Float, dfr_n)
            dfr = {k: dr.zeros(mi.Float, dfr_n * n_slots) for k in
                   ('ox', 'oy', 'oz', 'dx', 'dy', 'dz', 'itv', 'nu', 'nv', 'v')
                   + spectrum_keys('at') + spectrum_keys('as')
                   + wavelength_keys()}
            dfr['dep'] = dr.zeros(mi.UInt32, dfr_n * n_slots)
            dfr['ch'] = dr.zeros(mi.UInt32, dfr_n * n_slots)
            dfr['med'] = dr.zeros(mi.MediumPtr, dfr_n * n_slots)
        # Sample-matching segment state: a "segment" spans from the last real
        # scatter vertex (or last surface interaction) to the next one. Null
        # interactions do not end a segment: the direction is unchanged, so we
        # accumulate the distance traveled across them in `seg_dist`.
        seg_origin = mi.Point3f(ray.o)
        seg_dist = mi.Float(0.0)
        seg_ord = mi.UInt32(0)     # per-lane count of recorded segments

        while dr.hint(active,
                      label=f"PRB Sample Matching ({mode.name})"):
            active &= dr.any(throughput != 0.0)

            #--------------------- Perform russian roulette --------------------

            q = dr.minimum(dr.max(throughput) * dr.square(η), 0.99)
            perform_rr = (depth > self.rr_depth)
            active &= (sampler.next_1d(active) < q) | ~perform_rr
            throughput[perform_rr] = throughput * dr.rcp(q)

            active_medium = active & (medium != None)
            active_surface = active & ~active_medium

            with dr.resume_grad(when=not is_primal):
                #--------------------- Sample medium interaction -------------------

                # `handle_null_scattering` is set once at scene load: true
                # iff the scene contains a heterogeneous medium (same flag
                # and branch as upstream prbvolpath). It is a Python bool, so
                # `mode='scalar'` lets Dr.Jit pick one branch at trace time
                # and compile the other away entirely.
                #  - heterogeneous: null collisions exist, so free flight is
                #    delta-tracked by a fused C++ walk (majorant-grid DDA +
                #    accept-until-real inside sample_real_interaction);
                #    the loop body only ever sees real scatters or escapes.
                #  - homogeneous: free flight has a closed form, so we sample
                #    it analytically (verbatim upstream code path).
                if dr.hint(self.handle_null_scattering, mode='scalar'):
                    intersect = needs_intersection & active_medium
                    si[intersect] = scene.ray_intersect(ray, intersect)
                    needs_intersection &= ~active_medium
                    seed32 = mi.UInt32(sampler.next_1d(active_medium)
                                       * 4294967040.0)
                    with dr.suspend_grad():
                        mei, w_walk, scatter_prob = \
                            medium.sample_real_interaction(
                                ray, dr.detach(si.t), seed32, channel,
                                active_medium)
                    mei.t = dr.detach(mei.t)
                else:
                    mei = medium.sample_interaction(
                        ray, sampler.next_1d(active_medium), channel,
                        active_medium)
                    mei.t = dr.detach(mei.t)
                    ray.maxt[active_medium & medium.is_homogeneous() & mei.is_valid()] = mei.t
                    intersect = needs_intersection & active_medium
                    si[intersect] = scene.ray_intersect(ray, intersect)
                    needs_intersection &= ~active_medium
                    mei.t[active_medium & (si.t < mei.t)] = dr.inf

                # Free-flight weight of the sampled segment: the walk's
                # accumulated chain weight, or tr/pdf for the analytic case.
                # Detached either way — sigma_t derivatives come from the
                # matched probes below, not from these factors.
                weight = mi.Spectrum(1.0)
                if dr.hint(self.handle_null_scattering, mode='scalar'):
                    weight[active_medium] *= dr.detach(w_walk)
                else:
                    tr, free_flight_pdf = medium.transmittance_eval_pdf(mei, si, active_medium)
                    tr_pdf = index_spectrum(free_flight_pdf, channel)
                    weight[active_medium] *= dr.detach(dr.select(tr_pdf > 0.0, tr / tr_pdf, 0.0))
                    scatter_prob = mi.Float(1.0)

                escaped_medium = active_medium & ~mei.is_valid()
                active_medium &= mei.is_valid()
                act_medium_scatter = active_medium

                # NEE direction sample for this bounce, shared between the
                # path vertex and all gradient probes of the segment (the
                # emitter sample is "matched" as well).
                nee_dir_sample = sampler.next_2d(active)

                depth[act_medium_scatter] += 1
                last_scatter_event[act_medium_scatter] = dr.detach(mei)

                # Segment-end masks, captured *before* the depth cutoff so
                # that the final path segment still receives its probes.
                seg_end_scatter = mi.Bool(act_medium_scatter)
                seg_end = seg_end_scatter | escaped_medium

                # Don't estimate lighting if we exceeded number of bounces
                active &= depth < self.max_depth
                act_medium_scatter &= active

                # Adjoint weight for the probes' in-scattering term.
                #
                # The joint estimator (supplementary Eq. 54) integrates
                #
                #   sigma_t_i(t) T_i(t) [h_i(s) - h_i(t)] d sigma_t_i(s)/d0
                #
                # over (s, t), and Eq. 55-58 carry no weight at all because
                # they assume t was drawn from the free-flight density
                # p(t) ~ T(t) sigma_t(t), which cancels the leading factor
                # exactly. Delta tracking does not sample that density here:
                # all channels share one majorant and one accept/reject coin
                # whose probability is the channel *mean*
                #
                #   P = mean_j(sigma_t_j / kappa),   sigma_bar := kappa P,
                #
                # so real collisions arrive with density
                #
                #   q(t) = kappa P(t) exp(-int kappa P) = sigma_bar T_bar(t),
                #
                # i.e. the density of a fictitious gray medium with extinction
                # sigma_bar. Channel i is therefore short of
                #
                #   r_i = sigma_t_i(t) T_i(t) / (sigma_bar(t) T_bar(t)).
                #
                # Both halves are already accumulated by the walk. The null
                # events form a Poisson process of rate kappa - sigma_bar with
                # per-event weight (kappa - sigma_t_i)/(kappa - sigma_bar), so
                #
                #   E[prod nulls] = exp(int (sigma_bar - sigma_t_i))
                #                 = T_i(t) / T_bar(t),
                #
                # and that product is exactly `weight` up to the real hop's
                # 1/kappa. With sigma_bar = kappa * scatter_prob,
                #
                #   r_i = weight_i * sigma_t_i(t) / scatter_prob   (collision)
                #   r_i = weight_i                                 (escape)
                #
                # A gray medium gives weight = 1/kappa and sigma_t/P = kappa,
                # hence r = 1: this correction is invisible unless the
                # extinction actually varies across channels.
                #
                # The transmittance term needs no such factor: it is deposited
                # against `L`, which already carries the walk weight and the
                # vertex sigma_s / P -- together exactly r_i * h_i(t).
                #
                # `weight` still holds only the free-flight factor at this
                # point; the vertex sigma_s / scatter_prob is folded in below
                # and must stay out, because the probes re-evaluate sigma_s at
                # their own location.
                seg_chan_weight = dr.detach(dr.select(
                    seg_end_scatter,
                    weight * mei.sigma_t / scatter_prob,
                    weight))
                throughput_seg = throughput * seg_chan_weight

                weight[act_medium_scatter] *= dr.detach(mei.sigma_s) / scatter_prob
                throughput *= weight  # (all factors above are detached)

                mei = dr.detach(mei)

                if dr.hint(not is_primal, mode='scalar'):
                    # ==================== Sample matching ====================
                    # (Replaces prbvolpath's attached free-flight weight
                    # backpropagation.)

                    # (2) Matched gradient probes on the completed segment:
                    #     write one record; the probes themselves run in the
                    #     second-stage kernel after the loop.
                    interval = dr.detach(dr.select(
                        seg_end,
                        seg_dist + dr.select(escaped_medium, si.t, mei.t),
                        0.0))
                    # Rare geometric edge case: a lane inside the medium whose
                    # forward intersection failed (si.t = inf) while the walk
                    # exhausted the segment -> probe position at infinity ->
                    # NaN gradients in clamped boundary voxels. Skip it.
                    seg_end &= dr.isfinite(interval)
                    interval = dr.select(seg_end, interval, 0.0)
                    suffix_depth = dr.select(escaped_medium, depth + 1, depth)

                    # Segment j of this lane goes to slot min(j, n_slots-1),
                    # so only the last slot can ever receive more than one
                    # segment, and only it needs a reservoir. Two regimes come
                    # out of the same code:
                    #
                    #   n_slots = max_depth (default): a path cannot end more
                    #       segments than it takes bounces, so every segment
                    #       gets its own slot, the reservoir sees one candidate
                    #       and keeps it with probability v/v = 1, and the
                    #       compensation wsum/v is 1. Every segment is probed,
                    #       unweighted.
                    #   n_slots = 1: one reservoir per lane. The path retains a
                    #       single segment, which then receives its ENTIRE
                    #       probe workload scaled by wsum/v, standing in for
                    #       the sum over all segments.
                    #
                    # Selection uses the scalar mean of the segment's prefix
                    # throughput; the deposit compensates per channel, which
                    # is what keeps colored media unbiased. Nothing is ever
                    # dropped: a path with more segments than slots pools the
                    # overflow into the last slot's reservoir rather than
                    # overrunning a buffer.
                    tail = seg_ord >= (n_slots - 1)
                    v = dr.mean(dr.select(seg_end, dr.detach(throughput_seg),
                                          mi.Spectrum(0.0)))
                    # Segments before the last slot are always kept and simply
                    # appended. Everything from there on shares one reserved
                    # row per lane and is resolved by a weighted reservoir:
                    # keep with probability v / wsum, compensate with wsum / v.
                    # Both halves are unbiased on their own, so the estimator
                    # is unbiased at every slot count -- the count only moves
                    # the split between segments probed exactly and segments
                    # pooled into one.
                    res_wsum = dr.select(seg_end & tail, res_wsum + v, res_wsum)
                    dr.scatter(dfr_lane_wsum, res_wsum, lane_idx, seg_end)
                    ratio = dr.select(tail,
                                      dr.select(res_wsum > 0, v / res_wsum, 0.0),
                                      1.0)
                    keep = seg_end & (alt_sampler.next_1d(seg_end) <= ratio)
                    # scatter_inc hands out a single-use row id; use it as a
                    # scatter index only, never store it.
                    app = dr.scatter_inc(dfr_ctr, mi.UInt32(0), keep & ~tail)
                    slot = dr.select(tail, n_append_o + lane_idx, app)
                    ok = keep & (tail | (app < n_append_o))
                    at = dr.detach(δL * L)              # transmittance adjoint
                    asc = dr.detach(δL * throughput_seg)  # in-scattering adjoint
                    for _k, _v in (('ox', seg_origin.x), ('oy', seg_origin.y),
                                   ('oz', seg_origin.z), ('dx', ray.d.x),
                                   ('dy', ray.d.y), ('dz', ray.d.z),
                                   ('itv', interval), ('v', v),
                                   ('nu', nee_dir_sample.x),
                                   ('nv', nee_dir_sample.y)) \
                                  + spectrum_pack('at', at) \
                                  + spectrum_pack('as', asc) \
                                  + wavelength_pack(ray.wavelengths):
                        dr.scatter(dfr[_k], dr.detach(_v), slot, ok)
                    dr.scatter(dfr['dep'], suffix_depth, slot, ok)
                    dr.scatter(dfr['ch'], channel, slot, ok)
                    dr.scatter(dfr['med'], medium, slot, ok)
                    seg_ord = dr.select(seg_end, seg_ord + 1, seg_ord)

                    # =========================================================

                phase_ctx = mi.PhaseFunctionContext(sampler)
                phase = mei.medium.phase_function()
                phase[~act_medium_scatter] = dr.zeros(mi.PhaseFunctionPtr)

                #--------------------- Surface Interactions --------------------

                active_surface |= escaped_medium
                intersect = active_surface & needs_intersection
                si[intersect] = scene.ray_intersect(ray, intersect)

                # ---------------------- Hide area emitters ----------------------

                if dr.hint(self.hide_emitters, mode='scalar'):
                    # Are we on the first segment and did we hit an area emitter?
                    # If so, skip all area emitters along this ray
                    skip_emitters = (
                        si.is_valid() &
                        (si.shape.emitter() != None) &
                        (depth == 0) &
                        intersect
                    )

                    ray_skip = si.spawn_ray(ray.d)
                    pi = self.skip_area_emitters(scene, ray_skip, True, skip_emitters)
                    si_after_skip = pi.compute_surface_interaction(ray, mi.RayFlags.All, skip_emitters)
                    si[skip_emitters] = si_after_skip

                # ----------------- Intersection with emitters -----------------

                ray_from_camera = active_surface & (depth == 0)
                count_direct = ray_from_camera | specular_chain
                emitter = si.emitter(scene)
                active_e = active_surface & (emitter != None) & ~((depth == 0) & self.hide_emitters)

                # Get the PDF of sampling this emitter using next event estimation
                ds = mi.DirectionSample3f(scene, si, last_scatter_event)
                if dr.hint(self.use_nee, mode='scalar'):
                    emitter_pdf = scene.pdf_emitter_direction(last_scatter_event, ds, active_e)
                else:
                    emitter_pdf = 0.0
                emitted = emitter.eval(si, active_e)
                contrib = dr.select(count_direct, throughput * emitted,
                                    throughput * mis_weight(last_scatter_direction_pdf, emitter_pdf) * emitted)
                L[active_e] += dr.detach(contrib if is_primal else -contrib)
                if dr.hint(not is_primal and dr.grad_enabled(contrib), mode='scalar'):
                    dr.backward(δL * contrib)

                active_surface &= si.is_valid()
                ctx = mi.BSDFContext()
                bsdf = si.bsdf(ray)

                # ---------------------- Emitter sampling ----------------------

                if dr.hint(self.use_nee, mode='scalar'):
                    active_e_surface = active_surface & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth) & (depth + 1 < self.max_depth)
                    sample_emitters = mei.medium.use_emitter_sampling()
                    specular_chain &= ~act_medium_scatter
                    specular_chain |= act_medium_scatter & ~sample_emitters

                    active_e_medium = act_medium_scatter & sample_emitters
                    active_e = active_e_surface | active_e_medium

                    nee_sampler = sampler if is_primal else sampler.clone()
                    emitted, ds = self.sample_emitter(mei, si, active_e_medium, active_e_surface,
                        scene, sampler, medium, channel, active_e, mode=dr.ADMode.Primal,
                        dir_sample=nee_dir_sample)

                    # Query the BSDF for that emitter-sampled direction
                    bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, si.to_local(ds.d), active_e_surface)
                    phase_val, phase_pdf = phase.eval_pdf(phase_ctx, mei, ds.d, active_e_medium)
                    nee_weight = dr.select(active_e_surface, bsdf_val, phase_val)
                    nee_directional_pdf = dr.select(ds.delta, 0.0, dr.select(active_e_surface, bsdf_pdf, phase_pdf))

                    contrib = throughput * nee_weight * mis_weight(ds.pdf, nee_directional_pdf) * emitted
                    L[active_e] += dr.detach(contrib if is_primal else -contrib)

                    if dr.hint(not is_primal, mode='scalar'):
                        self.sample_emitter(mei, si, active_e_medium, active_e_surface,
                            scene, nee_sampler, medium, channel, active_e, adj_emitted=contrib,
                            δL=δL, mode=mode, dir_sample=nee_dir_sample)

                        if dr.hint(dr.grad_enabled(nee_weight) or dr.grad_enabled(emitted), mode='scalar'):
                            dr.backward(δL * contrib)

                #-------------------- Phase function sampling ------------------

                valid_ray |= act_medium_scatter
                with dr.suspend_grad():
                    wo, phase_weight, phase_pdf = phase.sample(phase_ctx, mei,
                                                               sampler.next_1d(act_medium_scatter),
                                                               sampler.next_2d(act_medium_scatter),
                                                               act_medium_scatter)
                act_medium_scatter &= phase_pdf > 0.0

                # Re evaluate the phase function value in an attached manner
                phase_eval, _ = phase.eval_pdf(phase_ctx, mei, wo, act_medium_scatter)
                if dr.hint(not is_primal and dr.grad_enabled(phase_eval), mode='scalar'):
                    Lo = phase_eval * dr.detach(dr.select(act_medium_scatter, L / dr.maximum(1e-8, phase_eval), 0.0))
                    if mode == dr.ADMode.Backward:
                        dr.backward_from(δL * Lo)
                    else:
                        δL += dr.forward_to(Lo)

                throughput[act_medium_scatter] *= phase_weight
                ray[act_medium_scatter] = mei.spawn_ray(wo)
                needs_intersection |= act_medium_scatter
                last_scatter_direction_pdf[act_medium_scatter] = phase_pdf

                # ------------------------ BSDF sampling -----------------------

                with dr.suspend_grad():
                    bs, bsdf_weight = bsdf.sample(ctx, si,
                                                  sampler.next_1d(active_surface),
                                                  sampler.next_2d(active_surface),
                                                  active_surface)
                    active_surface &= bs.pdf > 0

                bsdf_eval = bsdf.eval(ctx, si, bs.wo, active_surface)

                if dr.hint(not is_primal and dr.grad_enabled(bsdf_eval), mode='scalar'):
                    Lo = bsdf_eval * dr.detach(dr.select(active_surface, L / dr.maximum(1e-8, bsdf_eval), 0.0))
                    if dr.hint(mode == dr.ADMode.Backward, mode='scalar'):
                        dr.backward_from(δL * Lo)
                    else:
                        δL += dr.forward_to(Lo)

                throughput[active_surface] *= bsdf_weight
                η[active_surface] *= bs.eta
                bsdf_ray = si.spawn_ray(si.to_world(bs.wo))
                ray[active_surface] = bsdf_ray

                needs_intersection |= active_surface
                non_null_bsdf = active_surface & ~mi.has_flag(bs.sampled_type, mi.BSDFFlags.Null)
                depth[non_null_bsdf] += 1

                # update the last scatter PDF event if we encountered a non-null scatter event
                last_scatter_event[non_null_bsdf] = si
                last_scatter_direction_pdf[non_null_bsdf] = bs.pdf

                valid_ray |= non_null_bsdf
                specular_chain |= non_null_bsdf & mi.has_flag(bs.sampled_type, mi.BSDFFlags.Delta)
                specular_chain &= ~(active_surface & mi.has_flag(bs.sampled_type, mi.BSDFFlags.Smooth))
                has_medium_trans = active_surface & si.is_medium_transition()
                medium[has_medium_trans] = si.target_medium(ray.d)

                # A new segment starts at every real scatter vertex and at
                # every surface interaction (incl. null boundary crossings).
                seg_origin[act_medium_scatter] = dr.detach(mei.p)
                seg_origin[active_surface] = dr.detach(si.p)
                seg_dist[act_medium_scatter | active_surface] = 0.0

                active &= (active_surface | active_medium)

        # ---- Deferred-probe pass: estimate probe lighting in a compact,
        # coherent kernel of its own (records were written in the loop).
        if dr.hint(defer, mode='scalar'):
            self._flush_deferred_probes(scene, dfr, dfr_lane_wsum,
                                        n_append, salt=alt_seed)

        return L if is_primal else δL, valid_ray, [], L

    def _flush_deferred_probes(self, scene, dfr, dfr_lane_wsum, n_append,
                               include_indirect=True, probe0_from_rows=False,
                               salt=0):
        """
        Second stage of the deferred-probe design: gather the per-segment
        records written by the path-replay loop and run the (ray-tracing
        heavy) probe estimation at segment granularity.

        Most slots stay empty, so the occupied ones are compacted first and
        the probe work lands in a separate, much smaller kernel with coherent
        lanes. Compacting also forces the replay kernel to execute, which the
        LLVM backend needs: an eager backward on the unevaluated loop-exit
        graph silently drops part of the gradient there.
        """
        dr.eval(dfr)
        idx = dr.compress(dfr['v'] > 0)
        n = int(dr.width(idx))
        if n == 0:
            return
        g = lambda k: dr.gather(mi.Float, dfr[k], idx)
        # Appended rows were kept unconditionally and carry no weight. Rows in
        # the reserved tail region went through the reservoir, so each stands in
        # for every segment that shared its lane's tail and is scaled by the
        # inverse selection probability.
        is_tail = idx >= n_append
        lane = dr.select(is_tail, idx - n_append, 0)
        comp = dr.select(is_tail,
                         dr.gather(mi.Float, dfr_lane_wsum, lane, is_tail)
                         / dr.maximum(g('v'), 1e-30), 1.0)
        origin = mi.Point3f(g('ox'), g('oy'), g('oz'))
        seg_dir = mi.Vector3f(g('dx'), g('dy'), g('dz'))
        interval = g('itv')
        adj_trans = spectrum_unpack(g, 'at') * comp
        adj_scatt = spectrum_unpack(g, 'as') * comp
        nee_dir = mi.Point2f(g('nu'), g('nv'))
        suffix_depth = dr.gather(mi.UInt32, dfr['dep'], idx)
        channel = dr.gather(mi.UInt32, dfr['ch'], idx)
        medium = dr.gather(mi.MediumPtr, dfr['med'], idx)

        smp = mi.load_dict({'type': 'independent'})
        # Salted per render: keeps probe random streams fresh (see above).
        smp.seed(dr.opaque(mi.UInt32, (n ^ 0x9E3779B9) ^ salt), n)

        mei = dr.zeros(mi.MediumInteraction3f, n)
        mei.medium = medium
        mei.p = origin
        # Probes stand in for interactions on this ray: they must carry the
        # incident direction and frame, or anisotropic phase functions are
        # evaluated in a degenerate frame (same failure class as the C++
        # walks' missing wi/sh_frame).
        mei.wi = -seg_dir
        mei.sh_frame = mi.Frame3f(mei.wi)
        # The record replays in a fresh kernel: without this the medium is
        # evaluated at wavelength zero in spectral variants.
        wavelength_restore(mei, g)

        probe0_xi = None
        if probe0_from_rows:
            # Location key = (lane, per-lane record ordinal): derivable by
            # the record's own lane after the flush, unlike the row index
            # (a scatter_inc output, which must not be stored/reused).
            ln = dr.gather(mi.UInt32, dfr['ln'], idx)
            od = dr.gather(mi.UInt32, dfr['ord'], idx)
            # The per-render salt keeps the location fresh across renders —
            # without it every lane reuses the SAME probe locations forever
            # and per-voxel gradients converge to xi-conditioned values
            # (frozen speckle that no amount of averaging removes).
            salt_o = dr.opaque(mi.UInt32, salt)
            probe0_xi = mi.sample_tea_float32(ln ^ mi.UInt32(_PROBE0_TEA),
                                              od ^ salt_o)
        self._sample_segment_probes(scene, medium, channel, smp, mei,
                                    origin, seg_dir, interval,
                                    adj_trans, adj_scatt, nee_dir,
                                    suffix_depth, mi.Bool(True),
                                    include_indirect=include_indirect,
                                    probe0_xi=probe0_xi)

    def _sample_segment_probes(self, scene, medium, channel, alt_sampler, mei,
                               seg_origin, seg_dir, interval, adj_trans, adj_scatt,
                               nee_dir_sample, suffix_depth, active,
                               include_indirect=True, probe0_xi=None):
        """
        Sample-matched gradient probes for one completed path segment
        (adjoint pass only).

        Places `gradient_samples_per_segment` locations uniformly on the segment
        `seg_origin + t * seg_dir, t in [0, interval]` and deposits, at each
        probe location `y`:

          - the transmittance derivative  `-sigma_t(y) * adj_trans`, and
          - the in-scattering derivative  `+sigma_s(y) * adj_scatt * Li(y)`,

        where both terms share the *same* `sigma_t(y)` evaluation — this is
        the sample-matching estimator that activates the negative correlation
        between the two terms. `Li(y)` is decomposed into direct lighting
        (estimated at every probe with a shared NEE direction sample) and
        indirect lighting (estimated with a single recursive suffix path,
        shared by the whole segment).
        """
        n_probes = self.gradient_samples_per_segment
        within = active & (suffix_depth < self.max_depth)

        # Restrict the probe domain to the segment's overlap with the density
        # grid's bounding box. Transport treats the region outside that box as
        # vacuum (free flight), so the density derivative vanishes there —
        # but Volume::eval() *clamps* lookup coordinates, so probing outside
        # the box would deposit spurious gradients into the boundary voxels
        # whenever the medium's shape extends beyond the grid.
        seg_ray = mi.Ray3f(mi.Point3f(seg_origin), mi.Vector3f(seg_dir))
        bb_hit, bb0, bb1 = medium.intersect_aabb(seg_ray)
        t0 = dr.detach(dr.clip(bb0, 0.0, interval))
        t1 = dr.detach(dr.clip(bb1, 0.0, interval))
        sub_len = dr.select(active & bb_hit, t1 - t0, 0.0)
        active = active & (sub_len > 0)
        within &= active

        # Probe interactions inherit the frame/wavelengths/medium pointer of
        # the segment's medium interaction; only position/distance change.
        mei_sub = mi.MediumInteraction3f(mei)

        phase_ctx = mi.PhaseFunctionContext(alt_sampler)
        phase = mei_sub.medium.phase_function()
        phase[~active] = dr.zeros(mi.PhaseFunctionPtr)

        contribs = mi.Spectrum(0.0)
        for i in range(n_probes):
            if i == 0 and probe0_xi is not None:
                # Externally prescribed location for the main probe (row-
                # derived; lets a later kernel re-derive the same point).
                xi = probe0_xi
            else:
                xi = alt_sampler.next_1d(active)
            mei_sub.t = dr.fma(xi, sub_len, t0)
            mei_sub.p = dr.fma(seg_dir, mei_sub.t, seg_origin)

            with dr.suspend_grad():
                if i == 0 and include_indirect:
                    # Main probe: direct + one shared recursive suffix.
                    nee_Li, ind_Li = self._probe_radiance(
                        scene, medium, channel, alt_sampler, mei_sub,
                        phase_ctx, phase, nee_dir_sample, suffix_depth, within)
                    Li = nee_Li + n_probes * ind_Li
                else:
                    # Additional probes: direct lighting only (the indirect
                    # component is amortized over the segment by the
                    # `n_probes` factor above).
                    Li = self._probe_direct(
                        scene, medium, channel, alt_sampler, mei_sub,
                        phase_ctx, phase, nee_dir_sample, within)

            with dr.resume_grad():
                sigma_s_sub, _, sigma_t_sub = \
                    medium.get_scattering_coefficients(mei_sub, active)
                # Matched estimator: both terms evaluate the extinction at
                # the *same* location `mei_sub.p`. sigma_s is attached
                # directly — the chain rule routes its derivative to the
                # medium's parameters (sigma_t x albedo grids, or a direct
                # sigma_s grid). Never rebuild albedo as sigma_s/sigma_t:
                # the ratio is 0 where sigma_t = 0 while the true scattering
                # response is not, silently zeroing empty-voxel gradients.
                contribs -= sigma_t_sub * adj_trans
                contribs += sigma_s_sub * adj_scatt * Li

        # Uniform probe placement: pdf = 1 / sub_len (per probe)
        inv_pdf = sub_len / n_probes
        with dr.resume_grad():
            if dr.hint(dr.grad_enabled(contribs), mode='scalar'):
                safe = active & dr.all(dr.isfinite(contribs))
                # This backward runs in an *evaluated* context, unlike the
                # in-loop backward calls above, which are re-traced (and
                # hence re-attached) on every render. The default
                # ADFlag.ClearEdges would delete the persistent
                # parameter->texture edges shared with subsequent renders in
                # the same session, silently zeroing their gradients from the
                # second render onward. Keep the graph; only clear vertex
                # gradients.
                dr.backward(dr.select(safe, contribs, 0.0) * inv_pdf,
                            flags=dr.ADFlag.ClearVertices)

    def _probe_radiance(self, scene, medium, channel, alt_sampler, mei_sub,
                        phase_ctx, phase, nee_dir_sample, suffix_depth, active):
        """
        Estimate the in-scattered radiance at a probe location, decomposed
        into (direct NEE, indirect) components.
        """
        nee_Li = self._probe_direct(scene, medium, channel, alt_sampler, mei_sub,
                                    phase_ctx, phase, nee_dir_sample, active)
        ind_Li = self._probe_indirect(scene, channel, alt_sampler, mei_sub,
                                      phase_ctx, phase, suffix_depth, active)
        return dr.select(active, nee_Li, 0.0), ind_Li

    def _probe_indirect(self, scene, channel, alt_sampler, mei_sub,
                        phase_ctx, phase, suffix_depth, active):
        """
        Indirect in-scattered radiance at a probe location: phase-sample a
        direction and trace one detached suffix path by recursively invoking
        :py:meth:`sample` in primal mode.
        """
        wo, phase_weight, phase_pdf = phase.sample(
            phase_ctx, mei_sub,
            alt_sampler.next_1d(active), alt_sampler.next_2d(active), active)
        rec_active = active & (phase_pdf > 0.0)
        rec_ray = mei_sub.spawn_ray(wo)

        last_scatter = dr.zeros(mi.Interaction3f)
        last_scatter[rec_active] = mei_sub
        state = _SuffixState(depth=mi.UInt32(suffix_depth),
                             medium=mi.MediumPtr(mei_sub.medium),
                             last_scatter_event=last_scatter,
                             last_scatter_direction_pdf=dr.select(rec_active, phase_pdf, 1.0),
                             channel=channel,
                             active=rec_active)
        Li, _, _, _ = self.sample(dr.ADMode.Primal, scene, alt_sampler, rec_ray,
                                  δL=None, state_in=None, active=rec_active,
                                  path_state=state)

        return dr.select(rec_active, phase_weight * Li, 0.0)

    def _probe_direct(self, scene, medium, channel, alt_sampler, mei_sub,
                      phase_ctx, phase, nee_dir_sample, active):
        """
        Direct lighting (NEE) at a probe location, using the segment's shared
        emitter direction sample and MIS against phase sampling.
        """
        emitted, ds = self.sample_emitter(
            mei_sub, dr.zeros(mi.SurfaceInteraction3f), active, mi.Bool(False),
            scene, alt_sampler, medium, channel, active,
            mode=dr.ADMode.Primal, dir_sample=nee_dir_sample)
        phase_val, phase_pdf = phase.eval_pdf(phase_ctx, mei_sub, ds.d, active)
        nee_directional_pdf = dr.select(ds.delta, 0.0, phase_pdf)
        return dr.select(active,
                         phase_val * mis_weight(ds.pdf, nee_directional_pdf) * emitted,
                         0.0)

    def prepare_scene(self, scene):
        if self.is_prepared:
            return

        for shape in scene.shapes():
            for medium in [shape.interior_medium(), shape.exterior_medium()]:
                if medium is not None:
                    # Enable NEE if a medium specifically asks for it
                    self.use_nee = self.use_nee or medium.use_emitter_sampling()
                    self.nee_handle_homogeneous = self.nee_handle_homogeneous or medium.is_homogeneous()
                    self.handle_null_scattering = self.handle_null_scattering or (not medium.is_homogeneous())
        self.is_prepared = True
        # By default enable always NEE in case there are surfaces
        self.use_nee = True

    @dr.syntax
    def sample_emitter(self, mei, si, active_medium, active_surface, scene, sampler, medium, channel,
                       active, adj_emitted=None, δL=None, mode=None, dir_sample=None):
        is_primal = mode == dr.ADMode.Primal

        active = mi.Bool(active)

        ref_interaction = dr.zeros(mi.Interaction3f)
        ref_interaction[active_medium] = mei
        ref_interaction[active_surface] = si

        # An externally-provided direction sample allows correlating this
        # emitter sample with other estimators (e.g. `prbvolpath_sm` evaluates
        # matched gradient probes with the same emitter direction sample).
        if dir_sample is None:
            dir_sample = sampler.next_2d(active)

        ds, emitter_val = scene.sample_emitter_direction(ref_interaction,
                                                         dir_sample,
                                                         False, active)
        ds = dr.detach(ds)
        invalid = (ds.pdf == 0.0)
        emitter_val[invalid] = 0.0
        active &= ~invalid

        medium = dr.select(active, medium, dr.zeros(mi.MediumPtr))
        medium[(active_surface & si.is_medium_transition())] = si.target_medium(ds.d)

        ray = ref_interaction.spawn_ray_to(ds.p)
        max_dist = mi.Float(ray.maxt)
        total_dist = mi.Float(0.0)
        si = dr.zeros(mi.SurfaceInteraction3f)
        needs_intersection = mi.Bool(True)
        transmittance = mi.Spectrum(1.0)

        while dr.hint(active, label=f"PRB Next Event Estimation ({mode.name})"):
            remaining_dist = max_dist - total_dist
            ray.maxt = dr.detach(remaining_dist)
            active &= remaining_dist > 0.0

            # This ray will not intersect if it reached the end of the segment
            needs_intersection &= active
            # The NEE transmittance walk only consumes si.t/p/n and the shape
            # pointer (null-transmission + medium transitions); skip the full
            # shading-frame/UV construction to keep the traced state small.
            si[needs_intersection] = scene.ray_intersect(
                ray, ray_flags=mi.RayFlags.Minimal, coherent=mi.Bool(False),
                active=needs_intersection)
            needs_intersection &= False

            active_medium = active & (medium != None)
            active_surface = active & ~active_medium

            # Handle medium interactions / transmittance
            mei = medium.sample_interaction(ray, sampler.next_1d(active_medium), channel, active_medium)
            mei.t[active_medium & (si.t < mei.t)] = dr.inf
            mei.t = dr.detach(mei.t)

            tr_multiplier = mi.Spectrum(1.0)

            # Special case for homogeneous media: directly advance to the next surface / end of the segment
            if dr.hint(self.nee_handle_homogeneous, mode='scalar'):
                active_homogeneous = active_medium & medium.is_homogeneous()
                mei.t[active_homogeneous] = dr.minimum(remaining_dist, si.t)
                tr_multiplier[active_homogeneous] = medium.transmittance_eval_pdf(mei, si, active_homogeneous)[0]
                mei.t[active_homogeneous] = dr.inf

            escaped_medium = active_medium & ~mei.is_valid()

            # Ratio tracking transmittance computation
            active_medium &= mei.is_valid()
            ray.o[active_medium] = dr.detach(mei.p)
            si.t[active_medium] = dr.detach(si.t - mei.t)
            tr_multiplier[active_medium] *= mei.sigma_n / mei.combined_extinction


            # Handle interactions with surfaces
            active_surface |= escaped_medium
            active_surface &= si.is_valid() & ~active_medium
            bsdf = si.bsdf(ray)
            bsdf_val = bsdf.eval_null_transmission(si, active_surface)
            tr_multiplier[active_surface] *= bsdf_val

            if dr.hint(not is_primal and dr.grad_enabled(tr_multiplier), mode='scalar'):
                active_adj = (active_surface | active_medium) & (tr_multiplier > 0.0)
                dr.backward(tr_multiplier * dr.detach(dr.select(active_adj, δL * adj_emitted / tr_multiplier, 0.0)))

            transmittance *= dr.detach(tr_multiplier)

            # Update the ray with new origin & t parameter
            ray[active_surface] = dr.detach(si.spawn_ray(mi.Vector3f(ray.d)))
            ray.maxt = dr.detach(remaining_dist)
            needs_intersection |= active_surface

            # Continue tracing through scene if non-zero weights exist
            active &= (active_medium | active_surface) & dr.any(transmittance != 0.0)
            total_dist[active] += dr.select(active_medium, mei.t, si.t)

            # If a medium transition is taking place: Update the medium pointer
            has_medium_trans = active_surface & si.is_medium_transition()
            medium[has_medium_trans] = si.target_medium(ray.d)

        return emitter_val * dr.detach(transmittance), ds

    def to_string(self):
        return (f'PRBVolpathSMIntegrator[max_depth = {self.max_depth}, '
                f'gradient_samples_per_segment = {self.gradient_samples_per_segment}, '
                f'segment_slots = {self.segment_slots}]')

mi.register_integrator(
    "prbvolpath_sm", lambda props: PRBVolpathSMIntegrator(props))


def _linear(props):
    """`prbvolpath_sm` with a single record slot per lane."""
    if 'segment_slots' not in props:
        props['segment_slots'] = 1
    return PRBVolpathSMIntegrator(props)


mi.register_integrator("prbvolpath_sm_linear", _linear)

# The name the two-plugin layout used for the default configuration; kept so
# existing scenes and the stress harness resolve.
mi.register_integrator(
    "prbvolpath_sm_quad", lambda props: PRBVolpathSMIntegrator(props))

del RBIntegrator