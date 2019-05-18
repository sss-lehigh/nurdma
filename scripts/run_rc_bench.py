import argparse
import os
import subprocess
from time import sleep
import sys

# Runs benchmark for all configurations in the config_dir
def run_rc_bench(config_dir, bin, output_dir, as_client, servername):
    if bin is None:
        print("provide the location of the binary")
        exit(1)
    if as_client and servername is None:
        print("provide the server name when running as a client")
        exit(1)

    config_list = sorted(os.listdir(config_dir))

    for config in config_list:
        output_target = config.split('.')[0]
        output_target = ''.join(output_target)
        if as_client and output_dir is not None:
            with open(output_dir+'/'+output_target+'.result', 'w') as file:
                call_list = [bin, '--config='+config_dir+'/'+config, '--servername='+servername]
                if (subprocess.call(call_list, stdout=file) == 1):
                    return 1
                sleep(2) # ensures that the server starts first to avoid errors
        elif as_client:
            call_list = [bin, '--config='+config_dir+'/'+config, '--servername='+servername]
            if (subprocess.call(call_list) == 1):
                return 1
            sleep(2) # ensures that the server starts first to avoid errors
        else:
            call_list = [bin, '--config='+config_dir+'/'+config]
            if (subprocess.call(call_list) == 1):
                return 1

    return 0

if __name__ == "__main__":
    parser = argparse.ArgumentParser('Run rc_bench on all configuration files in a given directory. Output files will'
                                     'be created for each run')
    parser.add_argument('--config_dir', dest='config_dir', default=None, help='directory containing configuration files for rc_bench')
    parser.add_argument('--bin', dest='bin', default=None, help='location of binary file to run')
    parser.add_argument('--output_dir', dest='output_dir', default=None, help='name of directory where to save output')
    parser.add_argument('--as_client', dest='as_client', type=bool, default=False, help='whether to run the experiment'
                                                                                        'as a client')
    parser.add_argument('--servername', dest='servername', type=str, default=None, help='name of server if running as a '
                                                                                        'client')
    args = parser.parse_args()
    run_rc_bench(args.config_dir, args.bin, args.output_dir, args.as_client, args.servername)
