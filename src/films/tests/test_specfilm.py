import drjit as dr
import mitsuba
import pytest


def test01_construct(variant_scalar_spectral):
    from mitsuba.core import load_string

    # Check need at least one Sensor Response Function
    with pytest.raises(RuntimeError):
        film = load_string("""<film version="2.0.0" type="specfilm"></film>""")
    # With default reconstruction filter
    film = load_string("""<film version="2.0.0" type="specfilm">
            <spectrum name="srf_test" value="500:1.0 700:2.0 750:3.0"/>
        </film>""")
    assert film is not None
    assert film.rfilter() is not None

    # With a provided reconstruction filter
    film = load_string("""<film version="2.0.0" type="specfilm">
            <spectrum name="srf_test" value="500:1.0 700:2.0 750:3.0"/>
            <rfilter type="gaussian">
                <float name="stddev" value="18.5"/>
            </rfilter>
        </film>""")
    assert film is not None
    assert film.rfilter().radius() == (4 * 18.5)

    # Certain parameter values are not allowed
    with pytest.raises(RuntimeError):
        load_string("""<film version="2.0.0" type="specfilm">
            <spectrum name="srf_test" value="500:1.0 700:2.0 750:3.0"/>
            <string name="component_format" value="uint8"/>
        </film>""")

def test02_error_rgb(variant_scalar_rgb):
    from mitsuba.core import load_string

    # This film can only be used in spectral mode
    with pytest.raises(RuntimeError):
        film = load_string("""<film version="2.0.0" type="specfilm">
            <spectrum name="srf_test" value="500:1.0 700:2.0 750:3.0"/>
        </film>""")

def test03_crops(variant_scalar_spectral):
    from mitsuba.core import load_string

    film = load_string("""<film version="2.0.0" type="specfilm">
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <integer name="crop_width" value="11"/>
            <integer name="crop_height" value="5"/>
            <integer name="crop_offset_x" value="2"/>
            <integer name="crop_offset_y" value="3"/>
            <boolean name="sample_border" value="true"/>
            <spectrum name="srf_test" value="500:1.0 700:2.0 750:3.0"/>
        </film>""")
    assert film is not None
    assert dr.all(film.size() == [32, 21])
    assert dr.all(film.crop_size() == [11, 5])
    assert dr.all(film.crop_offset() == [2, 3])
    assert film.sample_border()

    # Crop size doesn't adjust its size, so an error should be raised if the
    # resulting crop window goes out of bounds.
    incomplete = """<film version="2.0.0" type="specfilm">
            <spectrum name="srf_test" value="500:1.0 700:2.0 750:3.0"/>
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <integer name="crop_offset_x" value="30"/>
            <integer name="crop_offset_y" value="20"/>"""
    with pytest.raises(RuntimeError):
        film = load_string(incomplete + "</film>")
    film = load_string(incomplete + """
            <integer name="crop_width" value="2"/>
            <integer name="crop_height" value="1"/>
        </film>""")
    assert film is not None
    assert dr.all(film.size() == [32, 21])
    assert dr.all(film.crop_size() == [2, 1])
    assert dr.all(film.crop_offset() == [30, 20])

def test04_without_prepare(variant_scalar_spectral):
    film = mitsuba.core.load_dict({
        'type': 'specfilm',
        'width': 3,
        'height': 2,
        'srf_test' : {
            'type': 'spectrum',
            'value': [(500,1.0), (700,2.0), (750,3.0)]
        }
    })

    with pytest.raises(RuntimeError, match=r'prepare\(\)'):
        _ = film.develop()

@pytest.mark.parametrize('develop', [False, True])
def test05_empty_film(variants_all_spectral, develop):
    film = mitsuba.core.load_dict({
        'type': 'specfilm',
        'width': 3,
        'height': 2,
        'srf_test' : {
            'type': 'spectrum',
            'value': [(500,1.0), (700,2.0), (750,3.0)]
        }
    })
    film.prepare([])

    if develop:
        image = dr.ravel(film.develop())
        assert dr.all((image == 0) | dr.isnan(image))
    else:
        from mitsuba.core import TensorXf
        image = TensorXf(film.bitmap())
        assert dr.all((image == 0) | dr.isnan(image))


def test05_multiple_channels(variants_all_spectral):
    dic = {
        'type': 'specfilm',
        'width': 3,
        'height': 2,
    }

    for i in range(1,10):
        dic['srf_{}'.format(i)] = {
            'type': 'spectrum',
            'value': [(500,1.0), (700,2.0), (750,3.0)]
        }
        film = mitsuba.core.load_dict(dic)
        chnl = film.prepare([])
        block = film.create_block(False)
        assert(block.channel_count() == chnl)
        assert(block.channel_count() == i+1)

def test06_aovs(variants_all_spectral):
    dic = {
        'type': 'specfilm',
        'width': 3,
        'height': 2,
    }

    for i in range(1,10):
        dic['srf_{}'.format(i)] = {
            'type': 'spectrum',
            'value': [(500,1.0), (700,2.0), (750,3.0)]
        }
        for j in range(1,5):
            film = mitsuba.core.load_dict(dic)
            chnl = film.prepare(['AOV{}'.format(x) for x in range(1, j+1)])
            block = film.create_block(False)
            assert(block.channel_count() == chnl)
            assert(block.channel_count() == (i+1+j))

    with pytest.raises(RuntimeError, match=r'prepare\(\)'):
        film = mitsuba.core.load_dict({
            'type': 'specfilm',
            'width': 3,
            'height': 2,
            'srf_test' : {
                'type': 'spectrum',
                'value': [(500,1.0), (700,2.0), (750,3.0)]
            }
        })
        film.prepare(['AOV', 'AOV'])
