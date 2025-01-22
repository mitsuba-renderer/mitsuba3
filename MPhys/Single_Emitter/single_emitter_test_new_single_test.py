import drjit as dr
import mitsuba as mi
import time
import pandas as pd
import numpy as np
import random


# mi.set_variant('cuda_mono')
mi.set_variant('llvm_mono')
mi.variants()
# dr.set_log_level(dr.LogLevel.Info)
mi.set_log_level(mi.LogLevel.Info)

nm_per_ev_constant = (float(6.6260715e-34)*float(3.00e8)*float(1e9))/(float(1.6021e-19)*float(1e6))

# Load in the CSV file
# column_names = ["time (ps)", "x", "y", "z", "px", "py", "pz", "E (MeV)"]
# The first column in the CSV file is an index column and we don't want to load that in
photon_detected = pd.read_csv('csv/photons_detected_spectral.csv', index_col=0) #, names = column_names)
print(photon_detected)

def generate_emitter_data(photon_data):
    """
    Generates the data for the photon plugin of Mitsuba using the photon data
    """
    x_position, y_position, z_position = photon_data.values[:, 1:4].T
    x_momentum, y_momentum, z_momentum = photon_data.values[:, 4:7].T
    # calculate the target coordinates of the photons
    x_target = x_position + x_momentum
    y_target = y_position + y_momentum
    z_target = z_position + z_momentum
    # combine them into a single array
    # emitter_data = np.column_stack((x_position, y_position, z_position, x_target, y_target, z_target)).flatten()
    # The y- and z- positions in other examples were inverted for some reason... ?
    emitter_data = np.column_stack((x_position, z_position, y_position, x_target, z_target, y_target)).flatten()
    emitter_data = np.insert(emitter_data, 0, len(x_position))
    # create a 3D array of the emitter data
    result = np.zeros((1, 1, len(emitter_data)), dtype=np.float32)
    result[0, 0, :] = emitter_data
    return result

# gen_photon_datas = []
photon_lists = []

# Loop to generate photon data from the photon_detected data frame

gen_photon_data = generate_emitter_data(photon_detected)
photon_list = mi.VolumeGrid(gen_photon_data)
photon_lists.append(photon_list)

print("--------------------------------")
print (photon_list)
print (np.array(photon_list))
print (gen_photon_data)

print ("n_photons into mitsuba ", (len(np.array(photon_list)[0][0]) - 1) // 6)

start_time = time.time()

intensity = 2000000.0

# Set up the scene description
scene_description = {
    'type': 'scene',

    'integrator': {
        'type': 'ptracer_c',  # ptracer?
        'max_depth': 50,
        'hide_emitters': False,
    },

    'sensor': {
        'type': 'perspective',
        'fov': 40,
        'to_world': mi.ScalarTransform4f().look_at(origin=[0, 1100, 950],
                                                 target=[0, 1500, 1500],
                                                 up=[0, 0, 1]),
        'sampler': {
            'type': 'independent',
            'sample_count': 1,
        },
        'film': {
            'type': 'hdrfilm',
            'width': 1024,
            'height': 1024,
            'file_format': 'openexr',
            'pixel_format': 'luminance',
            'component_format': 'float32',
            'filter': {
                'type': 'tent',
            },
        },      
    },

    'MirrorBSDF': {
        'type': 'twosided',
        'bsdf_id': {
            'type': 'conductor',
            'material': 'none',
        },
    },

    # 'RoughMirrorBSDF': {
    #     'type': 'conductor',
    #     'material': 'none',
    #     'alpha': 0.01,
    # },

    'test': {
        'type': 'twosided',
        'bsdf_id': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [1, 1, 1],
            },
        },
    },

    'test2': {
        'type': 'twosided',
        'bsdf_id': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.9, 0.5, 0.2],
            },
        },
    },

    'spherical_mirror': {
        'type': 'cube',
        'to_world': mi.ScalarTransform4f().look_at(
            origin=[0, 2000, 355], target=[0, 1000, 600], up=[0, 0, 1]).scale([1500, 650, 33]),
        'bsdf_id': {
            'type': 'ref',
            'id': 'MirrorBSDF',
        },
    },

    'flat_mirror': {
        'type': 'rectangle',
        'to_world': mi.ScalarTransform4f().look_at(
            origin=[0, 1000, 710], target=[0, 2000, 900], up=[0, 0, 1]).scale([740, 440, 0.1]),
        'bsdf_id': {
            'type': 'ref',
            'id': 'MirrorBSDF',
        },
    },

    'detector': {
        'type': 'rectangle',
        'to_world': mi.ScalarTransform4f().look_at(
            origin=[0, 1500, 1120], target=[0, 1000, 710], up=[0, 0, 1]).scale([1000, 500, 0.5]),
        'bsdf_id': {
            'type': 'ref',
            'id': 'test',
        },        
    },

    'backwall': {
        'type': 'rectangle',
        'to_world': mi.ScalarTransform4f().look_at(
            origin=[-1000, 1500, 500], target=[0, 1500, 500], up=[0, 1, 0]).scale([3000, 3000, 1]),
        'bsdf_id': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.4, 1, 0.2],
            },
        },        
    },
        
    'ceiling': {
        'type': 'rectangle',
        'to_world': mi.ScalarTransform4f().look_at(
            origin=[0, 1500, 3000], target=[0, 1500, 0], up=[0, 1, 0]).scale([3000, 3000, 1]),
        'bsdf_id': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.8, 0.3, 0.45],
            },
        },        
    },
        
    'leftwall': {
        'type': 'rectangle',
        'to_world': mi.ScalarTransform4f().look_at(
            origin=[0, 0, 500], target=[0, 1500, 500], up=[0, 0, 1]).scale([3000, 3000, 3000]),
        'bsdf_id': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.32, 0.46, 0.23],
            },
        },        
    },
        
    'rightwall': {
        'type': 'rectangle',
        'to_world': mi.ScalarTransform4f().look_at(
            origin=[0, 3000, 500], target=[0, 1500, 500], up=[0, 0, 1]).scale([3000, 3000, 3000]),
        'bsdf_id': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.92, 0.58, 1],
            },
        },        
    },

    'floor': {
        'type': 'rectangle',
        'to_world': mi.ScalarTransform4f().look_at(
            origin=[0, 1500, -1000], target=[0, 1500, 3000], up=[0, 1, 0]).scale([3000, 3000, 3000]),
        'bsdf_id': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.56, 0.23, 0.54],
            },
        },        
    },

    'photons': {
        'type': 'photon',
        'photon_list': photon_list,
        'filename': 'bintxt/photon_geometry.bin',
        'intensity': intensity,
    },        
}

# Load from dict or from XML file
print("================== load_dict ==================")
scene = mi.load_dict(scene_description)
# print("================== load_file ==================")
# scene = mi.load_file("./xml/real_geometry_int"+str(int(intensity))+".xml")
# print(scene)

print("================== render =====================")
original_image = mi.render(scene)
# print(original_image)
import matplotlib.pyplot as plt
import matplotlib
# Use agg backend for matplotlib since we're just writing to files
# Note: in WSL2 with python3-tk installed, allowing matplotlib to select automatically
#       (I assume it picks 'TkAgg' in this instance) causes a segmentation fault!
matplotlib.use('agg')

plt.figure(figsize = (20,20))
plt.axis('off')
plt.imshow(original_image ** (1.0 / 2.2)); 
end_time = time.time()
elapsed_time = end_time - start_time
print(f"Elapsed time: {elapsed_time:.2f} seconds")
print("new intensity = ", int(intensity))
plt.savefig('png/new intensity = ' + str(int(intensity)))

def compare_images(old_fname, new_fname):
    old_image = plt.imread(old_fname)
    new_image = plt.imread(new_fname)
    diff_image = old_image - new_image

    print(type(diff_image))
    print("Max/min in old image ", np.max(old_image), np.min(old_image))
    print("Max/min in new image ", np.max(new_image), np.min(new_image))
    print("Max/min in diff image ", np.max(diff_image), np.min(diff_image))
    print("Average/median in diff image ", np.average(diff_image), np.median(diff_image))
    print("Average/median of non-zero diffs ", np.average(diff_image[np.nonzero(diff_image)]),
          np.median(diff_image[np.nonzero(diff_image)]))
    print(diff_image.shape)
    
    fig, (ax1, ax2, ax3) = plt.subplots(3)
    fig.set_figheight(30)
    fig.set_figwidth(10)
    ax1.set_title('original image (from loaded XML) ' + old_fname)
    ax1.imshow(old_image)
    ax2.set_title('new image (from loaded vector via dict) ' + new_fname)
    ax2.imshow(new_image)
    ax3.set_title('difference image')
    ax3.imshow(diff_image - np.min(diff_image))
    
    print("diff image")
    plt.savefig('png/diff image' + old_fname[4:-4])

# Compare this image to the previous one
old_fname = 'png/intensity = '+str(int(intensity))+'.png'
new_fname = 'png/new intensity = '+str(int(intensity))+'.png'

compare_images(old_fname, new_fname)