import os
import re
import sys
import subprocess
import statistics
import json
import matplotlib.pyplot as plt

# Path to IOZone's executable (run make iozone before this script)
root_path = os.path.dirname(os.path.realpath(__file__))
iozone_path = root_path + '/../tests/60-iozone/iozone'
# Record sizes to test in KiB
recordsizes = range(1, 32)
# File size in KiB (must be greater than L3 cache)
filesize = 50*1024
# Number of identical benchmarks to compute 30% trimmed averages
# (must be a multiple of 3)
repeats = 6

read_results = dict()
write_results = dict()
for recordsize in recordsizes:
    ramsize = filesize*1024 + 20*1024*1024 # in bytes
    cmd = ("MUSL_LKLRAM=%d MUSL_VERBOSELKL=0 " + \
        "MUSL_ETHREADS=4 MUSL_STHREADS=4 " + \
        "%s -a -s%dk -r%dk -i0 -i2 | " + \
        "tail -n3 | head -n1 | tr -s ' ' | cut -d' ' -f5-6") % \
        (ramsize, iozone_path, filesize, recordsize)

    sys.stderr.write(' [+] Starting recordsize %d\n' % recordsize)
    read_results[recordsize] = []
    write_results[recordsize] = []
    for iteration in range(repeats):
        sys.stderr.write(' [+] Iteration %d/%d ...\r' % \
            (iteration+1, repeats))
        sys.stderr.flush()
        try:
            out = subprocess.check_output(cmd, stderr=subprocess.STDOUT,
                shell=True, universal_newlines=True)
            match = re.match(r'^\s*(\d+)\s*(\d+)\s*$', out)
            if match is None:
                sys.stderr.write(' [!] Unable to parse result: ' + \
                    out + '\n')
                sys.stderr.flush()
            else:
                read, write = int(match.group(1)), int(match.group(2))
                read_results[recordsize].append(read)
                write_results[recordsize].append(write)
        except subprocess.CalledProcessError as e:
            sys.stderr.write(' [!] Error during benchmark: ' + \
                str(e) + '\n')

with open(root_path + '/raw_results.json', 'w') as raw_results:
    raw_results.truncate()
    json.dump([read_results, write_results], raw_results)

if repeats >= 6:
    for recordsize in recordsizes:
        read_results[recordsize] = \
            sorted(read_results[recordsize])[int(repeats/3):int(2*repeats/3)]
        write_results[recordsize] = \
            sorted(write_results[recordsize])[int(repeats/3):int(2*repeats/3)]

with open(root_path + '/final_results.json', 'w') as final_results:
    final_results.truncate()
    json.dump([read_results, write_results], final_results)

sys.stderr.write('========= Results:\n')
read_means = dict()
read_stdevs = dict()
write_means = dict()
write_stdevs = dict()
for recordsize in recordsizes:
    read_means[recordsize] = statistics.mean(read_results[recordsize])
    read_stdevs[recordsize] = statistics.stdev(read_results[recordsize])
    write_means[recordsize] = statistics.mean(write_results[recordsize])
    write_stdevs[recordsize] = statistics.stdev(write_results[recordsize])
    sys.stderr.write('R %d KiB  %d  +/-%d (%.2f %%)\n' % (recordsize,
        read_means[recordsize], read_stdevs[recordsize],
        read_stdevs[recordsize]/read_means[recordsize]*100))
    sys.stderr.write('W %d KiB  %d  +/-%d (%.2f %%)\n' % (recordsize,
        write_means[recordsize], write_stdevs[recordsize],
        write_stdevs[recordsize]/write_means[recordsize]*100))

