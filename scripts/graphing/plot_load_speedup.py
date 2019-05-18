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


def plot_speedup(cread_file_list, cwrite_file_list, cread_data, cwrite_data, numa_zone):
    width = .2
    position = 0.0
    local_baseline_set = False
    remote_baseline_set = False
    for cread_file, cwrite_file in zip(cread_file_list, cwrite_file_list):
        if not local_baseline_set:
            cread_local_baseline = utils.get_averages(cread_data[cread_file], client=False)
            cwrite_local_baseline = utils.get_averages(cwrite_data[cwrite_file], client=False)
            local_baseline_set = True
            continue
        if not remote_baseline_set:
            cread_remote_baseline = utils.get_averages(cread_data[cread_file], client=False)
            cwrite_remote_baseline = utils.get_averages(cwrite_data[cwrite_file], client=False)
            remote_baseline_set = True
            continue
        # update baseline to be RDMA-local throughput

        cread_speedup =  utils.get_averages(cread_data[cread_file], client=False)
        cwrite_speedup = utils.get_averages(cwrite_data[cwrite_file], client=False)

        numaLocal = True
        if not re.search(numa_zone+'/0workers.result', cread_file):
            numaLocal = False
        if numaLocal:
            plt.bar((position // 2) + 0 * width, 1 - cread_speedup / cread_local_baseline, width, color='red', edgecolor='black', hatch="\\\\", label="/cread/local", zorder=3)
        else:
            plt.bar((position // 2) + 2 * width, 1 - cread_speedup / cread_remote_baseline, width, color='aqua', edgecolor='black', hatch="XXX", label="/cread/remote", zorder=3)

        if not re.search(numa_zone+'/0workers.result', cwrite_file):
            numaLocal = False
        if numaLocal:
            plt.bar((position // 2) + 1 * width, 1 - cwrite_speedup / cwrite_local_baseline, width, color='yellow', edgecolor='black', hatch="--", label="/cwrite/local", zorder=3)
        else:
            plt.bar((position // 2) + 3 * width, 1 - cwrite_speedup / cwrite_remote_baseline, width, color='grey', edgecolor='black', hatch="++", label="/cwrite/remote", zorder=3)
        position += 1
        print(str(cread_speedup) + ', ' + str(cwrite_speedup))

    labels = xaxis
    fontP = FontProperties()
    fontP.set_size('large')
    plt.xlabel('load buffer size', fontsize=12)
    plt.ylabel('speedup over RDMA-remote', fontsize=12)
    plt.grid(True, which='major', axis='y', zorder=0)
    ax = plt.axes()
    # ax.ticklabel_format(axis='y', style='sci', scilimits=(0,0))
    plt.yticks(fontsize=12)

    labels = ['10e'+ str(x) for x in np.arange(2,10)]
    ax.set_xticklabels(labels, fontsize=12)
    ax.set_xticks(np.arange(0, len(labels)) + 3/2 * width)
    plt.grid(True, which='minor', axis='x', zorder=0)
    box = ax.get_position()
    ax.set_position([box.x0, box.y0, box.width, box.height * 0.8])

    plt.legend(["RDMA reads", "RDMA writes"], loc='upper center', ncol=4, bbox_to_anchor=(0.5, 1.25), prop=fontP)
    plt.hlines(0,xmin=-.25, xmax=len(labels)-.25, linewidth=3, label=None)
    plt.show()

size = 'sixtyfour_byte'
clients = 'clients_thirtytwo'

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
        return -1 * 2 + numa
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

# plot_speedup(sorted(no_numa_cread.keys(), key=sort_on_load), sorted(no_numa_cwrite.keys(), key=sort_on_load), no_numa_cread,
#              no_numa_cwrite, 'numa_zero')
# plot_speedup(sorted(numa_zero_cread.keys(), key=sort_on_load), sorted(numa_zero_cwrite.keys(), key=sort_on_load), numa_zero_cread,
#              numa_zero_cwrite, 'numa_zero')
# plot_speedup(sorted(numa_one_cread.keys(), key=sort_on_load), sorted(numa_one_cwrite.keys(), key=sort_on_load), numa_one_cread,
#              numa_one_cwrite, 'numa_zero')

sorted_cread = [sorted(no_numa_cread.keys(), key=sort_on_load), sorted(numa_zero_cread.keys(), key=sort_on_load),
                sorted(numa_one_cread.keys(), key=sort_on_load)]
sorted_cwrite = [sorted(no_numa_cwrite.keys(), key=sort_on_load), sorted(numa_zero_cwrite.keys(), key=sort_on_load),
                 sorted(numa_one_cwrite.keys(), key=sort_on_load)]

cread_files = []
cread_files.append(sorted_cread[0][0])
cread_files.append(sorted_cread[0][1])
for i in sorted_cread:
    cread_files.append(i[-2])
    cread_files.append(i[-1])

cwrite_files = []
cwrite_files.append(sorted_cwrite[0][0])
cwrite_files.append(sorted_cwrite[0][1])
for i in sorted_cwrite:
    cwrite_files.append(i[-2])
    cwrite_files.append(i[-1])

cread_data = [no_numa_cread, numa_zero_cread, numa_one_cread]
cwrite_data = [no_numa_cwrite, numa_zero_cwrite, numa_one_cwrite]
numa_zone = 'numa_zero'
width = .2
position = 0.0
local_baseline_set = False
remote_baseline_set = False
idx = 0
for cread_file, cwrite_file in zip(cread_files, cwrite_files):
    if not local_baseline_set:
        cread_local_baseline = utils.get_averages(cread_data[idx//2][cread_file], client=False)
        cwrite_local_baseline = utils.get_averages(cwrite_data[idx//2][cwrite_file], client=False)
        local_baseline_set = True
        continue
    if not remote_baseline_set:
        cread_remote_baseline = utils.get_averages(cread_data[idx//2][cread_file], client=False)
        cwrite_remote_baseline = utils.get_averages(cwrite_data[idx//2][cwrite_file], client=False)
        remote_baseline_set = True
        continue
    # update baseline to be RDMA-local throughput

    cread_speedup =  utils.get_averages(cread_data[idx//2][cread_file], client=False)
    cwrite_speedup = utils.get_averages(cwrite_data[idx//2][cwrite_file], client=False)

    numaLocal = True
    if not re.search(numa_zone+'/0workers.result', cread_file):
        numaLocal = False
    if numaLocal:
        plt.bar((position // 2) + 0 * width, 1 - cread_speedup / cread_local_baseline, width, color='red', edgecolor='black', hatch="\\\\", label="/cread/local", zorder=3)
    else:
        plt.bar((position // 2) + 2 * width, 1 - cread_speedup / cread_remote_baseline, width, color='aqua', edgecolor='black', hatch="XXX", label="/cread/remote", zorder=3)

    if not re.search(numa_zone+'/0workers.result', cwrite_file):
        numaLocal = False
    if numaLocal:
        plt.bar((position // 2) + 1 * width, 1 - cwrite_speedup / cwrite_local_baseline, width, color='yellow', edgecolor='black', hatch="--", label="/cwrite/local", zorder=3)
    else:
        plt.bar((position // 2) + 3 * width, 1 - cwrite_speedup / cwrite_remote_baseline, width, color='grey', edgecolor='black', hatch="++", label="/cwrite/remote", zorder=3)
    position += 1
    idx += 1
    print(str(cread_speedup) + ', ' + str(cwrite_speedup))

fontP = FontProperties()
fontP.set_size('large')
plt.xlabel('workload locality', fontsize=12)
plt.ylabel('speedup for no workload', fontsize=12)
plt.grid(True, which='major', axis='y', zorder=0)
ax = plt.axes()
# ax.ticklabel_format(axis='y', style='sci', scilimits=(0,0))
plt.yticks(fontsize=12)

labels = ['NUMA-agnostic', 'RDMA-local', 'RDMA-remote']
ax.set_xticklabels(labels, fontsize=12)
ax.set_xticks(np.arange(0, len(labels)) + 3/2 * width)
ax.set_xticks(np.arange(0, len(labels) + 1) - width, minor=True)
plt.grid(True, which='minor', axis='x', zorder=0)
box = ax.get_position()
ax.set_position([box.x0, box.y0, box.width, box.height * 0.8])

plt.legend(["RDMA-local reads", "RDMA-local writes", "RDMA-remote reads", "RDMA-remote writes"], loc='upper center', ncol=2, bbox_to_anchor=(0.5, 1.25), prop=fontP)
plt.hlines(0,xmin=0 - width, xmax=len(labels)- width, linewidth=3, label=None)
plt.show()


