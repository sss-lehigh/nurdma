from matplotlib import pyplot as plt
import numpy as np
import argparse
import os

def byClients(path):
    """Sorts based on the number of clients used in the closed loop run
    
    Args:
        path (str): The file path to a run result
    
    Returns:
        int: The rank of the file
    """
    numa = 0
    write = 0
    if 'numa_one/' in path:
        numa = 1
    if 'cwrite/' in path:
        write = 1
    if 'clients_one/' in path:
        return 1 * 3 + numa + write
    if 'clients_two/' in path:
        return 2 * 3 + numa + write
    if 'clients_three/' in path:
        return 3 * 3 + numa + write
    if 'clients_four/' in path:
        return 4 * 3 + numa + write
    if 'clients_eight/' in path:
        return 5 * 3 + numa + write
    if 'clients_sixteen/' in path:
        return 6 * 3 + numa + write
    if 'clients_twentyfour/' in path:
        return 7 * 3 + numa + write
    if 'clients_thirtytwo/' in path:
        return 8 * 3 + numa + write
    if 'clients_fortyeight/' in path:
        return 9 * 3 + numa + write
    if 'clients_sixtyfour/' in path:
        return 10 * 3 + numa + write
    if 'clients_onetwentyeight/' in path:
        return 11 * 3 + numa + write


def getData(path_lst):
    data = {}
    for path in path_lst:
        # get run configuration
        cop = 'cread' if 'cread' in path else 'cwrite'
        numa = 'numa_zero' if 'numa_zero' in path else 'numa_one'

        if cop not in data:
            data[cop] = {}
        if numa not in data[cop]:
            data[cop][numa] = [[], []]

        with open(path, 'r') as f:
            line = f.readlines()[-1]
            line = line.split('\t')
            total = float(line[1].split('ns')[0])
            stddev = float(line[2].split('ns')[0])
            data[cop][numa][0] = np.append(data[cop][numa][0], total)
            data[cop][numa][1] = np.append(data[cop][numa][1], stddev)

    return data


def plotLatency(data, clients, speedup=False):
    plt.figure(figsize=(6,3))

    # plot raw latency
    ax = plt.subplot(1,1,1)

    read_data = data['cread']
    numa_zero = read_data['numa_zero']
    numa_one = read_data['numa_one']
    read_ratio =  1 - numa_zero[0] / numa_one[0]

    if not speedup:
      plt.plot(clients[:8], numa_zero[0][:8], color='red', marker='^', label='RDMA-local reads', markersize=10)
      plt.plot(clients[:8], numa_one[0][:8], color='yellow', marker='D', label='RDMA-remote reads', markersize=10)

    write_data = data['cwrite']
    numa_zero = write_data['numa_zero']
    numa_one = write_data['numa_one']
    write_ratio =  1 - numa_zero[0] / numa_one[0]

    if not speedup:
      plt.plot(clients[:8], numa_zero[0][:8], color='aqua', marker='*', label='RDMA-local writes', markersize=12)
      plt.plot(clients[:8], numa_one[0][:8], color='grey', marker='o', label='RDMA-remote writes', markersize=10)

    ax.set_ylabel('Latency (ns)', fontsize=14)
    ax.set_xlabel('# QPs', fontsize=14)

    # plot latency speedup
    if speedup:
      plt.plot(clients[:6], read_ratio[:6], color='red', marker='^', label='RDMA-local reads', markersize=10)
      plt.plot(clients[:6], write_ratio[:6], color='aqua', marker='*', label='RDMA-local writes', markersize=12)
      ax.set_ylabel('Speedup over\nRDMA-remote', fontsize=14)
        
    plt.legend(loc='best')
    plt.tight_layout()
    plt.draw()

    # reformat axes
    vals = ax.get_xticks()
    ax.set_xticklabels(['{:,.0f}'.format(x) for x in vals], fontsize=14)
    vals = ax.get_yticks()
    if speedup:
      ax.set_yticklabels(['{:,.0%}'.format(x) for x in vals], fontsize=14)
    else:
      ax.set_yticklabels(['{:,.0f}'.format(y) for y in vals], fontsize=14)
    plt.tight_layout()
    # plt.show()
    plt.savefig('latency.pdf' if not speedup else 'latspeedup.pdf')

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--basedir", type=str, required=True, help="root directory containing closed loop runs for a single size of memory access")
    args = parser.parse_args()

    # get list of all files to read from (ordered by number of clients)
    file_paths = []
    for dirpath, dirs, files in os.walk(args.basedir):
        if len(files) > 0: # ignore directories
            for f in files:
                file_paths.append(os.path.join(dirpath, f))
    file_paths = sorted(file_paths, key=byClients)

    # extract data from each file
    data = getData(file_paths)

    # first plot reads then writes
    clients = [1, 2, 3, 4, 8, 16, 24, 32, 48, 64, 128]
    plotLatency(data, clients)
    plotLatency(data, clients, speedup=True)
