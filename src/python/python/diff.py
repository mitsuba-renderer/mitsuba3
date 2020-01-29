def render_block(scene, spp=None, sensor_index=0):
    from mitsuba.core import (Float, UInt32, UInt64, Vector2f, Mask,
        is_monochromatic, is_rgb, is_polarized)
    from mitsuba.render import ImageBlock
    import enoki as ek

    sensor = scene.sensors()[sensor_index]
    film = sensor.film()
    sampler = sensor.sampler()
    film_size = film.crop_size()
    if spp is None:
        spp = sampler.sample_count()

    pos = ek.arange(UInt64, film_size[0] * film_size[1] * spp)
    sampler.seed(pos)

    pos /= spp
    scale = Vector2f(1.0 / film_size[0], 1.0 / film_size[1])
    pos = Vector2f(Float(pos % int(film_size[0])),
                   Float(pos / int(film_size[0])))

    pos += sampler.next_2d()

    rays, weights = sensor.sample_ray_differential(
        time=0,
        sample1=sampler.next_1d(),
        sample2=pos * scale,
        sample3=0
    )

    spec, mask, aovs = scene.integrator().sample(scene, sampler, rays)
    spec *= weights

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

    aovs = [*rgb, ek.select(mask, Float(1.0), Float(0.0)), Float(1.0)] + aovs
    del rgb

    wavelengths = rays.wavelengths
    del spec, mask, weights, rays

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

    del wavelengths, pos
    return block
