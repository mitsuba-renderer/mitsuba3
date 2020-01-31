
class Adam(Optimizer):
    def __init__(self, params, lr, beta_1=0.9, beta_2=0.999, epsilon=1e-8):
        """
        Implements the optimization technique presented in

        "Adam: A Method for Stochastic Optimization"
        Diederik P. Kingma and Jimmy Lei Ba
        ICLR 2015

        The method has the following parameters:

        ``lr``: learning rate

        ``beta_1``: controls the exponential averaging of first
        order gradient moments

        ``beta_2``: controls the exponential averaging of second
        order gradient moments
        """
        super(Adam, self).__init__(params, lr)
        assert beta_1 >= 0 and beta_1 < 1
        assert beta_2 >= 0 and beta_2 < 1
        self.beta_1 = beta_1
        self.beta_2 = beta_2
        self.epsilon = epsilon

        self.beta_1_t = FloatC(1, literal=False)
        self.beta_2_t = FloatC(1, literal=False)


    def step(self):
        self.beta_1_t *= self.beta_1
        self.beta_2_t *= self.beta_2
        lr_t = self.lr * sqrt(1 - self.beta_2_t) / (1 - self.beta_1_t)

        gradients = self.compute_gradients()
        for k, p in self.params.items():
            if slices(p) != slices(self.state[k][0]):
                # Reset state if data size has changed
                self.reset(k)

            if k not in gradients:
                continue
            g_t = gradients[k]

            m_tp, v_tp = self.state[k]
            m_t = self.beta_1 * m_tp + (1 - self.beta_1) * g_t
            v_t = self.beta_2 * v_tp + (1 - self.beta_2) * sqr(g_t)
            self.state[k] = (m_t, v_t)
            u = detach(p) - lr_t * m_t / (sqrt(v_t) + self.epsilon)
            u = type(p)(u)
            set_requires_gradient(u)
            self.params[k] = u

    def __repr__(self):
        return ('Adam[\n  lr = {:.2g},\n  beta_1 = {:.2g},\n  beta_2 = {:.2g},\n'
                '  params = {}\n]').format(self.lr.numpy().squeeze(), self.beta_1, self.beta_2)



def render_torch(scene, params=None, **kwargs):
    # Delayed import of PyTorch dependency
    ns = globals()
    if 'render_torch_helper' in ns:
        render_torch = ns['render_torch_helper']
    else:
        import torch

        class Render(torch.autograd.Function):
            @staticmethod
            def forward(ctx, scene, params, *args):
                assert len(args) % 2 == 0
                args = dict(zip(args[0::2], args[1::2]))

                pixel_format = Bitmap.EY
                spp = None

                ctx.inputs = [None, None]
                for k, v in args.items():
                    if k == 'spp':
                        spp = v
                    elif k == 'pixel_format':
                        pixel_format = v
                    else:
                        value = type(params[k])(v)
                        set_requires_gradient(value, v.requires_grad)
                        params[k] = value
                        ctx.inputs.append(None)
                        ctx.inputs.append(value)
                        continue
                    ctx.inputs.append(None)
                    ctx.inputs.append(None)

                ctx.output = render(scene,
                                    spp=spp,
                                    pixel_format=pixel_format)
                result = ctx.output.torch()
                cuda_malloc_trim()
                return result

            @staticmethod
            def backward(ctx, grad_output):
                if type(ctx.output) is FloatD:
                    set_gradient(ctx.output, FloatD(grad_output))
                elif type(ctx.output) is Vector3fD:
                    set_gradient(ctx.output, Vector3fD(grad_output))
                else:
                    raise Exception("Unsupported output type!")

                FloatD.backward()
                result = tuple(gradient(i).torch() if i is not None else None
                               for i in ctx.inputs)
                del ctx.output
                del ctx.inputs
                cuda_malloc_trim()
                return result

        render_torch = Render.apply
        ns['render_torch_helper'] = render_torch

    result = render_torch(scene, params,
        *[num for elem in kwargs.items() for num in elem])
    film_size = scene.film().size()

    return result.reshape(film_size[1], film_size[0], -1).squeeze()
