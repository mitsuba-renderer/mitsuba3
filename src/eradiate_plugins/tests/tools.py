import drjit as dr
import mitsuba as mi

def sample_eval_pdf_bsdf(
    plugin,
    wi,
    sample_count: int = 100000,
    seed: int = 0,
):
    """
    Sample a BSDF and get corresponding ``eval()`` and ``pdf()`` values.

    Parameters
    ----------
    plugin : mitsuba.BSDF
        A BSDF plugin to be evaluated.

    wi : mitsuba.ScalarVector3f
        An incoming direction.

    sample_count : int, optional, default: 100000
        The number of samples to generate.

    seed : int, optional, default: 0
        RNG seed to be used.

    Returns
    -------
    sample
        The result of a call to the plugin's ``sample()`` method.

    eval
        The result of a call to the plugin's ``eval()`` method.

    pdf
        The result of a call to the plugin's ``pdf()`` method.

    Warnings
    --------
    This function requires a JIT active variant (*e.g.* ``llvm_ad_rgb``).
    """

    # Generate a table of uniform variates
    idx = dr.arange(mi.UInt64, sample_count)
    v0, v1 = mi.sample_tea_32(idx, seed)

    # Scramble seed and stream index using TEA, a linearly increasing sequence
    # does not produce a sufficiently statistically independent set of streams
    rng = mi.PCG32(initstate=v0, initseq=v1)

    sample = mi.Vector3f()
    for i in range(3):
        sample[i] = rng.next_float32() if mi.Float is mi.Float32 else rng.next_float64()

    # Invoke sampling strategy
    n = dr.width(sample)
    ctx = mi.BSDFContext()
    si = dr.zeros(mi.SurfaceInteraction3f, n)
    si.wi = wi
    bs, weight = plugin.sample(ctx, si, sample[0], [sample[1], sample[2]])

    # Get corresponding eval() and pdf() calls
    eval = plugin.eval(ctx, si, bs.wo)
    pdf = plugin.pdf(ctx, si, bs.wo)

    del ctx
    del sample
    del n
    del si
    del rng
    del v0
    del v1
    del idx

    return (bs, weight), eval, pdf