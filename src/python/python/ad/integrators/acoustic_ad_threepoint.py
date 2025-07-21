from __future__ import annotations # Delayed parsing of type annotations
from typing import Optional, Tuple, Callable, Any, Union

import drjit as dr
import mitsuba as mi
import gc

from .common import mis_weight
from .acoustic_ad import AcousticADIntegrator

class AcousticADThreePointIntegrator(AcousticADIntegrator):


    @dr.syntax
    def sample(self,
               scene: mi.Scene,
               sampler: mi.Sampler,
               sensor: mi.Sensor,
               ray: mi.Ray3f,
               block: mi.ImageBlock,
               position_sample: mi.Point2f, # in [0,1]^2
               active: mi.Bool,
               **_ # Absorbs unused arguments
    ) -> Tuple[mi.Spectrum, mi.Bool, mi.Spectrum]:
        mi.Log(mi.LogLevel.Debug, f"Running sample().")
        
        film = sensor.film()
        n_frequencies = mi.ScalarVector2f(film.crop_size()).x
        n_channels = film.base_channels_count()

        # Standard BSDF evaluation context for path tracing
        bsdf_ctx = mi.BSDFContext()

        # --------------------- Configure loop state ----------------------

        distance     = mi.Float(0.0)
        max_distance = self.max_time * self.speed_of_sound

        # Copy input arguments to avoid mutating the caller's state
        ray = mi.Ray3f(ray)
        depth = mi.UInt32(0)                               # Depth of current vertex
        β = mi.Spectrum(1)                                 # Path throughput weight
        η = mi.Float(1)                                    # Index of refraction
        active = mi.Bool(active)                           # Active SIMD lanes

        # Variables caching information from the previous bounce
        prev_si         = dr.zeros(mi.SurfaceInteraction3f)
        prev_bsdf_pdf   = mi.Float(0.) if self.skip_direct else mi.Float(1.)
        prev_bsdf_delta = mi.Bool(True)
        si = None

        for it in range(self.max_depth):
            active_next = mi.Bool(active)

            # The first path vertex requires some special handling (see below)
            first_vertex = (depth == 0)
            if it == 0:
                si = scene.ray_intersect(dr.detach(ray),
                                        ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                        coherent=mi.Bool(True))
                weight = dr.select(si.is_valid(), sensor.sample_direction(si, [0, 0], active=si.is_valid())[1], 1)
                β *= weight/dr.detach(weight) # could be set as well


            si.wi = dr.select(active_next, si.to_local(dr.normalize(ray.o - si.p)), si.wi)

            # Get the BSDF, potentially computes texture-space differentials
            bsdf = si.bsdf(ray)

            # ---------------------- Direct emission ----------------------

            # Hide the environment emitter if necessary
            if self.hide_emitters:
                active_next &= ~((depth == 0) & ~si.is_valid())

            # Compute MIS weight for emitter sample from previous bounce
            ds = mi.DirectionSample3f(scene, si=si, ref=prev_si)

            si_pdf = scene.pdf_emitter_direction(prev_si, ds, ~prev_bsdf_delta)
            dr.disable_grad(si_pdf)

            dist_squared = dr.squared_norm(si.p-ray.o)
            dp = dr.dot(si.to_world(si.wi), si.n)
            D = dr.select(active_next, dr.norm(dr.cross(si.dp_du, si.dp_dv)) * dp / dist_squared, 1.)

            mis = mis_weight(
                prev_bsdf_pdf*D,
                si_pdf*D
            )

            # The first samples are sampled differently
            mis = dr.select(first_vertex, 1, mis)

            β *= dr.replace_grad(1, D/dr.detach(D))
            
            # Intensity of current emitter weighted by importance (def. by prev bsdf hits)
            Le = β * dr.detach(mis) * si.emitter(scene).eval(si, active_next)

            τ = dr.norm(si.p - ray.o)
            
            active_next &= si.is_valid()

            T       = distance + τ


            Le_pos = mi.Point2f(position_sample.x * n_frequencies,
                                block.size().y * T / max_distance)
            block.put(pos=Le_pos,
                      values=film.prepare_sample(Le[0], si.wavelengths, n_channels),
                      active= (Le[0] > 0.)) #FIXME: (TJ) should be active_next&(Le[0] > 0.) ?
            
            # ---------------------- Emitter sampling ----------------------

            # Should we continue tracing to reach one more vertex?
            active_next &= (depth + 1 < self.max_depth)

            # Is emitter sampling even possible on the current vertex?
            active_em = active_next & mi.has_flag(bsdf.flags(), mi.BSDFFlags.Smooth)

            # If so, randomly sample an emitter without derivative tracking.
            ds_em, em_weight = scene.sample_emitter_direction(si, sampler.next_2d(), True, active_em)
            em_weight *= dr.replace_grad(1, ds_em.pdf/dr.detach(ds_em.pdf))

            active_em &= (ds_em.pdf != 0.0)
            
            # We need to recompute the sample with follow shape so it is a detached uv sample
            si_em = scene.ray_intersect(dr.detach(si.spawn_ray(ds_em.d)), 
                                        ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                        coherent=mi.Bool(False),
                                        active=active_em)

            # calculate the bsdf weight (for path througput) and pdf (for mis weighting)
            diff_em = si_em.p - si.p
            ds_em.d = dr.normalize(diff_em)
            wo = si.to_local(ds_em.d)
            bsdf_value_em, bsdf_pdf_em = bsdf.eval_pdf(bsdf_ctx, si, wo, active_em)
            
            # ds_em.pdf includes the inv geometry term, 
            # and bsdf_pdf_em does not contain the geometry term.
            # -> We need to multiply both with the geometry term:
            dp_em = dr.dot(ds_em.d, si_em.n)
            dist_squared_em = dr.squared_norm(diff_em)
            D_em = dr.select(active_em, dr.norm(dr.cross(si_em.dp_du, si_em.dp_dv)) * -dp_em / dist_squared_em , 0.) 
            mis_em = dr.select(ds_em.delta, 1, mis_weight(ds_em.pdf*D_em, bsdf_pdf_em*D_em))
            # Detached Sampling
            em_weight *= dr.replace_grad(1, D_em/dr.detach(D_em))

            Lr_dir = β * dr.detach(mis_em) * bsdf_value_em * em_weight


            # Store (emission sample) intensity to the image block
            τ_dir = dr.norm(si_em.p - si.p)
            T_dir       = distance + τ + τ_dir
            Lr_dir_pos = mi.Point2f(position_sample.x * n_frequencies,
                                    block.size().y * T_dir / max_distance)                       
            block.put(pos=Lr_dir_pos,
                      values=film.prepare_sample(Lr_dir[0], si.wavelengths, n_channels),
                      active=active_em)

            # ------------------ Detached BSDF sampling -------------------
            with dr.suspend_grad():
                bsdf_sample, bsdf_weight = bsdf.sample(bsdf_ctx, si,
                                                    sampler.next_1d(),
                                                    sampler.next_2d(),
                                                    active_next)

            # ---- Update loop variables based on current interaction -----
            ray = si.spawn_ray(si.to_world(bsdf_sample.wo))

            si_next = scene.ray_intersect(dr.detach(ray),
                                            ray_flags=mi.RayFlags.All | mi.RayFlags.FollowShape,
                                            coherent=mi.Bool(False))
            
            diff_next = si_next.p - si.p
            dir_next = dr.normalize(diff_next)
            wo = si.to_local(dir_next)
            bsdf_val    = bsdf.eval(bsdf_ctx, si, wo, active_next)
            bsdf_weight = dr.replace_grad(bsdf_weight, dr.select((bsdf_sample.pdf != 0), bsdf_val / dr.detach(bsdf_sample.pdf), 0))
            
            
            # ---- Update loop variables based on current interaction -----

            η *= bsdf_sample.eta
            β *= bsdf_weight

            distance = T
            # Information about the current vertex needed by the next iteration
            prev_si = si
            prev_bsdf_pdf = bsdf_sample.pdf
            prev_bsdf_delta = mi.has_flag(bsdf_sample.sampled_type, mi.BSDFFlags.Delta)
            si = si_next
            # -------------------- Stopping criterion ---------------------

            # Don't run another iteration if the throughput has reached zero
            β_max = dr.max(β)
            active_next &= (β_max != 0)
            active_next &= distance <= max_distance

            # Russian roulette stopping probability (must cancel out ior^2
            # to obtain unitless throughput, enforces a minimum probability)
            rr_prob = dr.minimum(β_max * η**2, .95)

            # Apply only further along the path since, this introduces variance
            rr_active = depth >= self.rr_depth
            β[rr_active] *= dr.rcp(rr_prob)
            rr_continue = sampler.next_1d() < rr_prob
            active_next &= ~rr_active | rr_continue

            depth[si.is_valid()] += 1
            active = active_next


        return (depth != 0)

mi.register_integrator("acoustic_ad_threepoint", lambda props: AcousticADThreePointIntegrator(props))
