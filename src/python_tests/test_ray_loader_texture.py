# Not a real test yet
# A test scene with a texture and ray loader for optimization
# Each sensor views unique parts of the texture

import drjit as dr
import mitsuba as mi

mi.set_variant('cuda_ad_rgb')

from mitsuba import ScalarTransform4f as T

scene_dict = {
    'type': 'scene',
    'integrator': {
        'type': 'prb',
        'max_depth': 2,
    },
    'emitter': {
        'type': 'rectangle',
        'to_world': T().look_at(target=[0, 0, 0], origin=[0, 0, 2],
                                up=[0, 1, 0]).scale(0.1),
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'rgb',
                'value': [200.0, 150.0, 160.0],
            },
        },
    },
    'shape': {
        'type': 'rectangle',
        'bsdf': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'bitmap',
                'filename': 'tutorials/scenes/textures/'
                           'flower_photo_downscale.jpeg',
                'format': 'variant',
                'to_uv': T().scale([1, -1, 1]),
            }
        }
    },
    'film_box': {
        'type': 'hdrfilm',
        'width': 128, 'height': 128,
        'filter': {'type': 'box'},
        'sample_border': False,  # Required for ray_loader compatibility
    },
    'sensor1': {
        'type': 'perspective',
        'fov': 90,
        'to_world': T().look_at(target=[0.5, 0.5, 0],
                               origin=[0.5, 0.5, 0.5], up=[0, 1, 0]),
        'film': {
            'type': 'ref',
            'id': 'film_box',
        },
    },
    'sensor2': {
        'type': 'perspective',
        'fov': 90,
        'to_world': T().look_at(target=[0.5, -0.5, 0],
                               origin=[0.5, -0.5, 0.5], up=[0, 1, 0]),
        'film': {
            'type': 'ref',
            'id': 'film_box',
        }
    },
    'sensor3': {
        'type': 'perspective',
        'fov': 90,
        'to_world': T().look_at(target=[-0.5, 0.5, 0],
                               origin=[-0.5, 0.5, 0.5], up=[0, 1, 0]),
        'film': {
            'type': 'ref',
            'id': 'film_box',
        },
    },
    'sensor4': {
        'type': 'perspective',
        'fov': 90,
        'to_world': T().look_at(target=[-0.5, -0.5, 0],
                               origin=[-0.5, -0.5, 0.5], up=[0, 1, 0]),
        'film': {
            'type': 'ref',
            'id': 'film_box',
        }
    },
}


scene = mi.load_dict(scene_dict)
sensors = scene.sensors()

# Render reference image
image_ref = []
for sensor in sensors:
    image_ref.append(mi.render(scene, sensor=sensor, spp=256))

# Prepare an init scene
params = mi.traverse(scene)
key = 'shape.bsdf.reflectance.data'
mi.Bitmap(params[key]).write('test_texture_ref.exr')
params[key] = params[key] * 0 + 0.8  # Initialize to gray
params.update()

mi.Bitmap(params[key]).write('test_texture_init.exr')

# Create ray loader for batch optimization
pixels_per_batch = 8192 * 4  # 30000
ray_loader = mi.ad.loaders.Rayloader(
    sensors=sensors,
    target_images=image_ref,
    pixels_per_batch=pixels_per_batch,
    seed=42,
    regular_reshuffle=False,
)

print(f"Ray loader initialized:")
print(f"  Total pixels: {ray_loader.ttl_pixels}")
print(f"  Pixels per batch: {ray_loader.pixels_per_batch}")
print(f"  Iterations per shuffle: {ray_loader.iter_shuffle}")
print(f"Texture parameter shape: {params[key].shape}")

# Optimize
opt = mi.ad.Adam(lr=0.1)
opt[key] = params[key]
params.update(opt)

for iteration in range(50):
    # Get next batch
    target_batch, flat_sensor = ray_loader.next()

    batch_image = mi.render(scene, params, sensor=flat_sensor, spp=4,
                           spp_grad=1, seed=iteration)

    # Ray loader uses sampling, so multiply loss by pixel batch multiplier
    batch_loss = (
        dr.mean(dr.square(batch_image - target_batch)) *
        ray_loader.pixel_batch_multiplier
    )

    dr.backward(batch_loss)

    # Update parameters
    opt.step()
    opt[key] = dr.clip(opt[key], 0, 1)
    params.update(opt)

    print(f"Iteration {iteration + 1:02d}, Loss: {batch_loss}")

    if (iteration + 1) % 5 == 0:
        mi.Bitmap(params[key]).write(
            f'test_texture_rayloader_iter_{iteration+1:03d}.exr')
