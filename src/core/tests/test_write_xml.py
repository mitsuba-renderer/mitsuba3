import pytest
import drjit as dr
import mitsuba as mi
import numpy as np
import os
from shutil import copy

from mitsuba.test.util import fresolver_append_path


@fresolver_append_path
def test01_xml_save_plugin(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test01_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
            "type": "sphere",
            "center" : [0, 0, -10],
            "radius" : 10.0,
        }
    mi.xml.dict_to_xml(scene_dict, filepath)
    s1 = mi.load_dict({
        'type': 'scene',
        'sphere':{
            "type": "sphere",
            "center" : [0, 0, -10],
            "radius" : 10.0,
            }
        })
    s2 = mi.load_file(filepath)

    assert str(s1) == str(s2)


@fresolver_append_path
def test02_xml_missing_type(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test02_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
        'my_bsdf':{
            'type':'diffuse'
        }
    }
    with pytest.raises(ValueError) as e:
        mi.xml.dict_to_xml(scene_dict, filepath)
    e.match("Missing key: 'type'!")


@fresolver_append_path
def test03_xml_references(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test03_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
        'type': 'scene',
        'bsdf1': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [1.0, 0.0, 0.0]
            }
        },
        'texture1': {
            'type': 'checkerboard'
        },
        'bsdf4': {
            'type': 'dielectric',
            'specular_reflectance': {
                'type': 'ref',
                'id': 'texture1'
            },
            'id': 'bsdf2'
        },
        'shape1': {
            'type': 'sphere',
            'bsdf': {
                'type': 'ref',
                'id': 'bsdf1'
            }
        },
        'shape2': {
            'type': 'cylinder',
            'bsdf': {
                'type': 'ref',
                'id': 'bsdf2'
            }
        }
    }

    s1 = mi.load_dict(scene_dict)
    mi.xml.dict_to_xml(scene_dict, filepath)
    s2 = mi.load_file(filepath)

    assert str(s1.shapes()[0].bsdf()) == str(s2.shapes()[0].bsdf())


@fresolver_append_path
def test04_xml_point(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test04_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
        'type': 'scene',
        'light':{
            'type': 'point',
            'position': [0.0, 1.0, 2.0]
        }
    }
    mi.xml.dict_to_xml(scene_dict, filepath)
    s1 = mi.load_file(filepath)
    scene_dict = {
        'type': 'scene',
        'light':{
            'type': 'point',
            'position': mi.ScalarPoint3f(0, 1, 2)
        }
    }
    mi.xml.dict_to_xml(scene_dict, filepath)
    s2 = mi.load_file(filepath)
    scene_dict = {
        'type': 'scene',
        'light':{
            'type': 'point',
            'position': np.array([0, 1, 2])
        }
    }
    mi.xml.dict_to_xml(scene_dict, filepath)
    s3 = mi.load_file(filepath)

    assert str(s1) == str(s2)
    assert str(s1) == str(s3)


@fresolver_append_path
def test05_xml_split(variants_all_rgb, tmp_path):
    import numpy as np
    filepath = str(tmp_path / 'test_write_xml-test05_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
        'type': 'scene',
        'bsdf1': {
            'type': 'diffuse'
        },
        'envmap': {
            'type': 'envmap',
            'filename': 'resources/data/common/textures/museum.exr'
        },
        'shape':{
            'type': 'sphere',
            'bsdf': {
                'type': 'ref',
                'id': 'bsdf1'
            }
        }
    }
    mi.xml.dict_to_xml(scene_dict, filepath)
    s1 = mi.load_file(filepath)
    mi.xml.dict_to_xml(scene_dict, filepath, split_files=True)
    s2 = mi.load_file(filepath)

    assert str(s1) == str(s2)


@fresolver_append_path
def test06_xml_duplicate_id(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test06_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
        'type': 'scene',
        'my-bsdf': {
            'type': 'diffuse'
        },
        'bsdf1': {
            'type': 'roughdielectric',
            'id': 'my-bsdf'
        }
    }

    with pytest.raises(ValueError) as e:
        mi.xml.dict_to_xml(scene_dict, filepath)
    e.match("Id: my-bsdf is already used!")


@fresolver_append_path
def test07_xml_invalid_ref(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test07_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
        'type': 'scene',
        'bsdf': {
            'type': 'diffuse'
        },
        'sphere': {
            'type': 'sphere',
            'bsdf': {
                'type': 'ref',
                'id': 'my-bsdf'
            }
        }
    }

    with pytest.raises(ValueError) as e:
        mi.xml.dict_to_xml(scene_dict, filepath)
    e.match("Id: my-bsdf referenced before export.")


@fresolver_append_path
def test08_xml_defaults(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test08_output.xml')
    print(f"Output temporary file: {filepath}")

    spp = 250
    resx = 1920
    resy = 1080
    scene_dict = {
        'type': 'scene',
        'cam': {
            'type': 'perspective',
            'fov_axis': 'x',
            'fov': 35,
            'sampler': {
                'type': 'independent',
                'sample_count': spp  # --> default
            },
            'film': {
                'type': 'hdrfilm',
                'width': resx, # --> default
                'height': resy # --> default
            }
        }
    }
    mi.xml.dict_to_xml(scene_dict, filepath)
    # Load a file using default values
    s1 = mi.load_file(filepath)
    s2 = mi.load_dict(scene_dict)
    assert str(s1.sensors()[0].film()) == str(s2.sensors()[0].film())
    assert str(s1.sensors()[0].sampler()) == str(s2.sensors()[0].sampler())

    # Set new parameters
    spp = 45
    resx = 2048
    resy = 485
    # Load a file with options for the rendering parameters
    s3 = mi.load_file(filepath, spp=spp, resx=resx, resy=resy)
    scene_dict['cam']['sampler']['sample_count'] = spp
    scene_dict['cam']['film']['width'] = resx
    scene_dict['cam']['film']['height'] = resy
    s4 = mi.load_dict(scene_dict)
    assert str(s3.sensors()[0].film()) == str(s4.sensors()[0].film())
    assert str(s3.sensors()[0].sampler()) == str(s4.sensors()[0].sampler())


@fresolver_append_path
def test09_xml_decompose_transform(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test09_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
        'type': 'scene',
        'cam': {
            'type': 'perspective',
            'fov_axis': 'x',
            'fov': 35,
            'to_world': mi.ScalarTransform4f().look_at([15,42.3,25], [1.0,0.0,0.5], [1.0,0.0,0.0])
        }
    }
    mi.xml.dict_to_xml(scene_dict, filepath)
    s1 = mi.load_file(filepath)
    s2 = mi.load_dict(scene_dict)
    points = [
        mi.ScalarPoint3f(0,0,1),
        mi.ScalarPoint3f(0,1,0),
        mi.ScalarPoint3f(1,0,0)
    ]
    tr1 = s1.sensors()[0].world_transform()
    tr2 = s2.sensors()[0].world_transform()
    for p in points:
        assert dr.allclose(tr1 @ p, tr2 @ p)


@fresolver_append_path
def test10_xml_rgb(variants_all_scalar, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test10_output.xml')
    print(f"Output temporary file: {filepath}")

    d1 = {
        'type': 'scene',
        'point': {
            "type" : "point",
            "intensity" : {
                "type": "rgb",
                "value" : mi.ScalarColor3f(0.5, 0.2, 0.5)
            }
        }
    }

    d2 = {
        'type': 'scene',
        'point': {
            "type" : "point",
            "intensity" : {
                "type": "rgb",
                "value" : [0.5, 0.2, 0.5] # list -> mi.ScalarColor3f
            }
        }
    }

    mi.xml.dict_to_xml(d1, filepath)
    s1 = mi.load_file(filepath)
    mi.xml.dict_to_xml(d2, filepath)
    s2 = mi.load_file(filepath)
    s3 = mi.load_dict(d1)
    assert str(s1) == str(s2)
    assert str(s1) == str(s3)

    d1 = {
        'type': 'scene',
        'point': {
            "type" : "point",
            "intensity" : {
                "type": "rgb",
                "value" : 0.5 # float -> mi.ScalarColor3f
            }
        }
    }

    mi.xml.dict_to_xml(d1, filepath)
    s1 = mi.load_file(filepath)
    s2 = mi.load_dict(d1)
    assert str(s1) == str(s2)


@fresolver_append_path
def test11_xml_spectrum(variants_all_scalar, tmp_path):
    from re import escape
    fr = mi.Thread.thread().file_resolver()
    mts_root = str(fr[len(fr)-1])

    filepath = str(tmp_path / 'test_write_xml-test11_output.xml')
    print(f"Output temporary file: {filepath}")

    d1 = {
         'type': 'scene',
        'light': {
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "value" : [(400, 0.1), (500, 0.2), (600, 0.4), (700, 0.1)]
            }
        }
    }
    mi.xml.dict_to_xml(d1, filepath)
    s1 = mi.load_file(filepath)
    s2 = mi.load_dict(d1)

    assert str(s1.emitters()[0]) == str(s2.emitters()[0])

    d2 = {
        'type': 'scene',
        'light': {
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "value" : 0.44
            }
        }
    }

    mi.xml.dict_to_xml(d2, filepath)
    s1 = mi.load_file(filepath)
    s2 = mi.load_dict(d2)
    assert str(s1.emitters()[0]) == str(s2.emitters()[0])

    d3 = {
        'type': 'scene',
        'light': {
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "value" : [(400, 0.1), (500, 0.2), (300, 0.4)]
            }
        }
    }
    with pytest.raises(ValueError) as e:
        mi.xml.dict_to_xml(d3, filepath)
    e.match("Wavelengths must be sorted in strictly increasing order!")

    #wavelengths file
    d4 = {
        'type': 'scene',
        'light': {
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "filename" : os.path.join(mts_root, 'resources/data/ior/Al.eta.spd')
            }
        }
    }

    mi.xml.dict_to_xml(d4, filepath)
    s1 = mi.load_file(filepath)
    s2 = mi.load_dict(d4)
    assert str(s1.emitters()[0]) == str(s2.emitters()[0])

    d5 = {
        'type': 'scene',
        'light': {
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "filename" : os.path.join(mts_root, 'resources/data/blabla/Pr.spd')
            }
        }
    }
    with pytest.raises(ValueError) as e:
        mi.xml.dict_to_xml(d5, filepath)
    e.match(escape(f"File '{os.path.abspath(d5['light']['intensity']['filename'])}' not found!"))


@fresolver_append_path
def test12_xml_duplicate_files(variants_all_scalar, tmp_path):
    fr = mi.Thread.thread().file_resolver()
    mts_root = str(fr[len(fr)-1])

    filepath = str(tmp_path / 'test_write_xml-test12_output.xml')
    print(f"Output temporary file: {filepath}")

    spectrum_path = os.path.join(mts_root, 'resources/data/ior/Al.eta.spd')

    #Export the same file twice, this should only copy it once
    scene_dict = {
        'type': 'scene',
        'light': {
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "filename" : spectrum_path
            }
        },
        'light2': {
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "filename" : spectrum_path
            }
        }
    }
    mi.xml.dict_to_xml(scene_dict, filepath)
    spectra_files = os.listdir(os.path.join(os.path.split(filepath)[0], 'spectra'))

    assert len(spectra_files) == 1 and spectra_files[0] == "Al.eta.spd"

    spectrum_path2 = str(tmp_path / 'test_write_xml-test12_Al.eta.spd')
    copy(spectrum_path, spectrum_path2)
    # Export two files having the same name
    scene_dict = {
        'type': 'scene',
        'light': {
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "filename" : spectrum_path
            }
        },
        'light2': {
            "type" : "point",
            "intensity" : {
                "type" : "spectrum",
                "filename" : spectrum_path2
            }
        }
    }
    mi.xml.dict_to_xml(scene_dict, filepath)

    spectra_files = os.listdir(os.path.join(os.path.split(filepath)[0], 'spectra'))

    assert len(spectra_files) == 2
    assert "Al.eta.spd" in spectra_files
    assert "test_write_xml-test12_Al.eta.spd" in spectra_files


@fresolver_append_path
def test13_xml_multiple_defaults(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test13_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
        'type' : 'scene',
        'cam1' : {
            'type' : 'perspective',
            'sampler' : {
                'type' : 'independent',
                'sample_count': 150
            }
        },
        'cam2' : {
            'type' : 'perspective',
            'sampler' : {
                'type' : 'independent',
                'sample_count': 250
            }
        }
    }
    mi.xml.dict_to_xml(scene_dict, filepath)
    scene = mi.load_file(filepath, spp=45)

    assert scene.sensors()[0].sampler().sample_count() == scene.sensors()[1].sampler().sample_count()
    assert scene.sensors()[1].sampler().sample_count() == 45


@fresolver_append_path
def test14_xml_empty_dir(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test13_xml_empty_dir.xml')
    print(f"Output temporary file: {filepath}")

    cwd = os.getcwd()
    os.chdir(tmp_path)

    scene_dict = {
        "type": "sphere",
        "center": [0, 0, -10],
        "radius": 10.0,
    }
    mi.xml.dict_to_xml(scene_dict, os.path.split(filepath)[1])
    os.chdir(cwd)

    s1 = mi.load_dict({
        'type': 'scene',
        'sphere': {
            "type": "sphere",
            "center": [0, 0, -10],
            "radius": 10.0,
        }
    })
    s2 = mi.load_file(filepath)

    assert str(s1) == str(s2)


@fresolver_append_path
def test15_xml_volume(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test15_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
        'type': 'scene',
        'sphere' : {
            'type': 'sphere',
            'center': [0, 0, -10],
            'radius': 10.0,
            'interior': {
                'type': 'homogeneous'
            }
        }
    }

    for i in range(2):
        mi.xml.dict_to_xml(scene_dict, filepath, split_files=(i==0))

        s1 = mi.load_dict(scene_dict)
        s2 = mi.load_file(filepath)

        print(s1)
        print(s2)

        assert str(s1) == str(s2)


@fresolver_append_path
def test16_xml_volume_with_args(variants_all_rgb, tmp_path):
    filepath = str(tmp_path / 'test_write_xml-test16_output.xml')
    print(f"Output temporary file: {filepath}")

    scene_dict = {
        'type': 'scene',
        'sphere' : {
            'type': 'sphere',
            'center': [0, 0, -10],
            'radius': 10.0,
            'interior': {
                'type': 'homogeneous',
                'albedo': {
                    'type': 'rgb',
                    'value': [0.99, 0.9, 0.96]
                },
            }
        }
    }

    for i in range(2):
        mi.xml.dict_to_xml(scene_dict, filepath, split_files=(i==0))

        s1 = mi.load_dict(scene_dict)
        s2 = mi.load_file(filepath)

        print(s1)
        print(s2)

        assert str(s1) == str(s2)
