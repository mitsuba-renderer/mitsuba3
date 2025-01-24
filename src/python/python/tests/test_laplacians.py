import math
import numpy as np
import mitsuba as mi

EPS = 1e-6

# Tetrahedron
V_TETRA = np.array([
    [ 1,  1,  1],
    [-1, -1,  1],
    [-1,  1, -1],
    [ 1, -1, -1],
], dtype=np.float32)
F_TETRA = np.array([
    [0, 1, 2],
    [0, 1, 3],
    [0, 2, 3],
    [1, 2, 3],
], dtype=np.uint32)

# RECTANGLE
V_REC = np.array([
    [0, 0, 0],  # 0
    [2, 0, 0],  # 1
    [2, 1, 0],  # 2
    [0, 1, 0],  # 3
    [0, 1, 1],  # 4
    [2, 1, 1],  # 5
    [2, 0, 1],  # 6
    [0, 0, 1],  # 7
], dtype=np.float32)
F_REC = np.array([
    [0, 2, 1], [0, 3, 2], # Front
    [2, 3, 4], [2, 4, 5], # Top
    [1, 2, 5], [1, 5, 6], # Right
    [0, 7, 4], [0, 4, 3], # Left
    [5, 4, 7], [5, 7, 6], # Back
    [0, 6, 7], [0, 1, 6], # Bottom
], dtype=np.uint32)

def coo_to_dense(idx, val):
    num_rows = idx[0].max() + 1
    num_cols = idx[1].max() + 1
    dense = np.zeros((num_rows, num_cols), dtype=val.dtype)
    dense[idx[0], idx[1]] = val
    return dense

def is_symmetric(m):
    return np.allclose(m, m.T, atol=EPS)
    
def test01_combinatorial_tetrahedron(variants_all_ad_rgb):
    expected = np.array([
        [ 0, -1, -1, -1],
        [-1,  0, -1, -1],
        [-1, -1,  0, -1],
        [-1, -1, -1,  0]
    ])

    idx, val = mi.ad.combinatorial_laplacian(F_TETRA)
    dense = coo_to_dense(idx, val) 

    assert is_symmetric(dense)
    assert np.all(dense == expected)

def test02_combinatorial_rectangle(variants_all_ad_rgb):
    expected = np.array([
        [ 0, -1, -1, -1, -1,  0, -1, -1],
        [-1,  0, -1,  0,  0, -1, -1,  0],
        [-1, -1,  0, -1, -1, -1,  0,  0],
        [-1,  0, -1,  0, -1,  0,  0,  0],
        [-1,  0, -1, -1,  0, -1,  0, -1],
        [ 0, -1, -1,  0, -1,  0, -1, -1],
        [-1, -1,  0,  0,  0, -1,  0, -1],
        [-1,  0,  0,  0, -1, -1, -1,  0],
    ])

    idx, val = mi.ad.combinatorial_laplacian(F_REC)
    dense = coo_to_dense(idx, val) 

    assert is_symmetric(dense)
    assert np.all(dense == expected)

def test03_cotangent_tetrahedron(variants_all_ad_rgb):
    cot60 = 1 / math.tan(math.pi/3)
    v = cot60
    expected = np.array([
        [ 0, -v, -v, -v],
        [-v,  0, -v, -v],
        [-v, -v,  0, -v],
        [-v, -v, -v,  0]
    ])

    idx, val = mi.ad.cotangent_laplacian(V_TETRA, F_TETRA)
    dense = coo_to_dense(idx, val) 

    assert is_symmetric(dense)
    assert np.allclose(dense, expected, atol=EPS)

def test04_cotangent_rectangle(variants_all_ad_rgb):
    expected = np.array([
        [ 0. , -0.5,  0. , -1.5, -0. ,  0. ,  0. , -1.5],
        [-0.5,  0. , -1.5,  0. ,  0. , -0. , -1.5,  0. ],
        [ 0. , -1.5,  0. , -0.5,  0. , -1.5,  0. ,  0. ],
        [-1.5,  0. , -0.5,  0. , -1.5,  0. ,  0. ,  0. ],
        [-0. ,  0. ,  0. , -1.5,  0. , -0.5,  0. , -1.5],
        [ 0. , -0. , -1.5,  0. , -0.5,  0. , -1.5,  0. ],
        [ 0. , -1.5,  0. ,  0. ,  0. , -1.5,  0. , -0.5],
        [-1.5,  0. ,  0. ,  0. , -1.5,  0. , -0.5,  0. ],
    ])

    idx, val = mi.ad.cotangent_laplacian(V_REC, F_REC)
    dense = coo_to_dense(idx, val) 

    assert is_symmetric(dense)
    assert np.allclose(dense, expected, atol=EPS)

def test05_kernel_tetrahedron(variants_all_ad_rgb):
    def weight(dist_sqr, bandwidth):
        return 1/(4*math.pi*bandwidth**2) * math.exp(-dist_sqr/(4*bandwidth))
    
    bandwidth = 0.01
    dist_sqr = 8
    dual_area = 2/3 * (math.sqrt(3) * dist_sqr / 4) # Area of tetrahedron = sqrt(3) * a^2
    w = weight(dist_sqr, bandwidth) * dual_area
    expected = np.array([
        [ 0, -w, -w, -w],
        [-w,  0, -w, -w],
        [-w, -w,  0, -w],
        [-w, -w, -w,  0]
    ])

    idx, val = mi.ad.kernel_laplacian(V_TETRA, F_TETRA, bandwidth)
    dense = coo_to_dense(idx, val) 

    assert is_symmetric(dense)
    assert np.allclose(dense, expected, atol=EPS)

def test06_kernel_rectangle(variants_all_ad_rgb):
    def weight(dist_sqr, bandwidth):
        return 1/(4*math.pi*bandwidth**2) * math.exp(-dist_sqr/(4*bandwidth))
    bandwidth = 1/4

    # Weights for each squared distance (1, 2, 4, 5)
    w1 = -weight(1, bandwidth)
    w2 = -weight(2, bandwidth)
    w4 = -weight(4, bandwidth)
    w5 = -weight(5, bandwidth)

    # Dual area for each pair of adjacent vertices
    dual_areas = np.array([
        [  0, 2/3, 2/3, 1/2, 1/3,   0, 2/3, 1/2],
        [2/3,   0, 1/2,   0,   0, 1/3, 1/2,   0],
        [2/3, 1/2,   0, 2/3, 2/3, 1/2,   0,   0],
        [1/2,   0, 2/3,   0, 1/2,   0,   0,   0],
        [1/3,   0, 2/3, 1/2,   0, 2/3,   0, 1/2], 
        [  0, 1/3, 1/2,   0, 2/3,   0, 1/2, 2/3], 
        [2/3, 1/2,   0,   0,   0, 1/2,   0, 2/3], 
        [1/2,   0,   0,   0, 1/2, 2/3, 2/3,   0],
    ])
    expected = np.array([
        [ 0, w4, w5, w1, w2,  0, w5, w1],
        [w4,  0, w1,  0,  0, w2, w1,  0],
        [w5, w1,  0, w4, w5, w1,  0,  0],
        [w1,  0, w4,  0, w1,  0,  0,  0],
        [w2,  0, w5, w1,  0, w4,  0, w1],
        [ 0, w2, w1,  0, w4,  0, w1, w5],
        [w5, w1,  0,  0,  0, w1,  0, w4],
        [w1,  0,  0,  0, w1, w5, w4,  0],
    ]) * dual_areas

    idx, val = mi.ad.kernel_laplacian(V_REC, F_REC, bandwidth)
    dense = coo_to_dense(idx, val)

    assert is_symmetric(dense)
    assert np.allclose(dense, expected, atol=EPS)

def test07_adaptive_bandwidth(variants_all_ad_rgb):
    # Weight: C, user-defined
    # Power: d, user-defined
    # Dual area: A_i = 1/3 * (sum of the areas of the faces adjacent to x_i)
    # Laplacian: Lx_i = 1/A_i * sum_j { gamma_ij * (x_i - x_j) }
    # Bandwidth: h_i = (C * A_i / (pi * norm(Lx_i)^2))^(1/d)
    C = 0.3
    d = 7

    # Computed with reference implementation:
    # (https://github.com/CGLab-GIST/awl/blob/983ab0f9b592fece3ff6f5218ce69ff356a6dec2/lap.py#L133)
    expected = np.full(len(V_TETRA), 0.56772524)

    # Using the combinatorial laplacian here because our implementation of
    # the cotangent and kernelized ones slightly differ from awl base repo.
    h = mi.ad.adaptive_bandwidth(
        V_TETRA,
        F_TETRA,
        weight=C,
        pilot=lambda _, f: mi.ad.combinatorial_laplacian(f),
        power=d,
    )

    assert np.allclose(h, expected, atol=EPS)

def test08_adaptive_laplacian(variants_all_ad_rgb):
    C = 0.3
    pilot = lambda _, f: mi.ad.combinatorial_laplacian(f)
    d = 7

    # Computed with reference implementation:
    # (https://github.com/CGLab-GIST/awl/blob/983ab0f9b592fece3ff6f5218ce69ff356a6dec2/lap.py#L133)
    expected = np.array([
        [0.        , -0.02670011, -0.01424786, -0.13500164, -0.04846852,
         0.        , -0.01417272, -0.13748333],
       [-0.02670011,  0.        , -0.13964212,  0.        ,  0.        ,
        -0.04901689, -0.14048023,  0.        ],
       [-0.01424786, -0.13964212,  0.        , -0.02706333, -0.01409422,
        -0.13833174,  0.        ,  0.        ],
       [-0.13500164,  0.        , -0.02706333,  0.        , -0.13665   ,
         0.        ,  0.        ,  0.        ],
       [-0.04846852,  0.        , -0.01409422, -0.13665   ,  0.        ,
        -0.02667827,  0.        , -0.13916199],
       [ 0.        , -0.04901689, -0.13833174,  0.        , -0.02667827,
         0.        , -0.13916199, -0.01401989],
       [-0.01417272, -0.14048023,  0.        ,  0.        ,  0.        ,
        -0.13916199,  0.        , -0.02680333],
       [-0.13748333,  0.        ,  0.        ,  0.        , -0.13916199,
        -0.01401989, -0.02680333,  0.        ]
    ])

    # Using the combinatorial laplacian here because our implementation of
    # the cotangent and kernelized ones slightly differ from awl base repo.
    idx, val = mi.ad.adaptive_laplacian(V_REC, F_REC, weight=C, pilot=pilot, power=d)
    dense = coo_to_dense(idx, val)
    assert is_symmetric(dense)
    assert np.allclose(dense, expected, atol=EPS)
