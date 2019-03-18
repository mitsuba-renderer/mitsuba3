#if defined(_MSC_VER)
#  define NOMINMAX
#  define strcasecmp _stricmp
#endif

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <algorithm>

#include "details/cie1931.h"
#include "details/lu.h"

/*
   Potential coefficient conventions:

   RGB2SPEC_MAPPING == 1:
      Compute coefficients for polynomials defined on the interval [360, 830].
      This variant is the fastest to evaluate in a renderer.

   RGB2SPEC_MAPPING == 2:
      Evaluate the polynomial at wavelengths [445.772, 539.285, 602.785],
      which correspond to the peaks of the CIE RGB color matching curves.
      The polynomial can be reconstructed from these values via a 3x3
      matrix multiplication. This mapping is more linear and better-suited
      for optimization purposes.

*/

#if !defined(RGB2SPEC_MAPPING)
#  define RGB2SPEC_MAPPING 1
#endif

// Choose a parallelization scheme
#if defined(RGB2SPEC_USE_TBB)
#  include <tbb/tbb.h>
#elif defined(_OPENMP)
#  define RGB2SPEC_USE_OPENMP 1
#elif defined(__APPLE__)
#  define RGB2SPEC_USE_GCD    1
#  include <dispatch/dispatch.h>
#endif

/// Discretization of quadrature scheme
#define CIE_FINE_SAMPLES ((CIE_SAMPLES - 1) * 3 + 1)
#define RGB2SPEC_EPSILON 1e-4

/// Precomputed tables for fast spectral -> RGB conversion
double lambda_tbl[CIE_FINE_SAMPLES],
       rgb_tbl[3][CIE_FINE_SAMPLES],
       rgb_to_xyz[3][3],
       xyz_to_rgb[3][3],
       xyz_whitepoint[3];

/// Currently supported gamuts
enum Gamut {
    SRGB,
    ProPhotoRGB,
    ACES2065_1,
    REC2020,
    ERGB,
    XYZ,
    NO_GAMUT,
};

double sigmoid(double x) {
    return 0.5 * x / std::sqrt(1.0 + x * x) + 0.5;
}

double smoothstep(double x) {
    return x * x * (3.0 - 2.0 * x);
}

double sqr(double x) { return x * x; }

void cie_lab(double *p) {
    double X = 0.0, Y = 0.0, Z = 0.0,
      Xw = xyz_whitepoint[0],
      Yw = xyz_whitepoint[1],
      Zw = xyz_whitepoint[2];

    for (int j = 0; j < 3; ++j) {
        X += p[j] * rgb_to_xyz[0][j];
        Y += p[j] * rgb_to_xyz[1][j];
        Z += p[j] * rgb_to_xyz[2][j];
    }

    auto f = [](double t) -> double {
        double delta = 6.0 / 29.0;
        if (t > delta*delta*delta)
            return cbrt(t);
        else
            return t / (delta*delta * 3.0) + (4.0 / 29.0);
    };

    p[0] = 116.0 * f(Y / Yw) - 16.0;
    p[1] = 500.0 * (f(X / Xw) - f(Y / Yw));
    p[2] = 200.0 * (f(Y / Yw) - f(Z / Zw));
}

/**
 * This function precomputes tables used to convert arbitrary spectra
 * to RGB (either sRGB or ProPhoto RGB)
 *
 * A composite quadrature rule integrates the CIE curves, reflectance, and
 * illuminant spectrum over each 5nm segment in the 360..830nm range using
 * Simpson's 3/8 rule (4th-order accurate), which evaluates the integrand at
 * four positions per segment. While the CIE curves and illuminant spectrum are
 * linear over the segment, the reflectance could have arbitrary behavior,
 * hence the extra precations.
 */
void init_tables(Gamut gamut) {
    memset(rgb_tbl, 0, sizeof(rgb_tbl));
    memset(xyz_whitepoint, 0, sizeof(xyz_whitepoint));

    double h = (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN) / (CIE_FINE_SAMPLES - 1);

    const double *illuminant = nullptr;

    switch (gamut) {
        case SRGB:
            illuminant = cie_d65;
            memcpy(xyz_to_rgb, xyz_to_srgb, sizeof(double) * 9);
            memcpy(rgb_to_xyz, srgb_to_xyz, sizeof(double) * 9);
            break;

        case ERGB:
            illuminant = cie_e;
            memcpy(xyz_to_rgb, xyz_to_ergb, sizeof(double) * 9);
            memcpy(rgb_to_xyz, ergb_to_xyz, sizeof(double) * 9);
            break;

        case XYZ:
            illuminant = cie_e;
            memcpy(xyz_to_rgb, xyz_to_xyz, sizeof(double) * 9);
            memcpy(rgb_to_xyz, xyz_to_xyz, sizeof(double) * 9);
            break;

        case ProPhotoRGB:
            illuminant = cie_d50;
            memcpy(xyz_to_rgb, xyz_to_prophoto_rgb, sizeof(double) * 9);
            memcpy(rgb_to_xyz, prophoto_rgb_to_xyz, sizeof(double) * 9);
            break;

        case ACES2065_1:
            illuminant = cie_d60;
            memcpy(xyz_to_rgb, xyz_to_aces2065_1, sizeof(double) * 9);
            memcpy(rgb_to_xyz, aces2065_1_to_xyz, sizeof(double) * 9);
            break;

        case REC2020:
            illuminant = cie_d65;
            memcpy(xyz_to_rgb, xyz_to_rec2020, sizeof(double) * 9);
            memcpy(rgb_to_xyz, rec2020_to_xyz, sizeof(double) * 9);
            break;

        default:
            throw std::runtime_error("init_gamut(): invalid/unsupported gamut.");
    }

    for (int i = 0; i < CIE_FINE_SAMPLES; ++i) {
        double lambda = CIE_LAMBDA_MIN + i * h;

        double xyz[3] = { cie_interp(cie_x, lambda),
                          cie_interp(cie_y, lambda),
                          cie_interp(cie_z, lambda) },
               I = cie_interp(illuminant, lambda);

        double weight = 3.0 / 8.0 * h;
        if (i == 0 || i == CIE_FINE_SAMPLES - 1)
            ;
        else if ((i - 1) % 3 == 2)
            weight *= 2.f;
        else
            weight *= 3.f;

        lambda_tbl[i] = lambda;
        for (int k = 0; k < 3; ++k)
            for (int j = 0; j < 3; ++j)
                rgb_tbl[k][i] += xyz_to_rgb[k][j] * xyz[j] * I * weight;

        for (int i = 0; i < 3; ++i)
            xyz_whitepoint[i] += xyz[i] * I * weight;
    }
}

void eval_residual(const double *coeffs, const double *rgb, double *residual) {
    double out[3] = { 0.0, 0.0, 0.0 };

    for (int i = 0; i < CIE_FINE_SAMPLES; ++i) {
        /* Scale lambda to 0..1 range */
        double lambda = (lambda_tbl[i] - CIE_LAMBDA_MIN) /
                        (CIE_LAMBDA_MAX - CIE_LAMBDA_MIN);

        /* Polynomial */
        double x = 0.0;
        for (int i = 0; i < 3; ++i)
            x = x * lambda + coeffs[i];

        /* Sigmoid */
        double s = sigmoid(x);

        /* Integrate against precomputed curves */
        for (int j = 0; j < 3; ++j)
            out[j] += rgb_tbl[j][i] * s;
    }
    cie_lab(out);
    memcpy(residual, rgb, sizeof(double) * 3);
    cie_lab(residual);

    for (int j = 0; j < 3; ++j)
        residual[j] -= out[j];
}

void eval_jacobian(const double *coeffs, const double *rgb, double **jac) {
    double r0[3], r1[3], tmp[3];

    for (int i = 0; i < 3; ++i) {
        memcpy(tmp, coeffs, sizeof(double) * 3);
        tmp[i] -= RGB2SPEC_EPSILON;
        eval_residual(tmp, rgb, r0);

        memcpy(tmp, coeffs, sizeof(double) * 3);
        tmp[i] += RGB2SPEC_EPSILON;
        eval_residual(tmp, rgb, r1);

        for (int j = 0; j < 3; ++j)
            jac[j][i] = (r1[j] - r0[j]) * 1.0 / (2 * RGB2SPEC_EPSILON);
    }
}

double gauss_newton(const double rgb[3], double coeffs[3], int it = 15) {
    double r = 0;
    for (int i = 0; i < it; ++i) {
        double J0[3], J1[3], J2[3], *J[3] = { J0, J1, J2 };

        double residual[3];

        eval_residual(coeffs, rgb, residual);
        eval_jacobian(coeffs, rgb, J);

        int P[4];
        int rv = LUPDecompose(J, 3, 1e-15, P);
        if (rv != 1) {
            std::cout << "RGB " << rgb[0] << " " << rgb[1] << " " << rgb[2] << std::endl;
            std::cout << "-> " << coeffs[0] << " " << coeffs[1] << " " << coeffs[2] << std::endl;
            throw std::runtime_error("LU decomposition failed!");
        }

        double x[3];
        LUPSolve(J, P, residual, 3, x);

        r = 0.0;
        for (int j = 0; j < 3; ++j) {
            coeffs[j] -= x[j];
            r += residual[j] * residual[j];
        }
        double max = std::max(std::max(coeffs[0], coeffs[1]), coeffs[2]);

        if (max > 200) {
            for (int j = 0; j < 3; ++j)
                coeffs[j] *= 200 / max;
        }

        if (r < 1e-6)
            break;
    }
    return std::sqrt(r);
}

static Gamut parse_gamut(const char *str) {
    if (!strcasecmp(str, "sRGB"))
        return SRGB;
    if (!strcasecmp(str, "eRGB"))
        return ERGB;
    if (!strcasecmp(str, "XYZ"))
        return XYZ;
    if (!strcasecmp(str, "ProPhotoRGB"))
        return ProPhotoRGB;
    if (!strcasecmp(str, "ACES2065_1"))
        return ACES2065_1;
    if (!strcasecmp(str, "REC2020"))
        return REC2020;
    return NO_GAMUT;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Syntax: rgb2spec_opt <resolution> <output> [<gamut>]\n"
               "where <gamut> is one of sRGB,eRGB,XYZ,ProPhotoRGB,ACES2065_1,REC2020\n");
        exit(-1);
    }
    Gamut gamut = SRGB;
    if (argc > 3) gamut = parse_gamut(argv[3]);
    if (gamut == NO_GAMUT) {
        fprintf(stderr, "Could not parse gamut `%s'!\n", argv[3]);
        exit(-1);
    }
    init_tables(gamut);

    const int res = atoi(argv[1]);
    if (res == 0) {
        printf("Invalid resolution!\n");
        exit(-1);
    }

    printf("Optimizing spectra ");

    float *scale = new float[res];
    for (int k = 0; k < res; ++k)
        scale[k] = (float) smoothstep(smoothstep(k / double(res - 1)));

    size_t bufsize = 3*3*res*res*res;
    float *out = new float[bufsize];

#if defined(RGB2SPEC_USE_OPENMP)
#  pragma omp parallel for collapse(2) default(none) schedule(dynamic) shared(stdout,scale,out)
#endif
    for (int l = 0; l < 3; ++l) {
#if defined(RGB2SPEC_USE_TBB)
        tbb::parallel_for(0, res, [&](size_t j) {
#elif defined(RGB2SPEC_USE_GCD)
        dispatch_apply(res, dispatch_get_global_queue(0, 0), ^(size_t j) {
#else
        for (int j = 0; j < res; ++j) {
#endif
            const double y = j / double(res - 1);
            printf(".");
            fflush(stdout);
            for (int i = 0; i < res; ++i) {
                const double x = i / double(res - 1);
                double coeffs[3], rgb[3];
                memset(coeffs, 0, sizeof(double)*3);

                int start = res / 5;

                for (int k = start; k < res; ++k) {
                    double b = (double) scale[k];

                    rgb[l] = b;
                    rgb[(l + 1) % 3] = x*b;
                    rgb[(l + 2) % 3] = y*b;

                    double resid = gauss_newton(rgb, coeffs);
                    (void) resid;

                    double A = coeffs[0], B = coeffs[1], C = coeffs[2];

                    size_t idx = ((l*res + k) * res + j)*res+i;
#if RGB2SPEC_MAPPING == 1
                    double c0 = 360.0, c1 = 1.0 / (830.0 - 360.0);

                    double As = A*(sqr(c1)),
                           Bs = B*c1 - 2*A*c0*(sqr(c1)),
                           Cs = C - B*c0*c1 + A*(sqr(c0*c1));

                    out[3*idx + 0] = float(As);
                    out[3*idx + 1] = float(Bs);
                    out[3*idx + 2] = float(Cs);
#elif RGB2SPEC_MAPPING == 2
                    auto eval = [&](double x) { return (A*x + B)*x + C; };

                    double Rs = eval((602.785 - 360.0) / (830.0 - 360.0)),
                           Gs = eval((539.285 - 360.0) / (830.0 - 360.0)),
                           Bs = eval((445.772 - 360.0) / (830.0 - 360.0));

                    out[3*idx + 0] = float(Rs);
                    out[3*idx + 1] = float(Gs);
                    out[3*idx + 2] = float(Bs);
#else
#  error RGB_SPEC_MAPPING has unexpected value.
#endif
                }

                memset(coeffs, 0, sizeof(double)*3);
                for (int k = start; k>=0; --k) {
                    double b = (double) scale[k];

                    rgb[l] = b;
                    rgb[(l + 1) % 3] = x*b;
                    rgb[(l + 2) % 3] = y*b;

                    double resid = gauss_newton(rgb, coeffs);
                    (void) resid;

                    double A = coeffs[0], B = coeffs[1], C = coeffs[2];

                    size_t idx = ((l*res + k) * res + j)*res+i;
#if RGB2SPEC_MAPPING == 1
                    double c0 = 360.0, c1 = 1.0 / (830.0 - 360.0);

                    double As = A*(sqr(c1)),
                           Bs = B*c1 - 2*A*c0*(sqr(c1)),
                           Cs = C - B*c0*c1 + A*(sqr(c0*c1));

                    out[3*idx + 0] = float(As);
                    out[3*idx + 1] = float(Bs);
                    out[3*idx + 2] = float(Cs);
#elif RGB2SPEC_MAPPING == 2
                    auto eval = [&](double x) { return (A*x + B)*x + C; };

                    double Rs = eval((602.785 - 360.0) / (830.0 - 360.0)),
                           Gs = eval((539.285 - 360.0) / (830.0 - 360.0)),
                           Bs = eval((445.772 - 360.0) / (830.0 - 360.0));

                    out[3*idx + 0] = float(Rs);
                    out[3*idx + 1] = float(Gs);
                    out[3*idx + 2] = float(Bs);
#else
#  error RGB_SPEC_MAPPING has unexpected value.
#endif
                }
            }
        }
#if defined(RGB2SPEC_USE_TBB) || defined(RGB2SPEC_USE_GCD)
        );
#endif
    }

    FILE *f = fopen(argv[2], "wb");
    if (f == nullptr)
        throw std::runtime_error("Could not create file!");
    fwrite("SPEC", 4, 1, f);
    uint32_t resolution = res;
    fwrite(&resolution, sizeof(uint32_t), 1, f);
    fwrite(scale, res * sizeof(float), 1, f);

    fwrite(out, sizeof(float)*bufsize, 1, f);
    delete[] out;
    delete[] scale;
    fclose(f);
    printf(" done.\n");
}
