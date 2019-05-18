import matplotlib.pyplot as plt
import numpy as np
import argparse
import os
import re

from matplotlib.font_manager import FontProperties

import graph_utils as utils


def plot_totals_bar(cread_file_list, cwrite_file_list, cread_label_map, cwrite_label_map, cread_data, cwrite_data, numa_zone):
    width = .45
    position = 0.025
    cread_baseline = 1
    cwrite_baseline = 1
    plt.figure(figsize=(15,7))
    for cread_file, cwrite_file in zip(cread_file_list, cwrite_file_list):
        # update baseline to be RDMA-local throughput
        if re.search(numa_zone, cread_label_map[cread_file]):
            cread_baseline = utils.get_averages(cread_data[cread_file], client=False)
            cwrite_baseline = utils.get_averages(cwrite_data[cwrite_file], client=False)
            continue

        # get speedup over RDMA-remote
        cread_speedup =  1 - utils.get_averages(cread_data[cread_file], client=False) / cread_baseline
        cwrite_speedup = 1 - utils.get_averages(cwrite_data[cwrite_file], client=False) / cwrite_baseline

        plt.bar((position) + 0 * width, cread_speedup, width, color='red', edgecolor='black', hatch="\\\\", label="RDMA reads", zorder=3)
        plt.bar((position) + 1 * width, cwrite_speedup, width, color='yellow', edgecolor='black', hatch="-", label="RDMA reads", zorder=2)
        position += 1
        print(str(cread_speedup) + ', ' + str(cwrite_speedup))

    labels = [0, 1, 2, 4, 8, 16, 24, 32, 48]
    fontP = FontProperties()
    fontP.set_size('large')
    plt.xlabel('# worker threads', fontsize=32)
    plt.ylabel('% improvement\nover RDMA-remote', fontsize=32)
    ax = plt.axes()
    plt.grid(True, which='major', axis='y', zorder=0)
    plt.yticks(fontsize=28)
    vals = ax.get_yticks()
    ax.set_yticklabels(['{:,.0%}'.format(x) for x in vals])
    # ax.ticklabel_format(axis='y', style='sci', scilimits=(0,0))
    ax.set_xticks(np.arange(0, len(labels) + 1) - 1/4, minor=True)
    ax.set_xticks(np.arange(0, len(labels)) + 1/4)

    plt.grid(True, which='minor', axis='x', zorder=0, linewidth=3)
    box = ax.get_position()
    ax.set_position([box.x0*1.4, box.y0*2, box.width, box.height*0.9])
    ax.set_xticklabels(labels, fontsize=28)

    # plt.legend(["RDMA reads", "RDMA writes"], loc='upper center', ncol=4, bbox_to_anchor=(0.5, 1.25), fontsize=20)
    plt.hlines(0,xmin=-.25, xmax=len(labels)-.25, linewidth=3, label=None)
    # plt.show()
    plt.savefig("workers_xthree.pdf", dpi=300)

if __name__ == "__main__":
    # get arguments
    parser = argparse.ArgumentParser('Read output file from openloop run and produce graphs')
    parser.add_argument(dest='basedir', metavar='dir', type=str, default=None)
    parser.add_argument(dest='smode', metavar='server-mode', type=str, default=None)
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
            cread_file_list.extend(os.path.join(dirpath, f) for f in os.listdir(dirpath) if ('cread' in dirpath and args.smode in dirpath))
            cwrite_file_list.extend(os.path.join(dirpath, f) for f in os.listdir(dirpath) if ('cwrite' in dirpath and args.smode in dirpath))

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
    plot_totals_bar(sorted(cread_file_list, key=byNumWorkers), sorted(cwrite_file_list, key=byNumWorkers),
                    cread_label_map, cwrite_label_map, cread_data, cwrite_data, 'numa_zero')



    print('done')