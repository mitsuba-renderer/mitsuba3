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
