# from __future__ import print_function

# import numpy as np
# import pytest

# import mitsuba
# from mitsuba.scalar_rgb.core import MTS_WAVELENGTH_SAMPLES, MTS_WAVELENGTH_MIN, \
#                          MTS_WAVELENGTH_MAX, Properties, BoundingBox3f
# from mitsuba.scalar_rgb.core.xml import load_string
# from mitsuba.scalar_rgb.render import Interaction3f
# from mitsuba.test.util import tmpfile

# TODO enable this

# def test01_constant_construct():
#     t3d = load_string("""
#         <texture3d type="constant3d" version="2.0.0">
#             <transform name="to_world">
#                 <scale x="2" y="0.2" z="1"/>
#             </transform>
#             <spectrum name="color" value="500:3.0 600:3.0"/>
#         </texture3d>
#     """)
#     assert t3d is not None
#     assert t3d.bbox() == BoundingBox3f([0, 0, 0], [2, 0.2, 1])

# def test02_constant_eval():
#     try:
#         import mitsuba.packet_rgb.core.load_string as load_string_packet
#         import mitsuba.packet_rgb.core.Interaction3f as Interaction3fX
#     except ImportError:
#         pytest.skip("packet_rgb mode not enabled")

#     color_xml = """
#         <spectrum name="color" type="blackbody" {}>
#             <float name="temperature" value="3200"/>
#         </spectrum>
#     """
#     color = load_string_packet(color_xml.format('version="2.0.0"'))
#     t3d = load_string_packet("""
#         <texture3d type="constant3d" version="2.0.0">
#             {color}
#             {transform}
#         </texture3d>
#     """.format(color=color_xml.format(''), transform=""))
#     assert np.allclose(t3d.mean(), color.mean())
#     assert t3d.bbox() == BoundingBox3f([0, 0, 0], [1, 1, 1])

#     n_points = 20
#     it = Interaction3fX(n_points)
#     # Locations inside and outside the unit cube
#     it.p = np.concatenate([
#         np.random.uniform(size=(n_points//2, 3)),
#         3.0 + np.random.uniform(size=(n_points//2, 3))
#     ], axis=0)
#     it.wavelengths = np.random.uniform(low=MTS_WAVELENGTH_MIN, high=MTS_WAVELENGTH_MAX,
#                                        size=(n_points, MTS_WAVELENGTH_SAMPLES))
#     active = np.ones(shape=(n_points), dtype=np.bool)
#     results = t3d.eval(it, active)
#     expected = np.concatenate([
#         color.eval(it.wavelengths[:n_points//2, :], active[:n_points//2]),
#         np.zeros(shape=(n_points//2, MTS_WAVELENGTH_SAMPLES))
#     ], axis=0)
#     assert np.allclose(results, expected), "\n{}\nvs\n{}\n".format(results, expected)


# def write_grid_binary_data(filename, side, values):
#     values = np.array(values)
#     with open(filename, 'wb') as f:
#         f.write(b'V')
#         f.write(b'O')
#         f.write(b'L')
#         f.write(np.uint8(3).tobytes()) # Version
#         f.write(np.int32(1).tobytes()) # type
#         f.write(np.int32(side).tobytes()) # size
#         f.write(np.int32(side).tobytes())
#         f.write(np.int32(side).tobytes())
#         f.write(np.int32(1).tobytes()) # channels
#         f.write(np.float32(0.0).tobytes()) # bbox
#         f.write(np.float32(0.0).tobytes())
#         f.write(np.float32(0.0).tobytes())
#         f.write(np.float32(1.0).tobytes())
#         f.write(np.float32(1.0).tobytes())
#         f.write(np.float32(1.0).tobytes())
#         f.write(values.astype(np.float32).tobytes())

# def test03_grid_construct(tmpfile):
#     values = [0, 1, 2, 3, 4, 5, 6, 7]
#     values_str = ", ".join([str(v) for v in values])

#     # --- From string
#     t3d = load_string("""
#         <texture3d type="grid3d" version="2.0.0">
#             <transform name="to_world">
#                 <scale x="2" y="0.2" z="1"/>
#             </transform>
#             <integer name="side" value="2"/>
#             <string name="values" value="{}"/>
#         </texture3d>
#     """.format(values_str))
#     assert t3d is not None

#     with pytest.raises(RuntimeError, match='grid dimensions'):
#         t3d = load_string("""
#             <texture3d type="grid3d" version="2.0.0">
#                 <integer name="nx" value="2"/>
#                 <integer name="ny" value="0"/>
#                 <integer name="nz" value="1"/>
#             </texture3d>
#         """)

#     # --- From file
#     write_grid_binary_data(tmpfile, 2, values)
#     t3d = load_string("""
#         <texture3d type="grid3d" version="2.0.0">
#             <string name="filename" value="{}"/>
#         </texture3d>
#     """.format(tmpfile))
#     assert t3d is not None

#     with pytest.raises(RuntimeError, match='dimensions are already specified'):
#         t3d = load_string("""
#             <texture3d type="grid3d" version="2.0.0">
#                 <integer name="side" value="1"/>
#                 <string name="filename" value="{}"/>
#             </texture3d>
#         """.format(tmpfile))



# def do_grid_test(t3d, side, values):
#     assert t3d is not None
#     assert np.allclose(t3d.mean(), np.mean(values))

#     it = Interaction3f()
#     it.wavelengths = np.random.uniform(
#         low=MTS_WAVELENGTH_MIN, high=MTS_WAVELENGTH_MAX, size=(MTS_WAVELENGTH_SAMPLES))
#     # Row-major order
#     Z, Y, X = np.mgrid[0:side, 0:side, 0:side]
#     corners = np.stack(
#         [X.ravel(), Y.ravel(), Z.ravel()], axis=-1).astype(np.float)

#     # Should get back the exact values at the cell corners
#     for i in range(corners.shape[0]):
#         it.p = corners[i, :]
#         res  = np.mean(t3d.eval(it))
#         print("{} -> {}  vs  {}".format(it.p, res, values[i]))
#         # assert np.allclose(res, values[i]), "{} -> {}".format(it.p, res)
#     # Same for the cell corners
#     for i in range(corners.shape[0]):
#         it.p = corners[i, :]
#         res  = np.mean(t3d.eval(it))
#         assert np.allclose(res, values[i]), "{} -> {}".format(it.p, res)

#     # Check a few interpolated values between cells
#     if side >= 2:
#         it.p = 0.5 * (corners[0, :] + corners[1, :])
#         expected = np.mean([values[0], values[1]])
#         assert np.allclose(np.mean(t3d.eval(it)), expected)
#         it.p = 0.5 * (corners[5, :] + corners[6, :])
#         expected = np.mean([values[5], values[6]])
#         assert np.allclose(np.mean(t3d.eval(it)), expected)
#         # Center: mean value
#         it.p = [0.5, 0.5, 0.5]
#         expected = np.mean(values)
#         assert np.allclose(np.mean(t3d.eval(it)), expected)

# @pytest.mark.parametrize('grid_size', [2])
# def test04_grid_eval(grid_size, tmpfile):
#     values = np.arange(grid_size ** 3).astype(np.float)
#     values_str = ", ".join([str(v) for v in values])
#     t3d = load_string("""
#         <texture3d type="grid3d" version="2.0.0">
#             <integer name="side" value="{}"/>
#             <string name="values" value="{}"/>
#         </texture3d>
#     """.format(grid_size, values_str))
#     do_grid_test(t3d, grid_size, values)

#     # Same data, but read from a binary file
#     write_grid_binary_data(tmpfile, grid_size, values)
#     t3d = load_string("""
#         <texture3d type="grid3d" version="2.0.0">
#             <string name="filename" value="{}"/>
#         </texture3d>
#     """.format(tmpfile))
#     do_grid_test(t3d, grid_size, values)


# def test05_trampoline():
#     class CustomTexture3D(Texture3D):
#         def __init__(self, value_function, props = None):
#             props = props or Properties()
#             super().__init__(props)
#             self.value_function = value_function

#         def eval(self, it, active = True):
#             inside = active & np.all( (it.p >= 0) & (it.p <= 1), axis=0 )
#             val = self.value_function(it.p)
#             return np.where(inside, val, 0.)

#         def mean(self):
#             return 43

#     np.random.seed(1234)
#     def f(p):
#         return np.mean(np.exp(p), axis=-1)
#     t3d = CustomTexture3D(f)

#     it = Interaction3f()
#     it.wavelengths = np.random.uniform(
#         low=MTS_WAVELENGTH_MIN, high=MTS_WAVELENGTH_MAX, size=(MTS_WAVELENGTH_SAMPLES))

#     # Check inherited method
#     assert str(t3d).startswith("Texture3D")
#     # Check overriden methods
#     assert np.allclose(t3d.mean(), 43)
#     points = np.random.uniform(size=(15, 3))
#     for i in range(points.shape[0]):
#         it.p = points[i, :]
#         assert np.allclose(t3d.eval(it), f(points[i, :]))
#     it.p = [0.2, 0.4, 1.5]  # Outside
#     assert np.allclose(t3d.eval(it), 0.)


# def optimize_values(t3d, dp, param_name, loss, batch_size = 5, step_size = 0.1,
#                     max_its = 70):
#     from enoki import set_requires_gradient, backward, gradient, hsum, detach, \
#                       FloatD, Vector3fD, Vector4fD
#     from mitsuba.scalar_rgb.render import Interaction3fD

#     # Prepare queries
#     it = Interaction3fD(batch_size)
#     it.wavelengths = Vector4fD([
#         FloatD(np.random.uniform(size=(batch_size,),
#                                  low=MTS_WAVELENGTH_MIN, high=MTS_WAVELENGTH_MAX))
#         for _ in range(MTS_WAVELENGTH_SAMPLES)
#     ])

#     # Simple gradient descent optimization loop
#     for i in range(max_its):
#         it.p = Vector3fD(np.random.uniform(size=(batch_size, 3)))
#         set_requires_gradient(dp[param_name])
#         observed = t3d.eval(it)
#         l = loss(observed, it)
#         if (i % 25 == 0) or i == max_its - 1:
#             end = '\033[K\r' if max_its > 500 else '\n'
#             print('[{:02d}] Loss: {}, value: {}'.format(i, l, dp[param_name]), end=end)
#         # Gradient step
#         backward(l)
#         g = gradient(dp[param_name])
#         dp[param_name] = detach(dp[param_name] - step_size * g)

#     final_value = dp[param_name]
#     observed = t3d.eval(it)
#     final_loss = loss(observed, it)
#     return final_value, final_loss


# @pytest.mark.skipif(not mitsuba.ENABLE_OPTIX, reason='Autodiff-only test')
# def test06_autodiff_constant3d():
#     from enoki import hsum
#     from mitsuba.render.autodiff import get_differentiable_parameters

#     t3d = load_string("""
#         <texture3d type="constant3d" version="2.0.0">
#             <spectrum name="color" type="uniform">
#                 <float name="value" value="3.0"/>
#             </spectrum>
#         </texture3d>
#     """)
#     dp = get_differentiable_parameters(t3d)

#     param_names = list(dp.keys())
#     assert len(param_names) == 1
#     pname = param_names[0]

#     batch_size = 5
#     target = 13.37
#     def loss(value, *_):
#         diff = target - value
#         return hsum(hsum(diff * diff)) / (batch_size * MTS_WAVELENGTH_SAMPLES)

#     final_value, final_loss = optimize_values(t3d, dp, pname, loss,
#                                               batch_size, step_size=0.1, max_its=60)
#     print('Final value: {} vs target: {} (loss: {})               '.format(
#         final_value, target, final_loss))
#     assert np.allclose(final_loss, 0)
#     assert np.allclose(final_value, target)


# @pytest.mark.skipif(not mitsuba.ENABLE_OPTIX, reason='Autodiff-only test')
# @pytest.mark.parametrize('side', [2])
# def test07_autodiff_grid3d(side):
#     from enoki import FloatD, hsum
#     from mitsuba.scalar_rgb.core import float_dtype
#     from mitsuba.render.autodiff import get_differentiable_parameters

#     size = side ** 3
#     np.random.seed(1234)
#     initial_values = np.random.uniform(size=size).astype(float_dtype)
#     target_values  = np.random.normal(size=size, scale=5.).astype(float_dtype)

#     t3d = load_string("""
#         <texture3d type="grid3d" version="2.0.0">
#             <integer name="side" value="{}"/>
#             <string name="values" value="{}"/>
#         </texture3d>
#     """.format(side, ','.join([str(v) for v in initial_values])))
#     ref = load_string("""
#         <texture3d type="grid3d" version="2.0.0">
#             <integer name="side" value="{}"/>
#             <string name="values" value="{}"/>
#         </texture3d>
#     """.format(side, ','.join([str(v) for v in target_values])))
#     dp = get_differentiable_parameters(t3d)
#     param_names = list(dp.keys())
#     assert len(param_names) == 1
#     pname = param_names[0]

#     # Mean & max should get updated when changing parameter values
#     assert np.allclose(t3d.mean(), np.mean(initial_values))
#     assert np.allclose(t3d.max(), np.max(initial_values))
#     initial_values = np.random.uniform(size=size).astype(float_dtype)
#     dp[pname] = FloatD(initial_values)
#     assert np.allclose(t3d.mean(), np.mean(initial_values))
#     assert np.allclose(t3d.max(), np.max(initial_values))

#     batch_size = 150
#     max_its = 75 if side == 1 else 1250
#     def loss(value, it):
#         expected = ref.eval(it)
#         diff = expected - value
#         return hsum(hsum(diff * diff)) / (batch_size * MTS_WAVELENGTH_SAMPLES)

#     final_values, final_loss = optimize_values(t3d, dp, pname, loss, batch_size,
#                                                step_size=1.0, max_its=max_its)
#     print('\n{}\nvs\n{}'.format(
#         target_values, final_values.numpy().astype(float_dtype)))
#     assert np.allclose(final_loss, 0, atol=1e-8)
#     assert np.allclose(final_values, target_values, atol=1e-4)


# @pytest.mark.skipif(not mitsuba.ENABLE_OPTIX, reason='Autodiff-only test')
# def test08_autodiff_grid3d_multigrid():
#     """Tests the ability of Grid3D to handle data updates with doubled resolution."""
#     from enoki import FloatD, Vector4fD, Vector3fD
#     from mitsuba.scalar_rgb.core import float_dtype
#     from mitsuba.scalar_rgb.render import Interaction3fD
#     from mitsuba.render.autodiff import get_differentiable_parameters

#     initial_side = 2
#     levels = 5
#     np.random.seed(1234)
#     values = np.random.uniform(size=initial_side ** 3).astype(float_dtype)

#     # Loaded once, will update data several times
#     t3d = load_string("""
#         <texture3d type="grid3d" version="2.0.0">
#             <integer name="side" value="{}"/>
#             <string name="values" value="{}"/>
#         </texture3d>
#     """.format(initial_side, ','.join([str(v) for v in values])))

#     # Re-loaded each time with the new data. Should be equivalent.
#     def load_ref(side, values):
#         return load_string("""
#             <texture3d type="grid3d" version="2.0.0">
#                 <integer name="side" value="{}"/>
#                 <string name="values" value="{}"/>
#             </texture3d>
#         """.format(side, ','.join([str(v) for v in values])))

#     def upres(side, values_):
#         from scipy.interpolate import interpn
#         values = values_.copy().reshape((side, side, side))

#         t = np.linspace(0, 1, side)
#         side2 = 2 * side
#         grid = np.mgrid[:side2, :side2, :side2] / (side2 - 1)
#         grid = grid.reshape((3, -1)).T
#         interpolated = interpn((t, t, t), values, grid, method='linear')

#         # Sanity check: corners should match
#         interpolated2 = interpolated.reshape((side2, side2, side2))
#         sub = interpolated2[::side2-1, ::side2-1, ::side2-1]
#         sub_ref = values[::side-1, ::side-1, ::side-1]
#         assert np.allclose(sub, sub_ref), "\n{}\nvs\n{}".format(sub, sub_ref)

#         return interpolated

#     batch_size = 100
#     it = Interaction3fD(batch_size)
#     it.wavelengths = Vector4fD([
#         FloatD(np.random.uniform(size=(batch_size,),
#                                  low=MTS_WAVELENGTH_MIN, high=MTS_WAVELENGTH_MAX))
#         for _ in range(MTS_WAVELENGTH_SAMPLES)
#     ])
#     it.p = Vector3fD(np.random.uniform(size=(batch_size, 3)))
#     def check_equivalence(t3d, ref, values):
#         assert np.allclose(t3d.mean(), np.mean(values))
#         assert np.allclose(t3d.max(), np.max(values))
#         assert np.allclose(t3d.eval(it), ref.eval(it))

#     dp = get_differentiable_parameters(t3d)
#     param_names = list(dp.keys())
#     assert len(param_names) == 1
#     pname = param_names[0]

#     # Replace values with upsampled data from the previous round, check equivalence
#     side = initial_side
#     for i in range(levels):
#         if i > 0:
#             values = upres(side, values)
#             dp[pname] = FloatD(values)
#             side *= 2
#         ref = load_ref(side, values)
#         check_equivalence(t3d, ref, values)
