import numpy as np
import mitsuba as mi
import drjit as dr

EPS = 1e-4

V = np.array([
    [0, 0, 0],  # 0
    [2, 0, 0],  # 1
    [2, 1, 0],  # 2
    [0, 1, 0],  # 3
    [0, 1, 1],  # 4
    [2, 1, 1],  # 5
    [2, 0, 1],  # 6
    [0, 0, 1],  # 7
], dtype=np.float32)
F = np.array([
    [0, 2, 1], [0, 3, 2], # Front
    [2, 3, 4], [2, 4, 5], # Top
    [1, 2, 5], [1, 5, 6], # Right
    [0, 7, 4], [0, 4, 3], # Left
    [5, 4, 7], [5, 7, 6], # Back
    [0, 6, 7], [0, 1, 6], # Bottom
], dtype=np.uint32)

def __test_roundtrip(laplacian):
    v_mi, f_mi = mi.Float(V.flatten()), mi.UInt(F.flatten())

    # Lambda
    lambda_ = 25
    sopt = mi.ad.ShapeOptimizer(v_mi, f_mi, lambda_=lambda_, alpha=None, laplacian=laplacian)
    roundtrip = sopt.from_differential(sopt.to_differential(v_mi))
    assert dr.allclose(v_mi, roundtrip, atol=EPS)

    # Alpha
    alpha = 0.95
    sopt = mi.ad.ShapeOptimizer(v_mi, f_mi, lambda_=None, alpha=alpha, laplacian=laplacian)
    roundtrip = sopt.from_differential(sopt.to_differential(v_mi))
    assert dr.allclose(v_mi, roundtrip, atol=EPS)

def test01_combinatorial_roundtrip(variants_all_ad_rgb):
    combi = lambda _, f: mi.ad.combinatorial_laplacian(f)
    __test_roundtrip(combi)

def test02_cotangent_roundtrip(variants_all_ad_rgb):
    cot = mi.ad.cotangent_laplacian
    __test_roundtrip(cot)

def test03_kernel_roundtrip(variants_all_ad_rgb):
    ker = lambda v, f: mi.ad.kernel_laplacian(v, f, bandwidth=0.005)
    __test_roundtrip(ker)

def test04_adaptive_combinatorial_roundtrip(variants_all_ad_rgb):
    combi = lambda _, f: mi.ad.combinatorial_laplacian(f)
    ada = lambda v, f: mi.ad.adaptive_laplacian(v, f, weight=0.3, pilot=combi, power=7)
    __test_roundtrip(ada)

def test05_adaptive_cotangent_roundtrip(variants_all_ad_rgb):
    cot = mi.ad.cotangent_laplacian
    ada = lambda v, f: mi.ad.adaptive_laplacian(v, f, weight=0.3, pilot=cot, power=7)
    __test_roundtrip(ada)

def test06_adaptive_kernel_roundtrip(variants_all_ad_rgb):
    ker = lambda v, f: mi.ad.kernel_laplacian(v, f, bandwidth=0.005)
    ada = lambda v, f: mi.ad.adaptive_laplacian(v, f, weight=0.3, pilot=ker, power=7)
    __test_roundtrip(ada)

def test07_largesteps_roundtrip(variants_all_ad_rgb):
    v_mi, f_mi = mi.Float(V.flatten()), mi.UInt(F.flatten())

    # Lambda
    lambda_ = 25
    ls = mi.ad.LargeSteps(v_mi, f_mi, lambda_=lambda_, alpha=None)
    roundtrip = ls.from_differential(ls.to_differential(v_mi))
    assert dr.allclose(v_mi, roundtrip, atol=EPS)

    # Alpha
    alpha = 0.95
    ls = mi.ad.LargeSteps(v_mi, f_mi, lambda_=None, alpha=alpha)
    roundtrip = ls.from_differential(ls.to_differential(v_mi))
    assert dr.allclose(v_mi, roundtrip, atol=EPS)

def test08_non_unique_vertices(variants_all_ad_rgb):
    v = mi.Float(np.array([
        [0, 0, 0],  # 0
        [0, 0, 0],  # 0
        [2, 0, 0],  # 1
        [2, 1, 0],  # 2
        [2, 1, 0],  # 2 
        [0, 1, 0],  # 3
        [0, 1, 1],  # 4
        [0, 1, 1],  # 4
        [2, 1, 1],  # 5
        [2, 0, 1],  # 6
        [0, 0, 1],  # 7
    ], dtype=np.float32).flatten())

    sopt = mi.ad.ShapeOptimizer(v, mi.UInt(F.flatten()), lambda_=25, alpha=None, laplacian=mi.ad.cotangent_laplacian)
    assert sopt.n_verts == 8
