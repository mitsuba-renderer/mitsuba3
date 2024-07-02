import pytest
import drjit as dr
import mitsuba as mi

import numpy as np

def gen_sigma_grid(medium_height, num_layers, integral, lbd):
    scale = integral / lbd
    z = np.linspace(0., medium_height, num_layers, False) 
    ext_coefs = scale * np.exp(-z/lbd)
    ext_coefs = np.reshape(ext_coefs, (num_layers,1,1))
    return ext_coefs

# Mitsuba dictionary
def create_medium_dict():
    
    medium_height = 100000
    num_layers = 10
    integral = 3.
    lbd_dist = 8300

    ext_coefs = gen_sigma_grid(medium_height, num_layers, integral, lbd_dist )
    exp_volume_grid = mi.VolumeGrid( ext_coefs)
    sigma_scale = 1.

    medium_transform = mi.ScalarTransform4f.translate([-100000/2,-100000/2,0.]).scale([100000.,100000.,100000.])

    return mi.load_dict({
        'type': 'piecewise',
        'albedo': 0.8,
        'sigma_t':{    
            'type': 'gridvolume',
            'grid': exp_volume_grid,
            'use_grid_bbox': True,
            'filter_type': 'nearest',
            'to_world': medium_transform,
        },
        'scale': sigma_scale,
    })
        
def test01_sample_distances_down(variant_scalar_mono_double):
    medium = create_medium_dict()

    gt_dists    = [0., 74578.93910366, 82117.51365975, 88515.28423884, 98460.51859237]
    gt_trs      = [1., 0.75, 0.5, 0.25, 0.01]
    gt_pdfs     = [7.06034444e-09, 2.43563215e-05, 5.41709938e-05, 2.70854969e-05, 3.61445783e-06]

    samples = [0., 0.25, 0.5, 0.75, 0.99]
    si = mi.SurfaceInteraction3f()
    
    origins = mi.Point3f([0., 0., 100000.])
    directions = mi.Vector3f([0., 0., -1.])
    ray = mi.Ray3f(origins, directions)
    
    dists = []; trs = []; pdfs = []
    for idx, sample in enumerate(samples):
        mei, tr, pdf = medium.sample_interaction_real(ray, si, sample, 0, True)
        dists.append(mei.t); trs.append(tr); pdfs.append(pdf)

    assert dr.allclose(dists, gt_dists)
    assert dr.allclose(trs, gt_trs)
    assert dr.allclose(pdfs, gt_pdfs)

def test02_sample_distances_up(variant_scalar_mono_double):
    medium = create_medium_dict()

    gt_dists    = [0., 7.95920400e+02,  1.91770720e+03,  3.83541440e+03, 1.91443066e+04]
    gt_trs      = [1., 0.75, 0.5, 0.25, 0.01]
    gt_pdfs     = [3.61445783e-04, 2.71084337e-04, 1.80722892e-04, 9.03614458e-05, 1.08341988e-06]

    samples = [0., 0.25, 0.5, 0.75, 0.99]
    si = mi.SurfaceInteraction3f()
    
    origins = mi.Point3f([0., 0., 0.])
    directions = mi.Vector3f([0., 0., 1.])
    ray = mi.Ray3f(origins, directions)
    
    dists = []; trs = []; pdfs = []
    for idx, sample in enumerate(samples):
        mei, tr, pdf = medium.sample_interaction_real(ray, si, sample, 0, True)

        dists.append(mei.t); trs.append(tr); pdfs.append(pdf)

    print(dists)
    assert dr.allclose(dists, gt_dists)
    assert dr.allclose(trs, gt_trs)
    assert dr.allclose(pdfs, gt_pdfs)

def test03_sample_distances_horizontal(variant_scalar_mono_double):
    medium = create_medium_dict()

    gt_dists    = [ 0., 2655.31469158, 6397.77055374, 12795.54110748, 42505.86749422]
    gt_trs      = [1., 0.75, 0.5, 0.25, 0.01]
    gt_pdfs     = [1.08341988e-04, 8.12564910e-05, 5.41709940e-05, 2.70854970e-05, 1.08341988e-06]

    samples = [0., 0.25, 0.5, 0.75, 0.99]
    si = mi.SurfaceInteraction3f()
    
    origins = mi.Point3f([0., 0., 15000.])
    directions = mi.Vector3f([0., 1., 0.])
    ray = mi.Ray3f(origins, directions)
    
    dists = []; trs = []; pdfs = []
    for idx, sample in enumerate(samples):
        mei, tr, pdf = medium.sample_interaction_real(ray, si, sample, 0, True)

        dists.append(mei.t); trs.append(tr); pdfs.append(pdf)

    print(dists)
    assert dr.allclose(dists, gt_dists)
    assert dr.allclose(trs, gt_trs)
    assert dr.allclose(pdfs, gt_pdfs)

def test04_sample_distances_diag(variant_scalar_mono_double):
    medium = create_medium_dict()

    gt_dists    = [9.23464998e+00,  2.65531470e+03, 6.39777058e+03, 2.24562174e+04, dr.inf]

    samples = [0.001, 0.25, 0.5, 0.75, 0.99]
    si = mi.SurfaceInteraction3f()
    
    origins = mi.Point3f([0., 0., 15000.])
    directions = mi.Vector3f([0.5, 0.5, 0.5])
    ray = mi.Ray3f(origins, directions)
    
    dists = []; trs = []; pdfs = []
    for idx, sample in enumerate(samples):
        mei, tr, pdf = medium.sample_interaction_real(ray, si, sample, 0, True)
        dists.append(mei.t); trs.append(tr); pdfs.append(pdf)

    for idx in range(len(gt_dists)):
        if dr.isinf( gt_dists[idx] ) and dr.isinf( gt_dists[idx] ) == dr.isinf( dists[idx] ):
            gt_dists[idx] = 0.; dists[idx] = 0.

    assert dr.allclose(dists, gt_dists)

def test05_sample_distances_heights(variant_scalar_mono_double):
    
    medium = create_medium_dict()

    gt_dists    = [986.80067823, 2824.88988516,  3292.12110592, dr.inf, dr.inf, dr.inf, dr.inf]

    heights = [0.,9800.,10100.,27500.,40020.,75000,98000.]
    sample = 0.3
    direction = mi.Vector3f([0., 0., 1.])
    rays = []

    si = mi.SurfaceInteraction3f()
    
    for h in heights:
        origin = mi.Point3f([0., 0., h])
        rays.append( mi.Ray3f(origin, direction) )

    dists = []; trs = []; pdfs = []
    for idx, ray in enumerate(rays):
        mei, tr, pdf = medium.sample_interaction_real(ray, si, sample, 0, True)
        dists.append(mei.t); trs.append(tr); pdfs.append(pdf)

    for idx in range(len(gt_dists)):
        if dr.isinf( gt_dists[idx] ) and dr.isinf( gt_dists[idx] ) == dr.isinf( dists[idx] ):
            gt_dists[idx] = 0.; dists[idx] = 0.

    print(dists)
    assert dr.allclose(dists, gt_dists)

def test06_eval_transmittance_up(variant_scalar_mono_double):

    medium = create_medium_dict()

    gt_trs = [0.00573247, 0.03493101, 0.36588307, 0.7398143,  0.97331388, 0.99930119, 1.]

    heights = [0.,5000.,15000.,25000.,45000.,75000,100000.]
    direction = [0.,0.,1.]
    rays = []
    si = dr.zeros(mi.SurfaceInteraction3f,1)
    si.t = dr.inf

    for h in heights:
        origin = [0.,0.,h]
        rays.append( mi.Ray3f(origin, direction) )

    trs = []; pdfs = []
    for idx, ray in enumerate(rays):
        tr, pdf, _ = medium.eval_transmittance_pdf_real(ray, si, 0, True)
        trs.append(tr); pdfs.append(pdf)

    print(trs)
    assert dr.allclose(trs, gt_trs)

def test07_eval_transmittance_down(variant_scalar_mono):

    dr.set_thread_count(0)
    from mitsuba import Thread, LogLevel
    Thread.thread().logger().set_log_level(LogLevel.Debug)

    medium = create_medium_dict()

    gt_trs = [1., 0.03865759, 0.01566748, 0.00663011, 0.00588964, 0.00573872, 0.00573247]

    heights = [0.0,9000.,15000.,29800.,45000.,70020,100000.]
    direction = [0.,0.,-1.]
    rays = []
    si = dr.zeros(mi.SurfaceInteraction3f,1)
    si.t = dr.inf

    for h in heights:
        origin = [0.,0.,h]
        rays.append( mi.Ray3f(origin, direction) )

    trs = []; pdfs = []
    for idx, ray in enumerate(rays):
        tr, pdf, _ = medium.eval_transmittance_pdf_real(ray, si, 0, True)
        trs.append(tr); pdfs.append(pdf)

    print(trs)
    assert dr.allclose(trs, gt_trs)
