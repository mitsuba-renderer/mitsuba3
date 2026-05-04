"""
Render the Cornell Box scene using the Metal GPU backend.
"""
import time
import mitsuba as mi
# mi.set_variant('llvm_ad_rgb')
mi.set_variant('metal_ad_rgb')

# Load the Cornell Box scene
scene_dict = mi.cornell_box()

# Increase sample count for quality
scene_dict['sensor']['sampler'] = {
    'type': 'independent',
    'sample_count': 1024
}
scene_dict['sensor']['film']['width'] = 512
scene_dict['sensor']['film']['height'] = 512

scene = mi.load_dict(scene_dict)

start = time.time()

# Render
print("Rendering Cornell Box with Metal backend...")
img = mi.render(scene)

# Save
mi.util.write_bitmap('cornell_box_metal.exr', img)
mi.util.write_bitmap('cornell_box_metal.png', img)

end = time.time()
print(f"Rendering time: {end - start:.4f}s")

import numpy as np
arr = np.array(img)
print(f"Done! Image: {arr.shape}, mean={arr.mean():.4f}, "
      f"min={arr.min():.4f}, max={arr.max():.4f}")
print("Saved to cornell_box_metal.exr and cornell_box_metal.png")
