from __future__ import annotations # Delayed parsing of type annotations

import drjit as dr
import mitsuba

from .common import ADIntegrator, mis_weight

def index_spectrum(spec, idx):
    m = spec[0]
    if mitsuba.core.is_rgb:
        m[dr.eq(idx, 1)] = spec[1]
        m[dr.eq(idx, 2)] = spec[2]
    return m

class PRBVolpathIntegrator(ADIntegrator):
    """
    This class implements a volumetric Path Replay Backpropagation (PRB) integrator
    with the following properties:

    - Differentiable Delta tracking for free-flight distance sampling

    - Emitter sampling (a.k.a. next event estimation).

    - Russian Roulette stopping criterion.

    - No reparameterization. This means that the integrator cannot be used for
      shape optimization (it will return incorrect/biased gradients for
      geometric parameters like vertex positions.)

    - Detached sampling. This means that the properties of ideal specular
      objects (e.g., the IOR of a glass vase) cannot be optimized.

    See the paper

      "Path Replay Backpropagation: Differentiating Light Paths using
       Constant Memory and Linear Time" (Proceedings of SIGGRAPH'21)
       by Delio Vicini, Sébastien Speierer, and Wenzel Jakob

    for details on PRB and differentiable delta tracking.
    """
    def __init__(self, props=mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.get('max_depth', 4)
        self.rr_depth = props.get('rr_depth', 5)
        self.hide_emitters = props.get('hide_emitters', False)

        self.use_nee = False
        self.nee_handle_homogeneous = False
        self.handle_null_scattering = False
        self.is_prepared = False

    def prepare_scene(self, scene):
        if self.is_prepared:
            return

        for shape in scene.shapes():
            for medium in [shape.interior_medium(), shape.exterior_medium()]:
                if medium:
                    # Enable NEE if a medium specifically asks for it
                    self.use_nee = self.use_nee or medium.use_emitter_sampling()
                    self.nee_handle_homogeneous = self.nee_handle_homogeneous or medium.is_homogeneous()
                    self.handle_null_scattering = self.handle_null_scattering or (not medium.is_homogeneous())
        self.is_prepared = True
        # By default enable always NEE in case there are surfaces
        self.use_nee = True

    def sample(self,
               mode: drjit.ADMode,
               scene: mitsuba.render.Scene,
               sampler: mitsuba.render.Sampler,
               ray: mitsuba.core.Ray3f,
               δL: Optional[mitsuba.core.Spectrum],
               state_in: Optional[mitsuba.core.Spectrum],
               active: mitsuba.core.Bool,
               **kwargs # Absorbs unused arguments
    ) -> Tuple[mitsuba.core.Spectrum,
               mitsuba.core.Bool, mitsuba.core.Spectrum]:
        from mitsuba.core import Float, Loop, Bool, Ray3f, Spectrum, UInt32
        from mitsuba.render import (BSDFContext, BSDFFlags, DirectionSample3f,
                                    MediumPtr, PhaseFunctionContext,
                                    SurfaceInteraction3f, has_flag)
        self.prepare_scene(scene)

        is_primal = mode == dr.ADMode.Primal

        ray = Ray3f(ray)
        depth = UInt32(0)                          # Depth of current vertex
        L = Spectrum(0 if is_primal else state_in) # Radiance accumulator
        δL = Spectrum(δL if δL is not None else 0) # Differential/adjoint radiance
        throughput = Spectrum(1)                   # Path throughput weight
        η = Float(1)                               # Index of refraction
        active = Bool(active)

        si = dr.zero(SurfaceInteraction3f)
        needs_intersection = Bool(True)
        last_scatter_event = dr.zero(mitsuba.render.Interaction3f)
        last_scatter_direction_pdf = Float(1.0)

        # TODO: Support sensors inside media
        # medium = MediumPtr(medium)
        medium = dr.zero(MediumPtr)

        channel = 0
        depth = UInt32(0)
        valid_ray = Bool(False)
        specular_chain = Bool(True)

        if mitsuba.core.is_rgb: # Sample a color channel to sample free-flight distances
            n_channels = dr.array_size_v(Spectrum)
            channel = dr.min(n_channels * sampler.next_1d(active), n_channels - 1)

        loop = Loop(name=f"Path Replay Backpropagation ({mode.name})",
                    state=lambda: (sampler, active, depth, ray, medium, throughput, si,
                                   L, needs_intersection,
                                   last_scatter_event,
                                   last_scatter_direction_pdf, η,
                                   specular_chain, valid_ray))
        while loop(active):
            active &= dr.any(dr.neq(throughput, 0.0))
            q = dr.min(dr.hmax(throughput) * dr.sqr(η), 0.99)
            perform_rr = (depth > self.rr_depth)
            active &= (sampler.next_1d(active) < q) | ~perform_rr
            throughput[perform_rr] = throughput * dr.rcp(q)

            active_medium = active & dr.neq(medium, None) # TODO this is not necessary
            active_surface = active & ~active_medium

            # Handle medium sampling and potential medium escape
            u = sampler.next_1d(active_medium)
            mei = medium.sample_interaction(ray, u, channel, active_medium)
            mei.t = dr.detach(mei.t)

            ray.maxt[active_medium & medium.is_homogeneous() & mei.is_valid()] = mei.t
            intersect = needs_intersection & active_medium
            si_new = scene.ray_intersect(ray, intersect)
            si[intersect] = si_new

            needs_intersection &= ~active_medium
            mei.t[active_medium & (si.t < mei.t)] = dr.Infinity

            # Evaluate ratio of transmittance and free-flight PDF
            tr, free_flight_pdf = medium.eval_tr_and_pdf(mei, si, active_medium)
            tr_pdf = index_spectrum(free_flight_pdf, channel)
            weight = Spectrum(1.0)
            weight[active_medium] *= dr.select(tr_pdf > 0.0, tr / dr.detach(tr_pdf), 0.0)

            escaped_medium = active_medium & ~mei.is_valid()
            active_medium &= mei.is_valid()

            # Handle null and real scatter events
            if self.handle_null_scattering:
                scatter_prob = index_spectrum(mei.sigma_t, channel) / index_spectrum(mei.combined_extinction, channel)
                act_null_scatter = (sampler.next_1d(active_medium) >= scatter_prob) & active_medium
                act_medium_scatter = ~act_null_scatter & active_medium
                weight[act_null_scatter] *= mi.sigma_n / dr.detach(1 - scatter_prob)
            else:
                scatter_prob = Float(1.0)
                act_medium_scatter = active_medium

            depth[act_medium_scatter] += 1
            last_scatter_event[act_medium_scatter] = dr.detach(mi)

            # Dont estimate lighting if we exceeded number of bounces
            active &= depth < self.max_depth
            act_medium_scatter &= active
            if self.handle_null_scattering:
                ray.o[act_null_scatter] = dr.detach(mei.p)
                si.t[act_null_scatter] = si.t - dr.detach(mei.t)

            weight[act_medium_scatter] *= mi.sigma_s / dr.detach(scatter_prob)
            throughput[active_medium] *= dr.detach(weight)

            mei = dr.detach(mei)
            if not is_primal and dr.grad_enabled(weight):
                Lo = dr.detach(dr.select(active_medium | escaped_medium, L / dr.max(1e-8, weight), 0.0))
                dr.backward(δL * weight * Lo)

            phase_ctx = PhaseFunctionContext(sampler)
            phase = mei.medium.phase_function()
            phase[~act_medium_scatter] = dr.zero(mitsuba.render.PhaseFunctionPtr)

            valid_ray |= act_medium_scatter
            with dr.suspend_grad():
                wo, phase_pdf = phase.sample(phase_ctx, mei, sampler.next_1d(act_medium_scatter), sampler.next_2d(act_medium_scatter), act_medium_scatter)
            new_ray = mei.spawn_ray(wo)
            ray[act_medium_scatter] = new_ray
            needs_intersection |= act_medium_scatter
            last_scatter_direction_pdf[act_medium_scatter] = phase_pdf

            #--------------------- Surface Interactions ---------------------
            active_surface |= escaped_medium
            intersect = active_surface & needs_intersection
            si[intersect] = scene.ray_intersect(ray, intersect)

            # ---------------- Intersection with emitters ----------------
            ray_from_camera = active_surface & dr.eq(depth, 0)
            count_direct = ray_from_camera | specular_chain
            emitter = si.emitter(scene)
            active_e = active_surface & dr.neq(emitter, None) & ~(dr.eq(depth, 0) & self.hide_emitters)

            # Get the PDF of sampling this emitter using next event estimation
            ds = DirectionSample3f(scene, si, last_scatter_event)
            if self.use_nee:
                emitter_pdf = scene.pdf_emitter_direction(last_scatter_event, ds, active_e)
            else:
                emitter_pdf = 0.0
            emitted = emitter.eval(si, active_e)
            contrib = dr.select(count_direct, throughput * emitted,
                                throughput * mis_weight(last_scatter_direction_pdf, emitter_pdf) * emitted)
            L[active_e] += dr.detach(contrib if is_primal else -contrib)
            if not is_primal and dr.grad_enabled(contrib):
                dr.backward(δL * contrib)

            active_surface &= si.is_valid()
            ctx = BSDFContext()
            bsdf = si.bsdf(ray)

            # --------------------- Emitter sampling ---------------------
            if self.use_nee:
                active_e_surface = active_surface & has_flag(bsdf.flags(), BSDFFlags.Smooth) & (depth + 1 < self.max_depth)
                sample_emitters = mei.medium.use_emitter_sampling()
                specular_chain &= ~act_medium_scatter
                specular_chain |= act_medium_scatter & ~sample_emitters
                active_e_medium = act_medium_scatter & sample_emitters
                active_e = active_e_surface | active_e_medium
                ref_interaction = dr.zero(mitsuba.render.Interaction3f)
                ref_interaction[act_medium_scatter] = mei
                ref_interaction[active_surface] = si
                nee_sampler = sampler if is_primal else sampler.clone()
                emitted, ds = self.sample_emitter(ref_interaction, scene, sampler, medium, channel, active_e, mode=dr.ADMode.Primal)
                # Query the BSDF for that emitter-sampled direction
                bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, si.to_local(ds.d), active_e_surface)
                phase_val = phase.eval(phase_ctx, mei, ds.d, active_e_medium)
                nee_weight = dr.select(active_e_surface, bsdf_val, phase_val)
                nee_directional_pdf = dr.select(ds.delta, 0.0, dr.select(active_e_surface, bsdf_pdf, phase_val))

                contrib = throughput * nee_weight * mis_weight(ds.pdf, nee_directional_pdf) * emitted
                L[active_e] += dr.detach(contrib if is_primal else -contrib)

                if not is_primal:
                    self.sample_emitter(ref_interaction, scene, nee_sampler,
                        medium, channel, active_e, adj_emitted=contrib, δL=δL, mode=mode)
                    if dr.grad_enabled(nee_weight) or dr.grad_enabled(emitted):
                        dr.backward(δL * contrib)

            # ----------------------- BSDF sampling ----------------------
            with dr.suspend_grad():
                bs, bsdf_weight = bsdf.sample(ctx, si, sampler.next_1d(active_surface),
                                           sampler.next_2d(active_surface), active_surface)
                active_surface &= bs.pdf > 0

            bsdf_eval = bsdf.eval(ctx, si, bs.wo, active_surface)

            if not is_primal and dr.grad_enabled(bsdf_eval):
                Lo = bsdf_eval * dr.detach(dr.select(active, L / dr.max(1e-8, bsdf_eval), 0.0))
                if mode == dr.ADMode.Backward:
                    dr.backward_from(δL * Lo)
                else:
                    δL += dr.forward_to(Lo)

            throughput[active_surface] *= bsdf_weight
            η[active_surface] *= bs.eta
            bsdf_ray = si.spawn_ray(si.to_world(bs.wo))
            ray[active_surface] = bsdf_ray

            needs_intersection |= active_surface
            non_null_bsdf = active_surface & ~has_flag(bs.sampled_type, BSDFFlags.Null)
            depth[non_null_bsdf] += 1

            # update the last scatter PDF event if we encountered a non-null scatter event
            last_scatter_event[non_null_bsdf] = si
            last_scatter_direction_pdf[non_null_bsdf] = bs.pdf

            valid_ray |= non_null_bsdf
            specular_chain |= non_null_bsdf & has_flag(bs.sampled_type, BSDFFlags.Delta)
            specular_chain &= ~(active_surface & has_flag(bs.sampled_type, BSDFFlags.Smooth))
            has_medium_trans = active_surface & si.is_medium_transition()
            medium[has_medium_trans] = si.target_medium(ray.d)
            active &= (active_surface | active_medium)

        return L if is_primal else δL, valid_ray, L

    def sample_emitter(self, ref_interaction, scene, sampler, medium, channel,
                       active, adj_emitted=None, δL=None, mode=None):
        from mitsuba.core import Float, Loop, Bool, Spectrum, Vector3f
        from mitsuba.render import MediumPtr, SurfaceInteraction3f

        is_primal = mode == dr.ADMode.Primal

        active = Bool(active)
        medium = dr.select(active, medium, dr.zero(MediumPtr))

        ds, emitter_val = scene.sample_emitter_direction(ref_interaction, sampler.next_2d(active), False, active)
        ds = dr.detach(ds)
        invalid = dr.eq(ds.pdf, 0.0)
        emitter_val[invalid] = 0.0
        active &= ~invalid

        ray = ref_interaction.spawn_ray(ds.d)
        total_dist = Float(0.0)
        si = dr.zero(SurfaceInteraction3f)
        needs_intersection = Bool(True)
        transmittance = Spectrum(1.0)
        loop = Loop(name=f"PRB Next Event Estimation ({mode.name})",
                    state=lambda: (sampler, active, medium, ray, needs_intersection,
                                   si, total_dist, transmittance))
        while loop(active):
            remaining_dist = ds.dist * (1.0 - mitsuba.core.math.ShadowEpsilon) - total_dist
            ray.maxt = dr.detach(remaining_dist)
            active &= remaining_dist > 0.0

            # This ray will not intersect if it reached the end of the segment
            needs_intersection &= active
            si[needs_intersection] = scene.ray_intersect(ray, needs_intersection)
            needs_intersection &= False

            active_medium = active & dr.neq(medium, None)
            active_surface = active & ~active_medium

            # Handle medium interactions / transmittance
            mei = medium.sample_interaction(ray, sampler.next_1d(active_medium), channel, active_medium)
            mei.t[active_medium & (si.t < mei.t)] = dr.Infinity
            mei.t = dr.detach(mei.t)

            tr_multiplier = Spectrum(1.0)

            # Special case for homogeneous media: directly advance to the next surface / end of the segment
            if self.nee_handle_homogeneous:
                active_homogeneous = active_medium & medium.is_homogeneous()
                mei.t[active_homogeneous] = dr.min(remaining_dist, si.t)
                tr_multiplier[active_homogeneous] = medium.eval_tr_and_pdf(mei, si, active_homogeneous)[0]
                mei.t[active_homogeneous] = dr.Infinity

            escaped_medium = active_medium & ~mei.is_valid()

            # Ratio tracking transmittance computation
            active_medium &= mi.is_valid()
            ray.o[active_medium] = dr.detach(mi.p)
            si.t[active_medium] = dr.detach(si.t - mi.t)
            tr_multiplier[active_medium] *= mi.sigma_n / mi.combined_extinction

            # Handle interactions with surfaces
            active_surface |= escaped_medium
            active_surface &= si.is_valid() & ~active_medium
            bsdf = si.bsdf(ray)
            bsdf_val = bsdf.eval_null_transmission(si, active_surface)
            tr_multiplier[active_surface] = tr_multiplier * bsdf_val

            if not is_primal and dr.grad_enabled(tr_multiplier):
                active_adj = (active_surface | active_medium) & (tr_multiplier > 0.0)
                dr.backward(tr_multiplier * dr.detach(dr.select(active_adj, δL * adj_emitted / tr_multiplier, 0.0)))

            transmittance *= dr.detach(tr_multiplier)

            # Update the ray with new origin & t parameter
            new_ray = si.spawn_ray(Vector3f(ray.d))
            ray[active_surface] = dr.detach(new_ray)
            ray.maxt = dr.detach(remaining_dist)
            needs_intersection |= active_surface

            # Continue tracing through scene if non-zero weights exist
            active &= (active_medium | active_surface) & dr.any(dr.neq(transmittance, 0.0))
            total_dist[active] += dr.select(active_medium, mi.t, si.t)

            # If a medium transition is taking place: Update the medium pointer
            has_medium_trans = active_surface & si.is_medium_transition()
            medium[has_medium_trans] = si.target_medium(ray.d)

        return emitter_val * transmittance, ds

    def to_string(self):
        return f'PRBVolpathIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("prbvolpath", lambda props: PRBVolpathIntegrator(props))
