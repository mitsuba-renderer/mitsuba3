from __future__ import annotations # Delayed parsing of type annotations

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


class _SegmentRecord:
    """One completed segment: written by the replay loop, read by the second kernel"""
    DRJIT_STRUCT = {'origin': mi.Point3f, 'direction': mi.Vector3f,
                    'interval': mi.Float, 'segment_weight': mi.Float, 'nee': mi.Point2f,
                    'adj_trans': mi.Spectrum, 'adj_scatt': mi.Spectrum,
                    'wavelengths': type(dr.zeros(mi.Ray3f).wavelengths),
                    'depth': mi.UInt32, 'channel': mi.UInt32, 'medium': mi.MediumPtr}

    def __init__(self):
        for name, tp in self.DRJIT_STRUCT.items():
            setattr(self, name, dr.zeros(tp))


class _SuffixState:
    """State to re-enter `sample()` in primal mode from a gradient sample"""
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

    Sample Matching PRB Volumetric Integrator (:monosp:`prbvolpath_sm`)
    ------------------------------------------------------------------

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth. (Default: 6)

     * - rr_depth
       - |int|
       - Specifies the path depth, at which the implementation will begin to use
         the *russian roulette* path termination criterion. (Default: 5)

     * - gradient_samples_per_segment
       - |int|
       - Number of gradient samples placed on each path segment inside a
         medium. The first one also estimates indirect illumination with a
         recursive suffix path; the others estimate direct illumination
         only. (Default: 1)

     * - segment_slots
       - |int|
       - Number of segment records a path may keep. The default of
         :monosp:`max_depth + 1` keeps every segment. With :monosp:`1`, each
         path keeps a single segment chosen by weighted reservoir sampling and
         its gradient is reweighted by the inverse selection probability. This
         makes a smooth transition between the quadratic and linear complexity
         variants. The :monosp:`1` configuration is also registered as
         :monosp:`prbvolpath_sm_linear` and the default as
         :monosp:`prbvolpath_sm_quad`. (Default: :monosp:`max_depth + 1`)

    Differentiating volumetric transport with respect to the extinction
    coefficient gives two terms of opposite sign: denser media scatter more
    light toward the camera, and denser media block more of the light passing
    through. :monosp:`prbvolpath` estimates the two at unrelated points, so
    their negative correlation is wasted. This integrator evaluates both at
    the same points along each path segment, which is the sample matching
    estimator of :cite:`Yu2026SampleMatching`. Primal rendering is the same
    as in :monosp:`prbvolpath`; all changes brought by sample matching work in
    the adjoint pass.

    The adjoint pass runs as two kernels. The path replay loop writes one
    record per completed segment, and a second kernel reads the records back
    and traces new rays for the gradient samples. The two kernels are there
    because of OptiX: it saves and restores all live loop state around every
    trace call, so tracing inside the replay loop would carry the records
    through every iteration.

    Otherwise it has the same properties as :monosp:`prbvolpath`:

    - Differentiable delta tracking for free-flight distance sampling

    - Emitter sampling (a.k.a. next event estimation).

    - Russian Roulette stopping criterion.

    - No projective sampling. This means that the integrator cannot be used for
      shape optimization (it will return incorrect/biased gradients for
      geometric parameters like vertex positions.)

    - Detached sampling. This means that the properties of ideal specular
      objects (e.g., the IOR of a glass vase) cannot be optimized.

    See :cite:`NimierDavid2022Unbiased` for differential delta tracking and
    :cite:`Vicini2021` for path replay backpropagation.

    .. warning::
        This integrator is not supported in variants which track polarization
        states.

    .. tabs::

        .. code-tab:: python

            'type': 'prbvolpath_sm',
            'max_depth': 8
    """
    def __init__(self, props):
        super().__init__(props)
        self.use_nee = False
        self.nee_handle_homogeneous = False
        self.handle_null_scattering = False
        self.is_prepared = False
        self.gradient_samples_per_segment = props.get('gradient_samples_per_segment', 1)
        self.segment_slots = props.get('segment_slots', self.max_depth + 1)

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
            # Continue a suffix path from a gradient sample
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

        # Sampler for the reservoir choice and the gradient samples in the second kernel
        alt_seed = dr.reinterpret_array(mi.UInt32, sampler.next_1d(active))
        alt_sampler = None
        if dr.hint(not is_primal, mode='scalar'):
            alt_sampler = sampler.fork()
            alt_sampler.seed(alt_seed, sampler.wavefront_size())

        # Records are written to a global buffer instead of being carried as loop variables,
        # which OptiX would save and restore at every ray trace. Segment j of a lane goes to
        # row j * lanes + lane; the rows after n_append hold one reservoir per lane.
        reservoir_weight_sum = mi.Float(0.0)
        defer = (not is_primal) and (path_state is None)
        if dr.hint(defer, mode='scalar'):
            dfr_n = sampler.wavefront_size()
            n_slots = self.segment_slots
            lane_idx = dr.arange(mi.UInt32, dfr_n)
            n_append = dfr_n * (n_slots - 1)
            n_append_o = dr.opaque(mi.UInt32, n_append)
            # Reservoir weight sums, read back by the second kernel
            lane_reservoir_weight_sum = dr.zeros(mi.Float, dfr_n)
            records = dr.zeros(_SegmentRecord, dfr_n * n_slots)
        # A segment runs from one real scattering event or surface interaction to the next
        seg_origin = mi.Point3f(ray.o)
        seg_ord = mi.UInt32(0)     # Number of segments recorded by this lane

        while dr.hint(active,
                      label=f"PRB Sample Matching ({mode.name})"):
            active &= dr.any(throughput != 0.0)

            #--------------------- Perform russian roulette --------------------

            q = dr.minimum(dr.max(throughput) * dr.square(η), 0.99)
            perform_rr = (depth > self.rr_depth)
            active &= (sampler.next_1d(active) < q) | ~perform_rr
            throughput[perform_rr] = throughput * dr.rcp(q)

            # Camera rays trace with the camera mask, which hides emitters marked as invisible
            ray_mask = dr.select(depth == 0, mi.RayMask.Camera, mi.RayMask.All)

            active_medium = active & (medium != None)
            active_surface = active & ~active_medium

            with dr.resume_grad(when=not is_primal):
                #--------------------- Sample medium interaction -------------------

                # The nested loop takes null collisions; this loop sees real scatters and escapes
                if dr.hint(self.handle_null_scattering, mode='scalar'):
                    intersect = needs_intersection & active_medium
                    si[intersect] = scene.ray_intersect(
                        ray, mi.RayFlags.Default, False, intersect,
                        visibility_mask=ray_mask)
                    needs_intersection &= ~active_medium
                    with dr.suspend_grad():
                        mei, w_walk, scatter_prob = self.sample_real_interaction(
                            medium, ray, dr.detach(si.t), sampler, channel,
                            active_medium)
                    mei.t = dr.detach(mei.t)
                else:
                    mei = medium.sample_interaction(
                        ray, sampler.next_1d(active_medium), channel,
                        active_medium)
                    mei.t = dr.detach(mei.t)
                    ray.maxt[active_medium & medium.is_homogeneous() & mei.is_valid()] = mei.t
                    intersect = needs_intersection & active_medium
                    si[intersect] = scene.ray_intersect(
                        ray, mi.RayFlags.Default, False, intersect,
                        visibility_mask=ray_mask)
                    needs_intersection &= ~active_medium
                    mei.t[active_medium & (si.t < mei.t)] = dr.inf

                # Free flight weight, detached; the gradient samples carry the extinction derivative
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

                # Emitter direction sample, shared with the gradient samples
                nee_dir_sample = sampler.next_2d(active)

                depth[act_medium_scatter] += 1
                last_scatter_event[act_medium_scatter] = dr.detach(mei)

                # The last segment is recorded even if the depth limit ends the path here
                seg_end_scatter = mi.Bool(act_medium_scatter)
                seg_end = seg_end_scatter | escaped_medium

                # Don't estimate lighting if we exceeded number of bounces
                active &= depth < self.max_depth
                act_medium_scatter &= active

                # All channels share one acceptance test, so a real collision arrives with the
                # density of a gray medium at the channel mean. Restore each channel's own
                # sigma_t * T; the factor is 1 for gray media.
                seg_chan_weight = dr.detach(dr.select(
                    seg_end_scatter,
                    weight * mei.sigma_t / scatter_prob,
                    weight))
                throughput_seg = throughput * seg_chan_weight

                weight[act_medium_scatter] *= dr.detach(mei.sigma_s) / scatter_prob
                throughput *= weight  # All factors are detached

                mei = dr.detach(mei)

                if dr.hint(not is_primal, mode='scalar'):
                    # Record the completed segment for the second kernel
                    interval = dr.detach(dr.select(
                        seg_end, dr.select(escaped_medium, si.t, mei.t), 0.0))
                    # A missed intersection puts the segment end at infinity
                    seg_end &= dr.isfinite(interval)
                    interval = dr.select(seg_end, interval, 0.0)
                    suffix_depth = dr.select(escaped_medium, depth + 1, depth)

                    # Segment j goes to slot min(j, n_slots - 1). Only the last slot receives
                    # more than one segment; it keeps one with probability segment_weight / reservoir_weight_sum and
                    # weights it by reservoir_weight_sum / segment_weight.
                    tail = seg_ord >= (n_slots - 1)
                    segment_weight = dr.mean(dr.select(
                        seg_end, dr.detach(throughput_seg), mi.Spectrum(0.0)))
                    reservoir_weight_sum = dr.select(
                        seg_end & tail, reservoir_weight_sum + segment_weight,
                        reservoir_weight_sum)
                    dr.scatter(lane_reservoir_weight_sum, reservoir_weight_sum,
                               lane_idx, seg_end)
                    selection_probability = dr.select(
                        tail, dr.select(reservoir_weight_sum > 0,
                                        segment_weight / reservoir_weight_sum, 0.0), 1.0)
                    select_segment = seg_end & (alt_sampler.next_1d(seg_end) <= selection_probability)
                    slot = dr.select(tail, n_append_o, seg_ord * dfr_n) + lane_idx
                    rec = _SegmentRecord()
                    rec.origin, rec.direction = seg_origin, dr.detach(ray.d)
                    rec.interval, rec.segment_weight, rec.nee = interval, segment_weight, nee_dir_sample
                    rec.adj_trans = dr.detach(δL * L)                # Transmittance term
                    rec.adj_scatt = dr.detach(δL * throughput_seg)  # In-scattering term
                    rec.wavelengths = dr.detach(ray.wavelengths)
                    rec.depth, rec.channel, rec.medium = suffix_depth, channel, medium
                    dr.scatter(records, rec, slot, select_segment)
                    seg_ord = dr.select(seg_end, seg_ord + 1, seg_ord)

                phase_ctx = mi.PhaseFunctionContext(sampler)
                phase = mei.medium.phase_function()
                phase[~act_medium_scatter] = dr.zeros(mi.PhaseFunctionPtr)

                #--------------------- Surface Interactions --------------------

                active_surface |= escaped_medium
                intersect = active_surface & needs_intersection
                si[intersect] = scene.ray_intersect(
                    ray, mi.RayFlags.Default, False, intersect,
                    visibility_mask=ray_mask)

                # ----------------- Intersection with emitters -----------------

                ray_from_camera = active_surface & (depth == 0)
                count_direct = ray_from_camera | specular_chain
                # The same mask hides an invisible environment from escaped camera rays
                emitter = si.emitter(scene, visibility_mask=ray_mask)
                active_e = active_surface & (emitter != None)

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

                # A new segment starts at every scattering event and surface
                seg_origin[act_medium_scatter] = dr.detach(mei.p)
                seg_origin[active_surface] = dr.detach(si.p)

                active &= (active_surface | active_medium)

        if dr.hint(defer, mode='scalar'):
            self._flush_deferred_probes(scene, records, lane_reservoir_weight_sum, n_append,
                                        dr.gather(mi.UInt32, alt_seed, mi.UInt32(0)))

        return L if is_primal else δL, valid_ray, [], L

    @dr.syntax
    def sample_real_interaction(self, medium, ray, maxt, sampler, channel,
                                active):
        """
        Walk along `ray` to the next real scattering event. Null collisions are
        not returned here, only the real scattering event with its weight and
        acceptance probability.
        """
        ray = mi.Ray3f(ray)
        active = mi.Bool(active)
        first = mi.Bool(True)
        t_acc = mi.Float(0.0)
        maxt = mi.Float(maxt)
        mei = dr.zeros(mi.MediumInteraction3f)
        mei.medium = medium
        mei.wi = -ray.d
        mei.sh_frame = mi.Frame3f(mei.wi)
        mei.wavelengths = ray.wavelengths
        mei.time = ray.time
        mei.combined_extinction = medium.get_majorant(mei, active)
        mei.t = mi.Float(dr.inf)
        weight = mi.Spectrum(1.0)
        p = mi.Float(1.0)

        while dr.hint(active, label='Null-collision walk'):
            u1 = sampler.next_1d(active)
            mc = medium.sample_interaction(ray, u1, channel, active)
            escaped = active & (~mc.is_valid() | (mc.t > maxt))

            mei.mint[active & first] = mc.mint
            first[active] = False

            kappa = mc.combined_extinction
            # The reason for not using dr.mean(): it divides by the array size, which puts the
            # wavefront size into the kernel. This kernel's size changes from one iteration to
            # the next, so dr.mean() would recompile it every time.
            pc = dr.sum(mc.sigma_t / kappa) / dr.size_v(mi.Spectrum)
            u2 = sampler.next_1d(active)
            real = active & ~escaped & (u2 < pc)
            null = active & ~escaped & ~real

            mint = mi.Float(mei.mint)
            mei[real] = mc
            mei.t[real] = t_acc + mc.t
            mei.mint[real] = mint
            p[real] = pc
            weight[real] = weight * dr.rcp(kappa)

            weight[null] = weight * mc.sigma_n / (kappa * (1 - pc))
            ray.o[null] = mc.p
            t_acc[null] = t_acc + mc.t
            maxt[null] = maxt - mc.t

            active = null

        return mei, weight, p

    def _flush_deferred_probes(self, scene, records, lane_reservoir_weight_sum, n_append, seed):
        """
        Second kernel of the adjoint pass: compact the segment records and
        trace the gradient samples, one lane per record
        """
        dr.eval(records)
        idx = dr.compress(records.segment_weight > 0)
        n = int(dr.width(idx))
        if n == 0:
            return
        rec = dr.gather(_SegmentRecord, records, idx)

        # A reservoir row represents all the extra segments of its lane
        is_tail = idx >= n_append
        lane = dr.select(is_tail, idx - n_append, 0)
        inverse_selection_weight = dr.select(
            is_tail, dr.gather(mi.Float, lane_reservoir_weight_sum, lane, is_tail)
            / rec.segment_weight, 1.0)
        origin, seg_dir, interval = rec.origin, rec.direction, rec.interval
        adj_trans = rec.adj_trans * inverse_selection_weight
        adj_scatt = rec.adj_scatt * inverse_selection_weight
        nee_dir, suffix_depth, channel, medium = rec.nee, rec.depth, rec.channel, rec.medium

        smp = mi.load_dict({'type': 'independent'})
        smp.seed(seed, n)

        mei = dr.zeros(mi.MediumInteraction3f, n)
        mei.medium = medium
        mei.p = origin
        mei.wi = -seg_dir
        mei.sh_frame = mi.Frame3f(mei.wi)
        mei.wavelengths = rec.wavelengths

        self._sample_segment_probes(scene, medium, channel, smp, mei,
                                    origin, seg_dir, interval,
                                    adj_trans, adj_scatt, nee_dir,
                                    suffix_depth, mi.Bool(True))

    def _sample_segment_probes(self, scene, medium, channel, alt_sampler, mei,
                               seg_origin, seg_dir, interval, adj_trans,
                               adj_scatt, nee_dir_sample, suffix_depth,
                               active):
        """
        Place gradient samples on a segment and add their extinction gradient
        to the grid. Both terms use sigma_t at the same point: the
        transmittance term -sigma_t * adj_trans and the in-scattering term
        sigma_s * adj_scatt * Li, where Li is the in-scattered radiance.
        """
        n_probes = self.gradient_samples_per_segment
        within = active & (suffix_depth < self.max_depth)

        # Restrict the samples to the medium's bounding box. Volume lookups
        # clamp outside of it and would put gradients into boundary voxels.
        seg_ray = mi.Ray3f(mi.Point3f(seg_origin), mi.Vector3f(seg_dir))
        bb_hit, bb0, bb1 = medium.intersect_aabb(seg_ray)
        t0 = dr.detach(dr.clip(bb0, 0.0, interval))
        t1 = dr.detach(dr.clip(bb1, 0.0, interval))
        sub_len = dr.select(active & bb_hit, t1 - t0, 0.0)
        active = active & (sub_len > 0)
        within &= active

        mei_sub = mi.MediumInteraction3f(mei)

        phase_ctx = mi.PhaseFunctionContext(alt_sampler)
        phase = mei_sub.medium.phase_function()
        phase[~active] = dr.zeros(mi.PhaseFunctionPtr)

        contribs = mi.Spectrum(0.0)
        for i in range(n_probes):
            xi = alt_sampler.next_1d(active)
            mei_sub.t = dr.fma(xi, sub_len, t0)
            mei_sub.p = dr.fma(seg_dir, mei_sub.t, seg_origin)

            # The first sample also traces one suffix path for the segment's indirect illumination
            with dr.suspend_grad():
                if i == 0:
                    nee_Li, ind_Li = self._probe_radiance(
                        scene, medium, channel, alt_sampler, mei_sub,
                        phase_ctx, phase, nee_dir_sample, suffix_depth, within)
                    Li = nee_Li + n_probes * ind_Li
                else:
                    Li = self._probe_direct(
                        scene, medium, channel, alt_sampler, mei_sub,
                        phase_ctx, phase, nee_dir_sample, within)

            with dr.resume_grad():
                sigma_s_sub, _, sigma_t_sub = \
                    medium.get_scattering_coefficients(mei_sub, active)
                contribs -= sigma_t_sub * adj_trans
                contribs += sigma_s_sub * adj_scatt * Li

        inv_pdf = sub_len / n_probes
        with dr.resume_grad():
            if dr.hint(dr.grad_enabled(contribs), mode='scalar'):
                # Keep the AD edges: this graph is shared with later renders
                dr.backward(contribs * inv_pdf, flags=dr.ADFlag.ClearVertices)

    def _probe_radiance(self, scene, medium, channel, alt_sampler, mei_sub,
                        phase_ctx, phase, nee_dir_sample, suffix_depth, active):
        """In-scattered radiance at a gradient sample: (direct, indirect)"""
        nee_Li = self._probe_direct(scene, medium, channel, alt_sampler, mei_sub,
                                    phase_ctx, phase, nee_dir_sample, active)
        ind_Li = self._probe_indirect(scene, channel, alt_sampler, mei_sub,
                                      phase_ctx, phase, suffix_depth, active)
        return dr.select(active, nee_Li, 0.0), ind_Li

    def _probe_indirect(self, scene, channel, alt_sampler, mei_sub,
                        phase_ctx, phase, suffix_depth, active):
        """Indirect illumination from one suffix path traced in primal mode"""
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
        """Direct illumination with the segment's emitter direction sample"""
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

        # The gradient samples pass in the direction sample of their segment
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
            si[needs_intersection] = scene.ray_intersect(ray, needs_intersection)
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


def _linear(props):
    if 'segment_slots' not in props:
        props['segment_slots'] = 1
    return PRBVolpathSMIntegrator(props)


mi.register_integrator("prbvolpath_sm",
                       lambda props: PRBVolpathSMIntegrator(props))
mi.register_integrator("prbvolpath_sm_linear", _linear)
mi.register_integrator("prbvolpath_sm_quad",
                       lambda props: PRBVolpathSMIntegrator(props))

del RBIntegrator