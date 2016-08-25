#!/usr/bin/env python3

import sys
import time
import statistics
import matplotlib.pyplot as plt
from bench.benchmark import Benchmark
from bench.utils import parse_timedelta, pretty_print_duration

def bench_throughput_latency(samples=sys.maxsize, duration=sys.maxsize,
                             *args, **kwargs):
    throughputs = []
    latencies = []
    bench_start = time.time()
    iteration = 1
    errors = 0
    while (iteration <= samples and (time.time()-bench_start) < duration):
        sys.stdout.write(' [*] Iteration %d\r' % iteration)
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
    print(' [*] Benchmark duration: %s (%d iterations)' % \
        (pretty_print_duration(time.time() - bench_start), iteration-1))
    return throughputs, latencies

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print('Usage: ./%s <duration(s|m|h|d)>|<iterations>' % sys.argv[0])
        sys.exit(1)

    duration = parse_timedelta(sys.argv[1])
    samples = sys.maxsize
    if duration is None:
        duration = sys.maxsize
        samples = int(sys.argv[1])

    throughputs = dict()
    latencies = dict()
    for srv_type in (Benchmark.TYPE_VANILLA, Benchmark.TYPE_LKL):
        throughput_vals, latency_vals = \
            bench_throughput_latency(ycsb='a', samples=samples,
                duration=duration, type=srv_type, clients=1, ethreads=4,
                sthreads=100, appthreads=2, slots=256, espins=200,
                esleep=14000, sspins=100, ssleep=8000)

        throughputs[srv_type] = statistics.mean(throughput_vals)
        latencies[srv_type] = statistics.mean(latency_vals)
        print(' [*] Results for %s:' % \
            ('Vanilla' if srv_type == Benchmark.TYPE_VANILLA else 'LKL'))
        print(throughput_vals)
        print(latency_vals)
        print(' [*] Mean throughput: %.2f ops/s (RSD %.2f %%)' %
            (throughputs[srv_type],
            statistics.stdev(throughput_vals)/throughputs[srv_type]*100))
        print(' [*] Mean latency: %.2f us (RSD %.2f %%)' %
            (latencies[srv_type],
            statistics.stdev(latency_vals)/latencies[srv_type]*100))


