import pytest
import drjit as dr
import mitsuba as mi

def test01_create(variant_scalar_mono_polarized):
    b = mi.load_dict({'type': 'polarizer'})
    assert b is not None
    assert b.component_count() == 1
    assert b.flags(0) == mi.BSDFFlags.Null | mi.BSDFFlags.FrontSide | mi.BSDFFlags.BackSide
    assert b.flags() == b.flags(0)


def test02_sample_local(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i, 0] = v[i]
        return res

    # Test polarized implementation, version in local BSDF coordinate system
    # (surface normal aligned with "z").
    #
    # The polarizer is rotated to different angles and hit with fully
    # unpolarized light (Stokes vector [1, 0, 0, 0]).
    # We then test if the outgoing Stokes vector corresponds to the expected
    # rotation of linearly polarized light.

    # Incident direction
    wi = mi.Vector3f(0, 0, 1)
    stokes_in = spectrum_from_stokes([1, 0, 0, 0])

    ctx = mi.BSDFContext()
    ctx.mode = mi.TransportMode.Importance
    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.wi = wi
    n = [0, 0, 1]
    si.n = n
    si.sh_frame = mi.Frame3f(si.n)

    # Polarizer rotation angles
    angles = [0, 90, +45, -45]
    # Expected outgoing Stokes vector
    expected_states = [spectrum_from_stokes([0.5,  0.5,  0,   0]),
                       spectrum_from_stokes([0.5, -0.5,  0,   0]),
                       spectrum_from_stokes([0.5,  0,   +0.5, 0]),
                       spectrum_from_stokes([0.5,  0,   -0.5, 0])]

    for k in range(len(angles)):
        angle = angles[k]
        expected = expected_states[k]

        bsdf = mi.load_dict({
            'type': 'polarizer',
            'theta': {'type': 'spectrum', 'value': angle}
        })

        # Perpendicular incidence.
        bs, M = bsdf.sample(ctx, si, 0.0, [0.0, 0.0])

        stokes_out = M @ stokes_in
        assert dr.allclose(expected, stokes_out, atol=1e-3)


def test03_sample_world(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    # Test polarized implementation, version in world coordinates. This involves
    # additional coordinate changes to the local reference frame and back.
    #
    # The polarizer is rotated to different angles and hit with fully
    # unpolarized light (Stokes vector [1, 0, 0, 0]).
    # We then test if the outgoing Stokes vector corresponds to the expected
    # rotation of linearly polarized light.

    # Incident direction
    forward = [0, 1, 0]
    stokes_in = spectrum_from_stokes([1, 0, 0, 0])

    ctx = mi.BSDFContext()
    ctx.mode = mi.TransportMode.Importance
    ray = mi.Ray3f([0, -100, 0], forward, 0.0, mi.Color0f())

    # Polarizer rotation angles
    angles = [0, 90, +45, -45]
    # Expected outgoing Stokes vector
    expected_states = [spectrum_from_stokes([0.5, 0.5, 0, 0]),
                       spectrum_from_stokes([0.5, -0.5, 0, 0]),
                       spectrum_from_stokes([0.5, 0, +0.5, 0]),
                       spectrum_from_stokes([0.5, 0, -0.5, 0])]

    for k in range(len(angles)):
        angle = angles[k]
        expected = expected_states[k]

        # Build scene with given polarizer rotation angle
        scene_str = """<scene version='3.0.0'>
                           <shape type="rectangle">
                               <bsdf type="polarizer"/>
                               <transform name="to_world">
                                   <rotate x="0" y="0" z="1" angle="{}"/>   <!-- Rotate around optical axis -->
                                   <rotate x="1" y="0" z="0" angle="-90"/>  <!-- Rotate s.t. it is no longer aligned with local coordinates -->
                               </transform>
                           </shape>
                       </scene>""".format(angle)
        scene = mi.load_string(scene_str)

        # Intersect ray against geometry
        si = scene.ray_intersect(ray)

        bs, M_local = si.bsdf().sample(ctx, si, 0.0, [0.0, 0.0])
        M_world = si.to_world_mueller(M_local, -si.wi, bs.wo)

        # Make sure we are measuring w.r.t. to the optical table
        M = mi.mueller.rotate_mueller_basis_collinear(
            M_world,
            forward,
            mi.mueller.stokes_basis(forward),
            [-1, 0, 0]
        )

        stokes_out = M @ stokes_in
        assert dr.allclose(stokes_out, expected, atol=1e-3)


def test04_path_tracer_polarizer(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    # Test polarizer implementation inside of an actual polarized path tracer.
    # (Serves also as test case for the polarized path tracer itself.)
    #
    # The polarizer is place in front of a light source that emits unpolarized
    # light (Stokes vector [1, 0, 0, 0]). The filter is rotated to different
    # angles and the transmitted polarization state is directly observed with
    # a sensor.
    # We then test if the outgoing Stokes vector corresponds to the expected
    # rotation of linearly polarized light.

    horizontal_pol   = spectrum_from_stokes([1, +1, 0, 0])
    vertical_pol     = spectrum_from_stokes([1, -1, 0, 0])
    pos_diagonal_pol = spectrum_from_stokes([1, 0, +1, 0])
    neg_diagonal_pol = spectrum_from_stokes([1, 0, -1, 0])

    angles = [0, 90, +45, -45]
    expected = [horizontal_pol,
                vertical_pol,
                pos_diagonal_pol,
                neg_diagonal_pol]

    for k, angle in enumerate(angles):
        scene_str = """<scene version='3.0.0'>
                           <integrator type="path"/>

                           <sensor type="perspective">
                               <float name="fov" value="0.00001"/>
                               <transform name="to_world">
                                   <lookat origin="0, 10, 0"
                                           target="0, 0, 0"
                                           up    ="0, 0, 1"/>
                               </transform>
                               <film type="hdrfilm">
                                   <integer name="width" value="1"/>
                                   <integer name="height" value="1"/>
                                   <rfilter type="gaussian"/>
                                   <string name="pixel_format" value="rgb"/>
                               </film>
                           </sensor>

                           <!-- Light source -->
                           <shape type="rectangle">
                               <transform name="to_world">
                                   <rotate x="1" y="0" z="0" angle="-90"/>
                                   <translate y="-10"/>
                               </transform>

                               <emitter type="area">
                                   <rgb name="radiance" value="1"/>
                               </emitter>
                           </shape>

                           <!-- Polarizer -->
                           <shape type="rectangle">
                               <bsdf type="polarizer">
                               </bsdf>

                               <transform name="to_world">
                                   <rotate x="1" y="0" z="0" angle="-90"/>
                                   <rotate y="1" angle="{}"/>
                               </transform>
                           </shape>
                       </scene>""".format(angle)

        scene = mi.load_string(scene_str)
        integrator = scene.integrator()
        sensor     = scene.sensors()[0]
        sampler    = sensor.sampler()

        sampler.seed(0)

        # Sample ray from sensor
        ray, _ = sensor.sample_ray_differential(0.0, 0.5, [0.5, 0.5], [0.5, 0.5])

        # Call integrator
        value, _, _ = integrator.sample(scene, sampler, ray)

        # Normalize Stokes vector
        value = value * dr.rcp(value[0, 0][0])

        # Align output stokes vector (based on ray.d) with optical table. (In this configuration, this is a no-op.)
        forward = -ray.d
        basis_cur = mi.mueller.stokes_basis(forward)
        basis_tar = [-1, 0, 0]
        R = mi.mueller.rotate_stokes_basis_m(forward, basis_cur, basis_tar)
        value = R @ value

        assert dr.allclose(value, expected[k], atol=1e-3)


def test05_path_tracer_malus_law(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    # Test polarizer implementation inside of an actual polarized path tracer.
    # (Serves also as test case for the polarized path tracer itself.)
    #
    # This test involves a setup using two polarizers placed along an optical
    # axis between light source and camera.
    # - The first polarizer is fixed, and transforms the emitted radiance into
    #   linearly polarized light.
    # - The second polarizer rotates to different angles, thus attenuating the
    #   final observed intensity at the camera. We verify that this intensity
    #   follows Malus' law as expected.

    angles = [0, 15, 30, 45, 60, 75, 90]
    radiance = []

    for angle in angles:
        # Build scene with given polarizer rotation angle
        scene_str = """<scene version='3.0.0'>
                           <integrator type="path"/>

                           <sensor type="perspective">
                               <float name="fov" value="0.00001"/>
                               <transform name="to_world">
                                   <lookat origin="0, 10, 0"
                                           target="0, 0, 0"
                                           up    ="0, 0, 1"/>
                               </transform>
                               <film type="hdrfilm">
                                   <integer name="width" value="1"/>
                                   <integer name="height" value="1"/>
                                   <rfilter type="gaussian"/>
                                   <string name="pixel_format" value="rgb"/>
                               </film>
                           </sensor>

                           <!-- Light source -->
                           <shape type="rectangle">
                               <transform name="to_world">
                                   <rotate x="1" y="0" z="0" angle="-90"/>
                                   <translate y="-10"/>
                               </transform>

                               <emitter type="area">
                                   <rgb name="radiance" value="1"/>
                               </emitter>
                           </shape>

                           <!-- First polarizer. Stays fixed. -->
                           <shape type="rectangle">
                               <bsdf type="polarizer"/>

                               <transform name="to_world">
                                   <rotate x="1" y="0" z="0" angle="-90"/>
                                   <translate y="-5"/>
                               </transform>
                           </shape>

                           <!-- Second polarizer. Rotates. -->
                           <shape type="rectangle">
                               <bsdf type="polarizer"/>

                               <transform name="to_world">
                                   <rotate x="1" y="0" z="0" angle="-90"/>
                                   <rotate x="0" y="1" z="0" angle="{}"/>  <!-- Rotate around optical axis -->
                                   <translate y="0"/>
                               </transform>
                           </shape>

                       </scene>""".format(angle)

        scene = mi.load_string(scene_str)
        integrator = scene.integrator()
        sensor     = scene.sensors()[0]
        sampler    = sensor.sampler()
        sampler.seed(0)

        # Sample ray from sensor
        ray, _ = sensor.sample_ray_differential(0.0, 0.5, [0.5, 0.5], [0.5, 0.5])

        # Call integrator
        value, _, _ = integrator.sample(scene, sampler, ray)

        # Extract intensity from returned Stokes vector
        v = value[0,0]

        # Avoid occasional rounding problems
        v = dr.maximum(0.0, v)

        # Keep track of observed radiance
        radiance.append(v)

    # Check that Malus' law holds
    for i in range(len(angles)):
        theta = angles[i] * dr.pi/180
        malus = dr.cos(theta)**2
        malus *= radiance[0]
        assert dr.allclose(malus, radiance[i], atol=1e-2)


def test06_tilted(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i, 0] = v[i]
        return res

    # Test implementation for tilted polarizers, i.e. when incident directions
    # of the local coordinate system are non-perpendicular. See also
    # "The polarization properties of a tilted polarizer" by Korger et al. 2013.

    stokes_in = spectrum_from_stokes([1, 0, 0, 0])
    ctx = mi.BSDFContext()
    ctx.mode = mi.TransportMode.Importance

    # Check for different polarizer rotation angles
    for angle in [0, 90, +45, -45]:
        # Ray arriving at a 30˚ angle
        sin_theta, cos_theta = dr.sincos(30.0 / 180.0 * dr.pi)
        ray = mi.Ray3f(mi.Point3f(0, 0, 1), -mi.Vector3f(sin_theta, 0, cos_theta), 0.0, mi.Color0f())

        # Case 1: Rotate based on the internal `theta` angle parameter.
        scene_theta = mi.load_dict({
            'type': 'rectangle',
            'bsdf': {
                'type': 'polarizer',
                'theta': {'type': 'spectrum', 'value': angle}
            }
        })

        si_theta = scene_theta.ray_intersect(ray)
        bsdf_theta = si_theta.bsdf(ray)
        bs_theta, M_theta = bsdf_theta.sample(ctx, si_theta, 0.0, [0.0, 0.0])
        M_theta = si_theta.to_world_mueller(M_theta, -si_theta.wi, bs_theta.wo)
        wo_theta = si_theta.to_world(bs_theta.wo)

        # Case 2: Rotate the actual geometry of the polarizer.
        scene_rot = mi.load_dict({
            'type': 'rectangle',
            'bsdf': {
                'type': 'polarizer',
                'theta': {'type': 'spectrum', 'value': 0.0}
            },
            'to_world': mi.ScalarTransform4f().rotate(axis=(0, 0, 1), angle=-angle)
        })

        si_rot = scene_rot.ray_intersect(ray)
        bsdf_rot = si_rot.bsdf(ray)
        bs_rot, M_rot = bsdf_rot.sample(ctx, si_rot, 0.0, [0.0, 0.0])
        M_rot = si_rot.to_world_mueller(M_rot, -si_rot.wi, bs_rot.wo)
        wo_rot = si_rot.to_world(bs_rot.wo)

        stokes_theta = M_theta @ stokes_in
        stokes_rot = M_rot @ stokes_in

        assert dr.allclose(stokes_theta, stokes_rot, atol=1e-3)
