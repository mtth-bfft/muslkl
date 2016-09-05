#!/usr/bin/env python3

import os
import re
import sys
import subprocess
import statistics
import csv
from bench.utils import pretty_print_size, pretty_print_duration
from datetime import datetime
import matplotlib.pyplot as plt

# Installation paths and test encryption keys
syscalldrv_path = '/dev/syscall0'
root_path = os.path.dirname(os.path.realpath(__file__))
iozone_path = root_path + '/../tests/60-iozone/iozone'
encrypt_tool_path = root_path + '/../build/tools/disk_encrypt'
encrypt_default_key = '000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F'

# System-dependent functions
def flush_caches():
    if os.geteuid() != 0:
        return
    os.system('echo 3 >/proc/sys/vm/drop_caches')

# Detect binaries compiled with heavy debug options
def is_debug_executable(path):
    res = subprocess.check_output(['strings', path], shell=False,
            universal_newlines=True, stderr=subprocess.PIPE)
    match = re.search(r'^musl.*debug', res, flags=re.MULTILINE|re.IGNORECASE)
    return (match is not None)

def total_ram_bytes():
    return os.sysconf('SC_PAGE_SIZE') * os.sysconf('SC_PHYS_PAGES')

class IOZoneBenchmark(object):

    def __init__(self, root=True, kernel=True, recordsizes=range(1,33),
        repeats=5, max_rsd=0.10, ethreads=4, sthreads=100,
        espins=200, esleep=14000, sspins=100, ssleep=8000):

        self.recordsizes = list(recordsizes)
        self.repeats = repeats
        self.max_rsd = max_rsd
        self.kernel = kernel
        self.root = root
        self.ethreads = ethreads
        self.sthreads = sthreads
        self.espins = espins
        self.esleep = esleep
        self.sspins = sspins
        self.ssleep = ssleep
        self.disk_path = ''
        if not os.path.exists(iozone_path):
            raise RuntimeError("Benchmark executable not found. " + \
                "Please run 'make -C tests iozone' before this script.")
        if is_debug_executable(iozone_path):
            raise RuntimeError("Will not run benchmarks on executable" +\
                " compiled with DEBUG option");
        self.iozone_cmd = iozone_path + ' -a -s{filesizek}k ' + \
            '-r{recordsizek}k -i0 -i2'
        self.ramsize = 20*1024*1024

    def run(self):
        self.time_start = datetime.now()
        self.results = dict()
        self.env = os.environ.copy()
        self.env["MUSL_KERNEL"] = 0
        self.env["MUSL_RTPRIO"] = 0
        if self.root:
            if os.geteuid() != 0:
                raise RuntimeError("Must be run as root to use MUSL_KERNEL")
            self.env["MUSL_RTPRIO"] = 1
            self.iozone_cmd = 'nice -n -25 ' + self.iozone_cmd
        if self.kernel:
            if not os.path.exists(syscalldrv_path):
                raise RuntimeError("Please install a syscall driver")
            self.env["MUSL_KERNEL"] = 1
        self.env["MUSL_ETHREADS"] = self.ethreads
        self.env["MUSL_STHREADS"] = self.sthreads
        self.env["MUSL_ESPINS"] = self.espins
        self.env["MUSL_ESLEEP"] = self.esleep
        self.env["MUSL_SSPINS"] = self.sspins
        self.env["MUSL_SSLEEP"] = self.ssleep
        self.env["MUSL_NOLKL"] = 0
        self.env["MUSL_VERBOSELKL"] = 0
        self.env["MUSL_LKLRAM"] = self.ramsize
        self.env["MUSL_TAP"] = ''
        self.env["MUSL_HD"] = self.disk_path
        self.env = { k : str(v) for (k,v) in self.env.items() }
        for recordsize in self.recordsizes:
            read_results = []
            write_results = []
            iteration = 1
            errors = 0
            while iteration <= self.repeats:
                sys.stdout.write(' [*] Iteration %d/%d\r' % (iteration,
                    self.repeats))
                try:
                    flush_caches()
                    cmdline = self.iozone_cmd.format(
                        filesizek = int(self.file_size/1024),
                        recordsizek = recordsize,
                    )
                    print(' [*] Running ' + cmdline)
                    out = subprocess.check_output(cmdline.split(),
                        universal_newlines=True, env=self.env,
                        stderr=subprocess.STDOUT)
                    match = re.search(r'^\s*kB.+\n.+\D(\d+)\s+(\d+)\s*$', out,
                        flags=re.MULTILINE)
                    if match is None:
                        sys.stderr.write(' [!] Unable to parse result: %s\n' % out)
                        errors += 1
                    else:
                        read = int(match.group(1))/1024
                        write = int(match.group(2))/1024
                        read_results.append(read)
                        write_results.append(write)
                        iteration += 1
                except subprocess.CalledProcessError as e:
                    errors += 1
                    if errors >= self.repeats:
                        raise
                    sys.stderr.write(' [!] Iteration %d/%d failed:\n%s\n%s' %
                        (iteration, self.repeats, str(e), str(e.output)))
            read_mean = statistics.mean(read_results)
            write_mean = statistics.mean(write_results)
            read_rsd = 0
            write_rsd = 0
            if self.repeats >= 2:
                read_rsd = statistics.stdev(read_results)/read_mean
                write_rsd = statistics.stdev(write_results)/write_mean
            self.results[recordsize] = {
                "Record size (KiB)": recordsize,
                "Random reads (MiB/s)": read_mean,
                "Random writes (MiB/s)": write_mean,
                "Read RSD": read_rsd,
                "Write RSD": write_rsd,
            }
            sys.stdout.write((' [*] Record size %dKiB: R %.3f MiB/s (%.2f %%)' + \
                ' W %.3f MiB/s (%.2f %%)\n') % (recordsize, read_mean,
                    read_rsd*100, write_mean, write_rsd*100))
        self.time_elapsed = datetime.now() - self.time_start
        with open(self.results_path, 'w') as final_results:
            final_results.truncate()
            fieldnames = ['Record size (KiB)','Random reads (MiB/s)',
                'Random writes (MiB/s)','Read RSD', 'Write RSD']
            writer = csv.DictWriter(final_results, fieldnames=fieldnames)
            writer.writeheader()
            for recordsize in self.recordsizes:
                writer.writerow(self.results[recordsize])
        sys.stdout.write(' [*] Results written to %s\n' %
                self.results_path)

class IOZoneTMPFSBenchmark(IOZoneBenchmark):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.file_size = 1024*1024*1024
        self.ramsize += self.file_size
        self.results_path = 'iozone_results_tmpfs_%s_%s_%s_%dsamples.csv' %\
            ("kernel" if self.kernel else "nokernel",
            "root" if self.root else "noroot",
            pretty_print_size(self.file_size), self.repeats)

class IOZonePlainBenchmark(IOZoneBenchmark):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.iozone_cmd += ' -f /largefile'
        self.disk_size = int((total_ram_bytes() * 2)/1024)*1024
        self.file_size = 1024*1024*1024
        self.disk_path = root_path + '/test_disk_plain.img'
        self.results_path = 'iozone_results_plain_%s_%s_%s_%dsamples.csv' %\
            ("kernel" if self.kernel else "nokernel",
            "root" if self.root else "noroot",
            pretty_print_size(self.file_size), self.repeats)
        size = 0
        if os.path.isfile(self.disk_path):
            try:
                size = os.path.getsize(self.disk_path)
            except:
                pass
            if size != self.disk_size:
                os.unlink(self.disk_path)
        if size != self.disk_size:
            print(' [*] Creating test disk, this will take a while...')
            try:
                subprocess.check_call(['dd','if=/dev/zero','of=' + self.disk_path,
                    'bs=1k', 'count=' + str(int(self.disk_size/1024))],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                subprocess.check_call(['mkfs.ext4','-F',self.disk_path],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            except subprocess.CalledProcessError as e:
                raise RuntimeError("Unable to create benchmark disk:\n" +
                    str(e) + '\n=====\n' + str(e.output))

class IOZoneEncryptionBenchmark(IOZoneBenchmark):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.iozone_cmd += ' -f /largefile'
        self.disk_size = int((total_ram_bytes() * 2)/1024)*1024
        self.file_size = 1024*1024*1024
        self.disk_path = root_path + '/test_disk_encrypted.img'
        self.results_path = 'iozone_results_crypt_%s_%s_%s_%dsamples.csv' %\
            ("kernel" if self.kernel else "nokernel",
            "root" if self.root else "noroot",
            pretty_print_size(self.file_size), self.repeats)
        size = 0
        if os.path.isfile(self.disk_path):
            try:
                size = os.path.getsize(self.disk_path)
            except:
                pass
            if size != self.disk_size:
                os.unlink(self.disk_path)
        if size != self.disk_size:
            print(' [*] Creating test disk (%d bytes, %s), this will take a while...' %\
                    (self.disk_size, pretty_print_size(self.disk_size)))
            try:
                subprocess.check_call(['dd','if=/dev/zero','of=%s.tmp' % self.disk_path,
                    'bs=1k', 'count=' + str(int(self.disk_size/1024))],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                subprocess.check_call(['mkfs.ext4','-F',self.disk_path + '.tmp'],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                subprocess.check_call([encrypt_tool_path, '-e', '-k',
                    encrypt_default_key, '-i', self.disk_path + '.tmp',
                    '-o', self.disk_path])
                os.unlink(self.disk_path + '.tmp')
            except subprocess.CalledProcessError as e:
                raise RuntimeError("Unable to create benchmark disk:\n" +
                    str(e) + '\n=====\n' + str(e.output))

if __name__ == '__main__':
    bench = IOZonePlainBenchmark(repeats=5, recordsizes=range(1,49))
    bench.run()
    sys.stdout.write(' [*] Benchmark completed, total time %s\n' %
        pretty_print_duration(bench.time_elapsed))

