# -*- coding: utf-8 -*-

from __future__ import division
import numpy as np
import mitsuba
from mitsuba.scalar_rgb.core import float_dtype
import pytest


class ChiSquareTest(object):
    """
    Implements Pearson's chi-square test for goodness of fit of a distribution
    to a known reference distribution.

    The implementation here specifically compares a Monte Carlo sampling
    strategy on a 2D (or lower dimensional) space against a reference
    distribution obtained by numerically integrating a probability density
    function over grid in the distribution's parameter domain.

    Parameters
    ----------

    domain: object
       An implementation of the domain interface (``SphericalDomain``, etc.),
       which transforms between the parameter and target domain of the
       distribution

    sample_func: function
       An importance sampling function which maps an array of uniform variates
       of size ``[sample_dim, sample_count]`` to an array of ``sample_count``
       samples on the target domain.

    pdf_func: function
       Function that is expected to specify the probability density of the
       samples produced by ``sample_func``. The test will try to collect
       sufficient statistical evidence to reject this hypothesis.

    sample_dim: int, optional
       Numer of random dimensions consumed by ``sample_func`` per sample. The
       default value is ``2``.

    sample_count: int, optional
       Total number of samples to be generated. The test will have more
       evidence as this number tends to infinity. The default value is
       ``100000``.

    res: int, optional
       Vertical resolution of the generated histograms. The horizontal
       resolution will be calculated as ``res * domain.aspect()``. The
       default value of ``101`` is intentionally an odd number to prevent
       issues with floating point precision at sharp boundaries that may
       separate the domain into two parts (e.g. top hemisphere of a sphere
       parameterization).

    ires: int, optional
       Number of horizontal/vertical subintervals used to numerically integrate
       the probability density over each histogram cell (using Simpson
       quadrature). The default value is ``4``.

    dtype: np.dtype, optional
       Data type that should be used for generated samples (e.g. float32,
       float64, etc). Uses the default Mitsuba floating point precision chosen
       at compile time.


    Notes
    -----

    The following attributes are part of the public API:

    messages: string
        The implementation may generate a number of messages while running the
        test, which can be retrieved via this attribute.

    histogram: array
        The histogram array is populated by the ``tabulate_histogram()`` method
        and stored in this attribute.

    pdf: array
        The probability density function array is populated by the
        ``tabulate_pdf()`` method and stored in this attribute.

    p_value: float
        The p-value of the test is computed in the ``run()`` method and stored
        in this attribute.
    """
    def __init__(self, domain, sample_func, pdf_func, sample_dim=2,
                 sample_count=1000000, res=101, ires=6,
                 dtype=float_dtype):
        self.domain = domain
        self.sample_func = sample_func
        self.pdf_func = pdf_func
        self.sample_dim = sample_dim
        self.sample_count = sample_count
        if domain.aspect() == 0:
            self.res = [1, res]
        else:
            self.res = [res, res * domain.aspect()]
        self.res = np.array(self.res, dtype=np.int64)
        self.ires = ires
        self.dtype = dtype
        self.bounds = domain.bounds()
        self.pdf = None
        self.histogram = None
        self.p_value = None
        self.messages = ''
        self.fail = False

    def tabulate_histogram(self):
        """
        Invoke the provided sampling strategy many times and generate a
        histogram in the parameter domain. If ``sample_func`` returns a tuple
        ``(positions, weights)`` instead of just poisitions, the samples are
        considered to be weighted.
        """

        try:
            from mitsuba.scalar_rgb.core import PCG32
        except ImportError:
            pytest.skip("scalar_rgb mode not enabled")

        # Generate a table of uniform variates
        samples_in = PCG32().next_float((self.sample_count, self.sample_dim))

        if self.sample_dim == 1:
            samples_in = samples_in.ravel()

        # Invoke sampling strategy
        weights_out = None
        samples_out = self.sample_func(samples_in)

        if type(samples_out) is tuple:
            weights_out = samples_out[1]
            samples_out = samples_out[0]

        # Map samples into the parameter domain
        xy = self.domain.map_backward(samples_out)

        # Compute a histogram of the positions in the parameter domain
        self.histogram = np.histogram2d(
            xy[:, 1], xy[:, 0],
            bins=self.res,
            normed=False,
            weights=weights_out,
            range=[self.bounds[1, :], self.bounds[0, :]]
        )[0] / self.sample_count

        # A few sanity checks
        eps = 1e-4
        r = self.bounds[:, 1] - self.bounds[:, 0]
        oob = (xy >= self.bounds[:, 0] - eps * r) & \
              (xy <= self.bounds[:, 1] + eps * r)
        oob = ~np.all(oob, axis=1)
        if np.any(oob):
            self._log('Encountered samples outside of the specified '
                      'domain: %s' % str(xy[oob][0, :]))
            self.fail = True

        histogram_min = np.min(self.histogram)
        if not (histogram_min >= 0):
            self._log('Encountered a cell with negative sample '
                      'weights: %f' % histogram_min)
            self.fail = True

        histogram_sum = np.sum(self.histogram)
        if histogram_sum > 1.1:
            self._log('Sample weights add up to a value greater '
                      'than 1.0: %f' % histogram_sum)
            self.fail = True

    def tabulate_pdf(self):
        """
        Numerically integrate the provided probability density function over
        each cell to generate an array resembling the histogram computed by
        ``tabulate_histogram()``. The function uses 2D tensor product Simpson
        quadrature over intervals discretized into ``self.ires`` separate
        function evaluations.
        """
        from scipy.integrate import simps

        # Compute a set of nodes where the PDF should be evaluated
        x, y = np.array(
            np.meshgrid(
                self._interval_nodes(self.bounds[0, :], self.res[1]),
                self._interval_nodes(self.bounds[1, :], self.res[0])),
            dtype=self.dtype
        )

        # Map from parameterization onto the PDF domain
        pos = self.domain.map_forward(np.column_stack((x.ravel(), y.ravel())))
        pos = np.array(pos, dtype=self.dtype)

        # Evaluate the PDF at all positions
        pdf = np.float64(self.pdf_func(pos))

        # Perform two steps of composite Simpson quadrature (X & Y axis) over
        # the cell contents
        dx = (self.bounds[:, 1] - self.bounds[:, 0]) / \
             (self.res[:] * (self.ires - 1))
        pdf = pdf.reshape(self.ires * np.prod(self.res), self.ires)
        pdf = simps(pdf, dx=dx[0])
        pdf = pdf.reshape(self.res[0] * self.ires, self.res[1]).T
        pdf = simps(pdf.reshape(np.prod(self.res), self.ires), dx=dx[1])

        # Save table of integrated density values
        self.pdf = pdf.reshape(self.res[1], self.res[0]).T

        # A few sanity checks
        pdf_min = np.min(self.pdf)
        if not (pdf_min >= 0):
            self._log('Encountered a cell with a '
                      'negative PDF value: %f' % pdf_min)
            self.fail = True

        pdf_sum = np.sum(self.pdf)
        if pdf_sum > 1.1:
            self._log('PDF integrates to a value greater '
                      'than 1.0: %f' % pdf_sum)
            self.fail = True

    def run(self, significance_level, test_count=1):
        """
        Run the Chi^2 test

        Parameters
        ----------

        significance_level: float
            Denotes the desired significance level (e.g. 0.01 for a test at the
            1% significance level)

        test_count: int, optional
            Specifies the total number of statistical tests run by the user.
            This value will be used to adjust the provided significance level
            so that the combination of the entire set of tests has the provided
            significance level.

        Returns
        -------

        result: bool
            ``True`` upon success, ``False`` if the null hypothesis was
            rejected.

        """

        from scipy.stats import chi2

        if self.histogram is None:
            self.tabulate_histogram()

        if self.pdf is None:
            self.tabulate_pdf()

        # Sort entries by expected frequency (increasing)
        index     = self.pdf.ravel().argsort()
        histogram = self.histogram.ravel()[index]
        pdf       = self.pdf.ravel()[index]

        # Compute chi^2 statistic and pool low-valued cells
        chi2val, dof, pooled_in, pooled_out = \
            mitsuba.scalar_rgb.core.math.chi2(histogram * self.sample_count,
                                              pdf * self.sample_count, 5)

        if dof < 1:
            self._log('The number of degrees of freedom is too low!')
            self.fail = True

        if np.any((pdf == 0) & (histogram != 0)):
            self._log('Found samples in a cell with expected frequency 0. '
                      'Rejecting the null hypothesis!')
            self.fail = True

        if pooled_in > 0:
            self._log('Pooled %i low-valued cells into %i cells to '
                      'ensure sufficiently high expected cell frequencies'
                      % (pooled_in, pooled_out))
        self._log('Chi^2 statistic = %f (d.o.f = %i)' % (chi2val, dof))

        # Probability of observing a test statistic at least as
        # extreme as the one here assuming that the distributions match
        self.p_value = chi2.sf(chi2val, dof)

        # Apply the Šidák correction term, since we'll be conducting multiple
        # independent hypothesis tests. This accounts for the fact that the
        # probability of a failure increases quickly when several hypothesis
        # tests are run in sequence.
        significance_level = 1.0 - \
            (1.0 - significance_level) ** (1.0 / test_count)

        if self.fail:
            np.save('chi2_pdf.npy', self.pdf)
            np.save('chi2_histogram.npy', self.histogram)
            self._log('Not running the test for reasons listed above. Target '
                    'density and histogram were written to "chi2_pdf.npy" and '
                    '"chi2_histogram.npy\"')
            return False
        elif self.p_value < significance_level \
                or not np.isfinite(self.p_value):
            np.save('chi2_pdf.npy', self.pdf)
            np.save('chi2_histogram.npy', self.histogram)
            self._log('***** Rejected ***** the null hypothesis (p-value = %f,'
                    ' significance level = %f). Target density and histogram'
                    ' were written to "chi2_pdf.npy" and "chi2_histogram.npy".'
                    % (self.p_value, significance_level))
            return False

        self._log('Accepted the null hypothesis (p-value = %f, '
                  'significance level = %f)' %
                  (self.p_value, significance_level))

        return True

    # ------ Private implementation details ------

    def _log(self, msg):
        self.messages += msg + '\n'

    def _interval_nodes(self, bounds, res):
        """
        Generate nodes for a quadrature rule that computes integrals over
        ``res`` equal-sized intervals convering the range ``[bounds[0],
        bounds[1]]``. Each integral uses ``ires`` evaluations.
        """
        c0 = np.linspace(bounds[0], bounds[1], res,
                         endpoint=False, dtype=self.dtype)
        c1 = np.linspace(0, (bounds[1] - bounds[0]) / res, self.ires,
                         dtype=self.dtype)
        return np.repeat(c0, self.ires) + np.tile(c1, res)


class LineDomain(object):
    """
    The identity map on the line.
    """
    def __init__(self, bounds=np.array([-1.0, 1.0])):
        self._bounds = np.vstack((bounds, [-0.5, 0.5]))

    def bounds(self):
        return self._bounds

    def aspect(self):
        return 0

    def map_forward(self, p):
        return p[:, 0]

    def map_backward(self, p):
        return np.column_stack((p, np.zeros(p.shape[0])))


class PlanarDomain(object):
    """
    The identity map on the plane.
    """
    def __init__(self, bounds=np.array([[-1.0, 1.0],
                                        [-1.0, 1.0]])):
        self._bounds = bounds

    def bounds(self):
        return self._bounds

    def aspect(self):
        size = self._bounds[:, 0] - self._bounds[:, 1]
        return size[0] / size[1]

    def map_forward(self, p):
        return p

    def map_backward(self, p):
        return p


class SphericalDomain(object):
    """
    Maps between the unit sphere and a parameterization based on azimuth & the
    z coordinate.
    """
    def bounds(self):
        return np.array(
            [[-np.pi, np.pi], [-1, 1]])

    def aspect(self):
        return 2

    def map_forward(self, p):
        cos_theta = -p[:, 1]
        sin_theta = np.sqrt(np.maximum(0, 1 - cos_theta * cos_theta))
        sin_phi, cos_phi = np.sin(p[:, 0]), np.cos(p[:, 0])
        return np.column_stack(
            (cos_phi * sin_theta,
             sin_phi * sin_theta,
             cos_theta)
        )

    def map_backward(self, p):
        return np.column_stack((np.arctan2(p[:, 1], p[:, 0]), -p[:, 2]))


def SpectrumAdapter(value):
    """
    Adapter which permits testing 1D spectral power distributions using the
    Chi^2 test.
    """

    try:
        from mitsuba.packet_spectral.core.xml import load_string
        from mitsuba.packet_spectral.core import sample_shifted
        from mitsuba.packet_spectral.render import SurfaceInteraction3f as SurfaceInteraction3fX
    except ImportError:
        pytest.skip("packet_spectral mode not enabled")

    def instantiate(args):
        if hasattr(value, 'sample'):
            return value
        else:
            obj = load_string(value % args)
            expanded = obj.expand()
            if len(expanded) == 1:
                return expanded[0]
            else:
                return obj

    def sample_functor(sample, *args):
        plugin = instantiate(args)
        si = SurfaceInteraction3fX(len(sample))
        wavelength, weight = plugin.sample(si, sample_shifted(sample))
        return wavelength[:, 0]

    def pdf_functor(w, *args):
        plugin = instantiate(args)
        si = SurfaceInteraction3fX(len(w))
        si.wavelengths = np.repeat(w, 4).reshape(len(w), 4)
        return plugin.pdf(si)[:, 0]

    return sample_functor, pdf_functor


def MicrofacetAdapter(md_type, alpha, sample_visible=False):
    """
    Adapter for testing microfacet distribution sampling techniques
    (separately from BSDF models, which are also tested)
    """

    try:
        from mitsuba.packet_rgb.render import MicrofacetDistribution
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    def instantiate(args):
        wi = [0, 0, 1]
        if len(args) == 1:
            angle = args[0] * np.pi / 180
            wi = [np.sin(angle), 0, np.cos(angle)]
        return MicrofacetDistribution(md_type, alpha, sample_visible), wi

    def sample_functor(sample, *args):
        dist, wi = instantiate(args)
        s_value, _ = dist.sample(np.tile(wi, (len(sample), 1)), sample)
        return s_value

    def pdf_functor(m, *args):
        dist, wi = instantiate(args)
        return dist.pdf(np.tile(wi, (len(m), 1)), m)

    return sample_functor, pdf_functor


def BSDFAdapter(bsdf_type, extra, wi=[0, 0, 1]):
    """
    Adapter which permits testing BSDF distributions using the Chi^2 test.

    Parameters
    ----------
    bsdf_type: string
        Name of the BSDF plugin to instantiate.
    extra: string
        Additional XML used to specify the BSDF's parameters.
    wi: array(3,)
        Incoming direction, in local coordinates.
    """

    try:
        from mitsuba.packet_rgb.render import BSDFContext, SurfaceInteraction3f as SurfaceInteraction3fX
        from mitsuba.packet_rgb.core.xml import load_string
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    def make_context(n):
        si = SurfaceInteraction3fX(n)
        si.wi = np.tile(wi, (n, 1))
        si.wavelengths = np.full((n, 3),
                                 532, dtype=float_dtype)
        return (si, BSDFContext())

    def instantiate(args):
        xml = """<bsdf version="2.0.0" type="%s">
            %s
        </bsdf>""" % (bsdf_type, extra)
        return load_string(xml % args)

    def sample_functor(sample, *args):
        n = sample.shape[0]
        plugin = instantiate(args)
        (si, ctx) = make_context(n)
        bs, weight = plugin.sample(ctx, si, sample[:, 0], sample[:, 1:])

        w = np.ones(weight.shape[0])
        w[np.mean(weight, axis=1) == 0] = 0
        return bs.wo, w

    def pdf_functor(wo, *args):
        n = wo.shape[0]
        plugin = instantiate(args)
        (si, ctx) = make_context(n)

        return plugin.pdf(ctx, si, wo)

    return sample_functor, pdf_functor


def InteractiveBSDFAdapter(bsdf_type, extra):
    """
    Adapter for interactive & batch testing of BSDFs using the Chi^2 test
    """
    try:
        from mitsuba.packet_rgb.render import BSDFContext, SurfaceInteraction3f as SurfaceInteraction3fX
        from mitsuba.packet_rgb.core.xml import load_string
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    def make_context(n, theta, phi):
        theta *= np.pi / 180
        phi *= np.pi / 180
        sin_phi, cos_phi = np.sin(phi), np.cos(phi)
        sin_theta, cos_theta = np.sin(theta), np.cos(theta)
        wi = np.array([cos_phi * sin_theta, sin_phi * sin_theta,
                       cos_theta], dtype=float_dtype)
        si = SurfaceInteraction3fX(n)
        si.wi = np.tile(wi, (n, 1))
        si.wavelengths = np.full((n, 3),
                                 532, dtype=float_dtype)
        return (si, BSDFContext())

    cache = [None, None]

    def instantiate(args):
        xml = ("""<bsdf version="2.0.0" type="%s">
            %s
        </bsdf>""" % (bsdf_type, extra)) % args
        if xml != cache[0]:
            cache[1] = load_string(xml)
            cache[0] = xml
        return cache[1]

    def sample_functor(sample, *args):
        plugin = instantiate(args[2:])
        (si, ctx) = make_context(sample.shape[0], args[0], args[1])

        if sample.ndim == 3:
            bs, weight = plugin.sample(ctx, si, sample[:, 0], sample[:, 1:])
        else:
            bs, weight = plugin.sample(ctx, si, 0, sample)

        w = np.ones(weight.shape[0])
        w[np.mean(weight, axis=1) == 0] = 0
        return bs.wo, w

    def pdf_functor(wo, *args):
        plugin = instantiate(args[2:])
        (si, ctx) = make_context(wo.shape[0], args[0], args[1])
        return plugin.pdf(ctx, si, wo)

    return sample_functor, pdf_functor


def EnvironmentAdapter(emitter_type, extra):
    """
    Adapter for interactive & batch testing of environment map samplers
    """
    try:
        from mitsuba.packet_rgb.render import Interaction3f as Interaction3fX
        from mitsuba.packet_rgb.render import DirectionSample3f as DirectionSample3fX
        from mitsuba.packet_rgb.core.xml import load_string
    except ImportError:
        pytest.skip("packet_rgb mode not enabled")

    cache = [None, None]

    def instantiate(args):
        xml = ("""<emitter version="2.0.0" type="%s">
            %s
        </emitter>""" % (emitter_type, extra)) % args
        if xml != cache[0]:
            cache[1] = load_string(xml)
            cache[0] = xml
        return cache[1]

    def sample_functor(sample, *args):
        plugin = instantiate(args)
        n = sample.shape[0]
        it = Interaction3fX(n)
        ds, weight = plugin.sample_direction(it, sample)
        return ds.d

    def pdf_functor(wo, *args):
        plugin = instantiate(args)
        n = wo.shape[0]
        it = Interaction3fX(n)
        ds = DirectionSample3fX(n)
        ds.d = wo
        return plugin.pdf_direction(it, ds)

    return sample_functor, pdf_functor
