#!/usr/bin/env python
# coding: utf-8

# In[1]:


import drjit as dr
import mitsuba as mi
import time
import pandas as pd
import numpy as np
import random

# mi.set_variant('cuda_mono')
mi.set_variant('llvm_mono')
mi.variants()

nm_per_ev_constant = (float(6.6260715e-34)*float(3.00e8)*float(1e9))/(float(1.6021e-19)*float(1e6))


# In[2]:


#Values for linear interpolation.
wavelength_steps = [100, 200, 230, 270, 300, 330, 370, 400, 430, 470, 500, 530, 570, 600, 630, 670, 700, 1000]
qe_steps = [0, 0, 0.02, 0.20, 0.31, 0.35, 0.35, 0.33, 0.31, 0.24, 0.18, 0.08, 0.05, 0.02, 0.01, 0.002, 0, 0]
rows_to_drop =[]

#load data from original G4 output (csv format)
column_names = ["time (ps)", "x", "y", "z", "px", "py", "pz", "E (MeV)"]
photon_data_full = pd.read_csv('./csv/Photons_1000000_filtered.csv', names = column_names)
initial_number_of_photons = photon_data_full.count()[0]

def ev_to_nm (energy):
    return nm_per_ev_constant/energy

#gives linear relation between two wavelegths thanks to two efficiency values
def linear_interp (j,w_input):
    a = (qe_steps[j] - qe_steps[j-1])/(wavelength_steps[j] - wavelength_steps[j-1])
    b = qe_steps[j-1] - a*wavelength_steps[j-1]
    return a*w_input+b

#Convert energy column to wavelength in nm.
photon_data_full['E (MeV)'] = photon_data_full['E (MeV)'].apply(ev_to_nm)
photon_data_full.rename(columns={"E (MeV)": "Wavelength (nm)"}, inplace = True)

#iterate over each photon.
for i in range(initial_number_of_photons):
    for j in range(len(wavelength_steps)):
        #find index in wavelength_steps corresponding to the photon.
        if photon_data_full.loc[i, 'Wavelength (nm)']<wavelength_steps[j]:
            break
    #Use linear interpolation to calculate QE at this wavelength.
    qe_estimated = linear_interp(j, photon_data_full.loc[i, 'Wavelength (nm)']) 
    x = random.uniform(0, 1)
    if x > qe_estimated:
        rows_to_drop.append(i)

#generate new frame only with photons that will be detected.
photon_detected = photon_data_full.drop(rows_to_drop)
#I don't know why a first column is created and keeps track of the old index of photons.
photon_detected.reset_index(drop=True, inplace= True)

#Can be removed later. Outputs characteristics of saved photons.
photon_detected.to_csv('./csv/test_new_photons_detected_spectral.csv' )
final_number_of_photons = photon_detected.count()[0]
fraction_detected = final_number_of_photons/initial_number_of_photons
print (photon_detected)
print ("{0:.4} of emitted photon in G4 are actually detected.".format(fraction_detected))


# In[3]:


# scene = mi.load_file("./xml/two_mirrors.xml")
# original_image = mi.render(scene)
# import matplotlib.pyplot as plt
# plt.figure(figsize = (20,20))
# plt.axis('off')
# plt.imshow(original_image ** (1.0 / 2.2)); 
# plt.savefig('TEST')


# In[4]:


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

gen_photon_data = generate_emitter_data(photon_detected)
photon_list = mi.VolumeGrid(gen_photon_data)

print (photon_list)
print (np.array(photon_list))
print (gen_photon_data)


# In[5]:


start_time = time.time()
# scene = mi.load_file("./xml/real_geometry_int10000.xml")

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
            'component_format': 'uint32',
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
        'intensity': 1000.0,
    },        
}

scene = mi.load_dict(scene_description)
# print(scene)

original_image = mi.render(scene)
# print(original_image)
import matplotlib.pyplot as plt
plt.figure(figsize = (20,20))
plt.axis('off')
plt.imshow(original_image ** (1.0 / 2.2)); 
end_time = time.time()
elapsed_time = end_time - start_time
print(f"Elapsed time: {elapsed_time:.2f} seconds")
print("new intensity = 1000")
plt.savefig('png/new intensity = 1000')


# In[6]:


def compare_images(old_fname, new_fname):
    old_image = plt.imread(old_fname)
    new_image = plt.imread(new_fname)
    diff_image = old_image - new_image

    print(type(diff_image))
    print(np.max(diff_image), np.min(diff_image))
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
old_fname = 'png/intensity = 1000.png'
new_fname = 'png/new intensity = 1000.png'

compare_images(old_fname, new_fname)


# In[7]:


# scene = mi.load_file("./xml/real_geometry_int500fov100.xml")
# original_image = mi.render(scene)

# start_time = time.time()

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
        'fov': 100,
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
            'component_format': 'uint32',
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
        'intensity': 500.0,
    },        
}

scene = mi.load_dict(scene_description)
# print(scene)

original_image = mi.render(scene)
import matplotlib.pyplot as plt
plt.figure(figsize = (20,20))
plt.axis('off')
plt.imshow(original_image ** (1.0 / 2.2)); 
print("new intensity = 500, fov=100")
plt.savefig('png/new intensity = 500,fov=100')

# Compare this image to the previous one
old_fname = 'png/intensity = 500,fov=100.png'
new_fname = 'png/new intensity = 500,fov=100.png'

compare_images(old_fname, new_fname)


# In[8]:


# scene = mi.load_file("./xml/real_geometry_int1000fov100.xml")
# original_image = mi.render(scene)

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
        'fov': 100,
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
            'component_format': 'uint32',
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
        'intensity': 1000.0,
    },        
}

scene = mi.load_dict(scene_description)
# print(scene)

original_image = mi.render(scene)

import matplotlib.pyplot as plt
plt.figure(figsize = (20,20))
plt.axis('off')
plt.imshow(original_image ** (1.0 / 2.2)); 
print("new intensity = 1000, fov=100")
plt.savefig('png/new intensity = 1000,fov=100')

# Compare this image to the previous one
old_fname = 'png/intensity = 1000,fov=100.png'
new_fname = 'png/new intensity = 1000,fov=100.png'

compare_images(old_fname, new_fname)


# In[9]:


# scene = mi.load_file("./xml/real_geometry_int5000.xml")
# original_image = mi.render(scene)

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
            'component_format': 'uint32',
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
        'intensity': 5000.0,
    },        
}

scene = mi.load_dict(scene_description)
# print(scene)

original_image = mi.render(scene)

import matplotlib.pyplot as plt
plt.figure(figsize = (20,20))
plt.axis('off')
plt.imshow(original_image ** (1.0 / 2.2)); 
print("new intensity = 5000")
plt.savefig('png/new intensity = 5000')

# Compare this image to the previous one
old_fname = 'png/intensity = 5000.png'
new_fname = 'png/new intensity = 5000.png'

compare_images(old_fname, new_fname)


# In[10]:


# scene = mi.load_file("./xml/real_geometry_int10000.xml")
# original_image = mi.render(scene)

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
            'component_format': 'uint32',
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
        'intensity': 10000.0,
    },        
}

scene = mi.load_dict(scene_description)
# print(scene)

original_image = mi.render(scene)

import matplotlib.pyplot as plt
plt.figure(figsize = (20,20))
plt.axis('off')
plt.imshow(original_image ** (1.0 / 2.2)); 
print("new intensity = 10000")
plt.savefig('png/new intensity = 10000')

# Compare this image to the previous one
old_fname = 'png/intensity = 10000.png'
new_fname = 'png/new intensity = 10000.png'

compare_images(old_fname, new_fname)


# In[11]:


# scene = mi.load_file("./xml/real_geometry_int15000.xml")
# original_image = mi.render(scene)

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
            'component_format': 'uint32',
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
        'intensity': 15000.0,
    },        
}

scene = mi.load_dict(scene_description)
# print(scene)

original_image = mi.render(scene)

import matplotlib.pyplot as plt
plt.figure(figsize = (20,20))
plt.axis('off')
plt.imshow(original_image ** (1.0 / 2.2)); 
print("new intensity = 15000")
plt.savefig('png/new intensity = 15000')

# Compare this image to the previous one
old_fname = 'png/intensity = 15000.png'
new_fname = 'png/new intensity = 15000.png'

compare_images(old_fname, new_fname)


# In[12]:


# scene = mi.load_file("./xml/real_geometry_int20000.xml")
# original_image = mi.render(scene)

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
            'component_format': 'uint32',
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
        'intensity': 20000.0,
    },        
}

scene = mi.load_dict(scene_description)
# print(scene)

original_image = mi.render(scene)

import matplotlib.pyplot as plt
plt.figure(figsize = (20,20))
plt.axis('off')
plt.imshow(original_image ** (1.0 / 2.2)); 
print("new intensity = 20000")
plt.savefig('png/new intensity = 20000')

# Compare this image to the previous one
old_fname = 'png/intensity = 20000.png'
new_fname = 'png/new intensity = 20000.png'

compare_images(old_fname, new_fname)


# In[13]:


# scene = mi.load_file("./xml/real_geometry_int2000000.xml")
# original_image = mi.render(scene)

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
            'component_format': 'uint32',
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
        'intensity': 2000000.0,
    },        
}

scene = mi.load_dict(scene_description)
# print(scene)

original_image = mi.render(scene)

import matplotlib.pyplot as plt
plt.figure(figsize = (20,20))
plt.axis('off')
plt.imshow(original_image ** (1.0 / 2.2)); 
print("new intensity = 2000000")
plt.savefig('png/new intensity = 2000000')

# Compare this image to the previous one
old_fname = 'png/intensity = 2000000.png'
new_fname = 'png/new intensity = 2000000.png'

compare_images(old_fname, new_fname)


# In[14]:


# # mi.set_variant('cuda_mono')
# mi.set_variant('llvm_mono')
# scene = mi.load_file("./xml/real_geometry_emittorightwall.xml")
# original_image = mi.render(scene)

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
        'fov': 20,
        'to_world': mi.ScalarTransform4f().look_at(origin=[0, 2700, 300],
                                                 target=[0, 3000, 300],
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
            'component_format': 'uint32',
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
        'intensity': 20000.0,
        # 'to_world': mi.ScalarTransform4f().look_at(origin=[0, 2700, 300],
        #                                          target=[0, 3000, 300],
        #                                          up=[0, 0, 1]).scale([3000, 3000, 3000]),        
    },        
}

scene = mi.load_dict(scene_description)
# print(scene)

original_image = mi.render(scene)


import matplotlib.pyplot as plt
plt.figure(figsize = (20,20))
plt.axis('off')
plt.imshow(original_image ** (1.0 / 2.2)); 
plt.savefig('new emit to the right wall')

# Compare this image to the previous one
old_fname = 'png/emit to the right wall.png'
new_fname = 'png/new emit to the right wall.png'

compare_images(old_fname, new_fname)


# In[17]:


# Check the one where the diff looks weird to see if it's actually 500 not 1000

# Compare this image to the previous one
old_fname = 'png/intensity = 1000,fov=100.png'
new_fname = 'png/new intensity = 500,fov=100.png'

compare_images(old_fname, new_fname)


# In[15]:


# No need to bother repeating this one as it matches another example

# # mi.set_variant('cuda_mono')
# mi.set_variant('llvm_mono')
# scene = mi.load_file("./xml/real_geometry.xml")  # assume this is the intensity=20000 that real_geometry.xml is saved with
# original_image = mi.render(scene)
# import matplotlib.pyplot as plt
# plt.figure(figsize = (20,20))
# plt.axis('off')
# plt.imshow(original_image ** (1.0 / 2.2)); 

