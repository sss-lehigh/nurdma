import matplotlib.pyplot as plt
import numpy as np
import argparse
import os
import re

from matplotlib.font_manager import FontProperties

import graph_utils as utils

dir = './results/memcached/'
files = ['no_load', 'no_numa', 'numa_zero','numa_one']
xaxis = [1, 2, 4, 8, 12, 16, 24, 32]

def read_file(filename):
    data = {}
    with open(filename) as f:
        line = f.readline()
        while line != '':
            conns, numa_zero, numa_one, client_local, speedup_numa_one, speedup_client_local = line.split('\t')
            data[int(conns)] = [float(numa_zero), float(numa_one), float(client_local), float(speedup_numa_one), float(speedup_client_local[:-1])]
            line = f.readline()
    return data


def plot_throughput(numa_zero_throughput, numa_one_throughput, client_local_throughput, load_config):
    plt.figure(figsize=(20,10))
    plt.plot(xaxis, numa_zero_throughput[load_config], color='aqua', marker='s', markersize=18)
    plt.plot(xaxis, client_local_throughput[load_config], color='blue', marker = 'v', markersize=18)
    plt.plot(xaxis, numa_one_throughput[load_config], color='red', marker='o', markersize=18)

    ax = plt.axes()

    fontP = FontProperties()
    fontP.set_size('large')

    plt.xlabel('# clients', fontsize=20)
    plt.xticks(fontsize=20)
    plt.ylabel('total throughput (ops/s)', fontsize=20)
    plt.yticks(fontsize=20)
    ax.set_ylim(0, 6.5e5)
    ax.ticklabel_format(axis='y', style='sci', scilimits=(0,0), fontsize=20)
    # ax.tick_params(axis='y', labelsize=20)

    box = ax.get_position()
    ax.set_position([box.x0, box.y0, box.width, box.height*0.7])

    plt.legend(["Both RDMA-local", "Client RDMA-local", "Both RDMA-remote"], loc='upper center', ncol=3, bbox_to_anchor=(0.45, 1.25), fontsize=18)
    plt.grid(True, which='major', axis='y', zorder=0)

    # plt.show()
    plt.savefig("memcached_legend.pdf", dpi=300)


all = {}
for f in files:
    all[f] = read_file(dir+f+'.result')

numa_zero_throughput = {}
numa_one_throughput = {}
client_local_throughput = {}
for k in all.keys():
    for conns in sorted(all[k].keys()):
        if k not in numa_zero_throughput.keys():
            numa_zero_throughput[k] = [all[k][conns][0]]
            numa_one_throughput[k] = [all[k][conns][1]]
            client_local_throughput[k] = [all[k][conns][2]]
        else:
            numa_zero_throughput[k].extend([all[k][conns][0]])
            numa_one_throughput[k].extend([all[k][conns][1]])
            client_local_throughput[k].extend([all[k][conns][2]])

plot_throughput(numa_zero_throughput, numa_one_throughput, client_local_throughput, 'numa_zero')