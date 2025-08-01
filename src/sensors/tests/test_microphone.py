import pytest
import drjit as dr
import mitsuba as mi


def create_microphone(o, d=None, kappa=0.0):
    """Create a microphone with to_world transform"""
    microphone_dict = {
        "type": "microphone",
        "kappa": kappa,
        "film": {
            "type": "tape",
            "frequencies": "250, 500, 1000",
            "time_bins": 1,
        }
    }

    # Set transform either via to_world or origin/direction
    if d is not None:
        t = [o[0] + d[0], o[1] + d[1], o[2] + d[2]]
        microphone_dict["to_world"] = mi.ScalarTransform4f().look_at(
            origin=o,
            target=t,
            up=[0, 0, 1]
        )
    else:
        # Just use origin
        microphone_dict["to_world"] = mi.ScalarTransform4f().translate(o)

    return mi.load_dict(microphone_dict)


def create_microphone_with_origin_direction(o, d, kappa=0.0):
    """Create a microphone using origin and direction parameters"""
    microphone_dict = {
        "type": "microphone",
        "origin": o,
        "direction": d,
        "kappa": kappa,
        "film": {
            "type": "tape",
            "frequencies": "250, 500, 1000",
            "time_bins": 1,
        }
    }

    return mi.load_dict(microphone_dict)


origins = [[0.0, 0.0, 0.0], [1.0, 0.0, 1.5], [1.0, 4.0, 1.5]]
directions = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [1.0, 1.0, 1.0]]
kappa_values = [0.0, 1.0, 5.0]


@pytest.mark.parametrize("origin", origins)
@pytest.mark.parametrize("direction", directions)
@pytest.mark.parametrize("kappa", kappa_values)
def test01_create(variant_scalar_acoustic, origin, direction, kappa):
    """Test microphone creation and basic properties"""
    microphone = create_microphone(origin, direction, kappa=kappa)

    assert microphone.needs_aperture_sample()

    # Check that the microphone position is at the origin
    expected_pos = mi.Point3f(origin)
    actual_pos = microphone.world_transform().translation()
    assert dr.allclose(actual_pos, expected_pos)

    # Check that microphone has the correct kappa value
    assert dr.allclose(microphone.kappa(), kappa)

@pytest.mark.parametrize("origin", origins)
@pytest.mark.parametrize("direction", directions)
def test02_origin_direction_parameters(variant_scalar_acoustic, origin, direction):
    """Test microphone creation using origin and direction parameters"""
    microphone = create_microphone_with_origin_direction(origin, direction)

    # Check position
    actual_pos = microphone.world_transform().translation()
    assert dr.allclose(actual_pos, mi.Point3f(origin))

    # check direction
    expected_dir = mi.Vector3f(direction)
    transform_matrix = microphone.world_transform().matrix
    actual_dir = mi.Vector3f(
        transform_matrix[0, 2],
        transform_matrix[1, 2],
        transform_matrix[2, 2]
    )

    assert dr.allclose(actual_dir, expected_dir/dr.norm(expected_dir), atol=1e-6)


def test03_origin_direction_validation(variant_scalar_rgb):
    """Test that microphone requires both origin and direction when using those parameters"""
    # Should fail if only origin is provided
    with pytest.raises(RuntimeError, match=r"both values must be set"):
        mi.load_dict({
            "type": "microphone",
            "origin": [1.0, 2.0, 3.0],
            "film": {"type": "tape", "frequencies": "250, 500, 1000", "time_bins": 1}
        })

    # Should fail if only direction is provided
    with pytest.raises(RuntimeError, match=r"both values must be set"):
        mi.load_dict({
            "type": "microphone",
            "direction": [0.0, 0.0, 1.0],
            "film": {"type": "tape", "frequencies": "250, 500, 1000", "time_bins": 1}
        })


@pytest.mark.parametrize("origin", origins[:1])
@pytest.mark.parametrize("direction", directions[:2])
@pytest.mark.parametrize("kappa", [0.0, 2000])
def test04_sample_ray_differential(variants_all_acoustic, origin, direction, kappa):
    """Test the sample_ray_differential method"""
    microphone = create_microphone(origin, direction, kappa=kappa)

    time = 0.5 # not used in microphone but required by python binding interface
    wav_sample = 0.5  # Not used in microphone but required by interface
    pos_sample = [0., 0.]  # position_sample.x used for frequency sampling
    aperture_sample = [0.2, 0.8]  # Used for von Mises-Fisher sampling

    ray, weight = microphone.sample_ray_differential(time, wav_sample, pos_sample, aperture_sample)

    # Check basic ray properties
    assert dr.allclose(ray.time, time)
    assert dr.allclose(ray.o, mi.Point3f(origin))

    # Ray direction should be influenced by von Mises-Fisher sampling with kappa
    # For kappa=0, direction should be uniform on sphere
    # For kappa>0, direction should be concentrated around the forward direction
    ray_direction_norm = dr.norm(ray.d)
    assert dr.allclose(ray_direction_norm, 1.0, atol=1e-6)  # Direction should be normalized

    # Weight should be positive
    assert dr.all(weight > 0)

    # Check that differentials are set (even if not meaningful for 1x1 film)
    assert ray.has_differentials == False
    assert dr.allclose(ray.o_x, ray.o)
    assert dr.allclose(ray.o_y, ray.o)
    assert dr.allclose(ray.d_x, ray.d)
    assert dr.allclose(ray.d_y, ray.d)


@pytest.mark.parametrize("origin", origins[:1])
@pytest.mark.parametrize("direction", directions[:2])
@pytest.mark.parametrize("kappa", [0.0, 2e7])
def test05_von_mises_fisher_sampling(variants_all_acoustic, origin, direction, kappa):
    """Test that von Mises-Fisher sampling works correctly with different kappa values"""
    microphone = create_microphone(origin, direction, kappa=kappa)

    # Sample many rays to test distribution
    n_samples = 1000
    time = 0.0
    wav_sample = 0.5

    cos_angles = []
    for i in range(n_samples):
        position_sample = [0, 0]
        aperture_sample = [dr.rcp(n_samples) * i, dr.rcp(n_samples) * ((i * 17) % n_samples)]  # Quasi-random

        ray, weight = microphone.sample_ray_differential(time, wav_sample, position_sample, aperture_sample)

        # Calculate angle between ray direction and forward direction
        cos_angles.append(dr.dot(ray.d, mi.Vector3f(direction)))

    mean_cos_angle = dr.mean(cos_angles)

    if kappa == 0.0:
        # Uniform distribution on sphere should produce mean dot product close to 0
        assert dr.allclose(mean_cos_angle, 0, atol=1e-3)
    elif kappa >= 1e7:
        # rays should be concentrated around the forward direction
        assert dr.allclose(mean_cos_angle, 1, atol=1e-3)


@pytest.mark.parametrize("origin", origins)
@pytest.mark.parametrize("direction", directions)
def test06_sample_direction(variants_vec_spectral, origin, direction):
    """Test the sample_direction method"""
    microphone = create_microphone(origin, direction, kappa=0.0)

    # Create a test interaction point
    it = mi.Interaction3f()
    it.p = mi.Point3f([origin[0] + 2.0, origin[1] + 1.0, origin[2] + 1.0])

    sample_2d = [0., 0.]
    ds, weight = microphone.sample_direction(it, sample_2d)

    # Check that the direction sample points to the microphone
    expected_p = mi.Point3f(origin)
    assert dr.allclose(ds.p, expected_p)

    # Check that direction points from interaction to microphone
    expected_dir = dr.normalize(ds.p - it.p)
    assert dr.allclose(ds.d, expected_dir, atol=1e-6)

    # Check that normal points back toward interaction
    assert dr.allclose(ds.n, -ds.d)

    # Check distance
    expected_dist = dr.norm(ds.p - it.p)
    assert dr.allclose(ds.dist, expected_dist)

    # Direction sample should be delta (point sensor)
    assert ds.delta == True

    # Weight should be positive
    assert dr.all(weight > 0)


def test07_frequency_sampling(variants_all_acoustic):
    """Test frequency spectrum sampling"""
    microphone = create_microphone([0, 0, 0], [0, 0, 1])

    # The microphone uses position_sample.x for frequency sampling
    # Test that different position samples give different frequencies
    for freq_index, frequency in [(0.0, 250), (0.5, 500), (0.9999, 1000)]:
        ray, weight = microphone.sample_ray_differential(0.0, 0.0, mi.Point2f([freq_index, 0.0]), [0.0, 0.0])

        assert dr.allclose(ray.wavelengths, frequency)
