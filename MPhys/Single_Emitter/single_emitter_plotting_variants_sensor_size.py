#!/usr/bin/env python
# coding: utf-8

import numpy as np
import matplotlib.pyplot as plt

# Load in data from different mitsuba variant tests
variants = ["llvm_mono", "llvm_ad_rgb", "cuda_mono", "cuda_ad_rgb"]

max_time = 0
timing_data = []
timing_full_data = []
for variant in variants:
    full_data = np.loadtxt("csv/"+variant+"_full_timing_for_sensor_size.csv", delimiter=",")
    data = np.loadtxt("csv/"+variant+"_stats_timing_for_sensor_size.csv", delimiter=",")
    max_full = np.max([d[7] for d in data])
    if max_full > max_time:
        max_time = max_full
    timing_data.append([variant, data])
    timing_full_data.append(full_data)

print(timing_data)

# Plot each variant for each column 
columns = ["load", "render", "load_and_render", "full_time"]

for n_col, column in enumerate(columns):
    fig = plt.figure()
    for n_var, variant in enumerate(variants):
        sensor_size_list = [tn[0] for tn in timing_data[n_var][1]]
        plt.plot(sensor_size_list, [tn[2*n_col+1] for tn in timing_data[n_var][1]], label=column+' '+timing_data[n_var][0])
        # boxplot, somehow - get the relevant column from the full data
        full_variant_data = timing_full_data[n_var]
        timing_vs_sensor_size_dict = {}

        for t_data in full_variant_data:
            [sensor_size, load_time, render_time, load_and_render_time, elapsed_time] = t_data
            timing_vs_sensor_size_dict[sensor_size] = []

        for t_data in full_variant_data:
            [sensor_size, load_time, render_time, load_and_render_time, elapsed_time] = t_data
            timing_vs_sensor_size_dict[sensor_size].append([load_time, render_time, load_and_render_time, elapsed_time])

        data = []
        for key in timing_vs_sensor_size_dict.keys():
            data.append([f[n_col] for f in timing_vs_sensor_size_dict[key]])
        fixed_w = 0.1
        width = lambda p, w: 10**(np.log10(p)+w/2.)-10**(np.log10(p)-w/2.)
        plt.boxplot(data, positions=sensor_size_list, widths=width(sensor_size_list, fixed_w))

    plt.legend()
    plt.xlim(1e2, 1e4)
    plt.ylim(1e-4, max_time+(0.5*max_time))
    plt.xscale('log')
    plt.yscale('log')
    plt.ylabel('time (s)')
    plt.xlabel('Sensor size (side of square)')
    plt.title('timing vs sensor_size, mitsuba3 variants ('+column+')')
    plt.savefig('png/'+column+'_timing_for_sensor_size')
