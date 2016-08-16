#!/usr/bin/env python3

import os
import re
import sys
import subprocess
import signal
import time

ROOT_PATH = os.path.realpath(os.path.dirname(os.path.abspath( __file__ ))+'/../')
DEFAULT_VANILLA_CMDLINE = 'MUSL_ETHREADS={ethreads} MUSL_STHREADS={sthreads} ' + \
    'MUSL_ESPINS={espins} MUSL_ESLEEP={esleepns} ' + \
    '%s/../vanilla-sgx-musl/memcached-1.4.30/memcached ' % ROOT_PATH + \
    '-t{appthreads} -M -u root'
DEFAULT_CUSTOM_CMDLINE = 'MUSL_ETHREADS={ethreads} MUSL_STHREADS={sthreads} ' + \
    'MUSL_ESPINS={espins} MUSL_ESLEEP={esleepns} ' + \
    'MUSL_HD=%s/tests/80-memcached.img MUSL_TAP=tap0 ' + \
    'MUSL_IP4=10.0.1.1 MUSL_GW4=10.0.1.254 MUSL_MASK4=24 ' + \
    '%s/build/tests/80-memcached/memcached/bin/memcached ' % ROOT_PATH + \
    '-t{appthreads} -M -u root'
DEFAULT_BENCH_CMDLINE = '; '.join(
    ('cd %s/../YCSB/; ./bin/ycsb ' % ROOT_PATH + \
    '%s memcached -s -P workloads/workloada ' + \
    '-p "memcached.hosts={ip}"') % cmd for cmd in ('load','run')
)
DEFAULT_BENCH_TIMEOUT = 20 # seconds
DEFAULT_SERVER_WAITSTART = 3 # seconds

class Benchmark():
    TYPE_VANILLA = 1
    TYPE_LKL = 2

    def __init__(self, type, ethreads, sthreads,
                 espins, esleepns, appthreads,
                 srv_cmdline=None,
                 bench_cmdline=None,
                 bench_timeout=DEFAULT_BENCH_TIMEOUT):
        self.type = type
        self.ethreads = ethreads
        self.sthreads = sthreads
        self.espins = espins
        self.esleepns = esleepns
        self.appthreads = appthreads
        self.srv_proc = None
        if bench_cmdline is None:
            bench_cmdline = DEFAULT_BENCH_CMDLINE
        self.bench_cmdline = bench_cmdline
        self.bench_timeout = bench_timeout
        if srv_cmdline is None:
            if type == Benchmark.TYPE_VANILLA:
                self.srv_cmdline = DEFAULT_VANILLA_CMDLINE
            elif type == Benchmark.TYPE_LKL:
                self.srv_cmdline = DEFAULT_CUSTOM_CMDLINE
            else:
                raise ValueError("Unknown benchmark type")
        else:
            self.srv_cmdline = srv_cmdline
        self.srv_cmdline = self.srv_cmdline.format(
            appthreads=self.appthreads,
            ethreads=self.ethreads,
            sthreads=self.sthreads,
            espins=self.espins,
            esleepns=self.esleepns,
        )

    def start_server(self):
        self.srv_proc = subprocess.Popen(self.srv_cmdline, shell=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            preexec_fn=os.setsid)

    def stop_server(self):
        if self.srv_proc is not None:
            os.killpg(self.srv_proc.pid, signal.SIGKILL)
            self.srv_proc = None

    def parse_output(self, out):
        def extract(var, txt):
            return float(re.search(r'^.*' + var + '.*\s(\d+(\.\d+)?)\D*$',
                txt, re.IGNORECASE|re.MULTILINE).group(1))
        loadpos = out.find('[OVERALL], RunTime')
        runpos = out.find('[OVERALL], RunTime', loadpos+1)
        if loadpos < 0 or runpos < 0:
            raise ValueError("Unable to parse benchmark output:\n%s" % out)
        load = out[loadpos:runpos]
        run = out[runpos:]
        self.load_throughput = extract('overall.*throughput', load)
        self.insert_latency_avg = extract('insert.*average.*latency', load)
        self.insert_latency_min = extract('insert.*min.*latency', load)
        self.insert_latency_max = extract('insert.*max.*latency', load)
        self.insert_latency_95 = extract('insert.*95.*latency', load)
        self.run_throughput = extract('overall.*throughput', run)
        self.read_latency_avg = extract('read.*average.*latency', run)
        self.read_latency_min = extract('read.*min.*latency', run)
        self.read_latency_max = extract('read.*max.*latency', run)
        self.read_latency_95 = extract('read.*95.*latency', run)
        self.update_latency_avg = extract('update.*average.*latency', run)
        self.update_latency_min = extract('update.*min.*latency', run)
        self.update_latency_max = extract('update.*max.*latency', run)
        self.update_latency_95 = extract('update.*95.*latency', run)
        self.throughput = (self.load_throughput + self.run_throughput)/2

    def run(self):
        res = ''
        ip = '127.0.0.1' if self.type == Benchmark.TYPE_VANILLA else '10.0.1.1'
        cmdline = self.bench_cmdline.format(ip=ip)
        clean_exit = False
        stdout = ''
        stderr = ''
        try:
            self.start_server()
            proc = subprocess.Popen(cmdline, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, universal_newlines=True,
                shell=True, start_new_session=True)
            start_time = time.time()
            while proc.poll() is None:
                time.sleep(0.1)
                stdout += proc.stdout.read()
                stderr += proc.stderr.read()
                if ((time.time() - start_time) > self.bench_timeout):
                    raise RuntimeError(
                        "Subprocess timeout after %d seconds:\n====\n%s" % \
                        (time.time() - start_time), (stdout + stderr)
                    )
            clean_exit = True
        finally:
            if not clean_exit:
                os.killpg(proc.pid, signal.SIGTERM)
            self.stop_server()
        self.parse_output(stdout)

def range_custom(a, b, step="+1"):
    val = a
    mult = 1
    add = 0
    if len(step)>=2 and step[0] == 'x':
        mult = int(step[1:])
    else:
        add = int(step)
    while val <= b:
        val += add
        val *= mult
        yield val

if __name__ == "__main__":
    bench = Benchmark(Benchmark.TYPE_VANILLA, ethreads=4,
            sthreads=100, appthreads=2, espins=500, esleepns=16000)
    bench.run()
    print(bench.throughput)

