#!/usr/bin/env python
# coding: utf-8

import sys
import numpy as np
import matplotlib.pyplot as plt

if len(sys.argv) != 2:
    sys.exit("Usage: python single_emitter_plotting_timings.py variant \n")

variant = sys.argv[1]
timing_data = np.loadtxt("csv/"+variant+"_full_timing_for_n_photons.csv", delimiter=",")

print("----- full timing_data -----")
print(timing_data)

# Loop to run experiments and compare to old images
timing_vs_nphotons_dict = {}

for t_data in timing_data:
    [n_photons, load_time, render_time, load_and_render_time, elapsed_time, generate_time, volume_time] = t_data
    timing_vs_nphotons_dict[n_photons] = []

for t_data in timing_data:
    [n_photons, load_time, render_time, load_and_render_time, elapsed_time, generate_time, volume_time] = t_data
    timing_vs_nphotons_dict[n_photons].append([load_time, render_time, load_and_render_time, elapsed_time, generate_time, volume_time])

# This could probably be rewritten to save having to add extra lists every time a new timing section is added
stats_timing_vs_nphotons_data = []
nphotons_list = []
load_box_list = []
render_box_list = []
load_and_render_box_list = []
full_elapsed_box_list = []
generate_box_list = []
volume_box_list = []
for key in timing_vs_nphotons_dict.keys():
    timing_vals = timing_vs_nphotons_dict[key]

    nphotons_list.append(key)
    load = [timing[0] for timing in timing_vals]
    load_box_list.append(load)
    load_avg = np.average(load)
    load_sd = np.std(load)
    render = [timing[1] for timing in timing_vals]
    render_box_list.append(render)
    render_avg = np.average(render)
    render_sd = np.std(render)
    load_and_render = [timing[2] for timing in timing_vals]
    load_and_render_box_list.append(load_and_render)
    load_and_render_avg = np.average(load_and_render)
    load_and_render_sd = np.std(load_and_render)
    full_elapsed = [timing[3] for timing in timing_vals]
    full_elapsed_box_list.append(full_elapsed)
    full_elapsed_avg = np.average(full_elapsed)
    full_elapsed_sd = np.std(full_elapsed)
    generate = [timing[4] for timing in timing_vals]
    generate_box_list.append(generate)
    generate_avg = np.average(generate)
    generate_sd = np.std(generate)
    volume = [timing[5] for timing in timing_vals]
    volume_box_list.append(volume)
    volume_avg = np.average(volume)
    volume_sd = np.std(volume)
    # render_avg = np.average([timing[1] for timing in timing_vals])
    # render_sd = np.std([timing[1] for timing in timing_vals])
    # load_and_render_avg = np.average([timing[2] for timing in timing_vals])
    # load_and_render_sd = np.std([timing[2] for timing in timing_vals])
    # full_elapsed_avg = np.average([timing[3] for timing in timing_vals])
    # full_elapsed_sd = np.std([timing[3] for timing in timing_vals])

    stats_timing_vs_nphotons_data.append([key, load_avg, load_sd, render_avg, render_sd,
                                          load_and_render_avg, load_and_render_sd, full_elapsed_avg, full_elapsed_sd,
                                          generate_avg, generate_sd, volume_avg, volume_sd])

print("-------- timings --------")
max_time = 0
for n in range(len(stats_timing_vs_nphotons_data)):
    print(stats_timing_vs_nphotons_data[n])
    max_full = stats_timing_vs_nphotons_data[n][7]
    if max_full > max_time:
        max_time = max_full

# Save timing data to CSV files
np.savetxt('csv/' + variant + '_stats_timing_for_n_photons.csv', np.array(stats_timing_vs_nphotons_data), delimiter=',')

fixed_w = 0.1
width = lambda p, w: 10**(np.log10(p)+w/2.)-10**(np.log10(p)-w/2.)

fig = plt.figure()
plt.plot([tn[0] for tn in stats_timing_vs_nphotons_data], [tn[1] for tn in stats_timing_vs_nphotons_data], label='load')
plt.boxplot(render_box_list, positions=nphotons_list, widths=width(nphotons_list, fixed_w))
plt.plot([tn[0] for tn in stats_timing_vs_nphotons_data], [tn[3] for tn in stats_timing_vs_nphotons_data], label='render')
plt.plot([tn[0] for tn in stats_timing_vs_nphotons_data], [tn[5] for tn in stats_timing_vs_nphotons_data], label='load_and_render')
plt.boxplot(full_elapsed_box_list, positions=nphotons_list, widths=width(nphotons_list, fixed_w))
plt.plot([tn[0] for tn in stats_timing_vs_nphotons_data], [tn[7] for tn in stats_timing_vs_nphotons_data], label='full_elapsed')
plt.plot([tn[0] for tn in stats_timing_vs_nphotons_data], [tn[9] for tn in stats_timing_vs_nphotons_data], label='generate')
plt.plot([tn[0] for tn in stats_timing_vs_nphotons_data], [tn[11] for tn in stats_timing_vs_nphotons_data], label='volume')
plt.legend()
plt.xlim(1e-1, 1e9)
plt.ylim(1e-5, max_time+(0.5*max_time))
plt.xscale('log')
plt.yscale('log')
plt.ylabel('time (s)')
plt.xlabel('Number of photons')
plt.title('timing vs n_photons, mitsuba3 single photon emitter (' + variant + ')')
plt.savefig('png/' + variant +'_timing_for_n_photons')
plt.close(fig)
