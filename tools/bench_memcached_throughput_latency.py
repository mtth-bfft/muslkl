#!/usr/bin/env python3

import os
import sys
import time
import statistics
import matplotlib.pyplot as plt
import csv
from bench.benchmark import Benchmark
from bench.utils import parse_timedelta, pretty_print_duration

def bench_throughput_latency(samples=3, max_rsd=0.06,
                             *args, **kwargs):
    while True:
        throughputs = []
        latencies = []
        iteration = 1
        errors = 0
        while iteration <= samples:
            sys.stdout.write(' [*] Iteration %d/%d\r' % (iteration,samples))
            sys.stdout.flush()
            try:
                bench = Benchmark(*args, **kwargs)
                bench.run()
                throughputs.append(bench.throughput)
                latencies.append(bench.latency)
                iteration += 1
            except Exception as e:
                errors += 1
                if errors >= samples:
                    raise
                print(' [!] Iteration %d failed: %s\n%s' % \
                    (iteration, str(type(e)), str(e)))
        mean_throughput = statistics.mean(throughputs)
        mean_latency = statistics.mean(latencies)
        stdev_throughput = statistics.stdev(throughputs)
        stdev_latency = statistics.stdev(latencies)
        rsd_throughput = stdev_throughput/mean_throughput
        rsd_latency = stdev_latency/mean_latency
        if rsd_throughput > max_rsd or rsd_latency > max_rsd:
            sys.stderr.write(' [!] Discarding experiment: '+\
                'throughput RSD %.2f %%, latency RSD %.2f %%\n' %\
                (rsd_throughput*100, rsd_latency*100))
            continue
        return (mean_throughput, mean_latency,
            rsd_throughput, rsd_latency)

if __name__ == "__main__":
    samples = 5
    client_loads = range(1,16)
    max_rsd = 0.10
    type = Benchmark.TYPE_VANILLA
    ycsb = 'a'
    kernel = False
    throughput_vals = []
    latency_vals = []
    results = dict()
    if type == Benchmark.TYPE_VANILLA:
        espins, esleep, ssleep = 500, 16000, 4000
    else:
        espins, esleep, ssleep = 200, 14000, 8000
    for client_load in client_loads:
        throughput, latency, rsd_throughput, rsd_latency = \
            bench_throughput_latency(
                clients=client_load,
                samples=samples, max_rsd=max_rsd,
                ycsb=ycsb, kernel=kernel, type=type,
                ethreads=4, sthreads=100, appthreads=2, slots=256,
                espins=espins, esleep=esleep, sspins=100, ssleep=ssleep
            )
        results[client_load] = {
            "Clients": client_load,
            "Throughput": throughput,
            "Latency": latency,
            "Throughput RSD": rsd_throughput,
            "Latency RSD": rsd_latency,
        }
        print((' [*] %d clients | throughput %d (%.2f %% RSD) | ' + \
            'latency %d us (%.2f %% RSD)') % (client_load, throughput,
            rsd_throughput*100, latency, rsd_latency*100))

root_path = os.path.dirname(os.path.realpath(__file__))
csv_filename = "results_memcached_ycsb" + ycsb + "_" + \
    ("" if kernel else "no") + "kernel_" + \
    ("lkl" if type == Benchmark.TYPE_LKL else "vanilla") + \
    "_" + str(samples) + "samples.csv"

with open(root_path + '/' + csv_filename, 'w') as final_results:
    final_results.truncate()
    fieldnames = ['Clients','Throughput','Latency',
        'Throughput RSD','Latency RSD']
    writer = csv.DictWriter(final_results, fieldnames=fieldnames)
    writer.writeheader()
    for client_load in results:
        writer.writerow(results[client_load])

