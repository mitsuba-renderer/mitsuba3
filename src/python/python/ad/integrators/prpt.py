import enoki as ek
import mitsuba
from mitsuba.python.ad.integrators.integrator import prepare_sampler
from mitsuba.render import BSDFContext, has_flag, EmitterFlags, SurfaceInteraction3f, TransportMode, ImageBlock
from mitsuba.core import Loop, RayDifferential3f, UInt32, Spectrum, Frame3f, Float
from typing import Union


class PRPTIntegrator(mitsuba.render.ScatteringIntegrator):
    """
    This integrator implements a path replay particle tracer.
    """

    def __init__(self, props=mitsuba.core.Properties()):
        super().__init__(props)
        self.max_depth = props.get('max_depth', 3)
        self.rr_depth = props.get('rr_depth', 5)
        self.samples_per_pass = props.get('samples_per_pass', -1)
        self.hide_emitters = props.get('hide_emitters', False)
        self.spp = 0  # only used in backward rendering

    def render_forward(self: mitsuba.render.ScatteringIntegrator,
                       scene: mitsuba.render.Scene,
                       params: mitsuba.python.util.SceneParameters,
                       seed: int,
                       sensor: Union[int, mitsuba.render.Sensor] = 0,
                       spp: int = 0) -> None:
        from mitsuba.render import ImageBlock

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]
        assert False, "Not implemented"

    def render_backward(self: mitsuba.render.ScatteringIntegrator,
                        scene: mitsuba.render.Scene,
                        params: mitsuba.python.util.SceneParameters,
                        image_adj: mitsuba.core.TensorXf,
                        seed: int,
                        sensor: Union[int, mitsuba.render.Sensor] = 0,
                        spp: int = 0) -> None:
        from mitsuba.render import ImageBlock

        if isinstance(sensor, int):
            sensor = scene.sensors()[sensor]
        film = sensor.film()
        rfilter = film.reconstruction_filter()
        sampler = sensor.sampler()

        # Seed the sampler and compute the number of sample per pixels
        self.spp = prepare_sampler(sensor, seed, spp)
        # Construct an image block to receive radiance
        block = ImageBlock(film.crop_size(), channel_count=5,
                           filter=rfilter, border=False, normalize=True)
        block.set_offset(film.crop_offset())
        block.clear()
        # Prepare adjoint image for random query
        block_adj = ImageBlock(image_adj, rfilter, warn_negative=False, warn_invalid=False, normalize=False)  # why normalize=False?
        block_adj.set_offset(film.crop_offset())

        # FIXME: emitter parameter cannot be optimized
        # first pass: primal
        derivative = self.trace_ray(scene, sensor, sampler.clone(), block, block_adj)
        # second pass: adjoint
        for k, v in params.items():
            ek.enable_grad(v)
        self.trace_ray(scene, sensor, sampler, block, block_adj, derivative, False)

    def sample(self, scene, sensor, sampler, block, mask) -> None:

        if not self.hide_emitters:
            self.direct_visible_emitter(scene, sampler, sensor, block)
        self.trace_ray(scene, sensor, sampler, block)  # is_forward == True

    def direct_visible_emitter(self: mitsuba.render.ScatteringIntegrator,
                            scene: mitsuba.render.Scene,
                            sampler: mitsuba.render.Sampler,
                            sensor: mitsuba.render.Sensor,
                            block: mitsuba.render.ImageBlock):

        time = sensor.shutter_open()
        if (sensor.shutter_open_time() > 0):
            time += sampler.next_1d() * sensor.shutter_open_time()

        # Sample emitter
        if self.max_depth < 1:
            return
        emitter_idx, emitter_weight, _ = scene.sample_emitter(sampler.next_1d())
        emitters = ek.gather(mitsuba.render.EmitterPtr, scene.emitters_ek(), emitter_idx)
        active_d = ~has_flag(emitters.flags(), EmitterFlags.Delta)  # do not consider delta emitters

        # Sample position (do not consider infinite emitter)
        assert(~ek.any(has_flag(emitters.flags(), EmitterFlags.Infinite)))
        pos, pos_weight = emitters.sample_position(time, sampler.next_2d(active_d), active_d)

        si = ek.zero(SurfaceInteraction3f)
        si[active_d] = SurfaceInteraction3f(pos, ek.zero(mitsuba.core.Color0f))  # only has pos

        # From here basically doing "connect_sensor"
        # Connect to sensor
        sensor_ds, sensor_weight = sensor.sample_direction(si, sampler.next_2d(active_d), active_d)
        # sensor_weight = ek.select(ek.dot(sensor_ds.d, sensor_ds.n) < 0, sensor_weight, 0.0)
                # light coming from back of sensor? considered in sensor.sample_direction()
        active_d &= sensor_ds.pdf > 0.0
        si.wi = sensor_ds.d  # points from "si" to sensor ## ????

        # Shadow ray
        shadow_ray = si.spawn_ray_to(sensor_ds.p)
        active_d &= ~scene.ray_test(shadow_ray, active_d)
        if (ek.none_or(False, active_d)):
            return

        # Sample spectrum (auto done in emitter.sample_ray)
        wavelength, wav_weight = emitters.sample_wavelengths(si, sampler.next_1d(active_d), active_d)
        si.wavelengths = wavelength
        si.shape = emitters.shape()

        # foreshorten_term
        local_d = si.to_local(sensor_ds.d)
        foreshorten_term = ek.max(0.0, Frame3f.cos_theta(local_d))  # normal connect_sensor does not need this

        # Result
        res = emitter_weight * pos_weight * wav_weight * sensor_weight * foreshorten_term
        pixel_pos = sensor_ds.uv + block.offset()
        block.put(pixel_pos, wavelength, res, Float(1.0), active_d)

    def connect_sensor(self, scene, sensor, sampler, block,
                       si, throughput, Li_initial, active, ray, is_forward, is_primal=True, block_adj=None):

        active_ = active & True  # force copy
        sensor_ds, sensor_weight = sensor.sample_direction(si, sampler.next_2d(), active_)
        active_ &= sensor_ds.pdf > 0.0
        # shadow ray
        shadow_ray = si.spawn_ray_to(sensor_ds.p)
        active_ &= ~scene.ray_test(shadow_ray, active_)
        # bsdf val
        ctx = BSDFContext(TransportMode.Importance)
        bsdf = si.bsdf(RayDifferential3f(ray))

        local_d = si.to_local(sensor_ds.d)
        valid = ek.neq(bsdf, None) & active_

        ## correction
        # Using geometric normals
        wi_dot_geo_n = ek.dot(si.n, si.to_world(si.wi))
        wo_dot_geo_n = ek.dot(si.n, shadow_ray.d)
        # Prevent light leaks due to shading normals
        active_ &= (wi_dot_geo_n * Frame3f.cos_theta(si.wi) > 0.) & (wo_dot_geo_n * Frame3f.cos_theta(local_d) > 0.)
        # Adjoint BSDF for shading normals -- [Veach, p. 155]
        correction = ek.abs((Frame3f.cos_theta(si.wi) * wo_dot_geo_n) / (Frame3f.cos_theta(local_d) * wi_dot_geo_n))

        bsdf_val = ek.select(active_, bsdf.eval(ctx, si, local_d, active_), 0.0)
        Li = Li_initial * throughput * sensor_weight * bsdf_val
        Li = ek.select(active_, Li, 0.0)

        pixel_pos = sensor_ds.uv + block.offset()
        block.put(pixel_pos, ray.wavelengths, Li, Float(1.0), active_)

        # Accumulated derivative
        if not is_forward:
            t = throughput * bsdf_val * (block_adj.read(pixel_pos) * sensor_weight) / self.spp  # spp? since W channel divided
            derivative_change = ek.select(active_, t, 0.0)
        else:
            derivative_change = 0.0

        return derivative_change

    def trace_ray(self: mitsuba.render.ScatteringIntegrator,
               scene: mitsuba.render.Scene,
               sensor: mitsuba.render.Sensor,
               sampler: mitsuba.render.Sampler,
               block: mitsuba.render.ImageBlock,
               block_adj: mitsuba.render.ImageBlock=None,
               derivative: mitsuba.core.Spectrum=None,
               is_primal: bool=True):

        is_forward = ek.eq(block_adj, None)  # normal forward rendering
        derivative = ek.select(is_primal, Spectrum(0.0), derivative)

        # first intersection
        def sample_emitter_ray():
            emitter_idx, emitter_weight, sample_reuse = scene.sample_emitter(sampler.next_1d())
            emitter = ek.gather(mitsuba.render.EmitterPtr, scene.emitters_ek(), emitter_idx)
            ray, ray_weight = emitter.sample_ray(0., sample_reuse, sampler.next_2d(), sampler.next_2d())
            return ray, ray_weight*emitter_weight, emitter

        ray, Li_initial, _ = sample_emitter_ray()
        throughput = Spectrum(1.0)
        si = scene.ray_intersect(ray)
        active = si.is_valid()
        ray = RayDifferential3f(ray)

        # init
        ctx = BSDFContext(TransportMode.Importance)
        depth = UInt32(1)
        if (self.max_depth > 0):
            active &= depth < self.max_depth

        # loop
        loop = Loop("PRPTLoop" + "" if is_primal else " - adjoint")
        loop.put(lambda: (active, ray, throughput, si, depth, derivative))
        sampler.loop_register(loop)
        loop.init()
        while loop(active):
            # contribute of this interaction
            derivative_change = (  # in adjoint phase, subtract NEE contribution
                self.connect_sensor(scene, sensor, sampler, block,
                                    si, throughput, Li_initial, active, ray, is_forward, is_primal, block_adj))
            # sample
            bsdf = si.bsdf(ray)
            with ek.suspend_grad():
                bs, bsdf_val_pdf = bsdf.sample(ctx, si, sampler.next_1d(active), sampler.next_2d(active), active)
                active &= bs.pdf > 0.0

            ## Prevent light leak
            # Using geometric normals (wo points to the camera)
            wi_dot_geo_n = ek.dot(si.n, -ray.d)
            wo_dot_geo_n = ek.dot(si.n, si.to_world(bs.wo))
            # Prevent light leaks due to shading normals
            active &= (wi_dot_geo_n * Frame3f.cos_theta(si.wi)) > 0.0
            active &= (wo_dot_geo_n * Frame3f.cos_theta(bs.wo)) > 0.0
            ## adjoint BSDF correction
            correction = ek.abs((Frame3f.cos_theta(si.wi) * wo_dot_geo_n) / (Frame3f.cos_theta(bs.wo) * wi_dot_geo_n))  # may have nan for non-active rays

            # backpropagate
            derivative += ek.detach(derivative_change if is_primal else -derivative_change)
            if not is_forward and not is_primal:
                # print(ek.graphviz_str(ek.llvm.ad.Float))
                bsdf_eval = bsdf.eval(ctx, si, bs.wo, active) * correction
                ek.backward(
                    Li_initial * 
                    (derivative_change + bsdf_eval * ek.detach(ek.select(active, derivative / ek.max(1e-8, bsdf_eval), 0.0)))
                )

            # update throughput
            throughput *= bsdf_val_pdf * correction
            active &= ek.any(ek.neq(throughput, 0.0))  # ek.any is for spectrum

            # next intersection
            with ek.suspend_grad():
                ray = ek.detach(RayDifferential3f(si.spawn_ray(si.to_world(bs.wo))))
                si = scene.ray_intersect(ray, active)
                active &= si.is_valid()
            depth += UInt32(1)
            if (self.max_depth > 0):
                active &= depth < self.max_depth

            # russian roulette
            active_rr = (depth > self.rr_depth) & active
            if (ek.any_or(True, active_rr)):
                q = ek.min(ek.hmax(throughput), 0.95)
                active[active_rr] = sampler.next_1d(active_rr) < q
                throughput[active_rr] = throughput * ek.rcp(q)

        return derivative

    def to_string(self):
        return (
            f'PathReplayParticleTracer[\n'
            f'  max_depth = {self.max_depth}\n'
            f'  rr_depth = {self.rr_depth}\n'
            f']'
        )


mitsuba.render.register_integrator("prpt", lambda props: PRPTIntegrator(props))
