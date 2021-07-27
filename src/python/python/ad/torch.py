""" Mitsuba <--> PyTorch interoperability support """

# TODO update using new enoki / enoki-jit
def render_torch(scene, params=None, **kwargs):
    from mitsuba.core import Float
    # Delayed import of PyTorch dependency
    ns = globals()
    if 'render_torch_helper' in ns:
        render_torch = ns['render_torch_helper']
    else:
        import torch

        class Render(torch.autograd.Function):
            @staticmethod
            def forward(ctx, scene, params, *args):
                try:
                    assert len(args) % 2 == 0
                    args = dict(zip(args[0::2], args[1::2]))

                    spp = None
                    sensor_index = 0
                    unbiased = True
                    malloc_trim = False

                    ctx.inputs = [None, None]
                    for k, v in args.items():
                        if k == 'spp':
                            spp = v
                        elif k == 'sensor_index':
                            sensor_index = v
                        elif k == 'unbiased':
                            unbiased = v
                        elif k == 'malloc_trim':
                            malloc_trim = v
                        elif params is not None:
                            params[k] = type(params[k])(v)
                            ctx.inputs.append(None)
                            ctx.inputs.append(params[k] if v.requires_grad
                                              else None)
                            continue

                        ctx.inputs.append(None)
                        ctx.inputs.append(None)

                    if type(spp) is not tuple:
                        spp = (spp, spp)

                    result = None
                    ctx.malloc_trim = malloc_trim

                    if ctx.malloc_trim:
                        torch.cuda.empty_cache()

                    if params is not None:
                        params.update()

                        if unbiased:
                            result = render(scene, seed=0, spp=spp[0],
                                            sensor_index=sensor_index).torch()

                    for v in ctx.inputs:
                        if v is not None:
                            ek.enable_grad(v)

                    ctx.output = render(scene, seed=0, spp=spp[1],
                                        sensor_index=sensor_index)

                    if result is None:
                        result = ctx.output.torch()

                    if ctx.malloc_trim:
                        ek.cuda_malloc_trim()
                    return result
                except Exception as e:
                    print("render_torch(): critical exception during "
                          "forward pass: %s" % str(e))
                    raise e

            @staticmethod
            def backward(ctx, grad_output):
                try:
                    ek.set_grad(ctx.output, ek.detach(Float(grad_output)))
                    ek.backward(ctx.output)
                    result = tuple(ek.grad(i).torch() if i is not None
                                   else None
                                   for i in ctx.inputs)
                    del ctx.output
                    del ctx.inputs
                    if ctx.malloc_trim:
                        ek.cuda_malloc_trim()
                    return result
                except Exception as e:
                    print("render_torch(): critical exception during "
                          "backward pass: %s" % str(e))
                    raise e

        render_torch = Render.apply
        ns['render_torch_helper'] = render_torch

    result = render_torch(scene, params,
                          *[num for elem in kwargs.items() for num in elem])

    sensor_index = 0 if 'sensor_index' not in kwargs \
        else kwargs['sensor_index']
    crop_size = scene.sensors()[sensor_index].film().crop_size()
    return result.reshape(crop_size[1], crop_size[0], -1)
