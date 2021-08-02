from contextlib import contextmanager
from collections import defaultdict
import enoki as ek

class Optimizer:
    """
    Base class of all gradient-based optimizers (currently SGD and Adam)
    """
    def __init__(self, lr, params):
        """
        Parameter ``lr``:
            learning rate

        Parameter ``params`` (:py:class:`mitsuba.python.util.SceneParameters`):
            Scene parameters dictionary
        """
        self.lr = defaultdict(lambda: self.lr_default)
        self.lr_v = defaultdict(lambda: self.lr_default_v)

        self.set_learning_rate(lr)
        self.variables = {}
        self.params = params
        self.state = {}

    def __contains__(self, key: str):
        return self.variables.__contains__(key)

    def __getitem__(self, key: str):
        return self.variables[key]

    def __setitem__(self, key: str, value):
        """
        Overwrite the value of a parameter.

        This method can also be used to load a scene parameters to optimize.

        Note that the state of the optimizer (e.g. momentum) associated with the
        parameter is preserved, unless the parameter's dimensions changed. When
        assigning a substantially different parameter value (e.g. as part of a
        different optimization run), the previous momentum value is meaningless
        and calling `reset()` is advisable.
        """
        if not (ek.is_diff_array_v(value) and ek.is_floating_point_v(value)):
            raise Exception('Optimizer.__setitem__(): value should be differentiable!')
        needs_reset = (key not in self.variables) or ek.width(self.variables[key]) != ek.width(value)

        self.variables[key] = type(value)(ek.detach(value))
        ek.enable_grad(self.variables[key])
        if needs_reset:
            self.reset(key)

    def __delitem__(self, key: str) -> None:
        del self.variables[key]

    def __len__(self) -> int:
        return len(self.variables)

    def keys(self):
        return self.variables.keys()

    def items(self):
        class OptimizerItemIterator:
            def __init__(self, params):
                self.params = params
                self.it = params.keys().__iter__()

            def __iter__(self):
                return self

            def __next__(self):
                key = next(self.it)
                return (key, self.params[key])

        return OptimizerItemIterator(self.variables)

    def __match(regexps, params):
        """Return subset of keys matching any regex from a list of regex"""
        if params is None:
            return []

        if regexps is None:
            return params.keys()

        if type(regexps) is not list:
            regexps = [regexps]

        import re
        regexps = [re.compile(k).match for k in regexps]
        return [k for k in params.keys() if any (r(k) for r in regexps)]

    def load(self, keys=None):
        """
        Load a set of scene parameters to optimize.

        Parameter ``keys`` (``None``, ``str``, ``[str]``):
            Specifies which parameters should be loaded. Regex are supported to define
            a subset of parameters at once. If set to ``None``, all differentiable
            scene parameters will be loaded.
        """

        if self.params is None:
            raise Exception('Optimizer.load(): scene parameters dictionary should be'
                            'passed to the constructor!')

        for k in Optimizer.__match(keys, self.params):
            value = self.params[k]
            if ek.is_diff_array_v(value) and ek.is_floating_point_v(value):
                self[k] = value

    def update(self, keys=None):
        """
        Update a set of scene parameters.

        Parameter ``params`` (:py:class:`mitsuba.python.util.SceneParameters`):
            Scene parameters dictionary

        Parameter ``keys`` (``None``, ``str``, ``[str]``):
            Specifies which parameters should be updated. Regex are supported to update
            a subset of parameters at once. If set to ``None``, all scene parameters will
            be update.
        """

        if self.params is None:
            raise Exception('Optimizer.update(): scene parameters dictionary should be '
                            'passed to the constructor!')

        for k in Optimizer.__match(keys, self.variables):
            if k in self.params:
                self.params[k] = self.variables[k]

        ek.schedule(self.params)
        self.params.update()
        ek.eval()

    def set_learning_rate(self, lr, key=None):
        """Set the learning rate.

        Parameter ``lr``:
            The new learning rate

        Parameter ``key`` (``None``, ``str``):
            Optional parameter to specify for which parameter to set the learning rate.
            If set to ``None``, the default learning rate is updated.
        """

        from mitsuba.core import Float
        # Ensure that the JIT compiler does merge 'lr' into the PTX code
        # (this would trigger a recompile every time it is changed)

        if key is None:
            self.lr_default = lr
            self.lr_default_v = ek.opaque(ek.detached_t(Float), lr, size=1)
        else:
            self.lr[key] = lr
            self.lr_v[key] = ek.opaque(ek.detached_t(Float), lr, size=1)

    def set_grad_suspended(self, value):
        """Temporarily disable the generation of gradients."""
        self.params.set_grad_suspended(value)

    @contextmanager
    def suspend_gradients(self):
        """Temporarily disable the generation of gradients."""
        self.params.set_grad_suspended(True)
        try:
            yield
        finally:
            self.params.set_grad_suspended(False)

    @contextmanager
    def resume_gradients(self):
        """Temporarily enable the generation of gradients"""
        self.params.set_grad_suspended(False)
        try:
            yield
        finally:
            self.params.set_grad_suspended(True)

    def reset(self, key):
        """Resets the internal state associated with a parameter, if any (e.g. momentum)."""
        pass

class SGD(Optimizer):
    """
    Implements basic stochastic gradient descent with a fixed learning rate
    and, optionally, momentum :cite:`Sutskever2013Importance` (0.9 is a typical
    parameter value for the ``momentum`` parameter).

    The momentum-based SGD uses the update equation

    .. math::

        v_{i+1} = \\mu \\cdot v_i +  g_{i+1}

    .. math::
        p_{i+1} = p_i + \\varepsilon \\cdot v_{i+1},

    where :math:`v` is the velocity, :math:`p` are the positions,
    :math:`\\varepsilon` is the learning rate, and :math:`\\mu` is
    the momentum parameter.
    """
    def __init__(self, lr, momentum=0, params=None):
        """
        Parameter ``lr``:
            learning rate

        Parameter ``momentum``:
            momentum factor
        """
        assert momentum >= 0 and momentum < 1
        assert lr > 0
        self.momentum = momentum
        super().__init__(lr, params)

    def step(self):
        """Take a gradient step"""
        for k, p in self.variables.items():
            g_p = ek.grad(p)
            size = ek.width(g_p)
            if size == 0:
                continue

            if self.momentum != 0:
                if size != ek.width(self.state[k]):
                    # Reset state if data size has changed
                    self.reset(k)

                self.state[k] = self.momentum * self.state[k] + g_p
                value = ek.detach(p) - self.lr_v[k] * self.state[k]
                ek.schedule(self.state[k])
            else:
                value = ek.detach(p) - self.lr_v[k] * g_p


            value = type(p)(value)
            ek.enable_grad(value)
            self.variables[k] = value
            ek.schedule(self.variables[k])

        ek.eval()

    def reset(self, key):
        """Zero-initializes the internal state associated with a parameter"""
        if self.momentum == 0:
            return
        p = self.variables[key]
        size = ek.width(p)
        self.state[key] = ek.zero(ek.detached_t(p), size)

    def __repr__(self):
        return ('SGD[\n  params = %s,\n  lr = %s,\n  momentum = %.2g\n]') % \
            (list(self.keys()), dict(self.lr, default=self.lr_default), self.momentum)


class Adam(Optimizer):
    """
    Implements the Adam optimizer presented in the paper *Adam: A Method for
    Stochastic Optimization* by Kingman and Ba, ICLR 2015.
    """
    def __init__(self, lr, beta_1=0.9, beta_2=0.999, epsilon=1e-8, params=None):
        """
        Parameter ``lr``:
            learning rate

        Parameter ``beta_1``:
            controls the exponential averaging of first
            order gradient moments

        Parameter ``beta_2``:
            controls the exponential averaging of second
            order gradient moments
        """
        super().__init__(lr, params)

        assert 0 <= beta_1 < 1 and 0 <= beta_2 < 1 \
            and lr > 0 and epsilon > 0

        self.beta_1 = beta_1
        self.beta_2 = beta_2
        self.epsilon = epsilon
        self.t = 0

    def step(self):
        """Take a gradient step"""
        self.t += 1

        from mitsuba.core import Float

        lr_scale = ek.sqrt(1 - self.beta_2 ** self.t) / (1 - self.beta_1 ** self.t)
        lr_scale = ek.opaque(ek.detached_t(Float), lr_scale, size=1)
        for k, p in self.variables.items():
            lr_t = self.lr_v[k] * lr_scale
            g_p = ek.grad(p)
            size = ek.width(g_p)

            if size == 0:
                continue
            elif size != ek.width(self.state[k][0]):
                # Reset state if data size has changed
                self.reset(k)

            m_tp, v_tp = self.state[k]
            m_t = self.beta_1 * m_tp + (1 - self.beta_1) * g_p
            v_t = self.beta_2 * v_tp + (1 - self.beta_2) * ek.sqr(g_p)
            self.state[k] = (m_t, v_t)
            ek.schedule(self.state[k])

            u = ek.detach(p) - lr_t * m_t / (ek.sqrt(v_t) + self.epsilon)
            u = type(p)(u)
            ek.enable_grad(u)
            self.variables[k] = u
            ek.schedule(self.variables[k])

        ek.eval()

    def reset(self, key):
        """Zero-initializes the internal state associated with a parameter"""
        p = self.variables[key]
        size = ek.width(p)
        self.state[key] = (ek.zero(ek.detached_t(p), size),
                           ek.zero(ek.detached_t(p), size))

    def __repr__(self):
        return ('Adam[\n'
                '  params = %s,\n'
                '  lr = %s,\n'
                '  betas = (%g, %g),\n'
                '  eps = %g\n'
                ']' % (list(self.keys()), dict(self.lr, default=self.lr_default),
                       self.beta_1, self.beta_2, self.epsilon))
