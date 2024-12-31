from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba as mi

from .common import RBIntegrator, mis_weight

def index_spectrum(spec, idx):
    m = spec[0]
    if mi.is_rgb:
        m[idx == 1] = spec[1]
        m[idx == 2] = spec[2]
    return m


class PRBVolpathIntegrator(RBIntegrator):
    r"""
    .. _integrator-prbvolpath:

    Path Replay Backpropagation Volumetric Integrator (:monosp:`prbvolpath`)
    -------------------------------------------------------------------------

    .. pluginparameters::

     * - max_depth
       - |int|
       - Specifies the longest path depth in the generated output image (where -1
         corresponds to :math:`\infty`). A value of 1 will only render directly
         visible light sources. 2 will lead to single-bounce (direct-only)
         illumination, and so on. (Default: 6)

     * - rr_depth
       - |int|
       - Specifies the path depth, at which the implementation will begin to use
         the *russian roulette* path termination criterion. For example, if set to
         1, then path generation many randomly cease after encountering directly
         visible surfaces. (Default: 5)

     * - hide_emitters
       - |bool|
       - Hide directly visible emitters. (Default: no, i.e. |false|)


    This class implements a volumetric Path Replay Backpropagation (PRB) integrator
    with the following properties:

    - Differentiable delta tracking for free-flight distance sampling

    - Emitter sampling (a.k.a. next event estimation).

    - Russian Roulette stopping criterion.

    - No projective sampling. This means that the integrator cannot be used for
      shape optimization (it will return incorrect/biased gradients for
      geometric parameters like vertex positions.)

    - Detached sampling. This means that the properties of ideal specular
      objects (e.g., the IOR of a glass vase) cannot be optimized.

    See the paper :cite:`Vicini2021` for details on PRB and differentiable delta
    tracking.

    .. warning::
        This integrator is not supported in variants which track polarization
        states.

    .. tabs::

        .. code-tab:: python

            'type': 'prbvolpath',
            'max_depth': 8
    """

    def __init__(self, props):
        super().__init__(props)
        self.max_depth = props.get('max_depth', -1)
        self.rr_depth = props.get('rr_depth', 5)
        self.hide_emitters = props.get('hide_emitters', False)

        self.use_nee = False
        self.use_nee_medium = False
        self.nee_handle_homogeneous = False
        self.handle_null_scattering = False
        self.is_prepared = False

    def prepare_scene(self, scene):
        if self.is_prepared:
            return

        for shape in scene.shapes():
            for medium in [shape.interior_medium(), shape.exterior_medium()]:
                if medium is not None:
                    # Enable NEE if a medium specifically asks for it
                    self.use_nee = self.use_nee or medium.use_emitter_sampling()
                    self.use_nee_medium = self.use_nee or medium.is_emitter()
                    self.nee_handle_homogeneous = self.nee_handle_homogeneous or medium.is_homogeneous()
                    self.handle_null_scattering = self.handle_null_scattering or not medium.is_homogeneous() or medium.is_emitter()
        self.is_prepared = True
        # By default, always enable NEE in case there are surfaces
        self.use_nee = True
        self.use_nee_medium = True

    @dr.syntax
    def sample(self,
               mode: dr.ADMode,
               scene: mi.Scene,
               sampler: mi.Sampler,
               ray: mi.Ray3f,
               δL: Optional[mi.Spectrum],
               state_in: Optional[mi.Spectrum],
               initial_medium: mi.MediumPtr,
               active: mi.Bool,
               **kwargs # Absorbs unused arguments
               ) -> Tuple[mi.Spectrum, mi.Bool, List[mi.Float], mi.Spectrum]:
        self.prepare_scene(scene)

        is_primal = mode == dr.ADMode.Primal

        # Copy input arguments to avoid mutating the caller's state
        ray = mi.Ray3f(dr.detach(ray))
        depth = mi.UInt32(0)                          # Depth of current vertex
        L = mi.Spectrum(0 if is_primal else state_in) # Radiance accumulator
        δL = mi.Spectrum(δL if δL is not None else 0) # Differential/adjoint radiance
        β = mi.Spectrum(1)                            # Path throughput weight
        η = mi.Float(1)                               # Index of refraction
        active = mi.Bool(active)                      # Active SIMD lanes

        si = dr.zeros(mi.SurfaceInteraction3f)
        needs_intersection = mi.Bool(True)
        last_scatter_event = dr.zeros(mi.Interaction3f)
        last_scatter_direction_pdf = mi.Float(1.0)

        if initial_medium is None:
            initial_medium_ptr = dr.zeros(mi.MediumPtr)
        elif isinstance(initial_medium, mi.MediumPtr):
            initial_medium_ptr = initial_medium
        else:
            initial_medium_ptr = mi.MediumPtr(initial_medium)
        medium = dr.select((initial_medium != None), initial_medium_ptr, dr.zeros(mi.MediumPtr))

        channel = 0
        valid_ray = mi.Bool(scene.environment() != dr.zeros(mi.EmitterPtr))
        specular_chain = mi.Bool(active)

        if mi.is_rgb:
            # Sample a color channel to sample free-flight distances
            n_channels = dr.size_v(mi.Spectrum)
            channel = mi.UInt32(dr.minimum(n_channels * sampler.next_1d(active), n_channels - 1))

        while dr.hint(active,
                      label=f"Path Replay Backpropagation ({mode.name})"):
            # --------------------- Perform russian roulette --------------------

            active &= dr.any(β != 0.0)
            q = dr.minimum(dr.max(β) * dr.square(η), 0.95)
            perform_rr = (depth > self.rr_depth)
            active &= (sampler.next_1d(active) < q) | ~perform_rr
            β[perform_rr] = β * dr.rcp(q)

            active_medium = active & (medium != None)
            active_surface = active & ~active_medium

            # --------------------- Sample medium interaction -------------------

            # Handle medium sampling and potential medium escape
            u = sampler.next_1d(active_medium)
            with dr.resume_grad(when=not is_primal):
                mei = medium.sample_interaction(ray, u, channel, active_medium)

                ray.maxt[active_medium & medium.is_homogeneous() & mei.is_valid()] = mei.t
                intersect = needs_intersection & active_medium
                si[intersect] = scene.ray_intersect(ray, intersect)

            needs_intersection &= ~active_medium
            mei.t[active_medium & (si.t < mei.t)] = dr.inf

            # Evaluate ratio of transmittance and free-flight PDF
            with dr.resume_grad(when=not is_primal):
                weight_medium = mi.Spectrum(1.0)
                tr, free_flight_pdf = medium.transmittance_eval_pdf(mei, si, active_medium)
                tr_pdf = index_spectrum(free_flight_pdf, channel)
                weight_medium[active_medium] *= dr.select(tr_pdf > 0.0, tr * dr.detach(dr.rcp(tr_pdf)), 1.0)

                escaped_medium = active_medium & ~mei.is_valid()
                active_medium &= mei.is_valid()

            # ---------------- Intersection with medium emitter ----------------
            count_direct_medium = (active_medium & ((depth == 0) | specular_chain))
            emitter_medium = medium.emitter()
            active_e_medium_sampl = active_medium & (emitter_medium != None) & \
                                    ~((depth == 0) & self.hide_emitters)
            ds = mi.DirectionSample3f(mei, last_scatter_event)
            if dr.hint(self.use_nee and self.use_nee_medium, mode='scalar'):
                emitter_pdf = scene.pdf_emitter_direction(last_scatter_event, ds, active_e_medium_sampl)
            else:
                emitter_pdf = 0.0

            em_mis = dr.select(count_direct_medium, 1.0, mis_weight(last_scatter_direction_pdf, emitter_pdf))

            with dr.resume_grad(when=not is_primal):
                Le_medium = dr.detach(β * em_mis) * weight_medium * dr.select(active_e_medium_sampl, mei.radiance, 0.0)

            L[active_e_medium_sampl] += dr.detach(Le_medium if is_primal else -Le_medium)

            # Handle null and real scatter events
            if dr.hint(self.handle_null_scattering, mode='scalar'):
                (prob_scatter, prob_null), (weight_scatter, weight_null) = medium.get_interaction_probabilities(
                    dr.detach(mei.radiance), mei, dr.detach(β * weight_medium)
                )
                act_null_scatter = (sampler.next_1d(active_medium) >= index_spectrum(prob_scatter, channel)) & active_medium
                act_medium_scatter = ~act_null_scatter & active_medium
                with dr.resume_grad(when=not is_primal):
                    weight_medium[act_null_scatter] *= mei.sigma_n * dr.detach(index_spectrum(weight_null, channel))
                    ray.o[act_null_scatter] = dr.detach(mei.p)
                    si.t[act_null_scatter] = si.t - dr.detach(mei.t)
            else:
                weight_scatter = mi.UnpolarizedSpectrum(1.0)
                act_medium_scatter = active_medium

            depth[act_medium_scatter] += 1
            last_scatter_event[act_medium_scatter] = dr.detach(mei, True)

            # Don't estimate lighting if we exceeded number of bounces
            active &= depth < self.max_depth
            act_medium_scatter &= active

            with dr.resume_grad(when=not is_primal):
                weight_medium[act_medium_scatter] *= mei.sigma_s * dr.detach(index_spectrum(weight_scatter, channel))

                if dr.hint(not is_primal, mode='scalar'):
                    inv_weight_det = dr.select((weight_medium != 0.0), dr.detach(dr.rcp(weight_medium)), 0.0)
                    adj_L = (weight_medium * dr.detach(dr.select(active_medium | escaped_medium, L * inv_weight_det, 0.0)) + Le_medium)

                    if dr.hint(dr.grad_enabled(adj_L), mode='scalar'):
                        if dr.hint(mode == dr.ADMode.Backward, mode='scalar'):
                            dr.backward_from(δL * adj_L)
                        else:
                            δL += dr.forward_to(adj_L)

            mei = dr.detach(mei)
            β *= dr.detach(weight_medium)
            del weight_medium

            phase_ctx = mi.PhaseFunctionContext(sampler)
            phase = mei.medium.phase_function()
            phase[~act_medium_scatter] = dr.zeros(mi.PhaseFunctionPtr)

            # --------------------- Surface Interactions --------------------

            active_surface |= escaped_medium
            active_surface &= active
            intersect = active_surface & needs_intersection
            with dr.resume_grad(when=not is_primal):
                si[intersect] = scene.ray_intersect(ray, intersect)

            # ----------------- Intersection with emitters -----------------

            ray_from_camera = active_surface & (depth == 0)
            count_direct_surface = ray_from_camera | specular_chain
            emitter_surface = si.emitter(scene)
            active_e_surface_sampl = active_surface & (emitter_surface != None) & \
                                     ~((depth == 0) & self.hide_emitters)

            # Get the PDF of sampling this emitter using next event estimation
            ds = mi.DirectionSample3f(scene, si, last_scatter_event)
            if dr.hint(self.use_nee, mode='scalar'):
                emitter_pdf = scene.pdf_emitter_direction(last_scatter_event, ds, active_e_surface_sampl)
            else:
                emitter_pdf = 0.0

            em_mis = dr.select(count_direct_surface, 1.0, mis_weight(last_scatter_direction_pdf, emitter_pdf))

            with dr.resume_grad(when=not is_primal):
                Le_surface = β * em_mis * emitter_surface.eval(si, active_e_surface_sampl)

                dr.set_label(Le_surface, "Emitted Light")
                dr.set_label(em_mis, "MIS Weight")
                dr.set_label(active_e_surface_sampl, "Active (Mask)")
                dr.set_label(si, "Surface Interaction")

                if dr.hint(not is_primal and dr.grad_enabled(Le_surface), mode='scalar'):
                    if dr.hint(mode == dr.ADMode.Backward, mode='scalar'):
                        dr.backward_from(δL * Le_surface)
                    else:
                        δL += dr.forward_to(Le_surface)

            L[active_e_surface_sampl] += dr.detach(Le_surface)

            active_surface &= si.is_valid()
            bsdf_ctx = mi.BSDFContext()
            bsdf = si.bsdf(ray)

            # ---------------------- Emitter sampling ----------------------

            if dr.hint(self.use_nee, mode='scalar'):
                with dr.resume_grad(when=not is_primal):
                    active_e_surface = active_surface & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth) & (depth + 1 < self.max_depth)
                    sample_emitters = mei.medium.use_emitter_sampling()
                    specular_chain &= ~act_medium_scatter
                    specular_chain |= act_medium_scatter & ~sample_emitters

                    active_e_medium = act_medium_scatter & sample_emitters
                    active_e = active_e_surface | active_e_medium
                    nee_sampler = sampler if is_primal else sampler.clone()

                    emitted, ds = self.sample_emitter(mei, si, active_e_medium, active_e_surface,
                                                      scene, sampler, medium, channel, active_e, mode=dr.ADMode.Primal)

                    active_e &= (ds.pdf != 0.0)
                    active_e_surface &= active_e
                    active_e_medium &= active_e

                    wo = dr.select(active_e_surface, si.to_local(ds.d), ds.d)
                    # Query the BSDF for that emitter-sampled direction
                    bsdf_val, bsdf_pdf = bsdf.eval_pdf(bsdf_ctx, si, wo, active_e_surface)
                    # Query the Medium Phase Function for that emitter-sampled direction
                    phase_val, phase_pdf = phase.eval_pdf(phase_ctx, mei, wo, active_e_medium)

                    em_weight = dr.select(active_e_surface, bsdf_val, phase_val)
                    em_pdf = dr.select(ds.delta, 0.0, dr.select(active_e_surface, bsdf_pdf, phase_pdf))
                    em_throughput = β * em_weight * mis_weight(ds.pdf, em_pdf)
                    Lr_dir = dr.select(active_e, em_throughput * emitted, 0.0)

                    if dr.hint(dr.grad_enabled(Lr_dir), mode='scalar'):
                        if dr.hint(mode == dr.ADMode.Forward, mode='scalar'):
                            δL += dr.forward_to(Lr_dir, flags=dr.ADFlag.ClearEdges)
                            Lr_dir = dr.detach(Lr_dir)

                if dr.hint(not is_primal, mode='scalar'):
                    adj_throughput = dr.detach(em_throughput)
                    adj_emitted = dr.detach(Lr_dir)
                    self.sample_emitter(mei, si, active_e_medium, active_e_surface,
                                        scene, nee_sampler, medium, channel, active_e,
                                        adj_emitted=adj_emitted, adj_throughput=adj_throughput,
                                        δL=δL, mode=mode)

                L[active_e] += dr.detach(Lr_dir)

            # -------------------- Phase function sampling ------------------

            valid_ray |= act_medium_scatter
            phase_wo, phase_weight, phase_pdf = phase.sample(phase_ctx, mei,
                                                             sampler.next_1d(act_medium_scatter),
                                                             sampler.next_2d(act_medium_scatter),
                                                             act_medium_scatter)
            act_medium_scatter &= phase_pdf > 0.0

            β[act_medium_scatter] *= phase_weight
            ray[act_medium_scatter] = mei.spawn_ray(phase_wo)

            needs_intersection |= act_medium_scatter
            last_scatter_direction_pdf[act_medium_scatter] = dr.detach(phase_pdf)

            # ------------------------ BSDF sampling -----------------------

            bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                   sampler.next_1d(active_surface),
                                                   sampler.next_2d(active_surface),
                                                   active_surface)
            active_surface &= bsdf_sample.pdf > 0

            β[active_surface] *= bsdf_weight
            η[active_surface] *= bsdf_sample.eta
            ray[active_surface] = si.spawn_ray(si.to_world(bsdf_sample.wo))

            needs_intersection |= active_surface

            non_null_bsdf = active_surface & ~mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Null)
            depth[non_null_bsdf] += 1

            # update the last scatter PDF event if we encountered a non-null scatter event
            last_scatter_event[non_null_bsdf] = dr.detach(si, True)
            last_scatter_direction_pdf[non_null_bsdf] = dr.detach(bsdf_sample.pdf)

            # ------------------ Differential phase only ------------------

            if dr.hint(not is_primal, mode='scalar'):
                with dr.resume_grad():
                    # Re-evaluate BSDF * cos(theta) differentiably
                    bsdf_eval = bsdf.eval(bsdf_ctx, si, si.to_local(ray.d), active_surface)
                    bsdf_eval_detach = bsdf_weight * bsdf_sample.pdf
                    inv_bsdf_eval_detach = dr.select(bsdf_eval_detach != 0,
                                                     dr.rcp(bsdf_eval_detach), 0)

                    # Re-evaluate the phase function value in an attached manner
                    phase_eval, _ = phase.eval_pdf(phase_ctx, mei, ray.d, act_medium_scatter)
                    phase_eval_detach = phase_weight * phase_pdf
                    inv_phase_eval_detach = dr.select(phase_eval_detach != 0,
                                                      dr.rcp(phase_eval_detach), 0)


                    Lr_ind_surface     = dr.select(active_surface, L, 0.0) * dr.replace_grad(1.0, inv_bsdf_eval_detach * bsdf_eval)
                    Lr_ind_medium      = dr.select(act_medium_scatter, L, 0.0) * dr.replace_grad(1.0, inv_phase_eval_detach * phase_eval)
                    Lr_ind = Lr_ind_medium + Lr_ind_surface
                    Lo = Lr_dir + Lr_ind

                    dr.set_label(Lr_dir, "Direct Reflected Light")
                    dr.set_label(Lr_ind, "Indirect Reflected Light")
                    dr.set_label(bsdf_eval, "(Attached) BSDF Value")
                    dr.set_label(bsdf_eval_detach, "(Detached) BSDF Value")

                    # Propagate derivatives from/to 'Lo' based on 'mode'
                    if dr.hint(dr.grad_enabled(Lo), mode='scalar'):
                        if dr.hint(mode == dr.ADMode.Backward, mode='scalar'):
                            dr.backward_from(δL * Lo)
                        else:
                            δL += dr.forward_to(Lo)

            valid_ray |= non_null_bsdf
            specular_chain |= non_null_bsdf & mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)
            specular_chain &= ~(active_surface & mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Smooth))
            has_medium_trans = active_surface & si.is_medium_transition()
            medium[has_medium_trans] = si.target_medium(ray.d)
            active &= (active_surface | active_medium)

            ray = dr.detach(ray)
            si = dr.detach(si)

        return L if is_primal else δL, valid_ray, [], L

    @dr.syntax
    def sample_emitter(self, mei, si, active_medium, active_surface, scene, sampler, medium, channel,
                       active, adj_emitted=None, adj_throughput=None, δL=None, mode=None):
        is_primal = mode == dr.ADMode.Primal

        active = mi.Bool(active)

        ref_interaction = dr.zeros(mi.Interaction3f)
        ref_interaction[active_medium] = mei
        ref_interaction[active_surface] = si

        if dr.hint(self.use_nee_medium, mode='scalar'):
            em_sample = sampler.next_3d(active)
        else:
            em_sample = sampler.next_2d(active)
            em_sample = mi.Point3f(em_sample.x, em_sample.y, 0.0)

        ds, emitter_val = scene.sample_emitter_direction(ref_interaction,
                                                         em_sample,
                                                         False, active)
        if dr.hint(self.use_nee_medium, mode='scalar'):
            disable_medium_emitters = False
        else:
            disable_medium_emitters = active & mi.has_flag(ds.emitter.flags(), mi.EmitterFlags.Medium)
        ds = dr.detach(ds)
        invalid = (ds.pdf == 0.0) | disable_medium_emitters
        emitter_val[invalid] = 0.0
        active &= ~invalid

        is_medium_emitter = active & mi.has_flag(ds.emitter.flags(), mi.EmitterFlags.Medium)

        medium = dr.select(active, medium, dr.zeros(mi.MediumPtr))
        medium[(active_surface & si.is_medium_transition())] = si.target_medium(ds.d)
        medium = dr.detach(medium, True)

        ray = dr.detach(ref_interaction.spawn_ray_to(ds.p))

        max_dist = mi.Float(ray.maxt)
        total_dist = mi.Float(0.0)
        si = dr.zeros(mi.SurfaceInteraction3f)
        mei = dr.zeros(mi.MediumInteraction3f)
        medium_contribution = dr.zeros(mi.UnpolarizedSpectrum)
        needs_intersection = mi.Bool(True)
        transmittance = mi.Spectrum(1.0)

        while dr.hint(active, label=f"PRB Next Event Estimation ({mode.name})"):
            remaining_dist = max_dist - total_dist
            ray.maxt = dr.detach(remaining_dist)
            active &= remaining_dist > 0.0

            # This ray will not intersect if it reached the end of the segment
            needs_intersection &= active
            with dr.resume_grad(when=not is_primal):
                si[needs_intersection] = scene.ray_intersect(ray, needs_intersection)
            needs_intersection &= False

            active_medium = active & (medium != None)
            active_surface = active & ~active_medium

            # Handle medium interactions / transmittance
            with dr.resume_grad(when=not is_primal):
                mei[active_medium] = medium.sample_interaction(ray, sampler.next_1d(active_medium), channel, active_medium)
                mei.t[active_medium & (si.t < mei.t)] = dr.inf
                mei.t = dr.detach(mei.t)
            is_sampled_medium = active_medium & (mei.emitter(active_medium) == ds.emitter) & is_medium_emitter

            is_spectral = medium.has_spectral_extinction() & active_medium & (~medium.is_homogeneous() | is_sampled_medium)
            not_spectral = (~is_spectral) & active_medium

            with dr.resume_grad(when=not is_primal):
                t  = dr.minimum(remaining_dist, dr.minimum(mei.t, si.t)) - mei.mint
                tr  = dr.exp(-t * mei.combined_extinction)
                free_flight_pdf = dr.select((si.t < mei.t) | (mei.t > remaining_dist), tr, tr * mei.combined_extinction)
                tr_pdf = index_spectrum(free_flight_pdf, channel)
                free_flight_tr = dr.select(is_spectral, dr.select(tr_pdf > 0, tr / dr.detach(tr_pdf), 0), 1.0)
                tr_multiplier = dr.select(is_spectral, free_flight_tr, 1.0)

                # Special case for homogeneous media: directly advance to the next surface / end of the segment
                if dr.hint(self.nee_handle_homogeneous, mode='scalar'):
                    active_homogeneous = active_medium & medium.is_homogeneous() & ~is_sampled_medium
                    mei.t[active_homogeneous] = dr.minimum(remaining_dist, si.t)
                    tr_multiplier[active_homogeneous] *= medium.transmittance_eval_pdf(mei, si, active_homogeneous)[0]
                    mei.t[active_homogeneous] = dr.inf

            escaped_medium = active_medium & ~mei.is_valid()
            active_medium &= mei.is_valid()
            is_sampled_medium &= active_medium

            # Radiance contribution from medium
            with dr.resume_grad(when=not is_primal):
                medium_attenuation = dr.detach(transmittance * dr.select(ds.pdf > 0.0, dr.rcp(ds.pdf), 0.0))
                contrib_medium = tr_multiplier * medium_attenuation * dr.select(is_sampled_medium, mei.radiance, 0.0)
            medium_contribution[is_sampled_medium] += dr.detach(contrib_medium)

            # Ratio tracking transmittance computation
            ray[active_medium] = dr.detach(mei.spawn_ray_to(ds.p))
            si.t[active_medium] = dr.detach(si.t - mei.t)
            with dr.resume_grad(when=not is_primal):
                tr_multiplier[active_medium] *= mei.sigma_n * dr.select(not_spectral, dr.rcp(mei.combined_extinction), 1.0)

            # Handle interactions with surfaces
            active_surface |= escaped_medium
            active_surface &= si.is_valid() & ~active_medium

            bsdf = si.bsdf(ray)
            with dr.resume_grad(when=not is_primal):
                bsdf_val = bsdf.eval_null_transmission(si, active_surface)
                tr_multiplier[active_surface] *= bsdf_val

                if dr.hint(not is_primal, mode='scalar'):
                    active_adj = (active_surface | active_medium) & (tr_multiplier > 0.0)
                    inv_tr_det = dr.rcp(dr.maximum(tr_multiplier, 1e-8))
                    adj_Lo = tr_multiplier * dr.detach(dr.select(active_adj & ~is_sampled_medium, (adj_emitted - medium_contribution) * inv_tr_det, 0.0))
                    adj_Lo = adj_Lo + contrib_medium * dr.detach(adj_throughput)

                    if dr.hint(dr.grad_enabled(adj_Lo), mode='scalar'):
                        if dr.hint(mode == dr.ADMode.Backward, mode='scalar'):
                            dr.backward_from(δL * adj_Lo)
                        else:
                            δL += dr.forward_to(adj_Lo)

            transmittance *= dr.detach(tr_multiplier)
            si = dr.detach(si, True)
            mei = dr.detach(mei, True)
            ray = dr.detach(ray, True)
            medium_contribution = dr.detach(medium_contribution, True)

            # Update the ray with new origin & t parameter
            ray[active_surface] = dr.detach(si.spawn_ray_to(ds.p))
            ray.maxt = dr.detach(remaining_dist)
            needs_intersection |= active_surface

            # Continue tracing through scene if non-zero weights exist
            active &= (active_medium | active_surface) & dr.any(transmittance != 0.0)
            total_dist[active] += dr.detach(dr.select(active_medium, mei.t, si.t))

            # If a medium transition is taking place: Update the medium pointer
            has_medium_trans = active_surface & si.is_medium_transition()
            medium[has_medium_trans] = si.target_medium(ray.d)
            medium = dr.detach(medium, True)

        return dr.select(is_medium_emitter, medium_contribution, emitter_val * dr.detach(transmittance)), ds

    def to_string(self):
        return f'PRBVolpathIntegrator[max_depth = {self.max_depth}]'

mi.register_integrator("prbvolpath", lambda props: PRBVolpathIntegrator(props))

del RBIntegrator
