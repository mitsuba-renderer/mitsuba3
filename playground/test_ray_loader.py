import mitsuba as mi
import drjit as dr

mi.set_variant('llvm_ad_rgb')
T = mi.ScalarTransform4f

def test_ray_loader():
    """Test RayLoader with 2 cameras, 2x3 images, 4 channels"""

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
    target1 = mi.TensorXf(target1_data, shape=(3, 2, 4))  # height, width, channels

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
        scene=scene,
        sensors=sensors,
        target_images=target_images,
        pixels_per_batch=pixels_per_batch,
        seed=42,
        regular_reshuffle=True
    )

    # Test RayLoader initialization
    assert ray_loader.ttl_pixels == 12, f"Total pixels incorrect: {ray_loader.ttl_pixels}"
    assert ray_loader.channel_size == 4, f"Channel size incorrect: {ray_loader.channel_size}"
    assert ray_loader.pixels_per_batch == 4, f"Pixels per batch incorrect: {ray_loader.pixels_per_batch}"
    assert ray_loader.iter_shuffle == 3, f"Iterations per shuffle incorrect: {ray_loader.iter_shuffle}"

    # Note: flat_sensor.width/height gets overridden by initialize() to match original sensor dims
    # The actual flat sensor film is pixels_per_batch x 1, but these fields track original sensor dims
    assert ray_loader.flat_sensor.width == 2, f"Flat sensor width incorrect: {ray_loader.flat_sensor.width}"
    assert ray_loader.flat_sensor.height == 3, f"Flat sensor height incorrect: {ray_loader.flat_sensor.height}"

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
        assert ref_tensor.shape == (1, 4, 4), f"Reference tensor shape incorrect: {ref_tensor.shape}"

        # Get pixel indices and verify they're valid
        pixel_indices = dr.ravel(flat_sensor.pixel_idx)
        assert len(pixel_indices) == pixels_per_batch, f"Wrong number of pixel indices: {len(pixel_indices)}"

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
                # Correct indexing for INTERLEAVED memory layout: [RGBA RGBA ...]
                # The value for channel 'c' of pixel 'i' is at this index.
                actual_value = float(ref_array[i * 4 + c])
                expected_value = expected_pixel_values[c]
                assert abs(actual_value - expected_value) < 1e-6, \
                    f"Iteration {iteration}, pixel {i} (idx {pixel_idx}), channel {c}: " \
                    f"expected {expected_value}, got {actual_value}"

    print("All tests passed!")

if __name__ == "__main__":
    test_ray_loader()
