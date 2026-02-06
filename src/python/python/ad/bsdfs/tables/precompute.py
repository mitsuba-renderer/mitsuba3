# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# This code is derived from Blender Cycles and was developed at Meta.

"""
Precomputed GGX tables utilities and CLI.

This module provides:
- fetch_table(name): Load precomputed microfacet-related tables (stored as .npy files
  alongside this module) into Dr.Jit/Mitsuba textures for fast lookup during rendering.
- A command-line interface (when run as a script) to generate and cache these tables.
"""

from __future__ import annotations  # Delayed parsing of type annotations

from os.path import dirname, join

import drjit as dr
import mitsuba as mi
import numpy as np

__tables = {}


def fetch_table(name: str) -> mi.TensorXf:
    """
    Load a precomputed table by name from disk (or cached) as a Dr.Jit texture
    """
    if name not in __tables:
        mi.Log(mi.LogLevel.Debug, f"Load precomputed table: {name}")
        filename = join(dirname(__file__), f"{name}.npy")
        data = mi.TensorXf(np.load(filename))[..., None]
        dr.eval(data)
        texture_type = [mi.Texture1f, mi.Texture2f, mi.Texture3f][len(data.shape) - 2]
        __tables[name] = texture_type(data)
    return __tables[name]


# ------------------------------------------------------------------------------
# Table pre-computation
# ------------------------------------------------------------------------------

if __name__ == "__main__":
    mi.set_variant("llvm_ad_rgb")

    from .lobes import (
        microfacet_reflect,
        microfacet_refract,
        MicrofacetFresnel,
        MicrofacetLobe,
        r0_to_eta,
        sanitize_eta,
    )

    def compute_table(name: str, samples: int, dims: list, func):
        """
        Table pre-computation routine. The function to evaluate should take a number
        of arguments equal to the number of dimensions (==len(dims)) plus one for the
        3D random points.

        Parameters:
            name:    Name of the table
            samples: Number of samples to use for Monte Carlo integration
            dims:    Dimensions of the table
            func:    Function to evaluate
        """
        dims = list(reversed(dims))

        mi.Log(mi.LogLevel.Info, f"{name}: table computation")

        # Generate the axis arrays
        axis = dr.meshgrid(
            *[dr.linspace(mi.Float, 1e-4, 1.0, d, endpoint=True) for d in dims],
            dr.arange(mi.Float, samples),
            indexing="ij",
        )
        seed = axis[-1]
        axis = list(reversed(axis[:-1]))

        # Generate 3D random numbers
        v0, v1 = mi.sample_tea_32(dr.arange(mi.UInt32, dr.width(seed)), seed)
        rng = mi.PCG32(initstate=v0, initseq=v1)
        sample2 = mi.Point2f(rng.next_float32(), rng.next_float32())
        rand = rng.next_float32()

        # Evaluate the function
        data, weight = func(*axis, sample2, rand)

        # Sanitize output values
        data[~dr.isfinite(data)] = 0.0
        weight[~dr.isfinite(data)] = 0.0

        # Accumulate sample values in final buffer
        cell_id = dr.arange(mi.UInt32, dr.width(seed)) // samples

        # print(f'seed:      {seed}')
        # # print(f'v0:        {v0}')
        # # print(f'v1:        {v1}')
        # print(f'roughness: {axis[0]}')
        # print(f'mu:        {axis[1]}')
        # print(f'z:         {axis[2]}')
        # print(f'cell_id:   {cell_id}')

        table = dr.zeros(mi.TensorXf, shape=dims)
        table_weight = dr.zeros(mi.TensorXf, shape=dims)
        dr.scatter_reduce(dr.ReduceOp.Add, table.array, data, cell_id)
        dr.scatter_reduce(dr.ReduceOp.Add, table_weight.array, weight, cell_id)

        # Sanitize output table and weights
        table.array[~dr.isfinite(table.array)] = 0.0
        table_weight.array[table_weight.array == 0.0] = 1.0

        # Normalize samples
        table /= table_weight
        table = dr.clip(table, 0.0, 1.0)

        # np.set_printoptions(threshold=100000, linewidth=880, suppress=True)
        # print(f'table: {np.array(table)}')

        # Write buffer to file on disk
        filename = join(dirname(__file__), f"{name}.npy")
        mi.Log(mi.LogLevel.Info, f"  -> writing to file: {filename}")
        np.save(filename, table)

    def ggx_E(roughness, mu, sample2, fresnel_mode, eta=1.0, r0=None):
        """
        GGX sampling and evaluation routine used in pre-computations
        """
        result = mi.Float(0.0)
        valid = mi.Bool(False)

        wi = mi.Vector3f(dr.sqrt(1.0 - dr.square(mu)), 0.0, mu)
        wh = MicrofacetLobe.sample_wh(wi, sample2, roughness, 0.0)

        reflect_value, reflect_pdf = MicrofacetLobe.eval_pdf(
            wi,
            wo=microfacet_reflect(wi, wh),
            reflection=True,
            roughness=roughness,
            anisotropic=0.0,
            fresnel_mode=fresnel_mode,
            r0=r0,
            eta=eta,
        )
        valid_reflect = reflect_pdf != 0.0
        result += dr.select(
            valid_reflect, mi.luminance(reflect_value) / reflect_pdf, 0.0
        )
        valid |= valid_reflect

        if fresnel_mode == MicrofacetFresnel.DIELECTRIC:
            refract_value, refract_pdf = MicrofacetLobe.eval_pdf(
                wi,
                wo=microfacet_refract(wi, wh, eta),
                reflection=False,
                roughness=roughness,
                anisotropic=0.0,
                fresnel_mode=fresnel_mode,
                r0=r0,
                eta=eta,
            )
            valid_refract = refract_pdf != 0.0
            result += dr.select(
                valid_refract,
                mi.luminance(refract_value) / refract_pdf * dr.square(eta),
                0.0,
            )
            valid |= valid_refract

        return result, dr.select(valid, 1.0, 0.0)

    def reparam_eta(z):
        """
        Parameterization ensuring the entire [1..inf] range of IORs is covered
        """
        z = dr.clip(z, 1e-4, 0.99)
        return sanitize_eta(r0_to_eta(dr.square(dr.square(z))))

    # Albedo of the GGX microfacet BRDF, roughness X incident direction
    def f(r, mu, s2, rand):
        return ggx_E(r, mu, s2, MicrofacetFresnel.NONE)

    compute_table("ggx_E", int(1e5), [32, 32], f)

    # Averaged over incident direction
    def f(r, s2, rand):
        value, weight = ggx_E(r, rand, s2, MicrofacetFresnel.NONE)
        return 2.0 * rand * value, weight

    compute_table("ggx_Eavg", int(1e5), [32], f)

    # Overall albedo of the GGX microfacet BSDF with dielectric Fresnel, roughness X incident direction X IOR, for IOR>1
    def f(r, mu, z, s2, rand):
        return ggx_E(r, mu, s2, MicrofacetFresnel.DIELECTRIC, eta=reparam_eta(z))

    compute_table("ggx_glass_E", int(1e3), [16, 16, 16], f)

    # Averaged over incident direction
    def f(r, z, s2, rand):
        value, weight = ggx_E(
            r, rand, s2, MicrofacetFresnel.DIELECTRIC, eta=reparam_eta(z)
        )
        return 2.0 * rand * value, weight

    compute_table("ggx_glass_Eavg", int(1e6), [16, 16], f)

    # Overall albedo of the GGX microfacet BSDF with dielectric Fresnel, roughness X incident direction X IOR, for IOR<1
    def f(r, mu, z, s2, rand):
        return ggx_E(
            r, mu, s2, MicrofacetFresnel.DIELECTRIC, eta=dr.rcp(reparam_eta(z))
        )

    compute_table("ggx_glass_inv_E", int(1e3), [16, 16, 16], f)

    # Averaged over incident direction
    def f(r, z, s2, rand):
        value, weight = ggx_E(
            r, rand, s2, MicrofacetFresnel.DIELECTRIC, eta=dr.rcp(reparam_eta(z))
        )
        return 2.0 * rand * value, weight

    compute_table("ggx_glass_inv_Eavg", int(1e5), [16, 16], f)

    def ggx_gen_schlick_s(roughness, mu, sample2, fresnel_mode, eta):
        """
        GGX sampling and evaluation routine used in pre-computations
        """
        wi = mi.Vector3f(dr.sqrt(1.0 - dr.square(mu)), 0.0, mu)
        wh = MicrofacetLobe.sample_wh(wi, sample2, roughness, 0.0)
        wo = microfacet_reflect(wi, wh)

        value_0, pdf = MicrofacetLobe.eval_pdf(
            wi,
            wo,
            reflection=True,
            roughness=roughness,
            anisotropic=0.0,
            fresnel_mode=fresnel_mode,
            eta=eta,
            r0=0.0,
        )

        value_1 = MicrofacetLobe.eval_pdf(
            wi,
            wo,
            reflection=True,
            roughness=roughness,
            anisotropic=0.0,
            fresnel_mode=fresnel_mode,
            eta=eta,
            r0=1.0,
        )[0]

        return value_0 / value_1, dr.select(pdf != 0.0, 1.0, 0.0)

    # Interpolation factor between F0 and F90 for the generalized Schlick Fresnel, depending on cosI and roughness, for IOR>1, using dielectric Fresnel mode.
    def f(r, mu, z, s2, rand):
        return ggx_gen_schlick_s(
            r, mu, s2, MicrofacetFresnel.GENERALIZED_SCHLICK, reparam_eta(z)
        )

    compute_table("ggx_gen_schlick_ior_s", int(1e4), [16, 16, 16], f)
