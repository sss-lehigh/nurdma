import os
from time import sleep

from scripts.run_rc_bench import run_rc_bench
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser('Run all configurations for rc_bench')
    parser.add_argument('--root', dest='root', default=None, help='root directory of rc_bench configurations')
    parser.add_argument('--bin', dest='bin', default=None, help='binary file for rc_bench')
    parser.add_argument('--as_client', dest='as_client', type=bool, default=False, help='run the benchmarks as a client')
    parser.add_argument('--servername', dest='servername', default=None, help='name of server if running as client')
    parser.add_argument('--out_root', dest='out_root', default=None, help='root of output files')
    args = parser.parse_args()

    if args.root is None:
        print('You must provide a root directory for the configuration files. Exiting...')
        exit(1)
    if args.bin is None:
        print('Please provide a path to the rc_bench binary file')
        exit(1)
    if args.as_client and args.servername is None:
        print('When running as a client you must provide the server name')
        exit(1)

    root_list = os.listdir(args.root)
    dir_list = []
    for dirpath, _, files in os.walk(args.root):
        if len(files) > 0:
            dir_list.append(os.path.join(dirpath))

    dir_list = sorted(dir_list)
    results_list = dir_list[:]
    if args.out_root is not None:
        for i in range(len(results_list)):
            results_list[i] = results_list[i].replace(args.root, args.out_root)
            # r = results_list[i].replace(args.root, '')
            # results_list[i] = os.path.join(args.out_root, r)
            os.makedirs(results_list[i], exist_ok=True)
    else:
        for r in results_list:
            r = ''

    for d, r in zip(dir_list, results_list):
        if args.out_root is not None:
            if (run_rc_bench(d, args.bin, r, args.as_client, args.servername) == 1):
                exit(1)
            sleep(1)
        else:
            if (run_rc_bench(d, args.bin, None, args.as_client, args.servername) == 1):
                exit(1)
            sleep(1)
