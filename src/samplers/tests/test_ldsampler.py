import pytest
import drjit as dr
import mitsuba as mi

from .utils import (
    check_uniform_scalar_sampler,
    check_uniform_wavefront_sampler,
    check_deep_copy_sampler_scalar,
    check_deep_copy_sampler_wavefront,
    check_sampler_kernel_hash_wavefront,
)


def test01_ldsampler_scalar(variant_scalar_rgb):
    sampler = mi.load_dict({
        "type" : "ldsampler",
        "sample_count" : 1024,
    })
    sampler.seed(0)

    check_uniform_scalar_sampler(sampler)


def test02_ldsampler_wavefront(variants_vec_backends_once):
    sampler = mi.load_dict({
        "type" : "ldsampler",
        "sample_count" : 1024,
    })
    sampler.seed(0, 1024)

    check_uniform_wavefront_sampler(sampler)


def test03_ldsampler_deterministic_values(variant_scalar_rgb):
    sampler = mi.load_dict({
        "type" : "ldsampler",
        "sample_count" : 1024,
    })

    sampler.seed(0)

    values_1d_dim0 = [0.06483602523803711, 0.2767500877380371, 0.006242275238037109, 0.6273360252380371, 0.6273360252380371,
                      0.1634688377380371, 0.3734297752380371, 0.7845625877380371, 0.7415938377380371, 0.4085860252380371]

    values_2d_dim0 = [[0.014822006225585938, 0.06154896318912506], [0.34001731872558594, 0.7011973857879639], [0.7570095062255859, 0.8974864482879639], [0.7374782562255859, 0.8076426982879639], [0.7794704437255859, 0.26174429059028625], [
        0.5060329437255859, 0.8945567607879639], [0.16911888122558594, 0.6533458232879639], [0.34294700622558594, 0.9521739482879639], [0.16618919372558594, 0.09377552568912506], [0.15935325622558594, 0.44826772809028625]]

    values_1d_dim1 = [0.7005782127380371, 0.9085860252380371, 0.7601485252380371, 0.3939375877380371, 0.4876875877380371,
                      0.6576094627380371, 0.6908125877380371, 0.2113204002380371, 0.4847579002380371, 0.7865157127380371]

    values_2d_dim1 = [[0.8956813812255859, 0.03615833818912506], [0.8400173187255859, 0.20119740068912506], [0.27556419372558594, 0.23440052568912506], [0.026540756225585938, 0.23733021318912506], [0.0021266937255859375, 0.6015880107879639], [
        0.38884544372558594, 0.6836192607879639], [0.8956813812255859, 0.03615833818912506], [0.9542751312255859, 0.02443958818912506], [0.8409938812255859, 0.9502208232879639], [0.5636501312255859, 0.6650645732879639]]

    res = []
    for v in range(len(values_1d_dim0)):
        res.append(sampler.next_1d())
    assert dr.allclose(res, values_1d_dim0)
    # print(res)

    res = []
    for v in range(len(values_1d_dim0)):
        res.append(sampler.next_2d())
    assert dr.allclose(res, values_2d_dim0)
    # print(res)

    sampler.advance()

    res = []
    for v in range(len(values_1d_dim0)):
        res.append(sampler.next_1d())
    assert dr.allclose(res, values_1d_dim1)
    # print(res)

    res = []
    for v in range(len(values_1d_dim0)):
        res.append(sampler.next_2d())
    assert dr.allclose(res, values_2d_dim1)
    # print(res)


def test04_copy_sampler_scalar(variants_any_scalar):
    sampler = mi.load_dict({
        "type" : "ldsampler",
        "sample_count" : 1024,
    })
    sampler.seed(0)

    check_deep_copy_sampler_scalar(sampler)


def test05_copy_sampler_wavefront(variants_vec_backends_once):
    sampler = mi.load_dict({
        "type" : "ldsampler",
        "sample_count" : 1024,
    })
    sampler.seed(0, 1024)

    check_deep_copy_sampler_wavefront(sampler)
    
def test06_jit_seed(variants_vec_rgb):
    sampler = mi.load_dict({
        "type": "independent",
    })
    seed = mi.UInt(0)
    state_before = seed.state
    sampler.seed(seed, 64)
    assert seed.state == state_before

    check_sampler_kernel_hash_wavefront(mi.UInt, sampler)
