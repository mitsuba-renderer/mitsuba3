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

    N = 12
    for i in range(N):
        s = 0.15
        r = 2.5
        t = i / float(N)
        factory.add(
            mean=[r * dr.sin(t * dr.two_pi), r * dr.cos(t * dr.two_pi), 0.01 * t],
            scale=[s, 4*s, s],
            sigmat=1.0 * t,
            euler=[0.0, 0.0, -360 * t],
            albedo=[dr.abs(dr.sin(t * dr.pi)), 1.0 - t, t]
        )

    centers, scales, quaternions, sigmats, albedos = factory.build()

    scene = mi.load_dict({
        'type': 'scene',
        'integrator': {
            'type': 'volprim_rf_basic',
            'max_depth': 16,
        },

        'sensor': {
            'type': 'perspective',
            'fov': 80,
            'near_clip': 0.01,
            'far_clip': 1000.0,
            'to_world': mi.ScalarTransform4f().look_at(
                origin=[0, 0, -5],
                target=[0, 0, 0],
                up=[0, 1, 0]
            ),
            'film': {
                'type': 'hdrfilm',
                'width': 512,
                'height': 512,
                'filter': { 'type': 'box' },
                'pixel_format': 'rgb'
            }
        },

        'primitives': {
            'type': 'ellipsoids',
            # 'type': 'ellipsoidsmesh',
            # 'shell': 'uv_sphere',
            # 'shell': 'ico_sphere',
            'centers': centers,
            'scales': scales,
            'quaternions': quaternions,
            'opacities': sigmats,
            'sh_coeffs': albedos,
            'extent': 3.0,
        }
    })

    img = mi.render(scene, spp=64)
    mi.Bitmap(img).write('primitives.exr')
