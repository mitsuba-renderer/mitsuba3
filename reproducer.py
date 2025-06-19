import mitsuba as mi
from mitsuba.scalar_rgb import ScalarTransform4f as T

mi.set_variant('cuda_ad_rgb')


res = 128
scene_description = {
    'type': 'scene',
    'plane': {
        'type': 'obj',
        'filename': '/home/nroussel/rgl/mitsuba3/resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'to_world':  T().rotate([1, 0, 0], 45) @ T().scale(1.0)
    },
    'integrator': {
        'type': 'path',
        'max_depth': 2,
    },
    'light': {
        'type': 'obj',
        'filename': '/home/nroussel/rgl/mitsuba3/resources/data/common/meshes/rectangle.obj',
        'face_normals': True,
        'to_world': T().translate([0, -1.5, 0]) @ T().rotate([1, 0, 0], -90) @ T().scale(4.0),
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'bitmap',
                'filename': '/home/nroussel/rgl/personal/gradient.jpg',
                'wrap_mode': 'clamp',
            }
        }
    },
    #'light': {
    #    'type': 'sphere',
    #    'to_world': T().translate([-4, 0, 6]) @ T().scale(0.5),
    #    'emitter': {
    #        'type': 'area',
    #        'radiance': {'type': 'rgb', 'value': [15.0, 15.0, 15.0]}
    #    }
    #},
    'sensor': {
        'type': 'perspective',
        'to_world': T().look_at(origin=[0, 0.2, 4], target=[0, 0.2, 0], up=[0, 1, 0]),
        'fov': 18,
        'film': {
            'type': 'hdrfilm',
            'rfilter': { 'type': 'box' },
            'width': res,
            'height': res,
            'sample_border': True,
            'pixel_format': 'rgb',
            'component_format': 'float32',
            #'crop_offset_x': 98,
            #'crop_offset_y': 29,
            #'crop_width': 1,
            #'crop_height': 1,
        }
    }
}

scene = mi.load_dict(scene_description)
img = mi.render(scene, spp=2048)
mi.Bitmap(img).write('tmp.exr')
