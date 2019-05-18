import matplotlib.pyplot as plt
import numpy as np
import argparse
import os
import re

from matplotlib.font_manager import FontProperties

import graph_utils as utils


def plot_totals_bar(cread_file_list, cwrite_file_list, cread_label_map, cwrite_label_map, cread_data, cwrite_data, numa_zone):
    width = .2
    position = 0
    plt.figure(figsize=(15,3.5))
    for cread_file, cwrite_file in zip(cread_file_list, cwrite_file_list):
        cread_tot_average = utils.get_averages(cread_data[cread_file], client=False)
        cwrite_tot_average = utils.get_averages(cwrite_data[cwrite_file], client=False)
        numaLocal = True
        if not re.search(numa_zone, cread_label_map[cread_file]):
            numaLocal = False
        if numaLocal:
            plt.bar((position // 2) + 0 * width, cread_tot_average, width, color='red', edgecolor='black', hatch="\\\\", label="/cread/local")
        else:
            plt.bar((position // 2) + 2 * width, cread_tot_average, width, color='aqua', edgecolor='black', hatch="XXX", label="/cread/remote")

        if not re.search(numa_zone, cwrite_label_map[cwrite_file]):
            numaLocal = False
        if numaLocal:
            plt.bar((position // 2) + 1 * width, cwrite_tot_average, width, color='yellow', edgecolor='black', hatch="--", label="/cwrite/local")
        else:
            plt.bar((position // 2) + 3 * width, cwrite_tot_average, width, color='grey', edgecolor='black', hatch="++", label="/cwrite/remote")
        position += 1
        print(str(cread_tot_average) + ', ' + str(cwrite_tot_average))

    labels = [1, 2, 3, 4, 8, 16, 24, 32, 48, 64, 128]
    fontP = FontProperties()
    fontP.set_size('large')
    plt.xlabel('# QPs', fontsize=18)
    plt.ylabel('system throughput\n(ops/s)', fontsize=18)
    ax = plt.axes()
    ax.ticklabel_format(axis='y', style='sci', scilimits=(0,0))
    plt.yticks(fontsize=18)
    ax.set_xticks(np.arange(0, len(labels)) + 3/2 * width)
    ax.set_xticklabels(labels, fontsize=18)
    box = ax.get_position()
    ax.set_position([box.x0, box.y0*2, box.width, box.height * 0.7])

    plt.legend(["RDMA-local reads", "RDMA-local writes", "RDMA-remote reads", "RDMA-remote writes"], loc='upper center', ncol=4, bbox_to_anchor=(0.5, 1.4), fontsize=16)
    # plt.show()
    plt.savefig("baseline_throughput.pdf", dpi=300)

if __name__ == "__main__":
    # get arguments
    parser = argparse.ArgumentParser('Read output file from openloop run and produce graphs')
    parser.add_argument(dest='basedir', metavar='dir', type=str, default=None)
    args = parser.parse_args()

    # sort by the number of workers then numa locality
    def byNumWorkers(str):
        numStr = re.search("([0-9]+)", str)
        return int(numStr.group(0)) * 2 + (0 if re.search("(numa_zero)", str) is not None else 1)

    def byNumClients(str):
        numa = 0
        if 'numa_one' in str:
            numa = 1
        if 'clients_one/' in str:
            return 1 * 2 + numa
        elif 'clients_two/' in str:
            return 2 * 2 + numa
        elif 'clients_three/' in str:
            return 3 * 2 + numa
        elif 'clients_four/' in str:
            return 4 * 2 + numa
        elif 'clients_eight/' in str:
            return 8 * 2 + numa
        elif 'clients_sixteen/' in str:
            return 16 * 2 + numa
        elif 'clients_twentyfour/' in str:
            return 24 * 2 + numa
        elif 'clients_thirtytwo/' in str:
            return 32 * 2 + numa
        elif 'clients_fortyeight/' in str:
            return 48 * 2 + numa
        elif 'clients_sixtyfour/' in str:
            return 64 * 2 + numa
        elif 'clients_onetwentyeight/' in str:
            return 128 * 2 + numa

    # sort by NUMA locality then num workers
    def byNuma(str):
        numStr = re.search("([0-9]+)", str)
        if re.search("(numa_zero)", str) is not None:
            return int(numStr.group(0))
        else:
            return 10000 + int(numStr.group(0))

    cread_file_list = []
    cwrite_file_list = []
    for dirpath, dirs, files in os.walk(args.basedir):
        if len(files) != 0:
            cread_file_list.extend(os.path.join(dirpath, f) for f in os.listdir(dirpath) if (f == '0workers.result' and 'cread' in dirpath and 'sread' in dirpath))
            cwrite_file_list.extend(os.path.join(dirpath, f) for f in os.listdir(dirpath) if (f == '0workers.result' and 'cwrite' in dirpath and 'sread' in dirpath))

    cread_label_map = {}
    for i in range(len(cread_file_list)):
        l = cread_file_list[i].replace(args.basedir, '')
        l = l.split('/')
        l = l[0] + '/' + l[-2]
        cread_label_map[cread_file_list[i]] = l
    # file_list = sorted(file_list, key=byNumWorkers)

    cread_data = {}
    for file in cread_file_list:
        # get data and configuration
        cread_data[file] = utils.parse_file(file, utils.THROUGHPUT)

    cwrite_label_map = {}
    for i in range(len(cwrite_file_list)):
        l = cwrite_file_list[i].replace(args.basedir, '')
        l = l.split('/')
        l = l[0] + '/' + l[-2]
        cwrite_label_map[cwrite_file_list[i]] = l
    # file_list = sorted(file_list, key=byNumWorkers)

    cwrite_data = {}
    for file in cwrite_file_list:
        # get data and configuration
        cwrite_data[file] = utils.parse_file(file, utils.THROUGHPUT)

    # cread_avgs = []
    # cwrite_avgs = []
    # for file in cread_file_list:
    #     cread_avgs.append(utils.get_averages(cread_data[file], True))
    #
    # for file in cwrite_file_list:
    #     cwrite_avgs.append(utils.get_averages(cread_data[file], True))

    # Plot average total throughput for each run
    plot_totals_bar(sorted(cread_file_list, key=byNumClients), sorted(cwrite_file_list, key=byNumClients), cread_label_map, cwrite_label_map, cread_data, cwrite_data, 'numa_zero')


    print('done')