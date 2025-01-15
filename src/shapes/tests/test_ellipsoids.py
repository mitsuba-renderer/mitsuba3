import drjit as dr
import mitsuba as mi

class EllipsoidsFactory:
    '''
    Helper class to build Ellipsoids datasets for testing purposes
    '''
    def __init__(self):
        self.centers = []
        self.scales = []
        self.quaternions = []
        self.sigmats = []
        self.albedos = []

    def add(self, mean, scale, sigmat=1.0, albedo=1.0, euler=[0.0, 0.0, 0.0]):
        self.centers.append(mi.ScalarPoint3f(mean))
        self.scales.append(mi.ScalarVector3f(scale))
        quaternion = dr.slice(dr.euler_to_quat(dr.deg2rad(mi.ScalarPoint3f(euler))))
        self.quaternions.append(mi.ScalarQuaternion4f(quaternion))
        self.sigmats.append(sigmat)
        if isinstance(albedo, float):
            albedo = mi.ScalarColor3f(albedo)
        self.albedos.append(albedo)

    def build(self):
        import numpy as np
        num_gaussians = len(self.centers)
        centers = mi.TensorXf(np.ravel(np.array(self.centers)), shape=(num_gaussians, 3))
        scales  = mi.TensorXf(np.ravel(np.array(self.scales)), shape=(num_gaussians, 3))
        quats   = mi.TensorXf(np.ravel(np.array(self.quaternions)), shape=(num_gaussians, 4))
        sigmats = mi.TensorXf(self.sigmats, shape=(num_gaussians, 1))
        self.albedos = np.array(self.albedos).reshape((num_gaussians, -1))
        albedos = mi.TensorXf(np.array(self.albedos))
        return centers, scales, quats, sigmats, albedos

def test01_ellipsoids_depth(variants_vec_backends_once_rgb):
    factory = EllipsoidsFactory()
    N = 10
    for t in range(N):
        factory.add(
            mean=[0.0, t, N * t / N],
            scale=[0.1, 0.3, 0.5],
            euler=[0.0, 0.0, 10 * t],
        )
    centers, scales, quaternions, sigmats, albedos = factory.build()


    scene = mi.load_dict({
        'type': 'scene',
        'primitives': {
            'type': 'ellipsoids',
            'centers': centers,
            'scales': scales,
            'quaternions': quaternions,
        }
    })

    rays = mi.Ray3f(
        mi.Point3f(0, dr.linspace(mi.Float, 0.0, N, 20), -2),
        mi.Vector3f(0, 0, 1)
    )

    # List of precomputed depth values
    reference_depth = [0.5, 0.783229, 1.50319, 1.95389, 2.51999, 3.67589, 3.57093, 4.97172, 4.68854, 5.9255, 5.9255, 6.82178, 7.44238, 7.69941, dr.inf, 8.59273, dr.inf, 9.52326, dr.inf, dr.inf]

    assert dr.allclose(reference_depth, scene.ray_intersect_preliminary(rays).t)
