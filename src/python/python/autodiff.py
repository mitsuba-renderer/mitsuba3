import enoki as ek

def render_block(scene, spp=None, sensor_index=0):
    """
    Render the specified Mitsuba scene and return an ImageBlock instance
    containing RGB values and AOVs, if applicable
    """
    from mitsuba.core import (Float, UInt32, UInt64, Vector2f, Mask,
        is_monochromatic, is_rgb, is_polarized)
    from mitsuba.render import ImageBlock

    sensor = scene.sensors()[sensor_index]
    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.crop_size()
    if spp is None:
        spp = sampler.sample_count()

    pos = ek.arange(UInt64, ek.hprod(film_size) * spp)
    sampler.seed(pos)

    pos //= spp
    scale = Vector2f(1.0 / film_size[0], 1.0 / film_size[1])
    pos = Vector2f(Float(pos  % int(film_size[0])),
                   Float(pos // int(film_size[0])))

    pos += sampler.next_2d()

    rays, weights = sensor.sample_ray_differential(
        time=0,
        sample1=sampler.next_1d(),
        sample2=pos * scale,
        sample3=0
    )

    spec, mask, aovs = scene.integrator().sample(scene, sampler, rays)
    spec *= weights
    del mask

    if is_polarized:
        from mitsuba.core import depolarize
        spec = depolarize(spec)

    if is_monochromatic:
        rgb = [spec[0]]
    elif is_rgb:
        rgb = spec
    else:
        from mitsuba.core import spectrum_to_xyz, xyz_to_srgb
        xyz = spectrum_to_xyz(spec_u, rays.wavelengths, active);
        rgb = xyz_to_srgb(xyz)
        del xyz

    aovs = [Float(1.0), *rgb] + aovs
    del rgb

    del spec, weights, rays

    rfilter = film.reconstruction_filter()
    block = ImageBlock(
        size=film.crop_size(),
        channel_count=len(aovs),
        filter=film.reconstruction_filter(),
        warn_negative=False,
        warn_invalid=True,
        border=False
    )

    block.put(pos, aovs)
    del pos
    del aovs

    data = block.data()

    ch = block.channel_count()
    i = UInt32.arange(ek.hprod(block.size()) * (ch - 1))

    weight_idx = i // (ch - 1) * ch
    values_idx = (i * ch) // (ch - 1) + 1

    weight = ek.gather(data, weight_idx)
    values = ek.gather(data, values_idx)

    return values / weight
