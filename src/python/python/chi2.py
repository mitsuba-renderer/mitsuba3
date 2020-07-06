import mitsuba
import enoki as ek
import time


class ChiSquareTest:
    """
    Implements Pearson's chi-square test for goodness of fit of a distribution
    to a known reference distribution.

    The implementation here specifically compares a Monte Carlo sampling
    strategy on a 2D (or lower dimensional) space against a reference
    distribution obtained by numerically integrating a probability density
    function over grid in the distribution's parameter domain.

    Parameter ``domain`` (object):
       An implementation of the domain interface (``SphericalDomain``, etc.),
       which transforms between the parameter and target domain of the
       distribution

    Parameter ``sample_func`` (function):
       An importance sampling function which maps an array of uniform variates
       of size ``[sample_dim, sample_count]`` to an array of ``sample_count``
       samples on the target domain.

    Parameter ``pdf_func`` (function):
       Function that is expected to specify the probability density of the
       samples produced by ``sample_func``. The test will try to collect
       sufficient statistical evidence to reject this hypothesis.

    Parameter ``sample_dim`` (int):
       Number of random dimensions consumed by ``sample_func`` per sample. The
       default value is ``2``.

    Parameter ``sample_count`` (int):
       Total number of samples to be generated. The test will have more
       evidence as this number tends to infinity. The default value is
       ``1000000``.

    Parameter ``res`` (int):
       Vertical resolution of the generated histograms. The horizontal
       resolution will be calculated as ``res * domain.aspect()``. The
       default value of ``101`` is intentionally an odd number to prevent
       issues with floating point precision at sharp boundaries that may
       separate the domain into two parts (e.g. top hemisphere of a sphere
       parameterization).

    Parameter ``ires`` (int):
       Number of horizontal/vertical subintervals used to numerically integrate
       the probability density over each histogram cell (using the trapezoid
       rule). The default value is ``4``.

    Notes:

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
                 sample_count=1000000, res=101, ires=4):
        from mitsuba.core import ScalarVector2u

        assert res > 0
        assert ires >= 2, "The 'ires' parameter must be >= 2!"

        self.domain = domain
        self.sample_func = sample_func
        self.pdf_func = pdf_func
        self.sample_dim = sample_dim
        self.sample_count = sample_count
        if domain.aspect() is None:
            self.res = ScalarVector2u(res, 1)
        else:
            self.res = ek.max(ScalarVector2u(
                int(res / domain.aspect()), res), 1)
        self.ires = ires
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
        ``(positions, weights)`` instead of just positions, the samples are
        considered to be weighted.
        """

        # Generate a table of uniform variates
        from mitsuba.core import Float, Vector2f, Vector2u, Float32, \
            UInt64, PCG32

        rng = PCG32(initseq=ek.arange(UInt64, self.sample_count))

        samples_in = getattr(mitsuba.core, 'Vector%if' % self.sample_dim)()
        for i in range(self.sample_dim):
            samples_in[i] = rng.next_float32() if Float is Float32 \
                else rng.next_float64()

        self.pdf_start = time.time()

        # Invoke sampling strategy
        samples_out = self.sample_func(samples_in)

        if type(samples_out) is tuple:
            weights_out = samples_out[1]
            samples_out = samples_out[0]
        else:
            weights_out = Float(1.0)

        # Map samples into the parameter domain
        xy = self.domain.map_backward(samples_out)

        # Sanity check
        eps = self.bounds.extents() * 1e-4
        in_domain = ek.all((xy >= self.bounds.min - eps) &
                           (xy <= self.bounds.max + eps))
        if not ek.all(in_domain):
            self._log('Encountered samples outside of the specified '
                      'domain: %s' % str(ek.compress(xy, ~in_domain)))
            self.fail = True

        # Normalize position values
        xy = (xy - self.bounds.min) / self.bounds.extents()
        xy = Vector2u(ek.clamp(xy * Vector2f(self.res), 0,
                               Vector2f(self.res - 1)))

        # Compute a histogram of the positions in the parameter domain
        self.histogram = ek.zero(Float, ek.hprod(self.res))

        ek.scatter_add(
            target=self.histogram,
            index=xy.x + xy.y * self.res.x,
            source=weights_out
        )

        self.pdf_end = time.time()

        histogram_min = ek.hmin(self.histogram)
        if not histogram_min >= 0:
            self._log('Encountered a cell with negative sample '
                      'weights: %f' % histogram_min)
            self.fail = True

        self.histogram_sum = ek.hsum(self.histogram) / self.sample_count
        if self.histogram_sum > 1.1:
            self._log('Sample weights add up to a value greater '
                      'than 1.0: %f' % self.histogram_sum)
            self.fail = True

    def tabulate_pdf(self):
        """
        Numerically integrate the provided probability density function over
        each cell to generate an array resembling the histogram computed by
        ``tabulate_histogram()``. The function uses the trapezoid rule over
        intervals discretized into ``self.ires`` separate function evaluations.
        """

        from mitsuba.core import Float, Vector2f, ScalarVector2f

        extents = self.bounds.extents()
        endpoint = self.bounds.max - extents / ScalarVector2f(self.res)

        # Compute a set of nodes where the PDF should be evaluated
        x, y = ek.meshgrid(
            ek.linspace(Float, self.bounds.min.x, endpoint.x, self.res.x),
            ek.linspace(Float, self.bounds.min.y, endpoint.y, self.res.y)
        )

        endpoint = extents / ScalarVector2f(self.res)
        eps = 1e-4
        nx = ek.linspace(Float, eps, endpoint.x * (1 - eps), self.ires)
        ny = ek.linspace(Float, eps, endpoint.y * (1 - eps), self.ires)
        wx = [1 / (self.ires - 1)] * self.ires
        wy = [1 / (self.ires - 1)] * self.ires
        wx[0] = wx[-1] = wx[0] * .5
        wy[0] = wy[-1] = wy[0] * .5

        integral = 0

        self.histogram_start = time.time()
        for yi, dy in enumerate(ny):
            for xi, dx in enumerate(nx):
                xy = self.domain.map_forward(Vector2f(x + dx, y + dy))
                pdf = self.pdf_func(xy)
                integral = ek.fmadd(pdf, wx[xi] * wy[yi], integral)
        self.histogram_end = time.time()

        self.pdf = integral * (ek.hprod(extents / ScalarVector2f(self.res))
                               * self.sample_count)

        # A few sanity checks
        pdf_min = ek.hmin(self.pdf) / self.sample_count
        if not pdf_min >= 0:
            self._log('Failure: Encountered a cell with a '
                      'negative PDF value: %f' % pdf_min)
            self.fail = True

        self.pdf_sum = ek.hsum(self.pdf) / self.sample_count
        if self.pdf_sum > 1.1:
            self._log('Failure: PDF integrates to a value greater '
                      'than 1.0: %f' % self.pdf_sum)
            self.fail = True

    def run(self, significance_level=0.01, test_count=1, quiet=False):
        """
        Run the Chi^2 test

        Parameter ``significance_level`` (float):
            Denotes the desired significance level (e.g. 0.01 for a test at the
            1% significance level)

        Parameter ``test_count`` (int):
            Specifies the total number of statistical tests run by the user.
            This value will be used to adjust the provided significance level
            so that the combination of the entire set of tests has the provided
            significance level.

        Returns → bool:
            ``True`` upon success, ``False`` if the null hypothesis was
            rejected.

        """

        from mitsuba.core import UInt32, Float64
        from mitsuba.core.math import chi2
        from mitsuba.python.math import rlgamma

        if self.histogram is None:
            self.tabulate_histogram()

        if self.pdf is None:
            self.tabulate_pdf()

        index = UInt32([i[0] for i in sorted(enumerate(self.pdf),
                                             key=lambda x: x[1])])

        # Sort entries by expected frequency (increasing)
        pdf = Float64(ek.gather(self.pdf, index))
        histogram = Float64(ek.gather(self.histogram, index))

        # Compute chi^2 statistic and pool low-valued cells
        chi2val, dof, pooled_in, pooled_out = \
            chi2(histogram, pdf, 5)

        if dof < 1:
            self._log('Failure: The number of degrees of freedom is too low!')
            self.fail = True

        if ek.any(ek.eq(pdf, 0) & ek.neq(histogram, 0)):
            self._log('Failure: Found samples in a cell with expected '
                      'frequency 0. Rejecting the null hypothesis!')
            self.fail = True

        if pooled_in > 0:
            self._log('Pooled %i low-valued cells into %i cells to '
                      'ensure sufficiently high expected cell frequencies'
                      % (pooled_in, pooled_out))

        pdf_time = (self.pdf_end - self.pdf_start) * 1000
        histogram_time = (self.histogram_end - self.histogram_start) * 1000

        self._log('Histogram sum = %f (%.2f ms), PDF sum = %f (%.2f ms)' %
                  (self.histogram_sum, histogram_time, self.pdf_sum, pdf_time))

        self._log('Chi^2 statistic = %f (d.o.f = %i)' % (chi2val, dof))

        # Probability of observing a test statistic at least as
        # extreme as the one here assuming that the distributions match
        self.p_value = 1 - rlgamma(dof / 2, chi2val / 2)

        # Apply the Šidák correction term, since we'll be conducting multiple
        # independent hypothesis tests. This accounts for the fact that the
        # probability of a failure increases quickly when several hypothesis
        # tests are run in sequence.
        significance_level = 1.0 - \
            (1.0 - significance_level) ** (1.0 / test_count)

        if self.fail:
            self._log('Not running the test for reasons listed above. Target '
                      'density and histogram were written to "chi2_data.py')
            result = False
        elif self.p_value < significance_level \
                or not ek.isfinite(self.p_value):
            self._log('***** Rejected ***** the null hypothesis (p-value = %f,'
                      ' significance level = %f). Target density and histogram'
                      ' were written to "chi2_data.py".'
                      % (self.p_value, significance_level))
            result = False
        else:
            self._log('Accepted the null hypothesis (p-value = %f, '
                      'significance level = %f)' %
                      (self.p_value, significance_level))
            result = True
        if not quiet:
            print(self.messages)
            if not result:
                self._dump_tables()
        return result

    def _dump_tables(self):
        with open("chi2_data.py", "w") as f:
            pdf = str([[self.pdf[x + y * self.res.x]
                        for x in range(self.res.x)]
                       for y in range(self.res.y)])
            histogram = str([[self.histogram[x + y * self.res.x]
                              for x in range(self.res.x)]
                             for y in range(self.res.y)])

            f.write("pdf=%s\n" % str(pdf))
            f.write("histogram=%s\n\n" % str(histogram))
            f.write('if __name__ == \'__main__\':\n')
            f.write('    import matplotlib.pyplot as plt\n')
            f.write('    import numpy as np\n\n')
            f.write('    fig, axs = plt.subplots(1,3, figsize=(15, 5))\n')
            f.write('    pdf = np.array(pdf)\n')
            f.write('    histogram = np.array(histogram)\n')
            f.write('    diff=histogram - pdf\n')
            f.write('    absdiff=np.abs(diff).max()\n')
            f.write('    a = pdf.shape[1] / pdf.shape[0]\n')
            f.write('    pdf_plot = axs[0].imshow(pdf, aspect=a,'
                    ' interpolation=\'nearest\')\n')
            f.write('    hist_plot = axs[1].imshow(histogram, aspect=a,'
                    ' interpolation=\'nearest\')\n')
            f.write('    diff_plot = axs[2].imshow(diff, aspect=a, '
                    'vmin=-absdiff, vmax=absdiff, interpolation=\'nearest\','
                    ' cmap=\'coolwarm\')\n')
            f.write('    axs[0].title.set_text(\'PDF\')\n')
            f.write('    axs[1].title.set_text(\'Histogram\')\n')
            f.write('    axs[2].title.set_text(\'Difference\')\n')
            f.write('    props = dict(fraction=0.046, pad=0.04)\n')
            f.write('    fig.colorbar(pdf_plot, ax=axs[0], **props)\n')
            f.write('    fig.colorbar(hist_plot, ax=axs[1], **props)\n')
            f.write('    fig.colorbar(diff_plot, ax=axs[2], **props)\n')
            f.write('    plt.tight_layout()\n')
            f.write('    plt.show()\n')

    def _log(self, msg):
        self.messages += msg + '\n'


class LineDomain:
    ' The identity map on the line.'

    def __init__(self, bounds=[-1.0, 1.0]):
        from mitsuba.core import ScalarBoundingBox2f

        self._bounds = ScalarBoundingBox2f(
            min=(bounds[0], -0.5),
            max=(bounds[1], 0.5)
        )

    def bounds(self):
        return self._bounds

    def aspect(self):
        return None

    def map_forward(self, p):
        return p.x

    def map_backward(self, p):
        from mitsuba.core import Vector2f, Float
        return Vector2f(p.x, ek.zero(Float, len(p.x)))


class PlanarDomain:
    'The identity map on the plane'

    def __init__(self, bounds=None):
        from mitsuba.core import ScalarBoundingBox2f

        if bounds is None:
            bounds = ScalarBoundingBox2f(-1, 1)

        self._bounds = bounds

    def bounds(self):
        return self._bounds

    def aspect(self):
        extents = self._bounds.extents()
        return extents.x / extents.y

    def map_forward(self, p):
        return p

    def map_backward(self, p):
        return p


class SphericalDomain:
    'Maps between the unit sphere and a [cos(theta), phi] parameterization.'

    def bounds(self):
        from mitsuba.core import ScalarBoundingBox2f
        return ScalarBoundingBox2f([-ek.pi, -1], [ek.pi, 1])

    def aspect(self):
        return 2

    def map_forward(self, p):
        from mitsuba.core import Vector3f

        cos_theta = -p.y
        sin_theta = ek.safe_sqrt(ek.fnmadd(cos_theta, cos_theta, 1))
        sin_phi, cos_phi = ek.sincos(p.x)

        return Vector3f(
            cos_phi * sin_theta,
            sin_phi * sin_theta,
            cos_theta
        )

    def map_backward(self, p):
        from mitsuba.core import Vector2f
        return Vector2f(ek.atan2(y=p.y, x=p.x), -p.z)


# --------------------------------------
#                Adapters
# --------------------------------------


def SpectrumAdapter(value):
    """
    Adapter which permits testing 1D spectral power distributions using the
    Chi^2 test.
    """

    from mitsuba.core.xml import load_string
    from mitsuba.core import sample_shifted, Vector1f
    from mitsuba.render import SurfaceInteraction3f

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
        si = SurfaceInteraction3f.zero(ek.slices(sample))
        wavelength, weight = plugin.sample(si, sample_shifted(sample[0]))
        return Vector1f(wavelength[0])

    def pdf_functor(w, *args):
        plugin = instantiate(args)
        si = SurfaceInteraction3f.zero(ek.slices(w))
        si.wavelengths = w
        return plugin.pdf(si)[0]

    return sample_functor, pdf_functor


def BSDFAdapter(bsdf_type, extra, wi=[0, 0, 1], ctx=None):
    """
    Adapter to test BSDF sampling using the Chi^2 test.

    Parameter ``bsdf_type`` (string):
        Name of the BSDF plugin to instantiate.

    Parameter ``extra`` (string):
        Additional XML used to specify the BSDF's parameters.

    Parameter ``wi`` (array(3,)):
        Incoming direction, in local coordinates.
    """

    from mitsuba.render import BSDFContext, SurfaceInteraction3f
    from mitsuba.core import Float
    from mitsuba.core.xml import load_string

    if ctx is None:
        ctx = BSDFContext()

    def make_context(n):
        si = SurfaceInteraction3f.zero(n)
        si.wi = wi
        ek.set_slices(si.wi, n)
        si.wavelengths = []
        return (si, ctx)

    def instantiate(args):
        xml = """<bsdf version="2.0.0" type="%s">
            %s
        </bsdf>""" % (bsdf_type, extra)
        return load_string(xml % args)

    def sample_functor(sample, *args):
        n = ek.slices(sample)
        plugin = instantiate(args)
        (si, ctx) = make_context(n)
        bs, weight = plugin.sample(ctx, si, sample[0], [sample[1], sample[2]])

        w = Float.full(1.0, ek.slices(weight))
        w[ek.all(ek.eq(weight, 0))] = 0
        return bs.wo, w

    def pdf_functor(wo, *args):
        n = ek.slices(wo)
        plugin = instantiate(args)
        (si, ctx) = make_context(n)
        return plugin.pdf(ctx, si, wo)

    return sample_functor, pdf_functor


def MicrofacetAdapter(md_type, alpha, sample_visible=False):
    """
    Adapter for testing microfacet distribution sampling techniques
    (separately from BSDF models, which are also tested)
    """

    from mitsuba.render import MicrofacetDistribution

    def instantiate(args):
        wi = [0, 0, 1]
        if len(args) == 1:
            angle = args[0] * ek.pi / 180
            wi = [ek.sin(angle), 0, ek.cos(angle)]
        return MicrofacetDistribution(md_type, alpha, sample_visible), wi

    def sample_functor(sample, *args):
        dist, wi = instantiate(args)
        s_value, _ = dist.sample(wi, sample)
        return s_value

    def pdf_functor(m, *args):
        dist, wi = instantiate(args)
        return dist.pdf(wi, m)

    return sample_functor, pdf_functor


def PhaseFunctionAdapter(phase_type, extra, wi=[0, 0, 1]):
    """
    Adapter to test phase function sampling using the Chi^2 test.

    Parameter ``phase_type`` (string):
        Name of the phase function plugin to instantiate.

    Parameter ``extra`` (string):
        Additional XML used to specify the phase function's parameters.

    Parameter ``wi`` (array(3,)):
        Incoming direction, in local coordinates.
    """
    from mitsuba.render import MediumInteraction3f, PhaseFunctionContext
    from mitsuba.core import Float, Frame3f
    from mitsuba.core.xml import load_string

    def make_context(n):
        mi = MediumInteraction3f.zero(n)
        mi.wi = wi
        ek.set_slices(mi.wi, n)
        mi.sh_frame = Frame3f(-mi.wi)
        mi.wavelengths = []
        ctx = PhaseFunctionContext(None)
        return mi, ctx

    def instantiate(args):
        xml = """<phase version="2.0.0" type="%s">
            %s
        </phase>""" % (phase_type, extra)
        return load_string(xml % args)

    def sample_functor(sample, *args):
        n = ek.slices(sample)
        plugin = instantiate(args)
        mi, ctx = make_context(n)
        wo, pdf = plugin.sample(ctx, mi, [sample[0], sample[1]])
        w = Float.full(1.0, ek.slices(pdf))
        w[ek.eq(pdf, 0)] = 0
        return wo, w

    def pdf_functor(wo, *args):
        n = ek.slices(wo)
        plugin = instantiate(args)
        mi, ctx = make_context(n)
        return plugin.eval(ctx, mi, wo)

    return sample_functor, pdf_functor


if __name__ == '__main__':
    mitsuba.set_variant('packet_rgb')

    from mitsuba.core.warp import square_to_cosine_hemisphere
    from mitsuba.core.warp import square_to_cosine_hemisphere_pdf

    def my_sample(sample):
        return square_to_cosine_hemisphere(sample)

    def my_pdf(p):
        return square_to_cosine_hemisphere_pdf(p)

    chi2 = ChiSquareTest(
        domain=SphericalDomain(),
        sample_func=my_sample,
        pdf_func=my_pdf,
        sample_dim=2
    )

    chi2.run()
    chi2._dump_tables()
