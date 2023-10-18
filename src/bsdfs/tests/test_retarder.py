import pytest
import drjit as dr
import mitsuba as mi


def test01_create(variant_scalar_mono_polarized):
    b = mi.load_dict({'type': 'retarder'})
    assert b is not None
    assert b.component_count() == 1
    assert b.flags(0) == mi.BSDFFlags.Null | mi.BSDFFlags.FrontSide | mi.BSDFFlags.BackSide
    assert b.flags() == b.flags(0)


def test02_sample_quarter_wave_local(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    # Test polarized implementation. Special case of delta = 90˚, also known
    # as a quarter-wave plate. (In local BSDF coordinates.)
    #
    # Following  "Polarized Light, Third Edition" by Dennis H. Goldstein,
    # Chapter 6.3 eq. (6.46) & (6.47).
    # (Note that the fast and slow axis of retarders were flipped in the first
    # edition by Edward Collett.)
    #
    # Case 1) Linearly polarized +45˚ light (Stokes vector [1, 0, 1, 0]) yields
    #         left circularly polarized light (Stokes vector [1, 0, 0, -1]).
    # Case 2) Linearly polarized -45˚ light (Stokes vector [1, 0, -1, 0]) yields
    #         right circularly polarized light (Stokes vector [1, 0, 0, 1]).
    #
    # Case 3) Right circularly polarized light (Stokes vector [1, 0, 0, 1]) yields
    #         linearly polarized +45˚ light (Stokes vector [1, 0, 1, 0]).
    # Case 4) Left circularly polarized light (Stokes vector [1, 0, 0, -1]) yields
    #         linearly polarized -45˚ light (Stokes vector [1, 0, -1, 0]).

    linear_pos     = spectrum_from_stokes([1, 0, +1, 0])
    linear_neg     = spectrum_from_stokes([1, 0, -1, 0])
    circular_right = spectrum_from_stokes([1, 0, 0, +1])
    circular_left  = spectrum_from_stokes([1, 0, 0, -1])

    bsdf = mi.load_dict({
        'type': 'retarder',
        'theta': 0.0,
        'delta': 90.0,
    })

    # Incident direction
    wi = [0, 0, 1]

    ctx = mi.BSDFContext()
    ctx.mode = mi.TransportMode.Importance
    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.wi = wi
    n = [0, 0, 1]
    si.n = n
    si.sh_frame = mi.Frame3f(si.n)

    bs, M = bsdf.sample(ctx, si, 0.0, [0.0, 0.0])

    # Case 1)
    assert dr.allclose(M @ linear_pos, circular_left, atol=1e-3)
    # Case 2)
    assert dr.allclose(M @ linear_neg, circular_right, atol=1e-3)
    # Case 3)
    assert dr.allclose(M @ circular_right, linear_pos, atol=1e-3)
    # Case 4)
    assert dr.allclose(M @ circular_left, linear_neg , atol=1e-3)


def test03_sample_half_wave_local(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    # Test polarized implementation. Special case of delta = 180˚, also known
    # as a half-wave plate. (In local BSDF coordinates.)
    #
    # Following "Polarized Light - Fundamentals and Applications" by Edward Collett
    # Chapter 5.3:
    #
    # Case 1 & 2) Switch between diagonal linear polarization states (-45˚ & + 45˚)
    # Case 3 & 4) Switch circular polarization direction

    linear_pos     = spectrum_from_stokes([1, 0, +1, 0])
    linear_neg     = spectrum_from_stokes([1, 0, -1, 0])
    circular_right = spectrum_from_stokes([1, 0, 0, +1])
    circular_left  = spectrum_from_stokes([1, 0, 0, -1])

    bsdf = mi.load_dict({
        'type': 'retarder',
        'theta': 0.0,
        'delta': 180.0,
    })

    # Incident direction
    wi = [0, 0, 1]

    ctx = mi.BSDFContext()
    ctx.mode = mi.TransportMode.Importance
    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.wi = wi
    n = [0, 0, 1]
    si.n = n
    si.sh_frame = mi.Frame3f(si.n)

    bs, M = bsdf.sample(ctx, si, 0.0, [0.0, 0.0])

    # Case 1)
    assert dr.allclose(M @ linear_pos, linear_neg, atol=1e-3)
    # Case 2)
    assert dr.allclose(M @ linear_neg, linear_pos , atol=1e-3)
    # Case 3)
    assert dr.allclose(M @ circular_right, circular_left, atol=1e-3)
    # Case 4)
    assert dr.allclose(M @ circular_left, circular_right , atol=1e-3)



def test04_sample_quarter_wave_world(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    # Test polarized implementation. Special case of delta = 90˚, also known
    # as a quarter-wave plate. (World coordinates.)
    #
    # Following  "Polarized Light, Third Edition" by Dennis H. Goldstein,
    # Chapter 6.3 eq. (6.46) & (6.47).
    # (Note that the fast and slow axis of retarders were flipped in the first
    # edition by Edward Collett.)
    #
    # Case 1) Linearly polarized +45˚ light (Stokes vector [1, 0, 1, 0]) yields
    #         left circularly polarized light (Stokes vector [1, 0, 0, -1]).
    # Case 2) Linearly polarized -45˚ light (Stokes vector [1, 0, -1, 0]) yields
    #         right circularly polarized light (Stokes vector [1, 0, 0, 1]).
    #
    # Case 3) Right circularly polarized light (Stokes vector [1, 0, 0, 1]) yields
    #         linearly polarized +45˚ light (Stokes vector [1, 0, 1, 0]).
    # Case 4) Left circularly polarized light (Stokes vector [1, 0, 0, -1]) yields
    #         linearly polarized -45˚ light (Stokes vector [1, 0, -1, 0]).

    linear_pos     = spectrum_from_stokes([1, 0, +1, 0])
    linear_neg     = spectrum_from_stokes([1, 0, -1, 0])
    circular_right = spectrum_from_stokes([1, 0, 0, +1])
    circular_left  = spectrum_from_stokes([1, 0, 0, -1])

    # Incident direction
    forward = [0, -1, 0]

    ctx = mi.BSDFContext()
    ctx.mode = mi.TransportMode.Importance
    ray = mi.Ray3f([0, 100, 0], forward, 0.0, mi.Color0f())

    # Build scene with given polarizer rotation angle
    scene_str = """<scene version='2.0.0'>
                       <shape type="rectangle">
                           <bsdf type="retarder">
                               <spectrum name="delta" value="90"/>
                           </bsdf>

                           <transform name="to_world">
                               <rotate x="1" y="0" z="0" angle="-90"/>  <!-- Rotate s.t. it is no longer aligned with local coordinates -->
                           </transform>
                       </shape>
                   </scene>"""
    scene = mi.load_string(scene_str)

    # Intersect ray against geometry
    si = scene.ray_intersect(ray)

    bs, M_local = si.bsdf().sample(ctx, si, 0.0, [0.0, 0.0])
    M_world = si.to_world_mueller(M_local, -si.wi, bs.wo)

    # Make sure we are measuring w.r.t. to the optical table
    M = mi.mueller.rotate_mueller_basis_collinear(M_world,
                                       forward,
                                       mi.mueller.stokes_basis(forward), [-1, 0, 0])

    # Case 1)
    assert dr.allclose(M @ linear_pos, circular_left, atol=1e-3)
    # Case 2)
    assert dr.allclose(M @ linear_neg, circular_right, atol=1e-3)
    # Case 3)
    assert dr.allclose(M @ circular_right, linear_pos, atol=1e-3)
    # Case 4)
    assert dr.allclose(M @ circular_left, linear_neg , atol=1e-3)


def test05_sample_half_wave_world(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    # Test polarized implementation. Special case of delta = 180˚, also known
    # as a half-wave plate. (In world coordinates.)
    #
    # Following "Polarized Light - Fundamentals and Applications" by Edward Collett
    # Chapter 5.3:
    #
    # Case 1 & 2) Switch between diagonal linear polarization states (-45˚ & + 45˚)
    # Case 3 & 4) Switch circular polarization direction

    linear_pos     = spectrum_from_stokes([1, 0, +1, 0])
    linear_neg     = spectrum_from_stokes([1, 0, -1, 0])
    circular_right = spectrum_from_stokes([1, 0, 0, +1])
    circular_left  = spectrum_from_stokes([1, 0, 0, -1])

    # Incident direction
    forward = [0, -1, 0]

    ctx = mi.BSDFContext()
    ctx.mode = mi.TransportMode.Importance
    ray = mi.Ray3f([0, 100, 0], forward, 0.0, mi.Color0f())

    # Build scene with given polarizer rotation angle
    scene_str = """<scene version='2.0.0'>
                       <shape type="rectangle">
                           <bsdf type="retarder">
                               <spectrum name="delta" value="180"/>
                           </bsdf>

                           <transform name="to_world">
                               <rotate x="1" y="0" z="0" angle="-90"/>  <!-- Rotate s.t. it is no longer aligned with local coordinates -->
                           </transform>
                       </shape>
                   </scene>"""
    scene = mi.load_string(scene_str)

    # Intersect ray against geometry
    si = scene.ray_intersect(ray)

    bs, M_local = si.bsdf().sample(ctx, si, 0.0, [0.0, 0.0])
    M_world = si.to_world_mueller(M_local, -si.wi, bs.wo)

    # Make sure we are measuring w.r.t. to the optical table
    M = mi.mueller.rotate_mueller_basis_collinear(M_world,
                                       forward,
                                       mi.mueller.stokes_basis(forward), [-1, 0, 0])

    # Case 1)
    assert dr.allclose(M @ linear_pos, linear_neg, atol=1e-3)
    # Case 2)
    assert dr.allclose(M @ linear_neg, linear_pos , atol=1e-3)
    # Case 3)
    assert dr.allclose(M @ circular_right, circular_left, atol=1e-3)
    # Case 4)
    assert dr.allclose(M @ circular_left, circular_right , atol=1e-3)


def test06_path_tracer_quarter_wave(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    # Another test involving a quarter-wave plate, this time inside an actual
    # polarized path tracer.
    # (Serves also as test case for the polarized path tracer itself.)
    #
    # Following  "Polarized Light, Third Edition" by Dennis H. Goldstein,
    # Chapter 6.3 eq. (6.46) & (6.47).
    # (Note that the fast and slow axis of retarders were flipped in the first
    # edition by Edward Collett.)
    #
    # Case 1) Linearly polarized +45˚ light (Stokes vector [1, 0, 1, 0]) yields
    #         left circularly polarized light (Stokes vector [1, 0, 0, -1]).
    # Case 2) Linearly polarized -45˚ light (Stokes vector [1, 0, -1, 0]) yields
    #         right circularly polarized light (Stokes vector [1, 0, 0, 1]).
    #
    # In this test, a polarizer and quarter-wave plate are placed between light
    # source and camera.
    # The polarizer is used to convert emitted light into a +-45˚ linearly
    # polarized state. The subsequent retarder will convert all diagonal into
    # circular polarization.

    circular_right = spectrum_from_stokes([1, 0, 0, +1])
    circular_left  = spectrum_from_stokes([1, 0, 0, -1])

    angles = [+45.0, -45.0]
    expected = [circular_left, circular_right]
    observed = []

    for angle in angles:
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
                                   <rgb name="radiance" value="1"/>
                               </emitter>
                           </shape>

                           <!-- Polarizer at given angle -->
                           <shape type="rectangle">
                               <bsdf type="polarizer">
                               </bsdf>

                               <transform name="to_world">
                                   <rotate x="1" y="0" z="0" angle="-90"/>
                                   <rotate y="1" angle="{}"/>   <!-- Rotate around optical axis -->
                                   <translate y="-5"/>
                               </transform>
                           </shape>

                           <!-- Quarter-wave plate. -->
                           <shape type="rectangle">
                               <bsdf type="retarder">
                                   <spectrum name="delta" value="90"/>
                               </bsdf>

                                <transform name="to_world">
                                   <rotate x="1" y="0" z="0" angle="-90"/>
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

        # Normalize Stokes vector
        value = value * dr.rcp(value[0,0][0])

        # Align output stokes vector (based on ray.d) with optical table. (In this configuration, this is a no-op.)
        forward = -ray.d
        basis_cur = mi.mueller.stokes_basis(forward)
        basis_tar = [-1, 0, 0]

        R = mi.mueller.rotate_stokes_basis_m(forward, basis_cur, basis_tar)
        value = R @ value

        observed.append(value)

    # Make sure observations match expectations
    for k in range(len(expected)):
        assert dr.allclose(observed[k], expected[k], atol=1e-3)


def test07_path_tracer_half_wave(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    # Another test involving a half-wave plate, this time inside an actual
    # polarized path tracer.
    # (Serves also as test case for the polarized path tracer itself.)
    #
    # Following "Polarized Light - Fundamentals and Applications" by Edward Collett
    # Chapter 5.3:
    #
    # Case 1 & 2) Switch between diagonal linear polarization states (-45˚ & + 45˚)
    #
    # In this test, a polarizer and half-wave plate are placed between light
    # source and camera.
    # The polarizer is used to convert emitted light into a +-45˚ linearly
    # polarized state. The subsequent retarder will flip the polarization
    # axis to the opposite one.

    linear_pos = spectrum_from_stokes([1, 0, +1, 0])
    linear_neg = spectrum_from_stokes([1, 0, -1, 0])

    angles = [+45.0, -45.0]
    expected = [linear_neg, linear_pos]
    observed = []

    for angle in angles:
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
                                   <rgb name="radiance" value="1"/>
                               </emitter>
                           </shape>

                           <!-- Polarizer at given angle -->
                           <shape type="rectangle">
                               <bsdf type="polarizer">
                               </bsdf>

                               <transform name="to_world">
                                   <rotate x="1" y="0" z="0" angle="-90"/>
                                   <rotate y="1" angle="{}"/>   <!-- Rotate around optical axis -->
                                   <translate y="-5"/>
                               </transform>
                           </shape>

                           <!-- Half-wave plate. -->
                           <shape type="rectangle">
                               <bsdf type="retarder">
                                   <spectrum name="delta" value="180"/>
                               </bsdf>

                                <transform name="to_world">
                                   <rotate x="1" y="0" z="0" angle="-90"/>
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

        # Normalize Stokes vector
        value = value * dr.rcp(value[0,0][0])

        # Align output stokes vector (based on ray.d) with optical table. (In this configuration, this is a no-op.)
        forward = -ray.d
        basis_cur = mi.mueller.stokes_basis(forward)
        basis_tar = [-1, 0, 0]

        R = mi.mueller.rotate_stokes_basis_m(forward, basis_cur, basis_tar)
        value = R @ value

        observed.append(value)

    # Make sure observations match expectations
    for k in range(len(expected)):
        assert dr.allclose(observed[k], expected[k], atol=1e-3)


def test08_rotated_quarter_wave(variant_scalar_mono_polarized):

    def spectrum_from_stokes(v):
        res = mi.Spectrum(0.0)
        for i in range(4):
            res[i,0] = v[i]
        return res

    linear_horizontal = spectrum_from_stokes([1, +1, 0, 0])
    linear_vertical   = spectrum_from_stokes([1, -1, 0, 0])
    circular_right    = spectrum_from_stokes([1, 0, 0, +1])
    circular_left     = spectrum_from_stokes([1, 0, 0, -1])

    bsdf = mi.load_dict({
        'type': 'retarder',
        'theta': 45.0,
        'delta': 90.0,
    })

    ctx = mi.BSDFContext()
    ctx.mode = mi.TransportMode.Importance
    si = mi.SurfaceInteraction3f()
    si.p = [0, 0, 0]
    si.wi = [0, 0, 1]
    n = [0, 0, 1]
    si.n = n
    si.sh_frame = mi.Frame3f(si.n)

    _, M = bsdf.sample(ctx, si, 0.0, [0.0, 0.0])
    M_null = bsdf.eval_null_transmission(si)
    assert dr.allclose(M, M_null, atol=1e-3)

    assert dr.allclose(M @ linear_horizontal, circular_right, atol=1e-3)
    assert dr.allclose(M @ linear_vertical, circular_left, atol=1e-3)
    assert dr.allclose(M @ circular_right, linear_vertical, atol=1e-3)
    assert dr.allclose(M @ circular_left, linear_horizontal, atol=1e-3)

    # Reverse incident direction.
    si.wi = [0, 0, -1]

    _, M = bsdf.sample(ctx, si, 0.0, [0.0, 0.0])
    M_null = bsdf.eval_null_transmission(si)
    assert dr.allclose(M, M_null, atol=1e-3)

    assert dr.allclose(M @ linear_horizontal, circular_left, atol=1e-3)
    assert dr.allclose(M @ linear_vertical, circular_right, atol=1e-3)
    assert dr.allclose(M @ circular_right, linear_horizontal, atol=1e-3)
    assert dr.allclose(M @ circular_left, linear_vertical, atol=1e-3)
