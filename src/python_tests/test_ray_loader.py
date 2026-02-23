import pytest
import mitsuba as mi
import drjit as dr


def test_ray_loader(variants_vec_backends_once_rgb):
    """Test RayLoader with 2 cameras, 2x3 images, 4 channels"""
    T = mi.ScalarTransform4f

    # Create two simple sensors with 2x3 films
    sensor_dict = {
        'type': 'perspective',
        'fov_axis': 'smaller',
        'near_clip': 0.001,
        'far_clip': 100.0,
        'focus_distance': 1000,
        'fov': 39.3077,
        'to_world': T().look_at(
            origin=[0, 0, 3.90],
            target=[0, 0, 0],
            up=[0, 1, 0]
        ),
        'sampler': {
            'type': 'independent',
            'sample_count': 1
        },
        'film': {
            'type': 'hdrfilm',
            'width': 2,
            'height': 3,
            'rfilter': {
                'type': 'box',
            },
            'pixel_format': 'rgba',  # 4 channels
            'component_format': 'float32',
        }
    }
    sensor1 = mi.load_dict(sensor_dict)
    sensor_dict['to_world'] = T().look_at(
        origin=[0, 0, -3.90],
        target=[0, 0, 0],
        up=[0, 1, 0]
    )
    sensor2 = mi.load_dict(sensor_dict)

    # Create target images filled with arange values
    # Sensor 1: values 0-23 (2*3*4 = 24 values)
    target1_data = dr.arange(mi.Float, 24)
    target1 = mi.TensorXf(target1_data, shape=(3, 2, 4))

    # Sensor 2: values 100-123 (offset by 100)
    target2_data = dr.arange(mi.Float, 24) + 100
    target2 = mi.TensorXf(target2_data, shape=(3, 2, 4))

    # Verify target tensor shapes
    assert target1.shape == (3, 2, 4), f"Target1 shape incorrect: {target1.shape}"
    assert target2.shape == (3, 2, 4), f"Target2 shape incorrect: {target2.shape}"

    # Create a dummy scene (not actually used for this test)
    scene_dict = mi.cornell_box()
    scene = mi.load_dict(scene_dict)

    # Create the ray loader with batch size of 4 pixels
    sensors = [sensor1, sensor2]
    target_images = [target1, target2]
    pixels_per_batch = 4

    ray_loader = mi.ad.loaders.Rayloader(
        sensors=sensors,
        target_images=target_images,
        pixels_per_batch=pixels_per_batch,
        seed=42,
        regular_reshuffle=True
    )

    # Test RayLoader initialization
    assert ray_loader.ttl_pixels == 12, \
        f"Total pixels incorrect: {ray_loader.ttl_pixels}"
    assert ray_loader.channel_size == 4, \
        f"Channel size incorrect: {ray_loader.channel_size}"
    assert ray_loader.pixels_per_batch == 4, \
        f"Pixels per batch incorrect: {ray_loader.pixels_per_batch}"
    assert ray_loader.iter_shuffle == 3, \
        f"Iterations per shuffle incorrect: {ray_loader.iter_shuffle}"

    # Verify flat sensor dimensions match original sensors
    assert ray_loader.flat_sensor.source_film_width == 2, \
        f"Flat sensor width incorrect: {ray_loader.flat_sensor.width}"
    assert ray_loader.flat_sensor.source_film_height == 3, \
        f"Flat sensor height incorrect: {ray_loader.flat_sensor.height}"

    # Expected pixel to value mapping
    expected_values = {}
    for i in range(6):  # Sensor 1 pixels (0-5)
        expected_values[i] = [i*4, i*4+1, i*4+2, i*4+3]
    for i in range(6, 12):  # Sensor 2 pixels (6-11)
        base_val = (i-6)*4 + 100
        expected_values[i] = [base_val, base_val+1, base_val+2, base_val+3]

    # Test iterations and verify pixel mapping
    for iteration in range(5):
        ref_tensor, flat_sensor = ray_loader.next()

        # Test tensor shape
        assert ref_tensor.shape == (1, 4, 4), \
            f"Reference tensor shape incorrect: {ref_tensor.shape}"

        # Get pixel indices and verify they're valid
        pixel_indices = dr.ravel(flat_sensor.pixel_idx)
        assert len(pixel_indices) == pixels_per_batch, \
            f"Wrong number of pixel indices: {len(pixel_indices)}"

        min_idx = dr.min(pixel_indices)
        max_idx = dr.max(pixel_indices)
        assert min_idx >= 0, f"Pixel index too small: {min_idx}"
        assert max_idx <= 11, f"Pixel index too large: {max_idx}"

        # Verify the content matches expected values
        ref_array = dr.ravel(ref_tensor.array)
        for i in range(pixels_per_batch):
            pixel_idx = int(pixel_indices[i])
            expected_pixel_values = expected_values[pixel_idx]

            for c in range(4):  # 4 channels
                # Interleaved memory layout: [RGBA RGBA ...]
                actual_value = float(ref_array[i * 4 + c])
                expected_value = expected_pixel_values[c]
                assert abs(actual_value - expected_value) < 1e-6, \
                    f"Iteration {iteration}, pixel {i} (idx {pixel_idx}), " \
                    f"channel {c}: expected {expected_value}, " \
                    f"got {actual_value}"


def test_ray_loader_pixel_perm(variants_vec_backends_once_rgb):
    """Test that tile_size=1 performs pixel permutation correctly."""
    T = mi.ScalarTransform4f

    # Create simple sensor
    sensor_dict = {
        'type': 'perspective',
        'fov_axis': 'smaller',
        'fov': 45,
        'to_world': T().look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
        'sampler': {'type': 'independent', 'sample_count': 1},
        'film': {
            'type': 'hdrfilm',
            'width': 4, 'height': 4,
            'rfilter': {'type': 'box'},
            'pixel_format': 'rgb',
            'component_format': 'float32',
            'sample_border': False,
        }
    }
    sensor = mi.load_dict(sensor_dict)

    # Create target image
    target_data = dr.arange(mi.Float, 48)  # 4*4*3 = 48 values
    target = mi.TensorXf(target_data, shape=(4, 4, 3))

    ray_loader = mi.ad.loaders.Rayloader(
        sensors=[sensor],
        target_images=[target],
        pixels_per_batch=8,
        seed=42,
        tile_size=1
    )

    # Verify initialization
    assert ray_loader.tile_size == 1
    assert ray_loader.tiles_per_row == 4  # ceil(4/1) = 4
    assert ray_loader.tiles_per_col == 4  # ceil(4/1) = 4
    assert ray_loader.tiles_per_sensor == 16  # 4*4 = 16
    assert ray_loader.ttl_pixels == 16

    # Test pixel coverage is complete
    all_pixels_seen = set()
    for i in range(ray_loader.iter_shuffle):
        target_tensor, flat_sensor = ray_loader.next()
        assert target_tensor.shape == (1, 8, 3)
        pixel_indices = [int(x) for x in dr.ravel(flat_sensor.pixel_idx)]
        all_pixels_seen.update(pixel_indices)

    assert len(all_pixels_seen) == 16  # All pixels should be covered once


def test_ray_loader_tile_sampling_3x3(variants_vec_backends_once_rgb):
    """Test 3x3 tile sampling."""
    T = mi.ScalarTransform4f

    # Create larger image for meaningful tiles
    sensor_dict = {
        'type': 'perspective',
        'fov_axis': 'smaller',
        'fov': 45,
        'to_world': T().look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
        'sampler': {'type': 'independent', 'sample_count': 1},
        'film': {
            'type': 'hdrfilm',
            'width': 8, 'height': 6,  # 8x6 image
            'rfilter': {'type': 'box'},
            'pixel_format': 'rgb',
            'component_format': 'float32',
            'sample_border': False,
        }
    }
    sensor = mi.load_dict(sensor_dict)

    # Create target image
    target_data = dr.arange(mi.Float, 144)  # 8*6*3 = 144 values
    target = mi.TensorXf(target_data, shape=(6, 8, 3))

    # Test with tile_size=3
    ray_loader = mi.ad.loaders.Rayloader(
        sensors=[sensor],
        target_images=[target],
        pixels_per_batch=12,
        seed=42,
        tile_size=3
    )

    # Verify tile calculations
    assert ray_loader.tile_size == 3
    assert ray_loader.tiles_per_row == 3  # ceil(8/3) = 3 tiles per row
    assert ray_loader.tiles_per_col == 2  # ceil(6/3) = 2 tiles per col
    assert ray_loader.tiles_per_sensor == 6  # 3*2 = 6 tiles total
    assert ray_loader.ttl_pixels == 48  # 8*6 = 48 total pixels

    # Test that batches work correctly
    for i in range(3):
        target_tensor, flat_sensor = ray_loader.next()
        assert target_tensor.shape == (1, 12, 3)
        pixel_indices = [int(x) for x in dr.ravel(flat_sensor.pixel_idx)]
        assert len(pixel_indices) == 12
        assert all(0 <= idx < 48 for idx in pixel_indices)


def test_ray_loader_boundary_handling(variants_vec_backends_once_rgb):
    """Test boundary handling with tiles that don't fit evenly."""
    T = mi.ScalarTransform4f

    # Create 7x5 image with 3x3 tiles (uneven fit)
    sensor_dict = {
        'type': 'perspective',
        'fov_axis': 'smaller',
        'fov': 45,
        'to_world': T().look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
        'sampler': {'type': 'independent', 'sample_count': 1},
        'film': {
            'type': 'hdrfilm',
            'width': 7, 'height': 5,  # 7x5 image
            'rfilter': {'type': 'box'},
            'pixel_format': 'rgb',
            'component_format': 'float32',
            'sample_border': False,
        }
    }
    sensor = mi.load_dict(sensor_dict)

    target_data = dr.arange(mi.Float, 105)  # 7*5*3 = 105 values
    target = mi.TensorXf(target_data, shape=(5, 7, 3))

    ray_loader = mi.ad.loaders.Rayloader(
        sensors=[sensor],
        target_images=[target],
        pixels_per_batch=15,
        seed=42,
        tile_size=3
    )

    # Verify boundary calculations
    assert ray_loader.tiles_per_row == 3  # ceil(7/3) = 3
    assert ray_loader.tiles_per_col == 2  # ceil(5/3) = 2
    assert ray_loader.tiles_per_sensor == 6

    # Test that we can generate pixels for boundary tiles
    # Tile at (1,2) should only cover pixels in range x=[6,6], y=[3,4]
    tile_pixels = ray_loader.generate_tile_pixels(5, 0)  # Last tile (row=1, col=2)
    expected_pixels = [3*7 + 6, 4*7 + 6]  # Only 2 pixels in boundary tile
    assert len(tile_pixels) == 2
    assert set(tile_pixels) == set(expected_pixels)


def test_ray_loader_multi_sensor_tiling(variants_vec_backends_once_rgb):
    """Test tiling with multiple sensors."""
    T = mi.ScalarTransform4f

    sensor_dict = {
        'type': 'perspective',
        'fov': 45,
        'sampler': {'type': 'independent', 'sample_count': 1},
        'film': {
            'type': 'hdrfilm',
            'width': 4, 'height': 4,
            'rfilter': {'type': 'box'},
            'pixel_format': 'rgb',
            'sample_border': False,
        }
    }

    # Create two sensors
    sensor1_dict = dict(sensor_dict)
    sensor1_dict['to_world'] = T().look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0])
    sensor1 = mi.load_dict(sensor1_dict)

    sensor2_dict = dict(sensor_dict)
    sensor2_dict['to_world'] = T().look_at(origin=[0, 0, -3], target=[0, 0, 0], up=[0, 1, 0])
    sensor2 = mi.load_dict(sensor2_dict)

    # Create target images
    target1 = mi.TensorXf(dr.arange(mi.Float, 48), shape=(4, 4, 3))
    target2 = mi.TensorXf(dr.arange(mi.Float, 48) + 100, shape=(4, 4, 3))

    ray_loader = mi.ad.loaders.Rayloader(
        sensors=[sensor1, sensor2],
        target_images=[target1, target2],
        pixels_per_batch=8,
        seed=42,
        tile_size=2
    )

    # Verify multi-sensor setup
    assert ray_loader.num_sensors == 2
    assert ray_loader.ttl_pixels == 32  # 2 * 4 * 4
    assert ray_loader.tiles_per_sensor == 4  # ceil(4/2) * ceil(4/2) = 2*2 = 4

    # Test that pixels from both sensors are sampled
    sensor_pixels_seen = {0: set(), 1: set()}
    for i in range(ray_loader.iter_shuffle):
        target_tensor, flat_sensor = ray_loader.next()
        pixel_indices = [int(x) for x in dr.ravel(flat_sensor.pixel_idx)]

        for pixel_idx in pixel_indices:
            sensor_id = pixel_idx // 16  # 16 pixels per sensor
            sensor_pixels_seen[sensor_id].add(pixel_idx)

    # Both sensors should have pixels sampled
    assert len(sensor_pixels_seen[0]) > 0
    assert len(sensor_pixels_seen[1]) > 0


def test_ray_loader_tile_validation(variants_vec_backends_once_rgb):
    """Test validation of tile parameters."""
    T = mi.ScalarTransform4f

    sensor_dict = {
        'type': 'perspective',
        'fov': 45,
        'to_world': T().look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
        'sampler': {'type': 'independent', 'sample_count': 1},
        'film': {
            'type': 'hdrfilm',
            'width': 4, 'height': 4,
            'rfilter': {'type': 'box'},
            'pixel_format': 'rgb',
            'sample_border': False,
        }
    }
    sensor = mi.load_dict(sensor_dict)
    target = mi.TensorXf(dr.arange(mi.Float, 48), shape=(4, 4, 3))

    # Test negative tile_size
    with pytest.raises(ValueError, match="tile_size \\(-1\\) must be positive"):
        mi.ad.loaders.Rayloader(
            sensors=[sensor],
            target_images=[target],
            pixels_per_batch=8,
            tile_size=-1
        )

    # Test zero tile_size
    with pytest.raises(ValueError, match="tile_size \\(0\\) must be positive"):
        mi.ad.loaders.Rayloader(
            sensors=[sensor],
            target_images=[target],
            pixels_per_batch=8,
            tile_size=0
        )


def test_ray_loader_tile_spatial_coherence(variants_vec_backends_once_rgb):
    """Test that tiled sampling produces spatially coherent batches."""
    T = mi.ScalarTransform4f

    sensor_dict = {
        'type': 'perspective',
        'fov': 45,
        'to_world': T().look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
        'sampler': {'type': 'independent', 'sample_count': 1},
        'film': {
            'type': 'hdrfilm',
            'width': 8, 'height': 8,
            'rfilter': {'type': 'box'},
            'pixel_format': 'rgb',
            'sample_border': False,
        }
    }
    sensor = mi.load_dict(sensor_dict)
    target = mi.TensorXf(dr.arange(mi.Float, 192), shape=(8, 8, 3))

    # Use 4x4 tiles
    ray_loader = mi.ad.loaders.Rayloader(
        sensors=[sensor],
        target_images=[target],
        pixels_per_batch=16,  # Exactly one tile per batch
        seed=42,
        tile_size=4
    )

    # Get one batch and verify spatial coherence
    target_tensor, flat_sensor = ray_loader.next()
    pixel_indices = [int(x) for x in dr.ravel(flat_sensor.pixel_idx)]

    # Convert pixel indices to (y,x) coordinates
    coordinates = []
    for pixel_idx in pixel_indices:
        y = pixel_idx // 8
        x = pixel_idx % 8
        coordinates.append((y, x))

    # All pixels should be within a 4x4 tile
    min_y = min(coord[0] for coord in coordinates)
    max_y = max(coord[0] for coord in coordinates)
    min_x = min(coord[1] for coord in coordinates)
    max_x = max(coord[1] for coord in coordinates)

    assert max_y - min_y < 4, f"Y range {max_y - min_y} should be < 4"
    assert max_x - min_x < 4, f"X range {max_x - min_x} should be < 4"

    # Should have exactly 16 pixels (4x4 tile)
    assert len(pixel_indices) == 16


def test_ray_loader_large_tile_size(variants_vec_backends_once_rgb):
    """Test with large tile sizes relative to image size."""
    T = mi.ScalarTransform4f

    sensor_dict = {
        'type': 'perspective',
        'fov': 45,
        'to_world': T().look_at(origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]),
        'sampler': {'type': 'independent', 'sample_count': 1},
        'film': {
            'type': 'hdrfilm',
            'width': 3, 'height': 3,  # Small image
            'rfilter': {'type': 'box'},
            'pixel_format': 'rgb',
            'sample_border': False,
        }
    }
    sensor = mi.load_dict(sensor_dict)
    target = mi.TensorXf(dr.arange(mi.Float, 27), shape=(3, 3, 3))

    # Use tile_size=5 which is larger than image dimensions
    ray_loader = mi.ad.loaders.Rayloader(
        sensors=[sensor],
        target_images=[target],
        pixels_per_batch=9,
        seed=42,
        tile_size=5
    )

    # Should have only 1 tile that covers the entire image
    assert ray_loader.tiles_per_row == 1  # ceil(3/5) = 1
    assert ray_loader.tiles_per_col == 1  # ceil(3/5) = 1
    assert ray_loader.tiles_per_sensor == 1

    # All pixels should be covered
    target_tensor, flat_sensor = ray_loader.next()
    pixel_indices = [int(x) for x in dr.ravel(flat_sensor.pixel_idx)]
    assert len(pixel_indices) == 9
    assert set(pixel_indices) == set(range(9))  # All pixels 0-8


def test_ray_loader_with_rendering_optimization(variants_vec_backends_once_rgb):
    """Test RayLoader with actual rendering and optimization.

    This test demonstrates the typical usage pattern for RayLoader in an
    optimization loop where the loss should decrease over iterations.
    """
    T = mi.ScalarTransform4f

    # Create a simple scene with differentiable reflectance parameter
    scene_dict = {
        'type': 'scene',
        'integrator': {
            'type': 'path',
            'max_depth': 2,
        },
        'emitter': {
            'type': 'constant',
            'radiance': 1.0,
        },
        'shape': {
            'type': 'rectangle',
            'bsdf': {
                'type': 'diffuse',
                'reflectance': 0.3,  # Start with wrong value
            }
        },
        'sensor1': {
            'type': 'perspective',
            'fov': 45,
            'to_world': T().look_at(
                origin=[0, 0, 3], target=[0, 0, 0], up=[0, 1, 0]
            ),
            'film': {
                'type': 'hdrfilm',
                'width': 32, 'height': 32,
                'filter': {'type': 'box'},
                'sample_border': False,
            },
            'sampler': {
                'type': 'independent',
                'sample_count': 1
            }
        },
        'sensor2': {
            'type': 'perspective',
            'fov': 45,
            'to_world': T().look_at(
                origin=[1, 0, 3], target=[0, 0, 0], up=[0, 1, 0]
            ),
            'film': {
                'type': 'hdrfilm',
                'width': 32, 'height': 32,
                'filter': {'type': 'box'},
                'sample_border': False,
            },
            'sampler': {
                'type': 'independent',
                'sample_count': 1
            }
        },
    }

    scene = mi.load_dict(scene_dict)
    sensors = [scene.sensors()[0], scene.sensors()[1]]

    # Get the differentiable reflectance parameter
    params = mi.traverse(scene)
    key = 'shape.bsdf.reflectance.value'
    params.keep([key])

    # Optimizer
    opt = mi.ad.Adam(lr=0.1)
    opt[key] = params[key]
    params.update(opt)

    # Render reference images with target reflectance (0.8)
    scene_dict['shape']['bsdf']['reflectance'] = 0.8
    target_scene = mi.load_dict(scene_dict)
    image_ref = []
    for sensor in sensors:
        image_ref.append(mi.render(target_scene, sensor=sensor, spp=4))

    # Create RayLoader with tiled sampling
    ray_loader = mi.ad.loaders.Rayloader(
        sensors=sensors,
        target_images=image_ref,
        pixels_per_batch=256,
        seed=42,
        regular_reshuffle=True,
        tile_size=4
    )

    # Verify setup
    assert ray_loader.ttl_pixels == 2 * 32 * 32  # 2 sensors * 32*32 pixels each
    assert ray_loader.tile_size == 4
    assert ray_loader.pixels_per_batch == 256

    # Optimization loop with gradient descent
    losses = []

    for i in range(5):
        # Get next batch of pixels and corresponding sensor
        target_batch, flat_sensor = ray_loader.next()

        # Render the current batch with current reflectance
        rendered_batch = mi.render(scene, spp=1, sensor=flat_sensor, params=params)

        # Verify shapes match
        assert target_batch.shape == rendered_batch.shape
        assert target_batch.shape == (1, 256, 3)

        # Compute L2 loss between rendered and target
        # Scale by pixel_batch_multiplier to match full-image scale for consistent
        # weighting with other loss terms (e.g., regularization) that operate on full model
        batch_loss = (
            dr.mean(dr.square(rendered_batch - target_batch)) *
            ray_loader.pixel_batch_multiplier
        )
        losses.append(dr.detach(batch_loss))

        # Perform gradient descent step
        dr.backward(batch_loss)
        opt.step()
        # opt[key] = dr.clip(opt[key], 0, 1)
        params.update(opt)

    # Test that losses are decreasing (at least the last loss should be lower than first)
    assert all(loss >= 0.0 for loss in losses), "Loss should be non-negative"
    if i > 0:
        assert losses[-1] < losses[-2], "Loss should decrease through optimization"
