from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

import mitsuba as mi
import drjit as dr
import gc


class ProjectiveDetail():
    """
    Class holding implementation details of various operations needed by
    projective-sampling/path-space style integrators.
    """
    def __init__(self, parent):
        self.parent = parent  # To access the integrator

        # Internal storage for primarily visibel discontinuous derivatives
        self.primary_indices = []
        self.primary_distributions = []
        self.primary_shape_distribution = None

        # Guiding data structre
        self.guiding_distr = None

    #####################################################################
    ##          Primarily visible discontinuous derivatives            ##
    #####################################################################

    def init_primarily_visible_silhouette(self,
                                          scene: mi.Scene,
                                          sensor: mi.Sensor):
        """
        Precompute the silhouette of the scene as seen from the sensor and store
        the result in this python class.
        """
        self.primary_indices = []
        self.primary_distributions = []

        silhouette_shapes = scene.silhouette_shapes()
        viewpoint = mi.ScalarPoint3f(dr.slice(sensor.world_transform() @ mi.Point3f(0)))
        shapes_weight = []

        for i in range(len(silhouette_shapes)):
            indices, weights = silhouette_shapes[i].precompute_silhouette(viewpoint)

            self.primary_indices.append(indices)
            shapes_weight.append(silhouette_shapes[i].silhouette_sampling_weight())
            self.primary_distributions.append(mi.DiscreteDistribution(weights))

        shapes_weight = mi.Float(shapes_weight)
        self.primary_shape_distribution = mi.DiscreteDistribution(shapes_weight)

    def sample_primarily_visible_silhouette(self,
                                            scene: mi.Scene,
                                            viewpoint: mi.Point3f,
                                            sample2: mi.Point2f,
                                            active: mi.Mask) -> mi.SilhouetteSample3f:
        """
        Sample a primarily visible silhouette point as seen from the sensor.
        Returns a silhouette sample struct.
        """
        silhouette_shapes = scene.silhouette_shapes()

        shape_idx, sample2.x, shape_pmf = \
            self.primary_shape_distribution.sample_reuse_pmf(sample2.x, active)

        def sample_precomputed_silhouette(i: int, viewpoint: mi.Point3f,
                sample2: mi.Point2f, shape_pmf: mi.Float, active: mi.Mask):
            shape_distr = self.primary_distributions[i]
            shape_indices = self.primary_indices[i]

            distr_idx, silhouette_idx_pmf = shape_distr.sample_pmf(sample2.x, active)
            silhouette_idx = dr.gather(mi.UInt32, shape_indices, distr_idx, active)

            valid = ((silhouette_idx_pmf != 0) &
                     (shape_pmf != 0) &
                     active)

            ss = silhouette_shapes[i].sample_precomputed_silhouette(
                viewpoint, silhouette_idx, sample2.y, valid)
            ss.pdf *= silhouette_idx_pmf
            ss.pdf *= shape_pmf

            return ss

        def shape_sampling_wrapper(shape_index: int):
            def wrapped(*args, **kwargs):
                return sample_precomputed_silhouette(shape_index, *args, **kwargs)
            return wrapped

        funcs = []
        for i in range(len(silhouette_shapes)):
            funcs.append(shape_sampling_wrapper(i))

        return dr.switch(
            shape_idx,
            funcs,
            viewpoint, sample2, shape_pmf, active
        )

    def perspective_sensor_jacobian(self,
                                    sensor: mi.Sensor,
                                    ss: mi.SilhouetteSample3f):
        """
        The silhouette sample `ss` stores (1) the sampling density in the scene
        space, and (2) the motion of the silhouette point in the scene space.
        This Jacobian corrects both quantities to the camera sample space.
        """
        if not sensor.__repr__().startswith('PerspectiveCamera'):
            raise Exception("Only perspective cameras are supported")

        to_world = sensor.world_transform()
        near_clip = sensor.near_clip()
        sensor_center = to_world @ mi.Point3f(0)
        sensor_lookat_dir = to_world @ mi.Vector3f(0, 0, 1)
        x_fov = mi.traverse(sensor)["x_fov"][0]
        film = sensor.film()

        camera_to_sample = mi.perspective_projection(
            film.size(),
            film.crop_size(),
            film.crop_offset(),
            x_fov,
            near_clip,
            sensor.far_clip()
        )

        sample_to_camera = camera_to_sample.inverse()
        p_min = sample_to_camera @ mi.Point3f(0, 0, 0)
        multiplier = dr.square(near_clip) / dr.abs(p_min[0] * p_min[1] * 4.0)

        # Frame
        frame_t = dr.normalize(sensor_center - ss.p)
        frame_n = ss.n
        frame_s = dr.cross(frame_t, frame_n)

        J_num = dr.norm(dr.cross(frame_n, sensor_lookat_dir)) * \
                dr.norm(dr.cross(frame_s, sensor_lookat_dir)) * \
                dr.abs(dr.dot(frame_s, ss.silhouette_d))
        J_den = dr.square(dr.square(dr.dot(frame_t, sensor_lookat_dir))) * \
                dr.squared_norm(ss.p - sensor_center)

        return J_num / J_den * multiplier

    def eval_primary_silhouette_radiance_difference(self,
                                                    scene,
                                                    sampler,
                                                    ss,
                                                    sensor,
                                                    active=True) -> mi.Float:
        """
        Compute the difference in radiance between two rays that hit and miss a
        silhouette point ``ss.p`` viewed from ``viewpoint``.
        """
        if not sensor.__repr__().startswith('PerspectiveCamera'):
            raise Exception("Only perspective cameras are supported")

        with dr.suspend_grad():
            to_world = sensor.world_transform()
            sensor_center = to_world @ mi.Point3f(0)

            # Is the boundary point visible or is occluded ?
            ss_invert = mi.SilhouetteSample3f(ss)
            ss_invert.d = -ss_invert.d
            ray_test = ss_invert.spawn_ray()

            dist = dr.norm(sensor_center - ray_test.o)
            ray_test.maxt = dist * (1 - mi.math.ShadowEpsilon)
            visible = ~scene.ray_test(ray_test, active) & active

            # Is the boundary point within the view frustum ?
            it = dr.zeros(mi.Interaction3f)
            it.p = ss.p
            ds, _ = sensor.sample_direction(it, mi.Point2f(0), active)
            visible &= ds.pdf != 0

            # Estimate the radiance difference along that path
            radiance_diff, _ = self.parent.sample_radiance_difference(
                scene, ss, 0, sampler, visible)

        return radiance_diff

    #####################################################################
    ##               Indirect discontinuous derivatives                ##
    #####################################################################

    def get_projected_points(self,
                             scene: mi.Scene,
                             sensor: mi.Sensor,
                             sampler: mi.Sampler):
        """
        Helper function to project seed rays to obtain silhouette segments and
        map them to boundary sample space.
        """
        parent = self.parent

        projection = self.ProjectOperation(
            parent.proj_mesh_algo,
            parent.proj_mesh_max_walk,
            parent.proj_mesh_max_jump
        )

        # Call sample() to get the seed rays
        ray, _, _ = parent.sample_rays(scene, sensor, sampler)
        _, _, _, (ray_seed, active) = parent.sample(
            mode     = dr.ADMode.Primal,
            scene    = scene,
            sampler  = sampler,
            ray      = ray,
            depth    = 0,
            δL       = None,
            state_in = None,
            active   = mi.Bool(True),
            project  = True
        )

        # Get the intersection struct for the shape pointer
        flags = mi.RayFlags.All | mi.RayFlags.dNSdUV
        si = scene.ray_intersect(ray_seed, ray_flags=flags, coherent=True, active=active)
        active &= si.is_valid()

        # Is the shape we hit being differentiated in this scene?
        is_silhouette_shape = mi.Bool(False)
        scene_index = mi.UInt32(0)
        silhouette_shapes = scene.silhouette_shapes()
        for i in range(len(silhouette_shapes)):
            current_shape = (si.shape == mi.ShapePtr(silhouette_shapes[i]))
            scene_index[current_shape] = mi.UInt32(i)
            is_silhouette_shape |= (active & current_shape)
        active &= is_silhouette_shape

        # Perform projection
        ss, active = projection.eval(scene, ray_seed, si, sampler, active)
        ss.scene_index = scene_index

        # Map the silhouette segment to boundary sample space
        pt_3 = scene.invert_silhouette_sample(ss, active)

        return pt_3, active

    def init_indirect_silhouette(self,
                                 scene: mi.Scene,
                                 sensor: mi.Sensor,
                                 seed: mi.UInt32):
        """
        Initialize the guiding structure for indirect discontinuous derivatives
        based on the guiding mode. The result is stored in this python class.
        """
        parent = self.parent

        self.guiding_distr = None

        if not parent.guiding == 'none':
            proj_str = 'with' if parent.guiding_proj else 'without'
            mi.Log(mi.LogLevel.Info,
                f"Initializing boundary sample space with \"{parent.guiding}\" "
                f"guiding and {proj_str} projection."
            )

        # Call corresponding initialization function
        if parent.guiding == 'none':
            self.guiding_distr = mi.ad.UniformDistr()

        elif parent.guiding == 'grid' and not parent.guiding_proj:
            self.init_indirect_silhouette_grid_unif(scene, sensor, seed)

        elif parent.guiding == 'grid' and parent.guiding_proj:
            self.init_indirect_silhouette_grid_proj(scene, sensor, seed)

        elif parent.guiding == 'octree':
            self.init_indirect_silhouette_octree(scene, sensor, seed)


    @dr.syntax
    def init_indirect_silhouette_grid_unif(self, scene, sensor, seed):
        """
        Guiding structure initialization for uniform grid sampling.
        """
        parent = self.parent

        ## Grid guiding with uniform sampling initialization
        self.guiding_distr = mi.ad.GridDistr(
            parent.guiding_grid_reso,
            parent.clamp_mass_thres,
            parent.scale_mass,
            mi.log_level() == mi.LogLevel.Debug,
        )

        num_samples = (
            parent.guiding_grid_reso[0] *
            parent.guiding_grid_reso[1] *
            parent.guiding_grid_reso[2]
        )

        # Sampler with the wavefront size equal to the size of the grid
        sampler = sensor.sampler().clone()
        sampler.seed(seed, num_samples)

        res = dr.zeros(mi.Float, sampler.wavefront_size())
        cnt = mi.UInt32(0)
        while cnt < parent.guiding_rounds:
            # GridGuiding - Sample uniform points in [0, 1]^3
            sample_3 = self.guiding_distr.random_cell_sample(sampler)
            # GridGuiding - Evaluate point contribution
            value, _ = self.eval_indirect_integrand(
                scene, sensor, sample_3, sampler, preprocess=True)
            value = dr.max(value)  # dr.max over channels
            res += value
            cnt += 1

        res /= parent.guiding_rounds
        self.guiding_distr.set_mass(res)

    def init_indirect_silhouette_grid_proj(self, scene, sensor, seed):
        """
        Guiding structure initialization for projective grid sampling.
        """
        parent = self.parent

        ## Grid guiding with projection initialization
        self.guiding_distr = mi.ad.GridDistr(
            parent.guiding_grid_reso,
            parent.clamp_mass_thres,
            parent.scale_mass,
            mi.log_level() == mi.LogLevel.Debug
        )

        ttl_num_cells = parent.guiding_grid_reso[0] * \
                        parent.guiding_grid_reso[1] * \
                        parent.guiding_grid_reso[2]

        # Get the spp used to perform the projection to initialize the guiding
        if parent.proj_seed_spp == 0:
            parent.proj_seed_spp = (ttl_num_cells // dr.prod(sensor.film().crop_size())) + 1

        spp = parent.proj_seed_spp
        sampler, _ = parent.prepare(sensor, seed, spp, parent.aov_names())
        res = dr.zeros(mi.Float, ttl_num_cells)
        mi.Log(mi.LogLevel.Debug,
               "Initializing `grid` guiding distribution with projected"
               "samples.")
        for i in range(parent.guiding_rounds):
            mi.Log(mi.LogLevel.Debug, f"round {i+1} / {parent.guiding_rounds}")

            # Initialize the sampler
            sampler.seed(i + (seed ^ 0x5555aaaa), sampler.wavefront_size())

            # GridGuiding - Sample points in U3
            sample_3, active_seed = self.get_projected_points(scene, sensor, sampler)
            idx = self.guiding_distr.sample_to_cell_idx(sample_3, active_seed)

            # GridGuiding - Evaluate point contribution
            value, _ = self.eval_indirect_integrand(
                scene, sensor, sample_3, sampler, preprocess=True, active=active_seed)
            value = dr.max(value)  # dr.max over channels

            # Compute the sum of every cell
            res_sum = dr.zeros(mi.Float, ttl_num_cells)
            dr.scatter_reduce(dr.ReduceOp.Add, res_sum, value, idx, active_seed)

            # Compute the number of samples in every cell
            count = dr.zeros(mi.Float, ttl_num_cells)
            ones = dr.ones(mi.Float, dr.width(idx))
            dr.scatter_reduce(dr.ReduceOp.Add, count, ones, idx, active_seed)
            count[count == 0] = 1

            # Compute the average contribution of every cell
            res_tmp = res_sum / count

            # Accumulate the average contribution
            res += res_tmp
            dr.eval(res)

        # Divide by the number of rounds
        res /= parent.guiding_rounds

        # Initialize the guiding distribution using the average contribution
        self.guiding_distr.set_mass(res)

    def init_indirect_silhouette_octree(self, scene, sensor, seed):
        """
        Guiding structure initialization for octree-based guiding.
        """
        parent = self.parent

        ## Octree guiding with projective or uniform initialization
        def eval_indirect_integrand_handle(point_3, sampler):
            return self.eval_indirect_integrand(
                scene, sensor, point_3, sampler, preprocess=True)

        self.guiding_distr = mi.ad.OcSpaceDistr(
            parent.octree_max_depth,
            parent.octree_max_leaf_cnt,
            parent.octree_extra_leaf_sample,
            eval_indirect_integrand_handle,
            parent.octree_construction_mean_mult,
            parent.clamp_mass_thres,
            parent.octree_highres_x_slices,
            parent.octree_scatter_inc,
            parent.scale_mass,
            mi.log_level() == mi.LogLevel.Debug
        )

        ttl_num_cells = parent.guiding_grid_reso[0] * \
                        parent.guiding_grid_reso[1] * \
                        parent.guiding_grid_reso[2]

        # OctreeGuiding - Sample points in [0, 1]^3
        if parent.guiding_proj:
            # Projective sampling to get samples in boundary sample space
            if parent.proj_seed_spp == 0:
                film_shape = sensor.film().crop_size()
                parent.proj_seed_spp = (ttl_num_cells // dr.prod(film_shape)) + 1

            spp = parent.proj_seed_spp
            if parent.octree_scatter_inc:
                spp = parent.proj_seed_spp * parent.guiding_rounds

            sampler, _ = parent.prepare(sensor, seed, spp, parent.aov_names())

            if parent.guiding_rounds == 1 or parent.octree_scatter_inc:
                sampler.seed(seed ^ 0x5555aaaa, sampler.wavefront_size())
                sample_3, active_guide = self.get_projected_points(scene, sensor, sampler)
            else:
                # This logic only exists for the purpose of testing the efficiency of scatter_inc
                samples_buffer = dr.zeros(mi.Point3f, int(4e7))
                cnt_active = dr.opaque(dr.detached_t(mi.UInt32), 0, shape=1)
                for i in range(parent.guiding_rounds):
                    sampler.seed(i + (seed ^ 0x5555aaaa), sampler.wavefront_size())

                    samples_batch, active_batch = self.get_projected_points(scene, sensor, sampler)
                    dr.schedule(samples_batch, active_batch)
                    active_indices = dr.compress(active_batch)

                    samples_batch_active = dr.gather(mi.Point3f, samples_batch, active_indices)
                    buffer_indices = dr.arange(mi.UInt32, dr.width(samples_batch_active)) + cnt_active
                    dr.scatter(samples_buffer, samples_batch_active, buffer_indices)

                    cnt_active += dr.opaque(mi.UInt32, dr.width(samples_batch_active), shape=1)

                sample_3 = dr.gather(mi.Point3f, samples_buffer, dr.arange(mi.UInt32, dr.slice(cnt_active)))
                active_guide = mi.Mask(True)
                del samples_batch, samples_buffer, active_batch, samples_batch_active, buffer_indices
        else:
            # Uniform sampling to get samples in boundary sample space
            sampler = sensor.sampler().clone()
            sampler.seed(seed ^ 0x5555aaaa, ttl_num_cells)
            sample_3 = mi.Point3f(sampler.next_1d(), sampler.next_1d(), sampler.next_1d())
            active_guide = mi.Mask(True)

        # OctreeGuiding - Evaluate point contribution
        value, _ = self.eval_indirect_integrand(
            scene, sensor, sample_3, sampler, preprocess=True, active=active_guide)
        value[dr.isinf(value)] = 0
        value = dr.max(value)

        # Estimate mass threshold for the current scene once
        if parent.octree_contruction_thres == 0:
            """
            Boundary samples with contribution smaller than this value will be
            ignored during octree construction to save memory. If set to be 0,
            we estimate it once by launching an additional kernel.

            In the kernel launch, the wavefront size could be so large that even
            storing one float per sample would run out of device memory. The
            following code launches the kernel once to compute the summed
            contribution and the sample count without storing the per-lane
            results to memory. Only these two numbers are stored in the device
            memory. When the per-lane results are needed later, they are
            re-computed from scratch.
            """
            # This launches one kernel. Note: swapping the order of the
            # following two operations would launch the large kernel twice. This
            # is because ``dr.count()`` evaluates the kernel immediately.
            value_sum = mi.Float(0)
            dr.scatter_reduce(dr.ReduceOp.Add,
                              value_sum, value, mi.UInt32(0), active_guide)
            count_nonzero = dr.count(value > 0)

            # If no valid samples are found, raise an exception
            if count_nonzero == 0:
                mi.Log(mi.LogLevel.Warn,
                       "No valid indirect silhouette samples were found in the "
                       "scene. Please check the scene and the camera settings. "
                       "If no indirect discontinuous derivatives exists in the "
                       "scene, it is more efficient to set `sppi` to 0.")
                self.guiding_distr = None
                return

            # If we only have a small number of valid samples, do not filter
            # input samples: use all of them to construct the octree.
            num_valid_samples_lowerbound = 2 ** 20  # ~1.05 million
            if count_nonzero < num_valid_samples_lowerbound:
                parent.octree_contruction_thres = dr.smallest(mi.Float)  # Any non-zero small value
            else:
                mean_valid = value_sum / count_nonzero
                parent.octree_contruction_thres = mean_valid * parent.octree_construction_mean_mult

            mi.Log(mi.LogLevel.Debug,
                   "The sample's mass threshold for building the octree "
                   "guiding strucutre was not initialized yet. A threshold was "
                  f"computed by using {count_nonzero} non-zero samples. "
                   "This is a fairly expensive operation. The estimated "
                   "threshold will be used from now on unless it is reset to 0.")

        self.guiding_distr.mass_contruction_thres = parent.octree_contruction_thres

        try:
            self.guiding_distr.set_points(sample_3, value, seed, mi.log_level() == mi.LogLevel.Debug)
        except Exception as e:
            mi.Log(mi.LogLevel.Warn,
                   "Failed to build the Octree guiding distribution! No "
                   "guiding distibution for indirect visibility "
                   "discontinuities will be used.\n"
                   "The original error message from the octree construction:\n"
                   f"{e}")
            self.guiding_distr = None
            return

        del sample_3, sampler, active_guide, value

    def eval_indirect_integrand(self,
                                scene: mi.Scene,
                                sensor: mi.Sensor,
                                sample: mi.Vector3f,
                                sampler: mi.Sampler,
                                preprocess: bool,
                                active: mi.Mask = True):
        """
        Evaluate the indirect discontinuous derivatives integral for a given
        sample point in boundary sample space.

        Parameters ``sample`` (``mi.Point3f``):
            The sample point in boundary sample space.

        This function returns a tuple ``(result, sensor_uv)`` where

        Output ``result`` (``mi.Spectrum``):
            The integrand of the indirect discontinuous derivatives.

        Output ``sensor_uv`` (``mi.Point2f``):
            The UV coordinates on the sensor film to splat the result to. If
            ``preprocess`` is false, this coordinate is not used.
        """
        parent = self.parent

        with dr.suspend_grad():
            if parent.guiding == 'none':
                self.discontinuity_flags = mi.DiscontinuityFlags.AllTypes.value | \
                                           mi.DiscontinuityFlags.DirectionLune
            else:
                self.discontinuity_flags = mi.DiscontinuityFlags.AllTypes.value | \
                                           mi.DiscontinuityFlags.DirectionSphere

            ## Sample a boundary segment
            ss = scene.sample_silhouette(sample, self.discontinuity_flags, active)
            active &= ss.is_valid()

            # Estimate the importance
            fS, sensor_uv, sensor_depth, shading_p, active_i = parent.sample_importance(
                scene, sensor, ss, parent.max_depth, sampler, preprocess, active)
            active &= active_i

            # Estimate the radiance difference
            fE, active_e = parent.sample_radiance_difference(
                scene, ss, sensor_depth - 1, sampler, active)
            active &= active_e

            # Local boundary term without the local speed term
            fB = ss.foreshortening

            result = dr.select(active, fS * fB * fE * dr.rcp(ss.pdf), 0)

        # Compute the motion of the boundary segment if this is not a preprocess
        if preprocess: # TODO cleanup
            return dr.abs(result), mi.Point2f(0)
        else:
            si = dr.zeros(mi.SurfaceInteraction3f)
            si.p = ss.p
            si.prim_index = ss.prim_index
            si.uv = ss.uv
            x_b_follow = ss.shape.differential_motion(dr.detach(si), active)

            motion = dr.dot(dr.detach(ss.n), (x_b_follow - shading_p))
            result = dr.select(active, result * motion, 0)
            return result, sensor_uv


    #####################################################################
    ##                      Projection operation                       ##
    #####################################################################

    class ProjectOperation():
        """
        Projection operation takes a seed ray as input and outputs a
        \ref SilhouetteSample3f object.
        """
        def __init__(self, mesh_proj_algo, max_walk, max_jump) -> None:
            self.mesh_proj_algo = mesh_proj_algo
            if mesh_proj_algo not in ["hybrid", "walk", "jump"]:
                raise ValueError(f"ProjectOperation: unknown mesh_proj_algo: {mesh_proj_algo}")
            self.max_walk = max_walk
            self.max_jump = max_jump

        # ---------------------- Triangle mesh projection ----------------------

        @dr.syntax
        def mesh_walk(self,
                      si_: mi.SurfaceInteraction3f,
                      viewpoint: mi.Point3f,
                      state: mi.UInt64,
                      active: mi.Bool,
                      max_move: int):
            # TODO: This copy is necessary for "Dr.Jit reasons". Should be fixed
            # when Dr.Jit uses nanobind
            si = mi.SurfaceInteraction3f(si_)

            ss = dr.zeros(mi.SilhouetteSample3f)
            sampler = mi.PCG32(dr.width(si), state)

            active_loop = active & (max_move > 0)
            depth = mi.UInt32(0)
            last_succ_ss = mi.SilhouetteSample3f(ss)

            while active_loop:
                flags = (mi.DiscontinuityFlags.HeuristicWalk.value |
                         mi.DiscontinuityFlags.PerimeterType.value)
                ss[active_loop] = si.shape.primitive_silhouette_projection(
                    viewpoint, si, flags, sampler.next_float32(active_loop), active_loop)

                depth += 1
                last_succ_ss[active_loop & ss.is_valid()] = ss
                last_succ_ss.prim_index[active_loop & ss.is_valid()] = si.prim_index
                active_loop &= depth < max_move
                si.prim_index[active_loop] = ss.prim_index

            # Undo the last inplace `prim_index` update
            ss.prim_index = si.prim_index
            valid = active & ss.is_valid()

            return ss, sampler.state, valid

        @dr.syntax
        def mesh_jump(self,
                      scene: mi.Scene,
                      si_: mi.SurfaceInteraction3f,
                      viewpoint: mi.Point3f,
                      state: mi.UInt64,
                      active: mi.Bool,
                      max_jump: int):
            # TODO: This copy is necessary for "Dr.Jit reasons". Should be fixed
            # when Dr.Jit uses nanobind
            si = mi.SurfaceInteraction3f(si_)

            shape = si.shape
            sampler = mi.PCG32(dr.width(si), state)

            # The random number `edge_sample` is only used when a silhouette is found.
            # If previous projection has not reached any silhouette, we can safely reuse
            # the same number.
            edge_sample = sampler.next_float32(active)

            # Check if we already hit a silhouette before performing the jump
            flags = mi.DiscontinuityFlags.PerimeterType.value
            ss = shape.primitive_silhouette_projection(viewpoint, si, flags, edge_sample, active)
            depth = mi.UInt32(0)
            valid = active & ss.is_valid()
            loop_active = active & (~valid) & (max_jump > 0)

            while loop_active:
                # Project si onto the solution set
                H = dr.normalize(viewpoint - si.p)
                a = dr.dot(H, si.dn_du)
                b = dr.dot(H, si.dn_dv)
                c = dr.dot(H, si.sh_frame.n)
                rcp_ab_sqr = dr.rcp(a*a + b*b)
                Q = mi.Point2f(-a*c, -b*c) * rcp_ab_sqr
                projected_p = si.p + Q[0] * si.dp_du + Q[1] * si.dp_dv
                projected_normal = dr.normalize(si.sh_frame.n + Q[0] * si.dn_du + Q[1] * si.dn_dv)

                # Compute new mesh intersection
                ray_new = mi.Ray3f(
                    projected_p + projected_normal * mi.math.RayEpsilon,
                    -projected_normal
                )
                si = scene.ray_intersect(ray_new, mi.RayFlags.All | mi.RayFlags.dNSdUV,
                                         coherent=False, active=loop_active)
                loop_active &= si.is_valid() & (si.shape == shape)

                # Check if we hit a silhouette
                flags = mi.DiscontinuityFlags.PerimeterType.value
                ss[loop_active] = shape.primitive_silhouette_projection(viewpoint, si, flags, edge_sample, loop_active)
                valid |= loop_active & ss.is_valid()

                # Update
                depth += 1
                loop_active &= (~valid) & (depth < max_jump)

            return ss, valid

        def hybrid_mesh_projection(self,
                                   scene: mi.Scene,
                                   si: mi.SurfaceInteraction3f,
                                   viewpoint: mi.Point3f,
                                   state: mi.UInt64,
                                   active: mi.Bool,
                                   max_move: int):
            ## Local walk
            ss, state, valid = self.mesh_walk(si, viewpoint, state, active, max_move)

            sampler = mi.PCG32(dr.width(si), state)

            ## One jump
            # Proceed if a silhouette is not found
            active_jump = active & (~valid)

            # We construct a surface interaction `si_jump` with all the information
            # needed to perform the jump operation to avoid one expensive ray tracing.
            pi = dr.zeros(mi.PreliminaryIntersection3f, dr.width(ss))
            pi.t = 1.0
            sample_2d = mi.Point2f(sampler.next_float32(), sampler.next_float32())
            pi.prim_uv = mi.warp.square_to_uniform_triangle(sample_2d)
            pi.prim_index = ss.prim_index
            pi.shape = ss.shape
            dummy_ray = dr.zeros(mi.Ray3f, dr.width(ss))
            si_jump = pi.compute_surface_interaction(
                dummy_ray, mi.RayFlags.All | mi.RayFlags.dNSdUV, active=active)

            # Perform the jump operation
            ss_jump, valid_jump = self.mesh_jump(scene, si_jump, viewpoint, sampler.state, active_jump, 1)

            ss[valid_jump] = ss_jump

            return ss, valid | valid_jump

        def project_mesh(self, scene, ray_guide, si_guide, state, active):
            viewpoint = ray_guide.o

            if self.mesh_proj_algo == "hybrid":
                ss, active = self.hybrid_mesh_projection(scene, si_guide, viewpoint, state, active, self.max_walk)
            elif self.mesh_proj_algo == "walk":
                ss, _, active = self.mesh_walk(si_guide, viewpoint, state, active, self.max_walk)
            elif self.mesh_proj_algo == "jump":
                ss, active = self.mesh_jump(scene, si_guide, viewpoint, state, active, self.max_jump)

            ss.flags = mi.DiscontinuityFlags.PerimeterType | mi.DiscontinuityFlags.DirectionSphere

            return ss

        # ---------------------- Other primitives ----------------------

        def project_sphere(self, scene, ray_guide, si, state, active):
            ss = si.shape.primitive_silhouette_projection(
                ray_guide.o, si, mi.DiscontinuityFlags.InteriorType.value, 0.0, active)
            return ss

        def project_disk(self, scene, ray_guide, si, state, active):
            ss = si.shape.primitive_silhouette_projection(
                ray_guide.o, si, mi.DiscontinuityFlags.PerimeterType.value, 0.0, active)
            return ss

        def project_cylinder(self, scene, ray_guide, si, state, active):
            sampler = mi.PCG32(dr.width(si), state)

            ss_interior = si.shape.primitive_silhouette_projection(
                ray_guide.o, si, mi.DiscontinuityFlags.InteriorType.value, 0.0, active)
            active_interior = active & ss_interior.is_valid()

            ss_perimeter = si.shape.primitive_silhouette_projection(
                ray_guide.o, si, mi.DiscontinuityFlags.PerimeterType.value, 0.0, active)
            active_perimeter = active & ss_perimeter.is_valid()

            # Randomly choose between perimeter and interior
            only_interior_failed = active & ~active_interior & active_perimeter
            rand = sampler.next_float32(active)

            ss = dr.select(
                only_interior_failed | (active_interior & (rand > 0.5)),
                ss_perimeter,
                ss_interior
            )
            ss.flags = mi.DiscontinuityFlags.InteriorType | mi.DiscontinuityFlags.PerimeterType

            return ss

        def project_curve(self, scene, ray_guide, si, state, active):
            sampler = mi.PCG32(dr.width(si), state)

            ss_interior = si.shape.primitive_silhouette_projection(
                ray_guide.o, si, mi.DiscontinuityFlags.InteriorType.value, 0.0, active)
            active_interior = active & ss_interior.is_valid()

            ss_perimeter = si.shape.primitive_silhouette_projection(
                ray_guide.o, si, mi.DiscontinuityFlags.PerimeterType.value, 0.0, active)
            active_perimeter = active & ss_perimeter.is_valid()

            # Randomly choose between perimeter and interior
            only_interior_failed = active & ~active_interior & active_perimeter
            rand = sampler.next_float32(active)

            ss = dr.select(
                only_interior_failed | (active_interior & (rand > 0.5)),
                ss_perimeter,
                ss_interior
            )
            ss.flags = mi.DiscontinuityFlags.InteriorType | mi.DiscontinuityFlags.PerimeterType

            return ss

        def project_rectangle(self, scene, ray_guide, si, state, active):
            sampler = mi.PCG32(dr.width(si), state)
            sample = sampler.next_float32(active)
            ss = si.shape.primitive_silhouette_projection(
                ray_guide.o, si, mi.DiscontinuityFlags.PerimeterType.value, sample, active)

            return ss

        def project_sdf(self, scene, ray_guide, si_guide, state, active):
            #TODO : Not implemented yet
            return dr.zeros(mi.SilhouetteSample3f)

        def eval(self, scene, ray_guide, si_guide, sampler, active):
            """
            Dispatches the seed surface interaction object to the appropriate
            shape's projection algorithm.
            """
            #TODO pass discontinuity flags as an option
            def noop(*args,**kwargs):
                return dr.zeros(mi.SilhouetteSample3f)

            max_idx = mi.ShapeType.Other.value
            projection_funcs = [noop] * max_idx

            projection_funcs[mi.ShapeType.Mesh.value] = self.project_mesh
            projection_funcs[mi.ShapeType.BSplineCurve.value] = self.project_curve
            projection_funcs[mi.ShapeType.Cylinder.value] = self.project_cylinder
            projection_funcs[mi.ShapeType.Disk.value] = self.project_disk
            projection_funcs[mi.ShapeType.Rectangle.value] = self.project_rectangle
            projection_funcs[mi.ShapeType.SDFGrid.value] = self.project_sdf
            projection_funcs[mi.ShapeType.Sphere.value] = self.project_sphere

            shape_types = si_guide.shape.shape_type()
            state = mi.UInt64(sampler.next_1d() * mi.UInt64(0xFFFFFFFFFFFFFFFF))

            ss = dr.switch(
                shape_types,
                projection_funcs,
                scene, ray_guide, si_guide, state, active
            )
            active &= ss.is_valid()

            return ss, active
