#!/usr/bin/env python
# coding: utf-8

import numpy as np
import matplotlib.pyplot as plt

# Load in data from different mitsuba variant tests
# variants = ["cuda_mono", "cuda_ad_rgb"]
mitsuba_states = ["current_mitsuba", "old_mitsuba"]
variants = ["cuda_mono", "cuda_ad_rgb", "llvm_mono", "llvm_ad_rgb"]

max_time = 0
timing_data = []
timing_full_data = []
for mitsuba_state in mitsuba_states:
    for variant in variants:
        full_data = np.loadtxt(mitsuba_state+"_csv/"+variant+"_full_timing_for_n_photons.csv", delimiter=",")
        data = np.loadtxt(mitsuba_state+"_csv/"+variant+"_stats_timing_for_n_photons.csv", delimiter=",")
        max_full = np.max([d[7] for d in data])
        if max_full > max_time:
            max_time = max_full
        timing_data.append([mitsuba_state+" "+variant, data])
        timing_full_data.append(full_data)

print(timing_data)

# Plot each variant for each column 
columns = ["load", "render", "load_and_render", "full_time", "generate", "volume"]

for n_col, column in enumerate(columns):
    fig = plt.figure()
    for n_var in range(len(timing_data)):
        n_photons_list = [tn[0] for tn in timing_data[n_var][1]]
        if timing_data[n_var][0][:7] == "current":
            plt.plot(n_photons_list, [tn[2*n_col+1] for tn in timing_data[n_var][1]], label=column+' '+timing_data[n_var][0], linestyle='solid')
        else:
            plt.plot(n_photons_list, [tn[2*n_col+1] for tn in timing_data[n_var][1]], label=column+' '+timing_data[n_var][0], linestyle='dotted')
        # boxplot, somehow - get the relevant column from the full data
        full_variant_data = timing_full_data[n_var]
        timing_vs_nphotons_dict = {}

        for t_data in full_variant_data:
            [n_photons, load_time, render_time, load_and_render_time, elapsed_time, generate_time, volume_time] = t_data
            timing_vs_nphotons_dict[n_photons] = []

        for t_data in full_variant_data:
            [n_photons, load_time, render_time, load_and_render_time, elapsed_time, generate_time, volume_time] = t_data
            timing_vs_nphotons_dict[n_photons].append([load_time, render_time, load_and_render_time, elapsed_time, generate_time, volume_time])

        data = []
        for key in timing_vs_nphotons_dict.keys():
            data.append([f[n_col] for f in timing_vs_nphotons_dict[key]])
        # fixed_w = 0.1
        # width = lambda p, w: 10**(np.log10(p)+w/2.)-10**(np.log10(p)-w/2.)
        plt.boxplot(data, positions=n_photons_list)#, widths=width(n_photons_list, fixed_w))

    plt.legend()
    plt.xlim(1e1, 1e9)
    plt.ylim(1e-5, max_time+(0.5*max_time))
    plt.xticks(ticks=[1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8], labels=["1e2", "1e3", "1e4", "1e5", "1e6", "1e7", "1e8"])
    plt.xscale('log')
    plt.yscale('log')
    plt.ylabel('time (s)')
    plt.xlabel('Number of photons')
    plt.title('timing vs n_photons, mitsuba3 variants ('+column+')')
    plt.savefig('png/'+column+'_timing_for_n_photons')
