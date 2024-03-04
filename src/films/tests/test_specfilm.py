import pytest
import drjit as dr
import mitsuba as mi


def test01_construct(variant_scalar_spectral):
    # Check need at least one Sensor Response Function
    with pytest.raises(RuntimeError):
        film = mi.load_dict({'type': 'specfilm'})
    # With default reconstruction filter
    film = mi.load_dict({
        'type': 'specfilm',
        'srf_test': {
            'type': 'spectrum',
            'value': [(500,1.0), (700,2.0), (750,3.0)]
        },
    })
    assert film is not None
    assert film.rfilter() is not None

    # With a provided reconstruction filter
    film = mi.load_dict({
        'type': 'specfilm',
        'srf_test': {
            'type': 'spectrum',
            'value': [(500,1.0), (700,2.0), (750,3.0)]
        },
        'filter': {
            'type': 'gaussian',
            'stddev': 18.5
        }
    })
    assert film is not None
    assert film.rfilter().radius() == (4 * 18.5)

    # Certain parameter values are not allowed
    with pytest.raises(RuntimeError):
        mi.load_dict({
            'type': 'specfilm',
            'srf_test': {
                'type': 'spectrum',
                'value': [(500,1.0), (700,2.0), (750,3.0)]
            },
            'component_format': 'uint8'
        })

def test02_error_rgb(variant_scalar_rgb):
    # This film can only be used in spectral mode
    with pytest.raises(RuntimeError):
        film = mi.load_dict({
            'type': 'specfilm',
            'srf_test': {
                'type': 'spectrum',
                'value': [(500,1.0), (700,2.0), (750,3.0)]
            }
        })

def test03_crops(variant_scalar_spectral):
    film = mi.load_dict({
        'type': 'specfilm',
        'width': 32,
        'height': 21,
        'crop_width': 11,
        'crop_height': 5,
        'crop_offset_x': 2,
        'crop_offset_y': 3,
        'sample_border': True,
        'srf_test': {
            'type': 'spectrum',
            'value': [(500,1.0), (700,2.0), (750,3.0)]
        },
    })
    assert film is not None
    assert dr.all(film.size() == [32, 21])
    assert dr.all(film.crop_size() == [11, 5])
    assert dr.all(film.crop_offset() == [2, 3])
    assert film.sample_border()

    # Crop size doesn't adjust its size, so an error should be raised if the
    # resulting crop window goes out of bounds.
    incomplete = """<film version="3.0.0" type="specfilm">
            <spectrum name="srf_test" value="500:1.0 700:2.0 750:3.0"/>
            <integer name="width" value="32"/>
            <integer name="height" value="21"/>
            <integer name="crop_offset_x" value="30"/>
            <integer name="crop_offset_y" value="20"/>"""
    with pytest.raises(RuntimeError):
        film = mi.load_string(incomplete + "</film>")
    film = mi.load_string(incomplete + """
            <integer name="crop_width" value="2"/>
            <integer name="crop_height" value="1"/>
        </film>""")
    assert film is not None
    assert dr.all(film.size() == [32, 21])
    assert dr.all(film.crop_size() == [2, 1])
    assert dr.all(film.crop_offset() == [30, 20])

def test04_without_prepare(variant_scalar_spectral):
    film = mi.load_dict({
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
    film = mi.load_dict({
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
        image = mi.TensorXf(film.bitmap())
        assert dr.all((image == 0) | dr.isnan(image), axis=None)


def test06_multiple_channels(variants_all_spectral):
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
        film = mi.load_dict(dic)
        chnl = film.prepare([])
        block = film.create_block()
        assert(block.channel_count() == chnl)
        assert(block.channel_count() == i+1)

def test07_aovs(variants_all_spectral):
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
            film = mi.load_dict(dic)
            chnl = film.prepare(['AOV{}'.format(x) for x in range(1, j+1)])
            block = film.create_block()
            assert(block.channel_count() == chnl)
            assert(block.channel_count() == (i+1+j))

    with pytest.raises(RuntimeError, match=r'prepare\(\)'):
        film = mi.load_dict({
            'type': 'specfilm',
            'width': 3,
            'height': 2,
            'srf_test' : {
                'type': 'spectrum',
                'value': [(500,1.0), (700,2.0), (750,3.0)]
            }
        })
        film.prepare(['AOV', 'AOV'])

def test07_srf(variants_all_spectral):
    dic = {
        'type': 'specfilm',
        'channel1': {
            'type': 'regular',
            'wavelength_min': 400,
            'wavelength_max': 500,
            'values': "0.1, 0.2",
        },
        'channel2': {
            'type': 'regular',
            'wavelength_min': 700,
            'wavelength_max': 800,
            'values': "0.3, 0.4",
        },
    }

    film = mi.load_dict(dic)
    srf = film.sensor_response_function()
    params = mi.traverse(srf)
    key_range = "range"
    key_values = "values"

    dr.allclose(params[key_range], [400, 800])
    dr.allclose(params[key_values], [0.1, 0.2, 0., 0.3, 0.4])
