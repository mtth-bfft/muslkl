#!/usr/bin/env python3

import sys
import time
import statistics
from bench.benchmark import Benchmark
from bench.utils import parse_timedelta, pretty_print_duration

def bench_throughputs(samples=sys.maxsize, duration=sys.maxsize,
                      maxerrors=100, *args, **kwargs):
    throughputs = []
    bench_start = time.time()
    iteration = 0
    errors = 0
    while (iteration < samples and (time.time()-bench_start) < duration):
        sys.stdout.write(' [*] Running iteration %d%s\r' % (iteration+1,
            (('/%d' % samples) if samples != sys.maxsize else '')))
        try:
            bench = Benchmark(*args, **kwargs)
            bench.run()
            throughputs.append(bench.throughput)
            iteration += 1
        except Exception as e:
            errors += 1
            if errors >= maxerrors:
                raise
            print(' [!] Iteration %d failed: %s\n%s' % \
                (iteration, str(e), str(e)))
    print(' [*] Benchmark duration: %s (%d iterations)' % \
        (pretty_print_duration(time.time() - bench_start), iteration))
    return throughputs

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print('Usage: ./%s <duration(s|m|h|d)>|<iterations>' % sys.argv[0])
        sys.exit(1)

    duration = parse_timedelta(sys.argv[1])
    samples = sys.maxsize
    if duration is None:
        duration = sys.maxsize
        samples = int(sys.argv[1])
    for srv_type in (Benchmark.TYPE_VANILLA, Benchmark.TYPE_LKL):
        res = bench_throughputs(samples=samples, duration=duration,
            type=srv_type, ethreads=4, sthreads=100, appthreads=2,
            espins=500, esleepns=16000)
        print(' [*] Results for %s:' % \
            ('Vanilla' if srv_type == Benchmark.TYPE_VANILLA else 'LKL'))
        print(res)
        print(' [*] Mean throughput: %.2f ops/s' % \
            (samples, statistics.mean(res)))
        print(' [*] Throughput standard deviation:   %.2f ops/s' % \
            statistics.stdev(res))

