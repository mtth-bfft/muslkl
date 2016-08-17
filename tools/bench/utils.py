#!/usr/bin/env python3

from datetime import timedelta

def pretty_print_duration(secs):
        return str(timedelta(seconds=secs))

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

def parse_timedelta(txt):
    try:
        txt = txt.lower()
        val = int(txt[:-1])
        if txt.endswith('s'):
            return val
        elif txt.endswith('m'):
            return val*60
        elif txt.endswith('h'):
            return val*3600
        elif txt.endswith('d'):
            return val*3600*24
    except:
        pass
    return None

