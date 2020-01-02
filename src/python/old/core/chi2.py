




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
