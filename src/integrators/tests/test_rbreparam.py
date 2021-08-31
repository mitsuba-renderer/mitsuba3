import enoki as ek
import mitsuba
import pytest

from mitsuba.python.test.util import fresolver_append_path

@fresolver_append_path
def make_simple_scene(res=1):
    from mitsuba.core import xml, ScalarTransform4f as T

    return xml.load_dict({
        'type': 'scene',
        'integrator': {
            'type': 'rb',
            'max_depth': 3,
            # 'hide_emitters': True
        },
        'mysensor': {
            'type': 'perspective',
            'near_clip': 0.1,
            'far_clip': 1000.0,
            'to_world': T.look_at(origin=[0, 0, 6], target=[0, 0, 0], up=[0, 1, 0]),
            'myfilm': {
                'type': 'hdrfilm',
                # 'rfilter': { 'type': 'box'},
                'rfilter': { 'type': 'gaussian', 'stddev': 0.5 },
                # 'rfilter': { 'type': 'tent'},
                'width': res,
                'height': res,
            },
            'mysampler': {
                'type': 'independent',
                'sample_count': 1,
            },
        },
        'rect': {
            'type': 'obj',
            'filename': 'resources/data/common/meshes/rectangle.obj',
            # 'to_world': T.translate([2.0, 2.0, 0.0]),
            'face_normals': True
        },
        'envlight': {
            'type': 'constant',
        },
    })


if __name__ == "__main__":
    mitsuba.set_variant('cuda_ad_rgb')
    from mitsuba.core import xml, Vector3f, Color3f
    from mitsuba.python.util import traverse, write_bitmap

    scene = make_simple_scene(res=128)

    ek.set_flag(ek.JitFlag.VCallRecord,   True)
    ek.set_flag(ek.JitFlag.VCallOptimize, True)
    ek.set_flag(ek.JitFlag.LoopRecord,    True)
    ek.set_flag(ek.JitFlag.LoopOptimize,  True)

    rb_integrator = xml.load_dict({
        'type': 'rbreparam',
        # 'type': 'rbreparamdepth',
        'max_depth': 3,
        'num_aux_rays': 64,
        'kappa': 1e6,
        'power': 3.0
    })

    # Rendering reference image
    image_ref = rb_integrator.render(scene, seed=0, spp=128)
    write_bitmap('image_ref.exr', image_ref)

    # image_ref_pt = scene.integrator().render(scene, seed=0, spp=128)
    # write_bitmap('image_ref_pt.exr', image_ref_pt)

    # Attach scene parameter
    params = traverse(scene)
    key = 'rect.vertex_positions'
    params.keep([key])

    initial_v_pos = ek.unravel(Vector3f, params[key])
    trans_x = 0.01
    params[key] = ek.ravel(initial_v_pos + Vector3f(trans_x, 0.0, 0.0))
    ek.enable_grad(params[key])
    params.update()
    ek.eval()

    image = rb_integrator.render(scene, seed=0, spp=128)
    write_bitmap('image.exr', image)

    ek.enable_grad(image)
    ob_val = ek.hsum_async(ek.sqr(image - image_ref)) / len(image)
    ek.backward(ob_val)
    image_adj = ek.grad(image)
    ek.set_grad(image, 0.0)
    write_bitmap('image_adj.exr', image_adj)

    rb_integrator.render_backward(scene, params, image_adj, seed=0, spp=512)
    grad_backward = ek.grad(params[key])
    print(f"grad_backward: {grad_backward}")
    print(f"ek.hsum(grad_backward): {ek.hsum(grad_backward)}")

    # Finite differences
    eps = 1e-4
    params[key] = ek.ravel(initial_v_pos + Vector3f(trans_x + eps, 0.0, 0.0))
    params.update()

    image_init2 = rb_integrator.render(scene, seed=0, spp=128)

    fd_image_grad = ek.abs(image_init - image_init2) / eps
    write_bitmap('fd_image_grad.exr', fd_image_grad)

    rgb_image_adj = ek.unravel(Color3f, ek.detach(image_adj.array))
    rgb_fd_image_grad = ek.unravel(Color3f, ek.detach(fd_image_grad.array))
    grad_fd = rgb_fd_image_grad * rgb_image_adj
    print(f"grad_fd: {grad_fd}")
    print(f"ek.hsum_nested(grad_fd): {ek.hsum_nested(grad_fd)}")


