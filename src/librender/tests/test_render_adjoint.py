# import enoki as ek
# import mitsuba
# import pytest

# @pytest.mark.slow
# def test_adjoint_render():
#     from mitsuba.core import xml, Float
#     from mitsuba.python.util import traverse, write_bitmap

#     config = ... # TODO
# ​
#     # Set enoki JIT flags
#     # for k, v in jit_flags.items():
#     #     ek.set_flag(k, v)
# ​
#     spp_primal = 4096 * 256
#     spp_adjoint = 4096 * 256
#     max_depth = 4
#     fd_epsilon = 1e-2

#     #TODO There should not be a need to hardcode this
#     ek.set_flag(ek.JitFlag.VCallRecord,   True)
#     ek.set_flag(ek.JitFlag.VCallOptimize, True)
# ​
#     # Load the scene
#     scene = xml.load_xml(config.scene)
#     sensor = scene.sensors()[0]
#     sampler = sensor.sampler()
#     film_size = sensor.film().crop_size()

#     # Get the parameter to differentiate
#     params = traverse(scene)
#     params.keep([key])
#     assert len(params) > 0

#     # Create integrator
#     integrator = xml.load_dict(config.integrators[0])
# ​
#     # Rendering reference image
#     sampler.seed(0, spp_primal * ek.hprod(film_size))
#     image_1 = integrator.render(scene, spp=spp_primal)
#     # write_bitmap('image_1.exr', image_1, film_size)

#     # Backpropagate uniform gradients through the scene
#     image_adj = ek.full(Float, 1.0, len(image_1))
#     ek.enable_grad(params[key])
#     integrator.render_backward(scene, params, image_adj, spp=spp_adjoint)
#     grad_backward = ek.grad(params[key])
# ​
#     # Update scene parameter for FD
#     ek.disable_grad(params[key])
#     params[key] += fd_epsilon
#     params.update()
# ​
#     # Render primal image
#     sampler.seed(0)
#     image_2 = integrator.render(scene, spp=spp_primal)
#     # write_bitmap('image_2.exr', image_2, film_size)
# ​
#     # Finite difference on 1 by 1 rendered image
#     grad_fd = (image_2 - image_1) / fd_epsilon
#     if len(grad_backward) == 1:
#         grad_fd = ek.hsum(grad_fd)
# ​
#     grad_backward = ek.ravel(grad_backward)
# ​
#     print(f'grad_fd:  {grad_fd}')
#     print(f'grad_backward: {grad_backward}')
#     print(f"key: {key}")
#     assert ek.allclose(grad_fd, grad_backward, rtol=0.05, atol=1e-5)



# forward_spp = 32
# backward_spp = 16

# scene = ...
# sensor = scene.sensors()[0]
# film = sensor.film()
# integrator = scene.integrator()

# forward_sampler = scene.sensors()[0].sampler()
# backward_sampler = forward_sampler.fork()

# forward_sampler.set_sample_count(forward_spp)
# forward_sampler.seed(1, ek.hprod(film.crop_size()) * forward_spp)

# forward_sampler.set_sample_count(backward_spp)
# forward_sampler.seed(2, ek.hprod(film.crop_size()) * backward_spp)

# for it in range(loop_it_count):
#     img = integrator.render(scene, forward_sampler)
#     ...
#     integrator.render_backward(scene, backward_sampler)
#     ...