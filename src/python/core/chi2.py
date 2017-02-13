import numpy as np
import mitsuba


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
       resolution will be calculated as ``res * domain.get_aspect()``. The
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
                 dtype=mitsuba.core.float_dtype):
        self.domain = domain
        self.sample_func = sample_func
        self.pdf_func = pdf_func
        self.sample_dim = sample_dim
        self.sample_count = sample_count
        self.res = np.array([res, res * domain.get_aspect()], dtype=np.int64)
        self.ires = ires
        self.dtype = dtype
        self.bounds = domain.get_bounds()
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
        from mitsuba.core import PCG32

        # Generate a table of uniform variates
        samples_in = PCG32().next_float(self.sample_count, self.sample_dim)

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
        if not np.all((xy >= self.bounds[:, 0] - eps*r) &
                      (xy <= self.bounds[:, 1] + eps*r)):
            self._log("Encountered samples outside of the specified domain")
            self.fail = True

        if not np.all(self.histogram >= 0):
            self._log("Encountered a cell with negative sample weights")
            self.fail = True

        if np.sum(self.histogram) > 1.1:
            self._log("Sample weights add up to a value greater than 1.0")
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
        if not np.all(self.pdf >= 0):
            self._log("Encountered a cell with a negative PDF value")
            self.fail = True

        if np.sum(self.pdf) > 1.1:
            self._log("PDF integrates to a value greater than 1.0")
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
            mitsuba.core.math.chi2(histogram * self.sample_count,
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
            self._log("Not running the test for reasons listed above.")
            return False
        elif self.p_value < significance_level \
                or not np.isfinite(self.p_value):
            self._log('***** Rejected ***** the null hypothesis (p-value'
                    ' = %f, significance level = %f)' %
                    (self.p_value, significance_level))
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


class PlanarDomain(object):
    """
    The identity map on the plane
    """
    def __init__(self, bounds=np.array([[-1.0, 1.0],
                                        [-1.0, 1.0]])):
        self.bounds = bounds

    def get_bounds(self):
        return self.bounds

    def get_aspect(self):
        size = self.bounds[:, 0] - self.bounds[:, 1]
        return size[0] / size[1]

    def map_forward(self, p):
        return p

    def map_backward(self, p):
        return p


class SphericalDomain(object):
    """
    Maps between the unit sphere and a parameterization
    based on azimuth & the z coordinate
    """
    def get_bounds(self):
        return np.array(
            [[-np.pi, np.pi], [-1, 1]])

    def get_aspect(self):
        return 2

    def map_forward(self, p):
        cosTheta = -p[:, 1]
        sinTheta = np.sqrt(1 - cosTheta * cosTheta)
        sinPhi, cosPhi = np.sin(p[:, 0]), np.cos(p[:, 0])
        return np.column_stack(
            (cosPhi * sinTheta,
             sinPhi * sinTheta,
             cosTheta)
        )

    def map_backward(self, p):
        return np.column_stack((np.arctan2(p[:, 1], p[:, 0]), -p[:, 2]))
