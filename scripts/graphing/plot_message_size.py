import matplotlib.pyplot as plt
import numpy as np
import argparse
import os
import re

from matplotlib.font_manager import FontProperties

import scripts.graphing.graph_utils as utils


def plot_totals_bar(cread_file_list, cwrite_file_list, cread_label_map, cwrite_label_map, cread_data, cwrite_data, numa_zone):
    width = .20
    position = 0
    labels = {}
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
        numStr = re.search("([0-9]+)", cread_label_map[cread_file])
        labels[int(numStr.group(0))] = 1
        print(str(cread_tot_average) + ', ' + str(cwrite_tot_average))

    fontP = FontProperties()
    fontP.set_size('large')
    plt.xlabel('message size in bytes', fontsize=18)
    plt.ylabel('system throughput\n(ops/s)', fontsize=18)
    ax = plt.axes()
    ax.ticklabel_format(axis='y', style='sci', scilimits=(0,0))
    plt.yticks(fontsize=18)
    ax.set_xticks(np.arange(0, len(labels)) + 3/2 * width)
    ax.set_xticklabels(sorted(labels.keys()), fontsize=18)
    box = ax.get_position()
    ax.set_position([box.x0, box.y0*2, box.width, box.height * 0.7])

    plt.legend(["RDMA-local reads", "RDMA-local writes", "RDMA-remote reads", "RDMA-local writes"], loc='upper center', ncol=4, bbox_to_anchor=(0.5, 1.4), fontsize=16)
    # plt.show()
    plt.savefig("messagesize.pdf", dpi=300)

if __name__ == "__main__":
    # get arguments
    parser = argparse.ArgumentParser('Read output file from openloop run and produce graphs')
    parser.add_argument('--baseline', dest='baseline', default=None, help='Baseline file for percentage throughput calculation')
    parser.add_argument(dest='cread_root', metavar='RR', type=str, default=None, help='Filename of output file to read')
    parser.add_argument(dest='cwrite_root', metavar='WR', type=str, default=None, help='Filename of output file to read')
    parser.add_argument(dest='numa_zone', metavar='ZONE', type=str, default=None, help='Numa zone of RDMA adapter (values="numa_zero" or "numa_one"')
    args = parser.parse_args()

    if args.cread_root is None or args.cwrite_root is None:
        print("Please provide the directories containing rc_bench openloop data")
        exit(1)

    # sort by the number of workers then numa locality
    def byNumWorkers(str):
        numStr = re.search("([0-9]+)", str)
        return int(numStr.group(0)) * 2 + (0 if re.search("(numa_zero)", str) is not None else 1)

    # sort by NUMA locality then num workers
    def byNuma(str):
        numStr = re.search("([0-9]+)", str)
        if re.search("(numa_zero)", str) is not None:
            return int(numStr.group(0))
        else:
            return 10000 + int(numStr.group(0))


    cread_file_list = []
    for dirpath, dirs, files in os.walk(args.cread_root):
        if len(files) != 0:
            cread_file_list.extend(os.path.join(dirpath, f) for f in os.listdir(dirpath))

    cread_label_map = {}
    for i in range(len(cread_file_list)):
        l = cread_file_list[i].replace(args.cread_root, '')
        l = l.replace('.result','')
        cread_label_map[cread_file_list[i]] = l
    # file_list = sorted(file_list, key=byNumWorkers)

    cread_data = {}
    for file in cread_file_list:
        # get data and configuration
        cread_data[file] = utils.parse_file(file, 0)

    cwrite_file_list = []
    for dirpath, dirs, files in os.walk(args.cwrite_root):
        if len(files) != 0:
            cwrite_file_list.extend(os.path.join(dirpath, f) for f in os.listdir(dirpath))

    cwrite_label_map = {}
    for i in range(len(cwrite_file_list)):
        l = cwrite_file_list[i].replace(args.cwrite_root, '')
        l = l.replace('.result','')
        cwrite_label_map[cwrite_file_list[i]] = l
    # file_list = sorted(file_list, key=byNumWorkers)

    cwrite_data = {}
    for file in cwrite_file_list:
        # get data and configuration
        cwrite_data[file] = utils.parse_file(file)


    # Plot system throughput with respect to time
    # plot_totals(sorted(cread_file_list, key=byNuma), sorted(cwrite_file_list, key=byNuma), cread_label_map, cwrite_label_map, cread_data, cwrite_data)

    # Plot percentage of baseline throughput with respect to time (baseline provided in arguments)
    # plot_percentages(data, stats, config)

    # Plot average client throughput for each run
    # plot_client_average(sorted(cread_file_list, key=byNumWorkers), sorted(cwrite_file_list, key=byNuma), cread_label_map, cwrite_label_map, cread_data, cwrite_data)

    # Plot average total throughput for each run
    plot_totals_bar(sorted(cread_file_list, key=byNumWorkers), sorted(cwrite_file_list, key=byNumWorkers), cread_label_map, cwrite_label_map, cread_data, cwrite_data, args.numa_zone)

    print("CREAD")
    for i, f in enumerate(sorted(cread_file_list, key=byNumWorkers)):
        if i == 0:
            loc_baseline = utils.get_averages(cread_data[f], client=False)
            print(1, utils.get_averages(cread_data[f], client=False))
        elif i == 1:
            rem_baseline = utils.get_averages(cread_data[f], client=False)
            print(1, utils.get_averages(cread_data[f], client=False))
        else:
            print(utils.get_averages(cread_data[f], client=False)/ (loc_baseline if i % 2 == 0 else rem_baseline), utils.get_averages(cread_data[f], client=False))

    print("CWRITE")
    for i, f in enumerate(sorted(cwrite_file_list, key=byNumWorkers)):
        if i == 0:
            loc_baseline = utils.get_averages(cwrite_data[f], client=False)
            print(1)
        elif i == 1:
            rem_baseline = utils.get_averages(cwrite_data[f], client=False)
            print(1)
        else:
            print(utils.get_averages(cwrite_data[f], client=False)/ (loc_baseline if i % 2 == 0 else rem_baseline), utils.get_averages(cwrite_data[f], client=False))