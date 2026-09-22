import pytest
import drjit as dr
import mitsuba as mi


def test01_trampoline(variants_vec_backends_once_rgb):
    class DummyFilm(mi.Film):
        def __init__(self, props):
            mi.Film.__init__(self, props)

        def to_string(self):
            return f"DummyFilm ({self.size()})"

    mi.register_film('dummy_film', DummyFilm)
    film = mi.load_dict({
        'type': 'dummy_film'
    })

    assert str(film) == "DummyFilm ([768, 576])"

    # The storage exists before the first call to prepare()
    assert film.create_block().channel_count() == 4
    assert film.develop().shape == (576, 768, 3)

    # Films that don't name their base channels get RGB
    film.prepare([])
    assert film.base_channels() == ['R', 'G', 'B']


def test02_pixel_weight(variants_vec_backends_once_rgb):
    """Films prepared with a pixel weight omit the weight channel"""
    film = mi.load_dict({
        'type': 'hdrfilm', 'width': 3, 'height': 2, 'pixel_format': 'rgba',
        'rfilter': {'type': 'box'}
    })
    assert film.prepare(['depth'], pixel_weight=4) == 5
    assert film.pixel_weight() == 4
    assert film.create_block().channel_count() == 5

    block = film.create_block()
    block.put([0.5, 0.5], [4.0, 8.0, 12.0, 4.0, 2.0])
    film.put_block(block)

    image = film.develop()
    assert image.shape == (2, 3, 5)
    assert dr.allclose(image[0, 0, :], [1, 2, 3, 1, 0.5])
    assert dr.allclose(image[1, 2, :], 0)

    # The storage still reports the weight channel
    storage = film.storage()
    assert storage.shape == (2, 3, 6)
    assert dr.allclose(storage[..., 5], 4)
    assert dr.allclose(storage[0, 0, :5], [4, 8, 12, 4, 2])

    # Preparing without a pixel weight restores the weight channel
    assert film.prepare([]) == 5
    assert film.pixel_weight() == 0


def test03_box_filter_render(variants_vec_backends_once_rgb):
    """Rendering with a box filter sets the pixel weight to the sample count"""
    scene = mi.load_dict({
        'type': 'scene',
        'integrator': {'type': 'path', 'max_depth': 2},
        'emitter': {'type': 'constant', 'radiance': 0.5},
        'sensor': {
            'type': 'perspective',
            'film': {'type': 'hdrfilm', 'width': 3, 'height': 2,
                     'rfilter': {'type': 'box'}},
            'sampler': {'type': 'independent'}
        }
    })
    film = scene.sensors()[0].film()

    image = mi.render(scene, spp=1)
    assert film.pixel_weight() == 1
    assert dr.allclose(image, 0.5)

    image = mi.render(scene, spp=4)
    assert film.pixel_weight() == 4
    assert dr.allclose(image, 0.5)
    assert dr.allclose(film.storage()[..., 3], 4)
