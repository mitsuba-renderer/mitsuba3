#!/usr/bin/env python
# coding: utf-8

import sys
import numpy as np
import matplotlib.pyplot as plt

if len(sys.argv) != 2:
    sys.exit("Usage: python single_emitter_plotting_timings_sensor_size.py variant \n")

variant = sys.argv[1]
timing_data = np.loadtxt("csv/"+variant+"_full_timing_for_sensor_size.csv", delimiter=",")

print("----- full timing_data -----")
print(timing_data)

# Loop to run experiments and compare to old images
timing_vs_sensor_size_dict = {}

for t_data in timing_data:
    [sensor_size, load_time, render_time, load_and_render_time, elapsed_time] = t_data
    timing_vs_sensor_size_dict[sensor_size] = []

for t_data in timing_data:
    [sensor_size, load_time, render_time, load_and_render_time, elapsed_time] = t_data
    timing_vs_sensor_size_dict[sensor_size].append([load_time, render_time, load_and_render_time, elapsed_time])

stats_timing_vs_sensor_size_data = []
sensor_size_list = []
load_box_list = []
render_box_list = []
load_and_render_box_list = []
full_elapsed_box_list = []
for key in timing_vs_sensor_size_dict.keys():
    timing_vals = timing_vs_sensor_size_dict[key]

    sensor_size_list.append(key)
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
    # render_avg = np.average([timing[1] for timing in timing_vals])
    # render_sd = np.std([timing[1] for timing in timing_vals])
    # load_and_render_avg = np.average([timing[2] for timing in timing_vals])
    # load_and_render_sd = np.std([timing[2] for timing in timing_vals])
    # full_elapsed_avg = np.average([timing[3] for timing in timing_vals])
    # full_elapsed_sd = np.std([timing[3] for timing in timing_vals])

    stats_timing_vs_sensor_size_data.append([key, load_avg, load_sd, render_avg, render_sd,
                                          load_and_render_avg, load_and_render_sd, full_elapsed_avg, full_elapsed_sd])

print("-------- timings --------")
max_time = 0
for n in range(len(stats_timing_vs_sensor_size_data)):
    print(stats_timing_vs_sensor_size_data[n])
    max_full = stats_timing_vs_sensor_size_data[n][7]
    if max_full > max_time:
        max_time = max_full

# Save timing data to CSV files
np.savetxt('csv/' + variant + '_stats_timing_for_sensor_size.csv', np.array(stats_timing_vs_sensor_size_data), delimiter=',')

fixed_w = 0.1
width = lambda p, w: 10**(np.log10(p)+w/2.)-10**(np.log10(p)-w/2.)

fig = plt.figure()
plt.plot([tn[0] for tn in stats_timing_vs_sensor_size_data], [tn[1] for tn in stats_timing_vs_sensor_size_data], label='load')
plt.boxplot(render_box_list, positions=sensor_size_list, widths=width(sensor_size_list, fixed_w))
plt.plot([tn[0] for tn in stats_timing_vs_sensor_size_data], [tn[3] for tn in stats_timing_vs_sensor_size_data], label='render')
plt.plot([tn[0] for tn in stats_timing_vs_sensor_size_data], [tn[5] for tn in stats_timing_vs_sensor_size_data], label='load_and_render')
plt.boxplot(full_elapsed_box_list, positions=sensor_size_list, widths=width(sensor_size_list, fixed_w))
plt.plot([tn[0] for tn in stats_timing_vs_sensor_size_data], [tn[7] for tn in stats_timing_vs_sensor_size_data], label='full_elapsed')
plt.legend()
plt.xlim(1e2, 1e4)
plt.ylim(1e-4, max_time+(0.5*max_time))
plt.xscale('log')
plt.yscale('log')
plt.ylabel('time (s)')
plt.xlabel('Sensor size (side of square)')
plt.title('timing vs sensor_size, mitsuba3 single photon emitter (' + variant + ')')
plt.savefig('png/' + variant +'_timing_for_sensor_size')
plt.close(fig)
