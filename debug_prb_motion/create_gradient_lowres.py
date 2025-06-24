import mitsuba as mi
import numpy as np

mi.set_variant('cuda_ad_rgb')

width = 16
height = 16

# Create a grid of normalized coordinates.
x = np.linspace(0., 1., width, dtype=np.float32)

# Create a 2D tensor for the image.
# The gradient should be from left to right, so we can just tile the `x` array.
image = np.tile(x, (height, 1))

# Set the outermost pixels to 0.
image[0, :] = 0
image[-1, :] = 0
image[:, 0] = 0
image[:, -1] = 0

# Convert to a 3-channel RGB image.
# The values are already in [0, 1], so we can just repeat them.
image_rgb = np.stack([image]*3, axis=-1)

# Create a Mitsuba Bitmap and save it.
# write_async=False to ensure it's written before the script exits.
mi.util.write_bitmap('gradient_lowres.jpg', image_rgb, write_async=False)

print("Successfully created gradient_lowres.jpg")
