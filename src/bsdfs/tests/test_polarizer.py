import mitsuba
import pytest
import enoki as ek

def test01_create(variant_scalar_mono_polarized):
    from mitsuba.render import BSDFFlags
    from mitsuba.core.xml import load_string

    b = load_string("<bsdf version='2.0.0' type='polarizer'></bsdf>")
    assert b is not None
    assert b.component_count() == 1
    assert b.flags(0) == BSDFFlags.Null | BSDFFlags.FrontSide | BSDFFlags.BackSide
    assert b.flags() == b.flags(0)


def test02_sample_local(variant_scalar_mono_polarized):
    from mitsuba.core import Frame3f, Transform4f, Spectrum
    from mitsuba.core.xml import load_string
    from mitsuba.render import BSDFContext, TransportMode, SurfaceInteraction3f

    def spectrum_from_stokes(v):
        res = Spectrum(0.0)
        for i in range(4):
            res[i, 0] = v[i]
        return res

    # Test polarized implementation, version in local BSDF coordinate system
    # (surface normal aligned with "z").
    #
    # The polarizer is rotated to different angles and hit with fully
    # unpolarized light (Stokes vector [1, 0, 0, 0]).
    # We then test if the outgoing Stokes vector corresponds to the expected
    # rotation of linearly polarized light (Case 1).
    #
    # Additionally, a perfect linear polarizer should be invariant to "tilting",
    # i.e. rotations around "x" or "z" in this local frame (Case 2, 3).

    # Incident direction
    wi = [0, 0, 1]
    stokes_in = spectrum_from_stokes([1, 0, 0, 0])

    ctx = BSDFContext()
    ctx.mode = TransportMode.Importance
    si = SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.wi = wi
    n = [0, 0, 1]
    si.n = n
    si.sh_frame = Frame3f(si.n)

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

        bsdf = load_string("""<bsdf version='2.0.0' type='polarizer'>
                                  <spectrum name="theta" value="{}"/>
                              </bsdf>""".format(angle))

        # Case 1: Perpendicular incidence.
        bs, M = bsdf.sample(ctx, si, 0.0, [0.0, 0.0])

        stokes_out = M @ stokes_in
        assert ek.allclose(expected, stokes_out, atol=1e-3)

        def rotate_vector(v, axis, angle):
            return Transform4f.rotate(axis, angle).transform_vector(v)

        # Case 2: Tilt polarizer around "x". Should not change anything.
        # (Note: to stay with local coordinates, we rotate the incident direction instead.)
        si.wi = rotate_vector(wi, [1, 0, 0], angle=30.0)
        bs, M = bsdf.sample(ctx, si, 0.0, [0.0, 0.0])
        stokes_out = M @ stokes_in
        assert ek.allclose(expected, stokes_out, atol=1e-3)

        # Case 3: Tilt polarizer around "y". Should not change anything.
        # (Note: to stay with local coordinates, we rotate the incident direction instead.)
        si.wi = rotate_vector(wi, [0, 1, 0], angle=30.0)
        bs, M = bsdf.sample(ctx, si, 0.0, [0.0, 0.0])
        stokes_out = M @ stokes_in
        assert ek.allclose(expected, stokes_out, atol=1e-3)


def test03_sample_world(variant_scalar_mono_polarized):
    from mitsuba.core import Ray3f, Spectrum
    from mitsuba.core.xml import load_string
    from mitsuba.render import BSDFContext, TransportMode
    from mitsuba.render.mueller import stokes_basis, rotate_mueller_basis_collinear

    def spectrum_from_stokes(v):
        res = Spectrum(0.0)
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

    ctx = BSDFContext()
    ctx.mode = TransportMode.Importance
    ray = Ray3f([0, -100, 0], forward, 0.0, 0.0)

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
        scene_str = """<scene version='2.0.0'>
                           <shape type="rectangle">
                               <bsdf type="polarizer"/>

                               <transform name="to_world">
                                   <rotate x="0" y="0" z="1" angle="{}"/>   <!-- Rotate around optical axis -->
                                   <rotate x="1" y="0" z="0" angle="-90"/>  <!-- Rotate s.t. it is no longer aligned with local coordinates -->
                               </transform>
                           </shape>
                       </scene>""".format(angle)
        scene = load_string(scene_str)

        # Intersect ray against geometry
        si = scene.ray_intersect(ray)

        bs, M_local = si.bsdf().sample(ctx, si, 0.0, [0.0, 0.0])
        M_world = si.to_world_mueller(M_local, -si.wi, bs.wo)

        # Make sure we are measuring w.r.t. to the optical table
        M = rotate_mueller_basis_collinear(M_world,
                                           forward,
                                           stokes_basis(forward), [-1, 0, 0])

        stokes_out = M @ stokes_in
        assert ek.allclose(stokes_out, expected, atol=1e-3)


def test04_path_tracer_polarizer(variant_scalar_mono_polarized):
    from mitsuba.core import Spectrum
    from mitsuba.core.xml import load_string
    from mitsuba.render import BSDFContext, TransportMode
    from mitsuba.render.mueller import stokes_basis, rotate_stokes_basis_m

    def spectrum_from_stokes(v):
        res = Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    # Test polarizer implementation inside of an actual polarized path tracer.
    # (Serves also as test case for the polarized path tracer itself.)
    #
    # The polarizer is place in front of a light source that emits unpolarized
    # light (Stokes vector [1, 0, 0, 0]). The filter is rotated to different
    # angles and the transmitted polarization state is direclty observed with
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
        scene_str = """<scene version='2.0.0'>
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
                                   <spectrum name="radiance" value="1"/>
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

        scene = load_string(scene_str)
        integrator = scene.integrator()
        sensor     = scene.sensors()[0]
        sampler    = sensor.sampler()

        # Sample ray from sensor
        ray, _ = sensor.sample_ray_differential(0.0, 0.5, [0.5, 0.5], [0.5, 0.5])

        # Call integrator
        value, _, _ = integrator.sample(scene, sampler, ray)

        # Normalize Stokes vector
        value /= value[0, 0]

        # Align output stokes vector (based on ray.d) with optical table. (In this configuration, this is a no-op.)
        forward = -ray.d
        basis_cur = stokes_basis(forward)
        basis_tar = [-1, 0, 0]
        R = rotate_stokes_basis_m(forward, basis_cur, basis_tar)
        value = R @ value

        assert ek.allclose(value, expected[k], atol=1e-3)


def test05_path_tracer_malus_law(variant_scalar_mono_polarized):
    from mitsuba.core import Spectrum
    from mitsuba.core.xml import load_string
    from mitsuba.render import BSDFContext, TransportMode
    from mitsuba.render.mueller import stokes_basis, rotate_stokes_basis_m

    def spectrum_from_stokes(v):
        res = Spectrum(0.0)
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
        scene_str = """<scene version='2.0.0'>
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
                                   <spectrum name="radiance" value="1"/>
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

        scene = load_string(scene_str)
        integrator = scene.integrator()
        sensor     = scene.sensors()[0]
        sampler    = sensor.sampler()

        # Sample ray from sensor
        ray, _ = sensor.sample_ray_differential(0.0, 0.5, [0.5, 0.5], [0.5, 0.5])

        # Call integrator
        value, _, _ = integrator.sample(scene, sampler, ray)

        # Extract intensity from returned Stokes vector
        v = value[0,0]

        # Avoid occasional rounding problems
        v = ek.max(0.0, v)

        # Keep track of observed radiance
        radiance.append(v)

    # Check that Malus' law holds
    for i in range(len(angles)):
        theta = angles[i] * ek.pi/180
        malus = ek.cos(theta)**2
        malus *= radiance[0]
        assert ek.allclose(malus, radiance[i], atol=1e-2)
