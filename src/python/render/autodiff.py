from mitsuba.core import Bitmap

from mitsuba.render import DifferentiableParameters, \
    RadianceSample3fD, ImageBlock

from enoki import UInt32D, BoolD, FloatC, FloatD, Vector2fD, Vector3fD, \
    Vector4fD, Matrix4fD, select, eq, rcp, set_requires_gradient, \
    detach, gradient, sqr, sqrt


def indent(s, amount=2):
    return '\n'.join((' ' * amount if i > 0 else '') + l
                     for i, l in enumerate(s.splitlines()))


class SGD:
    def __init__(self, params, lr, momentum=0):
        """
        Implements basic stochastic gradient descent with a fixed learning rate
        and, optionally, momentum (0.9 is a typical parameter value).
        """
        assert momentum >= 0 and momentum < 1
        self.lr = lr
        self.params = params
        self.momentum = momentum
        self.state = {}

        for k, p in params.items():
            set_requires_gradient(p)
            if self.momentum > 0:
                t = type(p)
                if t is FloatD:
                    size = len(p)
                elif t is Vector2fD or t is Vector3fD or t is Vector4fD:
                    size = len(p[0])
                elif t is Matrix4fD:
                    size = len(p[0, 0])
                else:
                    raise "SGD: invalid parameter type " + str(t)
                self.state[k] = detach(t.zero(size))

    def step(self):
        for k, p in self.params.items():
            if self.momentum == 0:
                self.params[k] = detach(p) - self.lr * gradient(p)
            else:
                self.state[k] = self.momentum * self.state[k] + \
                                self.lr * gradient(p)
                self.params[k] = detach(p) - self.state[k]
            set_requires_gradient(p)

    def __repr__(self):
        return ('SGD[\n  lr = %.2g,\n  momentum = %.2g\n]') % (self.lr, self.momentum)


class Adam:
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
        self.lr = lr
        assert beta_1 >= 0 and beta_1 < 1
        assert beta_2 >= 0 and beta_2 < 1
        self.beta_1 = beta_1
        self.beta_2 = beta_2
        self.epsilon = epsilon
        self.params = params
        self.state = {}

        for k, p in params.items():
            set_requires_gradient(p)
            t = type(p)
            if t is FloatD:
                size = len(p)
            elif t is Vector2fD or t is Vector3fD or t is Vector4fD:
                size = len(p[0])
            elif t is Matrix4fD:
                size = len(p[0, 0])
            else:
                raise "Adam: invalid parameter type " + str(t)
            self.state[k] = (detach(t.zero(size)), detach(t.zero(size)))

        self.beta_1_t = FloatC(1)
        self.beta_2_t = FloatC(1)

    def step(self):
        self.beta_1_t *= self.beta_1
        self.beta_2_t *= self.beta_2
        lr_t = self.lr * sqrt(1 - self.beta_2_t) / (1 - self.beta_1_t)

        for k, p in self.params.items():
            g_t = gradient(p)
            m_tp, v_tp = self.state[k]
            m_t = self.beta_1 * m_tp + (1 - self.beta_1) * g_t
            v_t = self.beta_2 * v_tp + (1 - self.beta_2) * sqr(g_t)
            self.state[k] = (m_t, v_t)
            self.params[k] = detach(p) - lr_t * m_t / (sqrt(v_t) + self.epsilon)
            set_requires_gradient(p)

    def __repr__(self):
        return ('Adam[\n  lr = %.2g,\n  beta_1 = %.2g,\n  beta_2 = %.2g,\n'
            '  params = %s\n]') % \
            (self.lr, self.beta_1, self.beta_2, indent(str(self.params)))


def get_differentiable_parameters(scene):
    params = DifferentiableParameters()
    children = set()

    def traverse(path, n):
        if n in children:
            return

        path += type(n).__name__
        if hasattr(n, "id"):
            identifier = n.id()
            if len(identifier) > 0 and '[0x' not in identifier:
                path += '[id=\"%s\"]' % identifier
        path += '/'
        params.set_prefix(path)

        children.add(n)

        if hasattr(n, 'put_parameters'):
            n.put_parameters(params)

        if hasattr(n, 'children'):
            for c in n.children():
                traverse(path, c)

    traverse('/', scene)
    return params


def render(scene, spp=None):
    sensor = scene.sensor()
    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.size()
    if spp is None:
        spp = sampler.sample_count()

    pos = UInt32D.arange(film_size[0] * film_size[1] * spp) / spp

    scale = Vector2fD(1.0 / film_size[0], 1.0 / film_size[1])
    pos = Vector2fD(FloatD(pos % int(film_size[0])),
                    FloatD(pos / int(film_size[0])))

    active = BoolD.full(True, len(pos.x))

    wavelength_sample = sampler.next_1d_d(active)
    position_sample = pos + sampler.next_2d_d(active)

    rays, weights = sensor.sample_ray_differential(
        time=0,
        sample1=wavelength_sample,
        sample2=position_sample * scale,
        sample3=0,
        active=active
    )
    del wavelength_sample

    rs = RadianceSample3fD(scene, sampler)
    result = scene.integrator().eval(rays, rs) * weights
    wavelengths = rays.wavelengths
    del weights, rs, rays

    block = ImageBlock(Bitmap.EXYZAW, film.crop_size(), warn=False,
                       filter=film.reconstruction_filter(), border=False)
    block.put(position_sample, wavelengths, result, 1)
    return block


def render_rgb(scene, spp=None):
    block = render(scene, spp)

    X, Y, Z, A, W = block.bitmap_d()
    W = select(eq(W, 0), 0, rcp(W))
    X *= W; Y *= W; Z *= W

    return Vector3fD(
         3.240479*X + -1.537150*Y + -0.498535*Z,
        -0.969256*X +  1.875991*Y +  0.041556*Z,
         0.055648*X + -0.204043*Y +  1.057311*Z
    )


def render_y(scene, spp=None):
    block = render(scene, spp)
    X, Y, Z, A, W = block.bitmap_d()
    W = select(eq(W, 0), 0, rcp(W))
    return Y * W
