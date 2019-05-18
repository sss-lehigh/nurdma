import numpy as np

def parse_throughput(input):
    input = str.split(input, "\t")
    if len(input) < 3:
        print(input)
    client = int(input[0])
    epoch = int(input[1])
    throughput = float(input[2])
    return client, epoch, throughput


def parse_latency(line):
    input = str.split(line, "\t")
    if len(input) < 3:
        print(input)
    client = int(input[0] if input[0] != "total" else -1) + 1
    average = float(input[1][:input[1].find('ns')])
    stddev = float(input[2][:input[2].find('ns')])
    return client, average, stddev

THROUGHPUT = 0
LATENCY = 1
WORKER_LATENCY = 2

parse_func_map = {THROUGHPUT: parse_throughput,
                  LATENCY: parse_latency}

def parse_file(filename, filetype=0):
    data = {}
    with open(filename, 'r') as f:
        print("Parsing " + filename)
        line = f.readline()
        while line != "":
            c, e, t = parse_func_map[filetype](line)
            if c not in data:
                data[c] = {}
            data[c][e] = t
            line = f.readline()
        if filetype == THROUGHPUT:
            for i in range(len(data.keys())):
                d = np.zeros(len(data[i]))
                for j in data[i]:
                    d[j-1] = data[i][j]
                data[i] = d

        return data


def get_averages(data, client=False):
    num_clients = len(data.keys())
    averages = np.zeros(num_clients)
    for i in range(num_clients):
        avg = 0.0
        num_epochs = 0
        # calculates the average throughput for each client
        for j in range(data[i].size):
            avg += data[i][j]
            num_epochs += 1
        averages[i-1] = avg / num_epochs
    # calculates the average total system throughput for the entire run
    total_average = 0.0
    for i in range(num_clients):
        total_average += averages[i]
    # calculates the average of client throughput averages
    client_avg = total_average / num_clients
    return client_avg if client else total_average


def get_totals(data):
    num_epochs = 0
    # get the longest running result
    for i in range(len(data.keys())):
        if len(data[i]) > num_epochs:
            num_epochs = len(data[i])

    totals = np.zeros(num_epochs)
    for i in range(len(data.keys())):
        for j in range(data[i].size):
            totals[j-1] += data[i][j]

    return totals