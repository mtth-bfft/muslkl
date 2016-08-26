#!/usr/bin/env python3

import os
import sys
import re
import time
import base64
import subprocess
import signal
import statistics
from collections import OrderedDict

test_duration = 20
test_repeats = 5

tools_dir = os.path.dirname(os.path.realpath(__file__))
tool_path = tools_dir + '/../build/tools/disk_encrypt'
random_bits = os.urandom(128)
random_pass = base64.b64encode(random_bits)

commands = OrderedDict()
commands['/dev/random'] = tool_path + ' -i /dev/random'
commands['/dev/urandom'] = tool_path + ' -i /dev/urandom'
commands['OpenSSL AES256-CTR'] = ('openssl enc -aes-256-ctr -pass pass:"%s" -nosalt '+\
        '< /dev/zero | %s') % (random_pass, tool_path)
commands['/dev/zero'] = tool_path + ' -i /dev/zero'

def get_bench_results(cmd):
    results = []
    for iteration in range(test_repeats):
        proc = subprocess.Popen(cmd, shell=True, universal_newlines=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                preexec_fn=os.setsid)
        time.sleep(test_duration)
        os.killpg(os.getpgid(proc.pid), signal.SIGINT)
        out, _ = proc.communicate()
        bytes = re.search(r'\b(\d+)\b\W*bytes', out)
        secs = re.search(r'\b(\d+)\b\W*seconds', out)
        if bytes is None or secs is None:
            raise RuntimeError("Unable to parse benchmark output: %s" % out)
        results.append(float(bytes.group(1))/int(secs.group(1)))
    return (statistics.mean(results), statistics.stdev(results))

if __name__ == '__main__':
    sys.stdout.write(' '*20 + '        Encryption        |          Decryption\n')
    for name, cmd in commands.items():
        cmd += ' -o /dev/null'
        cmd += ' -k0102030405060708091011121314151617181920212223242526272829303132'
        sys.stdout.write(name.rjust(20) + ' ')
        sys.stdout.flush()
        mean, stdev = get_bench_results(cmd + ' -e')
        sys.stdout.write('{:9.4f} MiB/s ({:.2f} %)  '.format(
            float(mean)/(1024*1024), float(stdev)/mean*100))
        sys.stdout.flush()
        mean, stdev = get_bench_results(cmd + ' -e')
        sys.stdout.write('{:9.4f} MiB/s ({:.2f} %)'.format(
            float(mean)/(1024*1024), float(stdev)/mean*100))
        print('')

