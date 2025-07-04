import drjit as dr
import mitsuba as mi

mi.set_variant('llvm_ad_rgb')

from mitsuba import ScalarTransform4f as T

scene_dict = {
    'type': 'scene',
    'integrator': {
        'type': 'prb',
        'max_depth': 2,
    },
    'emitter': {
        # 'type': 'constant',
        # 'radiance': {
        #     'type': 'rgb',
        #     'value': 1.0,
        # }
        'type': 'rectangle',
        'to_world': T().look_at(target=[0, 0, 0], origin=[0, 0, 2], up=[0, 1, 0]).scale(0.1),
        'emitter': {
            'type': 'area',
            'radiance': {
                'type': 'rgb',
                'value': [200.0, 200.0, 200.0],
            },
        },
    },
    'shape': {
        'type': 'rectangle',
        'bsdf': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'bitmap',
                'filename': '../tutorials/scenes/textures/flower_photo.jpeg',
                'format': 'variant',
                # 'raw': True,
                'to_uv': T().scale([1, -1, 1]),
            }
        }
    },
    'sensor': {
        'type': 'perspective',
        'fov': 90,
        'to_world': T().look_at(target=[0, 0, 0], origin=[0, 0, 1], up=[0, 1, 0]),
        'film': {
            'type': 'hdrfilm',
            'width': 128, 'height': 128,  # Smaller for faster testing
            'filter': {'type': 'box'},
            'sample_border': False,  # Disable for ray_loader compatibility
        },
    }
}


scene = mi.load_dict(scene_dict)
sensor = scene.sensors()[0]

# Render reference image
image_ref = mi.render(scene, spp=256)
mi.Bitmap(image_ref).write('test_texture_ref.exr')

# Prepare an init scene
params = mi.traverse(scene)
key = 'shape.bsdf.reflectance.data'
print(f"Texture parameter type: {type(params[key])}")
print(f"Texture parameter shape: {params[key].shape}")
params[key] = params[key] * 0 + 0.8  # Set to gray
params.update()

image_init = mi.render(scene, spp=32)
mi.Bitmap(image_init).write('test_texture_init.exr')

# Create ray loader for batch optimization
pixels_per_batch = 8192  # Process 1024 pixels at a time
ray_loader = mi.ad.loaders.Rayloader(
    scene=scene,
    sensors=[sensor],
    target_images=[image_ref],
    pixels_per_batch=pixels_per_batch,
    seed=42,
    regular_reshuffle=True
)

print(f"Ray loader initialized:")
print(f"  Total pixels: {ray_loader.ttl_pixels}")
print(f"  Pixels per batch: {ray_loader.pixels_per_batch}")
print(f"  Iterations per shuffle: {ray_loader.iter_shuffle}")

# Optimize
opt = mi.ad.Adam(lr=0.1)
opt[key] = params[key]
params.update(opt)

print(f"Starting optimization with {opt}")

# Simple optimization loop
for iteration in range(50):  # 50 iterations total
    # Get next batch
    target_batch, flat_sensor = ray_loader.next()

    # Render the batch
    batch_image = mi.render(scene, params, sensor=flat_sensor, spp=4, seed=iteration)

    # Compute loss for this batch
    batch_loss = dr.mean(dr.square(batch_image - target_batch))

    # Backward pass
    dr.backward(batch_loss)

    # Update parameters
    opt.step()
    opt[key] = dr.clip(opt[key], 0, 1)
    params.update(opt)

    # print(f"Iteration {iteration + 1}, Loss: {batch_loss:.6f}")
    print(f"Iteration {iteration + 1}, Loss: {batch_loss}")

    # Render full image every 10 iterations for visualization
    if (iteration + 1) % 5 == 0:
        full_image = mi.render(scene, params, spp=16)
        mi.Bitmap(full_image).write(f'test_texture_rayloader_iter_{iteration+1:03d}.exr')

print("Optimization completed!")
