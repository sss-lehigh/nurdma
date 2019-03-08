import matplotlib.pyplot as plt
import numpy as np
import argparse
import os
import re

from matplotlib.font_manager import FontProperties

import scripts.graphing.graph_utils as utils

dir = './results/load/'
xaxis = np.arange(2,10)
xaxis = (10 ** xaxis) * 64


def plot_throughput(cread_file_list, cwrite_file_list, cread_data, cwrite_data, numa_zone):
    width = .2
    position = 0
    for cread_file, cwrite_file in zip(cread_file_list, cwrite_file_list):
        cread_tot_average = utils.get_averages(cread_data[cread_file], client=False)
        cwrite_tot_average = utils.get_averages(cwrite_data[cwrite_file], client=False)
        numaLocal = True
        if not re.search(numa_zone+'/0workers.result', cread_file):
            numaLocal = False
        if numaLocal:
            plt.bar((position // 2) + 0 * width, cread_tot_average, width, color='red', edgecolor='black', hatch="\\\\", label="/cread/local", zorder=3)
        else:
            plt.bar((position // 2) + 2 * width, cread_tot_average, width, color='aqua', edgecolor='black', hatch="XXX", label="/cread/remote", zorder=3)

        if not re.search(numa_zone+'/0workers.result', cwrite_file):
            numaLocal = False
        if numaLocal:
            plt.bar((position // 2) + 1 * width, cwrite_tot_average, width, color='yellow', edgecolor='black', hatch="--", label="/cwrite/local", zorder=3)
        else:
            plt.bar((position // 2) + 3 * width, cwrite_tot_average, width, color='grey', edgecolor='black', hatch="++", label="/cwrite/remote", zorder=3)
        position += 1
        print(str(cread_tot_average) + ', ' + str(cwrite_tot_average))

    labels = xaxis
    fontP = FontProperties()
    fontP.set_size('large')
    plt.xlabel('load buffer size', fontsize=12)
    plt.ylabel('total system throughput (Ops/s)', fontsize=12)
    plt.grid(True, which='major', axis='y', zorder=0)
    ax = plt.axes()
    ax.ticklabel_format(axis='y', style='sci', scilimits=(0,0))
    plt.yticks(fontsize=12)
    labels = ['None']
    labels.extend(['10e'+ str(x) for x in np.arange(2,10)])
    ax.set_xticks(np.arange(0, len(labels)) + 3/2 * width)
    ax.set_xticklabels(labels, fontsize=12)
    box = ax.get_position()
    ax.set_position([box.x0, box.y0, box.width, box.height * 0.8])

    plt.legend(["/cread/local", "/cwrite/local", "/cread/remote", "/cwrite/remote"], loc='upper center', ncol=4, bbox_to_anchor=(0.5, 1.25), prop=fontP)
    plt.show()

size = 'eight_byte'
clients = 'clients_eight'

result_files = []
for dirpath, dirs, files in os.walk(dir):
    if size in dirpath and clients in dirpath and len(files) > 0:
        result_files.append(os.path.join(dirpath, files[0])) # only one file in folder

no_numa_cread = {}
no_numa_cwrite = {}
numa_zero_cread = {}
numa_zero_cwrite = {}
numa_one_cread = {}
numa_one_cwrite = {}
for f in result_files:
    if 'no_numa/'+size+'/'+clients in f:
        if 'cread' in f:
            no_numa_cread[f] = utils.parse_file(f)
        elif 'cwrite' in f:
            no_numa_cwrite[f] = utils.parse_file(f)
    elif 'numa_zero/'+size+'/'+clients in f:
        if 'cread' in f:
            numa_zero_cread[f] = utils.parse_file(f)
        elif 'cwrite' in f:
            numa_zero_cwrite[f] = utils.parse_file(f)
    elif 'numa_one/'+size+'/'+clients in f:
        if 'cread' in f:
            numa_one_cread[f] = utils.parse_file(f)
        elif 'cwrite' in f:
            numa_one_cwrite[f] = utils.parse_file(f)


def sort_on_load(key):
    numa = 0
    if 'numa_one/0workers.result' in key:
        numa = 1
    if 'no_load' in key:
        return -1 * 2 - numa
    if 'onehundo' in key:
        return 1 * 2 + numa
    if 'onek' in key:
        return 2 * 2 + numa
    if 'tenk' in key:
        return 3 * 2 + numa
    if 'hundok' in key:
        return 4 * 2 + numa
    if 'onemil' in key:
        return 5 * 2 + numa
    if 'tenmil' in key:
        return 6 * 2 + numa
    if 'hundomil' in key:
        return 7 * 2 + numa
    if 'billion' in key:
        return 8 * 2 + numa

plot_throughput(sorted(no_numa_cread.keys(), key=sort_on_load), sorted(no_numa_cwrite.keys(), key=sort_on_load), no_numa_cread,
                no_numa_cwrite, 'numa_zero')
# plot_throughput(sorted(numa_zero_cread.keys(), key=sort_on_load), sorted(numa_zero_cwrite.keys(), key=sort_on_load), numa_zero_cread,
#                 numa_zero_cwrite, 'numa_zero')
# plot_throughput(sorted(numa_one_cread.keys(), key=sort_on_load), sorted(numa_one_cwrite.keys(), key=sort_on_load), numa_one_cread,
#                 numa_one_cwrite, 'numa_zero')
