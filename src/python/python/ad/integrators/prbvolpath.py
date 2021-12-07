import enoki as ek
import mitsuba
from .common import prepare_sampler, mis_weight, sample_sensor_rays

from typing import Union

def index_spectrum(spec, idx):
    m = spec[0]
    if mitsuba.core.is_rgb:
        m[ek.eq(idx, 1)] = spec[1]
        m[ek.eq(idx, 2)] = spec[2]
    return m

class PRBVolpathIntegrator(mitsuba.render.SamplingIntegrator):
    """
    This integrator implements a path replay backpropagation volumetric path tracer.
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

    def prepare(self, scene):
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

    def render(self: mitsuba.render.SamplingIntegrator,
               scene: mitsuba.render.Scene,
               seed: int,
               sensor: Union[int, mitsuba.render.Sensor] = 0,
               develop_film: bool = True,
               spp: int = 0) -> mitsuba.core.TensorXf:
        if not self.is_prepared:
            self.prepare(scene)
        return super().render(scene, seed, sensor, develop_film, spp)

    def render_forward(self: mitsuba.render.SamplingIntegrator,
                       scene: mitsuba.render.Scene,
                       params: mitsuba.python.util.SceneParameters,
                       seed: int,
                       sensor: Union[int, mitsuba.render.Sensor] = 0,
                       spp: int=0) -> mitsuba.core.TensorXf:
        if not self.is_prepared:
            self.prepare(scene)

        from mitsuba.render import ImageBlock

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]
        film = sensor.film()
        rfilter = film.reconstruction_filter()
        sampler = sensor.sampler()

        spp = prepare_sampler(sensor, seed, spp)

        ray, weight, pos, _, _ = sample_sensor_rays(sensor)

        # Sample forward paths (not differentiable)
        with ek.suspend_grad():
            primal_result = self.Li(None, scene, sampler.clone(), ray)[0]

        grad_img = self.Li(ek.ADMode.Forward, scene, sampler,
                           ray, params=params, grad=weight,
                           primal_result=primal_result)[0]

        block = ImageBlock(film.crop_size(), channel_count=5,
                           filter=rfilter, border=False)
        block.set_offset(film.crop_offset())
        block.clear()
        block.put(pos, ray.wavelengths, grad_img)
        film.prepare([])
        film.put(block)
        return film.develop()

    def render_backward(self: mitsuba.render.SamplingIntegrator,
                        scene: mitsuba.render.Scene,
                        params: mitsuba.python.util.SceneParameters,
                        image_adj: mitsuba.core.TensorXf,
                        seed: int,
                        sensor: Union[int, mitsuba.render.Sensor] = 0,
                        spp: int = 0) -> None:
        from mitsuba.core import Spectrum
        from mitsuba.render import ImageBlock

        if not self.is_prepared:
            self.prepare(scene)

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]
        film = sensor.film()
        rfilter = sensor.film().reconstruction_filter()
        sampler = sensor.sampler()

        spp = prepare_sampler(sensor, seed, spp)

        ray, weight, pos, _, _ = sample_sensor_rays(sensor)

        # sample forward path (not differentiable)
        with ek.suspend_grad():
            result, _ = self.Li(scene, sampler.clone(), ray, medium=sensor.medium())

        block = ImageBlock(ek.detach(image_adj), rfilter, normalize=True)
        block.set_offset(film.crop_offset())
        grad_values = Spectrum(block.read(pos)) * weight / spp

        # Replay light paths and accumulate gradients
        self.Li(scene, sampler, ray, 0, params, grad_values, sensor.medium(), True, result)

    def sample(self, scene, sampler, ray, medium, active):
        res, valid = self.Li(scene, sampler, ray, medium=medium, active_=active)
        return res, valid, []

    def Li(self, scene, sampler, ray, depth=0, params=mitsuba.python.util.SceneParameters(),
           grad=None, medium=None, active_=True, result=None):
        from mitsuba.core import (Float, Loop, Mask, Ray3f, RayDifferential3f,
                                  Spectrum, UInt32)
        from mitsuba.render import (BSDFContext, BSDFFlags, DirectionSample3f,
                                    MediumPtr, PhaseFunctionContext,
                                    SurfaceInteraction3f, has_flag)

        is_primal = len(params) == 0

        active = Mask(active_)
        ray = Ray3f(ray)
        wavefront_size = ek.width(ray.d)

        si = ek.zero(SurfaceInteraction3f, wavefront_size)
        needs_intersection = Mask(True)
        last_scatter_event = ek.zero(mitsuba.render.Interaction3f, wavefront_size)
        last_scatter_direction_pdf = ek.zero(Float, wavefront_size) + 1.0
        medium = MediumPtr(medium)
        channel = 0
        depth = UInt32(depth)
        valid_ray = Mask(False)
        specular_chain = Mask(True)
        eta = Float(1.0)

        if is_primal:
            result = Spectrum(0.0)
        throughput = Spectrum(1.0)

        if mitsuba.core.is_rgb:
            n_channels = ek.array_size_v(Spectrum)
            channel = ek.min(n_channels * sampler.next_1d(active), n_channels - 1)

        loop = Loop("PRBVolpathLoop" + ('' if is_primal else '_adjoint'))
        loop.put(lambda: (active, depth, ray, medium, throughput, si, result, needs_intersection,
                          last_scatter_event, last_scatter_direction_pdf, eta, specular_chain, valid_ray))
        sampler.loop_register(loop)
        loop.init()
        while loop(active):
            active &= ek.any(ek.neq(throughput, 0.0))
            q = ek.min(ek.hmax(throughput) * ek.sqr(eta), 0.99)
            perform_rr = (depth > self.rr_depth)
            active &= (sampler.next_1d(active) < q) | ~perform_rr
            throughput[perform_rr] = throughput * ek.rcp(q)

            active_medium = active & ek.neq(medium, None) # TODO this is not necessary
            active_surface = active & ~active_medium

            # Handle medium sampling and potential medium escape
            u = sampler.next_1d(active_medium)
            mi = medium.sample_interaction(ray, u, channel, active_medium)
            mi.t = ek.detach(mi.t)

            ray.maxt[active_medium & medium.is_homogeneous() & mi.is_valid()] = mi.t
            intersect = needs_intersection & active_medium
            si_new = scene.ray_intersect(ray, intersect)
            si[intersect] = si_new

            needs_intersection &= ~active_medium
            mi.t[active_medium & (si.t < mi.t)] = ek.Infinity

            weight = Spectrum(1.0)

            # Evaluate ratio of transmittance and free-flight PDF
            tr, free_flight_pdf = medium.eval_tr_and_pdf(mi, si, active_medium)
            tr_pdf = index_spectrum(free_flight_pdf, channel)
            weight[active_medium] = weight * ek.select(tr_pdf > 0.0, tr / ek.detach(tr_pdf), 0.0)

            escaped_medium = active_medium & ~mi.is_valid()
            active_medium &= mi.is_valid()

            # Handle null and real scatter events
            if self.handle_null_scattering:
                scatter_prob = index_spectrum(mi.sigma_t, channel) / index_spectrum(mi.combined_extinction, channel)
                act_null_scatter = (sampler.next_1d(active_medium) >= scatter_prob) & active_medium
                act_medium_scatter = ~act_null_scatter & active_medium
                weight[act_null_scatter] = weight * mi.sigma_n / ek.detach(1 - scatter_prob)
            else:
                act_medium_scatter = active_medium

            depth[act_medium_scatter] = depth + 1
            last_scatter_event[act_medium_scatter] = ek.detach(mi)

            # Dont estimate lighting if we exceeded number of bounces
            active &= depth < self.max_depth
            act_medium_scatter &= active
            if self.handle_null_scattering:
                ray.o[act_null_scatter] = ek.detach(mi.p)
                si.t[act_null_scatter] = si.t - ek.detach(mi.t)

            weight[act_medium_scatter] = weight * mi.sigma_s * ek.detach(index_spectrum(mi.combined_extinction, channel) / index_spectrum(mi.sigma_t, channel))
            throughput[active_medium] = throughput * ek.detach(weight)

            mi = ek.detach(mi)
            if not is_primal and ek.grad_enabled(weight):
                ek.backward(weight * ek.detach(ek.select(active_medium | escaped_medium, grad * result / ek.max(1e-8, weight), 0.0)))

            phase_ctx = PhaseFunctionContext(sampler)
            phase = mi.medium.phase_function()
            phase[~act_medium_scatter] = ek.zero(mitsuba.render.PhaseFunctionPtr, 1)

            valid_ray |= act_medium_scatter
            with ek.suspend_grad():
                wo, phase_pdf = phase.sample(phase_ctx, mi, sampler.next_1d(act_medium_scatter), sampler.next_2d(act_medium_scatter), act_medium_scatter)
            new_ray = mi.spawn_ray(wo)
            ray[act_medium_scatter] = new_ray
            needs_intersection |= act_medium_scatter
            last_scatter_direction_pdf[act_medium_scatter] = phase_pdf

            #--------------------- Surface Interactions ---------------------
            active_surface |= escaped_medium
            intersect = active_surface & needs_intersection
            si[intersect] = scene.ray_intersect(ray, intersect)

            # ---------------- Intersection with emitters ----------------
            ray_from_camera = active_surface & ek.eq(depth, 0)
            count_direct = ray_from_camera | specular_chain
            emitter = si.emitter(scene)
            active_e = active_surface & ek.neq(emitter, None) & ~(ek.eq(depth, 0) & self.hide_emitters)
            # Get the PDF of sampling this emitter using next event estimation
            ds = DirectionSample3f(scene, si, last_scatter_event)
            if self.use_nee:
                emitter_pdf = scene.pdf_emitter_direction(last_scatter_event, ds, active_e)
            else:
                emitter_pdf = 0.0
            emitted = emitter.eval(si, active_e)
            contrib = ek.select(count_direct, throughput * emitted,
                                throughput * mis_weight(last_scatter_direction_pdf, emitter_pdf) * emitted)
            result[active_e] = result + ek.detach(contrib if is_primal else -contrib)
            if not is_primal and ek.grad_enabled(contrib):
                ek.backward(grad * contrib)

            active_surface &= si.is_valid()
            ctx = BSDFContext()
            bsdf = si.bsdf(RayDifferential3f(ray))

            # --------------------- Emitter sampling ---------------------
            if self.use_nee:
                active_e_surface = active_surface & has_flag(bsdf.flags(), BSDFFlags.Smooth) & (depth + 1 < self.max_depth)
                sample_emitters = mi.medium.use_emitter_sampling()
                specular_chain &= ~act_medium_scatter
                specular_chain |= act_medium_scatter & ~sample_emitters
                active_e_medium = act_medium_scatter & sample_emitters
                active_e = active_e_surface | active_e_medium
                ref_interaction = ek.zero(mitsuba.render.Interaction3f, wavefront_size)
                ref_interaction[act_medium_scatter] = mi
                ref_interaction[active_surface] = si
                nee_sampler = sampler if is_primal else sampler.clone()
                emitted, ds = self.sample_emitter(ref_interaction, scene, sampler, medium, channel, active_e)
                # Query the BSDF for that emitter-sampled direction
                bsdf_val, bsdf_pdf = bsdf.eval_pdf(ctx, si, si.to_local(ds.d), active_e_surface)
                phase_val = phase.eval(phase_ctx, mi, ds.d, active_e_medium)
                nee_weight = ek.select(active_e_surface, bsdf_val, phase_val)
                nee_directional_pdf = ek.select(ds.delta, 0.0, ek.select(active_e_surface, bsdf_pdf, phase_val))

                contrib = throughput * nee_weight * mis_weight(ds.pdf, nee_directional_pdf) * emitted
                result[active_e] = result + ek.detach(contrib if is_primal else -contrib)

                if not is_primal:
                    self.sample_emitter(ref_interaction, scene, nee_sampler,
                        medium, channel, active_e, adj_emitted=contrib, grad=grad)
                    if ek.grad_enabled(nee_weight) or ek.grad_enabled(emitted):
                        ek.backward(grad * contrib)

            # ----------------------- BSDF sampling ----------------------
            with ek.suspend_grad():
                bs, bsdf_weight = bsdf.sample(ctx, si, sampler.next_1d(active_surface),
                                           sampler.next_2d(active_surface), active_surface)
                active_surface &= bs.pdf > 0

            bsdf_eval = bsdf.eval(ctx, si, bs.wo, active_surface)

            if not is_primal and ek.grad_enabled(bsdf_eval):
                ek.backward(bsdf_eval * ek.detach(ek.select(active, grad * result / ek.max(1e-8, bsdf_eval), 0.0)))

            throughput[active_surface] = throughput * bsdf_weight
            eta[active_surface] = eta * bs.eta
            bsdf_ray = si.spawn_ray(si.to_world(bs.wo))
            ray[active_surface] = bsdf_ray

            needs_intersection |= active_surface
            non_null_bsdf = active_surface & ~has_flag(bs.sampled_type, BSDFFlags.Null)
            depth[non_null_bsdf] = depth + 1

            # update the last scatter PDF event if we encountered a non-null scatter event
            last_scatter_event[non_null_bsdf] = si
            last_scatter_direction_pdf[non_null_bsdf] = bs.pdf

            valid_ray |= non_null_bsdf
            specular_chain |= non_null_bsdf & has_flag(bs.sampled_type, BSDFFlags.Delta)
            specular_chain &= ~(active_surface & has_flag(bs.sampled_type, BSDFFlags.Smooth))
            has_medium_trans = active_surface & si.is_medium_transition()
            medium[has_medium_trans] = si.target_medium(ray.d)
            active &= (active_surface | active_medium)

        return result, valid_ray


    def sample_emitter(self, ref_interaction, scene, sampler, medium, channel,
                       active, adj_emitted=None, grad=None):
        from mitsuba.core import (Float, Loop, Mask, RayDifferential3f,
                                  Spectrum, Vector3f)
        from mitsuba.render import MediumPtr, SurfaceInteraction3f

        is_primal = grad is None
        wavefront_size = ek.width(ref_interaction)

        active = Mask(active)
        medium = ek.select(active, medium, ek.zero(MediumPtr, wavefront_size))

        ds, emitter_val = scene.sample_emitter_direction(ref_interaction, sampler.next_2d(active), False, active)
        ds = ek.detach(ds)
        emitter_val[ek.eq(ds.pdf, 0.0)] = 0.0
        active &= ek.neq(ds.pdf, 0.0)

        ray = ref_interaction.spawn_ray(ds.d)

        total_dist = ek.zero(Float, wavefront_size)
        si = ek.zero(SurfaceInteraction3f, wavefront_size)
        needs_intersection = Mask(True)
        transmittance = Spectrum(1.0)

        loop = Loop("PRBVolpathNEELoop")
        loop.put(lambda: (active, medium, ray, needs_intersection, si, total_dist, transmittance))
        sampler.loop_register(loop)
        loop.init()
        while loop(active):
            remaining_dist = ds.dist * (1.0 - mitsuba.core.math.ShadowEpsilon) - total_dist
            ray.maxt = ek.detach(remaining_dist)
            active &= remaining_dist > 0.0

            # This ray will not intersect if it reached the end of the segment
            needs_intersection &= active
            si[needs_intersection] = scene.ray_intersect(ray, needs_intersection)
            needs_intersection &= False

            active_medium = active & ek.neq(medium, None)
            active_surface = active & ~active_medium

            # Handle medium interactions / transmittance
            mi = medium.sample_interaction(ray, sampler.next_1d(active_medium), channel, active_medium)
            mi.t[active_medium & (si.t < mi.t)] = ek.Infinity
            mi.t = ek.detach(mi.t)

            tr_multiplier = Spectrum(1.0)
            # Special case for homogeneous media: directly jump to the next surface / end of the segment
            if self.nee_handle_homogeneous:
                active_homogeneous = active_medium & medium.is_homogeneous()
                mi.t[active_homogeneous] = ek.min(remaining_dist, si.t)
                tr_multiplier[active_homogeneous] = medium.eval_tr_and_pdf(mi, si, active_homogeneous)[0]
                mi.t[active_homogeneous] = ek.Infinity

            escaped_medium = active_medium & ~mi.is_valid()

            # Ratio tracking transmittance computation
            active_medium &= mi.is_valid()
            ray.o[active_medium] = ek.detach(mi.p)
            si.t[active_medium] = ek.detach(si.t - mi.t)
            tr_multiplier[active_medium] = tr_multiplier * mi.sigma_n / mi.combined_extinction

            # Handle interactions with surfaces
            active_surface |= escaped_medium
            active_surface &= si.is_valid() & ~active_medium
            bsdf = si.bsdf(RayDifferential3f(ray))
            bsdf_val = bsdf.eval_null_transmission(si, active_surface)
            tr_multiplier[active_surface] = tr_multiplier * bsdf_val

            if not is_primal and ek.grad_enabled(tr_multiplier):
                active_adj = (active_surface | active_medium) & (tr_multiplier > 0.0)
                ek.backward(tr_multiplier * ek.detach(ek.select(active_adj, grad * adj_emitted / tr_multiplier, 0.0)))

            transmittance *= ek.detach(tr_multiplier)

            # Update the ray with new origin & t parameter
            new_ray = si.spawn_ray(Vector3f(ray.d))
            ray[active_surface] = ek.detach(new_ray)
            ray.maxt = ek.detach(remaining_dist)
            needs_intersection |= active_surface

            # Continue tracing through scene if non-zero weights exist
            active &= (active_medium | active_surface) & ek.any(ek.neq(transmittance, 0.0))
            total_dist[active] = total_dist + ek.select(active_medium, mi.t, si.t)

            # If a medium transition is taking place: Update the medium pointer
            has_medium_trans = active_surface & si.is_medium_transition()
            medium[has_medium_trans] = si.target_medium(ray.d)

        return emitter_val * transmittance, ds

    def to_string(self):
        return f'PRBVolpathIntegrator[max_depth = {self.max_depth}]'


mitsuba.render.register_integrator("prbvolpath", lambda props: PRBVolpathIntegrator(props))
