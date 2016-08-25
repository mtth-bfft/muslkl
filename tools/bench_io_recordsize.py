#!/usr/bin/env python3

import os
import re
import sys
import subprocess
import statistics
import json
import csv
from datetime import datetime
import matplotlib.pyplot as plt

# Path to IOZone's executable (run make iozone before this script)
root_path = os.path.dirname(os.path.realpath(__file__))
iozone_path = root_path + '/../tests/60-iozone/iozone'

def bench_iozone(samples=3, max_rsd=0.06, *args, **kwargs):
    while True:

# Record sizes to test in KiB
recordsizes = range(1, 33)
# File size in KiB (must be greater than L3 cache)
filesize = 100*1024
# Number of identical benchmarks per experiment
repeats = 1 # 5
# RSD threshold to accept an experiment
max_rsd = 0.10
# Which FS to benchmark: ephemeral or on-disk?
tmpfs = False

time_start = datetime.now()
read_means = dict()
read_stdevs = dict()
write_means = dict()
write_stdevs = dict()
read_results = dict()
write_results = dict()
for recordsize in recordsizes:
    ramsize = 20*1024*1024 # in bytes
    if tmpfs:
        ramsize += filesize * 1024
    cmd = ("MUSL_RTPRIO=1 MUSL_LKLRAM=%d MUSL_VERBOSELKL=0 " + \
        "MUSL_ETHREADS=3 MUSL_STHREADS=100 MUSL_HD=%s " + \
        "%s -a -s%dk %s -r%dk -i0 -i2 | " + \
        "tail -n3 | head -n1 | tr -s ' ' | cut -d' ' -f5-6") % \
        (ramsize, root_path + '/largedisk.img', iozone_path,
         filesize, ("" if tmpfs else "-f/largefile"), recordsize)

    sys.stderr.write(' [+] Starting recordsize %d\n' % recordsize)
    while True:
        read_results[recordsize] = []
        write_results[recordsize] = []
        for iteration in range(repeats):
            sys.stderr.write(' [+] Iteration %d/%d ...\r' % (iteration+1, repeats))
            sys.stderr.flush()
            try:
                out = subprocess.check_output(cmd, stderr=subprocess.STDOUT,
                    shell=True, universal_newlines=True)
                match = re.search(r'^\s*(\d+)\s*(\d+)\s*$', out,
                        flags=re.MULTILINE)
                if match is None:
                    sys.stderr.write(' [!] Unable to parse result: %s\n' % out)
                else:
                    read, write = int(match.group(1)), int(match.group(2))
                    read_results[recordsize].append(read)
                    write_results[recordsize].append(write)
            except subprocess.CalledProcessError as e:
                sys.stderr.write(' [!] Error during benchmark: %s\n' % str(e))

        read_means[recordsize] = statistics.mean(read_results[recordsize])
        read_stdevs[recordsize] = statistics.stdev(read_results[recordsize])
        write_means[recordsize] = statistics.mean(write_results[recordsize])
        write_stdevs[recordsize] = statistics.stdev(write_results[recordsize])
        sys.stderr.write(' [+] Iteration %d/%d (stdev: R %.2f %% W %.2f %%)\n' %
            (iteration+1, repeats, read_stdevs[recordsize]/read_means[recordsize]*100,
            write_stdevs[recordsize]/write_means[recordsize]*100))
        if read_stdevs[recordsize]/read_means[recordsize] > max_rsd or \
            write_stdevs[recordsize]/write_means[recordsize] > max_rsd:
            sys.stderr.write(' [!] Discarding experiment\n')
            continue
        break

csv_filename = "stable_results_" + ("tmpfs" if tmpfs else "ondisk") + ".csv"
with open(root_path + '/' + csv_filename, 'w') as final_results:
    final_results.truncate()
    fieldnames = ['Record size','Read mean','Write mean',
        'Read stdev','Write stdev']
    writer = csv.DictWriter(final_results, fieldnames=fieldnames)
    writer.writeheader()
    for recordsize in recordsizes:
        writer.writerow({'Record size':recordsize,
            'Read mean': float(read_means[recordsize])/1024,
            'Write mean': float(write_means[recordsize])/1024,
            'Read stdev': float(read_stdevs[recordsize])/1024,
            'Write stdev': float(write_stdevs[recordsize])/1024,
        })

sys.stderr.write('Total benchmark time: %s\n' % str(datetime.now()-time_start))
sys.stderr.write('Results written to %s\n' % csv_filename)
