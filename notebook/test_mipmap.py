# %% [markdown]
# ## Tensor test

# %%
import os
import sys
sys.path.insert(0, "/home/axiwa/Learn/CG/covariance/mipmap/build/python")
import sys, os
# sys.path.insert(0, f"/home/ziyizhang/Desktop/Projects/mitsuba-uni/mitsuba3/build/python")
import mitsuba as mi
import drjit as dr
from matplotlib import pyplot as plt
mi.set_variant('cuda_ad_rgb')
from mitsuba.python.util import traverse

dr.set_log_level(3)
dr.set_flag(dr.JitFlag.Recording, False)
dr.set_flag(dr.JitFlag.KernelHistory, True)

# %%
# Tensor
b = mi.load_dict({
        'type': 'bitmap',
        'filename': 'figures/smallcheck.png',
        'filter_type': 'bilinear',
        'wrap_mode': 'repeat',
#         'mipmap_filter_type': 'ewa',
        'to_uv': mi.ScalarTransform4f.scale([1, 1, 1])
})
src = mi.traverse(b)['data']
# dr.make_opaque(src.array)
pi = mi.Float(src.array[0])
dr.set_label(pi, 'pi')
dr.enable_grad(pi)
dr.scatter(src.array, pi, 0)

# %%
# def index(i, j, res):
#     channel = res[2]
#     i = dr.clamp(i, 0, res[0]-1)
#     j = dr.clamp(j, 0, res[1]-1)
#     return channel * (i * res[1] + j)

def downsample_trivial(src):
    assert len(src.shape) == 3

    target_res = (src.shape[0]//2, src.shape[1]//2, src.shape[2])
    dst_array = dr.zeros(mi.Float, target_res[0] * target_res[1] * target_res[2])

    x, y = dr.meshgrid(
            dr.arange(mi.UInt, target_res[0]),
            dr.arange(mi.UInt, target_res[1])
    )
    dx = [0, 0, 1, 1]
    dy = [0, 1, 0, 1]
    for i in range(4):
        x_ = dr.clamp(2*x+dx[i], 0, 2*target_res[0]-1)
        y_ = dr.clamp(2*y+dy[i], 0, 2*target_res[1]-1)
        dst_array += dr.gather(mi.Float, src.array, x_ * 2*target_res[0] + y_)

    dst_array /= 4
    dst = type(src)(dst_array, target_res)

    # for i in range(target_res[0]):
        # for j in range(target_res[1]):
            # for c in range(target_res[2]):
                # find 4 pixels of lower level
                # dst[i, j, c] = \
                    # dr.gather(mi.Float, src.array, index(2*i, 2*j, src.shape) + c) + \
                    # dr.gather(mi.Float, src.array, index(2*i+1, 2*j, src.shape) + c) + \
                    # dr.gather(mi.Float, src.array, index(2*i, 2*j+1, src.shape) + c) + \
                    # dr.gather(mi.Float, src.array, index(2*i+1, 2*j+1, src.shape) + c)

    return dst

# %%
dst = downsample_trivial(src)

# import matplotlib.pyplot as plt
# bmp = mi.Bitmap(src)
# plt.imshow(bmp); plt.axis('off');plt.show()
# bmp = mi.Bitmap(dst)
# plt.imshow(bmp); plt.axis('off');plt.show()


# print(dr.grad_enabled(src))
# print(dr.grad_enabled(dst))

# # %%
dst2 = downsample_trivial(dst)

dr.set_label(src, 'src')
dr.set_label(dst, 'dst')
dr.set_label(dst2, 'dst2')
graph = dr.graphviz()
graph.render('mipmap.dot')

print(dr.kernel_history())

if False:
    dr.forward_from(pi, flags=dr.ADFlag.ClearNone)

    # print(dr.grad(src).numpy())
    # print(dr.grad(dst).numpy())


    dr.set_label(src, 'src')
    dr.set_label(dst, 'dst')
    dr.set_label(dst2, 'dst2')
    graph = dr.graphviz_ad()
    graph.render('mipmap.dot')
