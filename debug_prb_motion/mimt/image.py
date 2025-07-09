import numpy as np
from typing import Tuple

def gamma_encode(img: np.array):
    return img ** (1/2.2)

def create_gradient_image(width: int, height: int, color_1: Tuple[float, float, float], color_2: Tuple[float, float, float]):
    """ Generate a vertical gradient image
    """

    t = np.repeat(np.linspace(0, 1, height, dtype=np.float32)[:, None], width, axis=1) # (height,width)

    color_1 = np.array(color_1, dtype=np.float32)
    color_2 = np.array(color_2, dtype=np.float32)

    return (1 - t[:, :, None]) * color_1[None, None, :] + t[:, :, None] * color_2[None, None, :]