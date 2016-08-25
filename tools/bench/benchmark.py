#!/usr/bin/env python3

import os
import re
import sys
import subprocess
import signal
import time
import datetime
import threading

ROOT_PATH = os.path.realpath(
    os.path.dirname(os.path.abspath( __file__ )) + '/../../'
)
DEFAULT_VANILLA_CMDLINE = ROOT_PATH + \
    '/../vanilla-sgx-musl/memcached-1.4.30/memcached -t{appthreads} -M -u root'
DEFAULT_CUSTOM_CMDLINE = ROOT_PATH + \
    '/build/tests/80-memcached/memcached/bin/memcached -t{appthreads} -M -u root'
DEFAULT_BENCH_CWD = ROOT_PATH + '/../YCSB/'
DEFAULT_BENCH_CMDLINES = [
    ROOT_PATH + '/../YCSB/bin/ycsb load memcached -threads 10 -s ' + \
    '-P workloads/workload{ycsb} -p memcached.hosts={ip}',
    ROOT_PATH + '/../YCSB/bin/ycsb run memcached -threads {clients} ' + \
    '-s -P workloads/workload{ycsb} -p memcached.hosts={ip}',
]
DEFAULT_BENCH_IP4 = '10.0.1.1'
DEFAULT_BENCH_GW4 = '10.0.1.254'
DEFAULT_BENCH_MASK4 = 24
DEFAULT_BENCH_TAP = 'tap0'
DEFAULT_BENCH_HD = ROOT_PATH + '/tests/80-memcached.img'
DEFAULT_BENCH_TIMEOUT = 20 # seconds
DEFAULT_SERVER_WAITSTOP = 1 # seconds

class Benchmark():
    TYPE_VANILLA = "Host kernel"
    TYPE_LKL = "LKL kernel"

    def __init__(self, type, ethreads=2, sthreads=100, clients=1,
                 espins=500, esleep=16000, sspins=100, ssleep=4000,
                 appthreads=2, ycsb='a', slots=256,
                 kernel=True):
        self.type = type
        self.ycsb = ycsb
        self.kernel = kernel
        self.ethreads = ethreads
        self.sthreads = sthreads
        self.espins = espins
        self.esleep = esleep
        self.sspins = sspins
        self.ssleep = ssleep
        self.appthreads = appthreads
        self.clients = clients
        self.srv_pid = None
        self.srv_out = ''
        if type == Benchmark.TYPE_VANILLA:
            self.bench_ip4 = '127.0.0.1'
        else:
            self.bench_ip4 = DEFAULT_BENCH_IP4
        self.bench_timeout = DEFAULT_BENCH_TIMEOUT
        self.bench_cmdlines = [
            cmd.format(
                ip = self.bench_ip4,
                ycsb = self.ycsb,
                clients = self.clients,
            ) for cmd in DEFAULT_BENCH_CMDLINES
        ]

        if type == Benchmark.TYPE_VANILLA:
            self.srv_cmdline = DEFAULT_VANILLA_CMDLINE
        elif type == Benchmark.TYPE_LKL:
            self.srv_cmdline = DEFAULT_CUSTOM_CMDLINE
        else:
            raise ValueError("Unknown benchmark type")
        self.srv_cmdline = self.srv_cmdline.format(
            appthreads=self.appthreads,
        )

    def server_thread(self):
        srv_env = os.environ.copy()
        srv_env["MUSL_RTPRIO"] = 1
        srv_env["MUSL_KERNEL"] = 1 if self.kernel else 0
        srv_env["MUSL_ETHREADS"] = self.ethreads
        srv_env["MUSL_STHREADS"] = self.sthreads
        srv_env["MUSL_ESPINS"] = self.espins
        srv_env["MUSL_ESLEEP"] = self.esleep
        srv_env["MUSL_SSPINS"] = self.sspins
        srv_env["MUSL_SSLEEP"] = self.ssleep
        srv_env["MUSL_NOLKL"] = 0
        srv_env["MUSL_VERBOSELKL"] = 0
        srv_env["MUSL_HD"] = DEFAULT_BENCH_HD
        srv_env["MUSL_TAP"] = DEFAULT_BENCH_TAP
        srv_env["MUSL_IP4"] = DEFAULT_BENCH_IP4
        srv_env["MUSL_GW4"] = DEFAULT_BENCH_GW4
        srv_env["MUSL_MASK4"] = DEFAULT_BENCH_MASK4
        srv_env = { k : str(v) for (k,v) in srv_env.items() }
        self.srv_out = ''
        srv_proc = None
        err = None
        print(' [*] Starting server ' + self.srv_cmdline)
        try:
            srv_proc = subprocess.Popen(self.srv_cmdline.split(),
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                universal_newlines=True, start_new_session=True,
                env=srv_env)
            self.srv_pid = srv_proc.pid
            self.srv_out = srv_proc.communicate()[0]
        except Exception as e:
            err = e
        if err is not None:
            raise RuntimeError(
                ("Benchmarked server died: %s %s\n" +\
                "====\n%s") % (str(type(e)), str(e), self.srv_out))
        if not self.killed:
            raise RuntimeError(
                ("Benchmark server exited with code %d before " + \
                "the end:\n====\n%s") % (int(srv_proc.poll()),
                self.srv_out))
        print(' [*] Server killed normally')

    def start_server(self):
        self.killed = False
        srv_thread = threading.Thread(target=self.server_thread)
        srv_thread.start()

    def stop_server(self):
        if not self.killed and self.srv_pid is not None:
            print(' [*] Killing server')
            self.killed = True
            os.killpg(self.srv_pid, signal.SIGINT)
            self.srv_pid = None
        else:
            print(' [*] Server already killed')
        time.sleep(DEFAULT_SERVER_WAITSTOP)

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
        self.throughput = self.run_throughput
        self.latency = (self.read_latency_avg + self.update_latency_avg)/2

    def run(self):
        self.start_time = time.time()
        output, cmdline = '', ''
        # This part of the code cannot use Popen(timeout) because
        # it waits indefinitely for subprocesses to die.
        proc = None
        try:
            self.start_server()
            for cmdline in self.bench_cmdlines:
                print(' [*] Running benchmark cmd: ' + cmdline)
                proc = subprocess.Popen(cmdline.split(),
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT, universal_newlines=True,
                    start_new_session=True, cwd=DEFAULT_BENCH_CWD)
                output += proc.communicate(timeout=self.bench_timeout)[0]
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            output += proc.communicate()[0]
            raise RuntimeError(
                "Benchmark cmd timed out:\n%s\n%s" % (cmdline, output)
            )
        except Exception as e:
            if proc is not None and proc.poll() is None:
                os.killpg(proc.pid, signal.SIGKILL)
            if proc is None or proc.poll() is None:
                ret = -1
            else:
                ret = proc.poll()
                output += proc.communicate()[0]
            raise RuntimeError(
                ("Benchmark cmd exited with code %d:\n" + \
                "=========\n%s") % (int(ret), output))
        finally:
            self.stop_server()
            if proc is not None and proc.poll() is None:
                os.killpg(proc.pid, signal.SIGKILL)
        try:
            self.parse_output(output)
        except Exception as e:
            raise RuntimeError(
                ("Benchmark gave unexpected output: %s %s\n" + \
                "=========\n" + \
                "%s\n" + \
                "=========\n" + \
                "%s") % (str(type(e)), str(e), cmdline, output)
            )
        self.stop_time = time.time()
        self.elapsed_time = self.stop_time - self.start_time

