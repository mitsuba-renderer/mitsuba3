import pytest
import drjit as dr
import mitsuba as mi


def test01_trampoline(variants_vec_backends_once_rgb):
    class DummyFilm(mi.Film):
        def __init__(self, props):
            mi.Film.__init__(self, props)
            self.m_flags = mi.FilmFlags.Special

        def to_string(self):
            return f"DummyFilm ({self.m_flags})"

    mi.register_film('dummy_film', DummyFilm)
    film = mi.load_dict({
        'type': 'dummy_film'
    })

    assert str(film) == "DummyFilm (4)"
