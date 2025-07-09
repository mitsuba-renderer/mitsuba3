import drjit as dr
import mitsuba as mi
import numpy as np

def compute_gradient_finite_differences(func: callable, x: float, h: float = 0.01):
    h = dr.opaque(mi.Float, h)
    return (func(x + h) - func(x - h)) / (2*h)

def compute_gradient_forward(func: callable, x: float):
    x_attached = mi.Float(x)
    dr.enable_grad(x_attached)

    # TODO: It is recommended to forward right before mi.render
    output = func(x_attached)

    dr.set_grad(x_attached, 1)
    dr.forward_from(x_attached)
    return dr.grad(output)

def compute_gradient_backward(func: callable, x: float):
    from tqdm import tqdm

    x_attached = mi.Float(x)
    dr.enable_grad(x_attached)

    output = func(x_attached)
    shape = output.numpy().shape
    res = np.zeros((shape[0], shape[1]))
    
    for i in tqdm(range(shape[0]), leave=False):
        for j in tqdm(range(shape[1]), leave=False):
            dr.set_grad(x_attached, 0)
            loss = output[i,j,0]
            dr.backward_from(loss, dr.ADFlag.ClearVertices)
            res[i,j] = dr.grad(x_attached).numpy()[0]    
    return res