#!/usr/bin/env python3

import sys
import os
import subprocess
import shlex
import time
import math
from typing import List
import matplotlib as mpl
import matplotlib.pyplot as plt

KILO = 1024
MEGA = KILO * KILO
GIGA = MEGA * KILO
RUNTIME = 60


class FioInput:

    def __init__(self, bs: int, rwmixread: int, size: int) -> None:
        self.bs = bs
        self.numjobs = 1
        self.rw = 'randrw'
        self.rwmixread = rwmixread
        self.rwmixwrite = 100 - rwmixread
        self.size = size
        self.ioengine = 'psync'
        self.runtime = RUNTIME


class FioOutput:

    def __init__(self, output) -> None:
        # see: https://www.andypeace.com/fio_minimal.html
        fileds = ['0'] + output.split(';')
        self.rd_iops = int(fileds[8])
        self.rd_bw = int(fileds[7])
        self.rd_lat = float(fileds[40])
        self.wr_iops = int(fileds[49])
        self.wr_bw = int(fileds[48])
        self.wr_lat = float(fileds[81])


def create_fio_command(workdir: str, tag: str, fio_in: FioInput) -> str:
    name = 'fio-{}-{}-{}'.format(fio_in.rw, fio_in.ioengine, tag)
    filename = os.path.join(workdir, name)
    cmd = 'fio '
    cmd = cmd + '--filename={} '.format(filename)
    cmd = cmd + '--fsync_on_close=1 '
    cmd = cmd + '--thinktime=0 '
    cmd = cmd + '--norandommap '
    cmd = cmd + '--fallocate=none '
    cmd = cmd + '--group_reporting --minimal '
    cmd = cmd + '--gtod_reduce=1 --randrepeat=1 '
    cmd = cmd + '--unlink=1 '
    cmd = cmd + '--numjobs={} '.format(fio_in.numjobs)
    cmd = cmd + '--name={} '.format(name)
    cmd = cmd + '--bs={} '.format(fio_in.bs)
    cmd = cmd + '--size={} '.format(fio_in.size)
    cmd = cmd + '--rw={} '.format(fio_in.rw)
    cmd = cmd + '--rwmixread={} '.format(fio_in.rwmixread)
    cmd = cmd + '--rwmixwrite={} '.format(fio_in.rwmixwrite)
    cmd = cmd + '--ioengine={} '.format(fio_in.ioengine)
    cmd = cmd + '--runtime={} '.format(fio_in.runtime)
    return cmd


def execute_fio_subprocess(workdir: str, fio_in: FioInput) -> FioOutput:
    pipe = subprocess.PIPE
    env = os.environ.copy()
    tag = str(time.time()).split('.')[0]
    cmd = create_fio_command(workdir, tag, fio_in)
    pipes = subprocess.Popen(shlex.split(cmd), stdout=pipe, stderr=pipe,
                             cwd=workdir, shell=False, env=env)
    std_out, _ = pipes.communicate()
    fio_out = FioOutput(str(std_out.decode('UTF-8')))
    return fio_out


def create_fio_inputs(bs: int, jobs: int) -> List[FioInput]:
    fio_ins = []
    size = int(GIGA)
    rwmixread = range(10, 95, 5)
    for rwm in rwmixread:
        fio_in = FioInput(bs, rwm, int(size / jobs))
        fio_ins.append(fio_in)
    return fio_ins


def collect_fio_results(workdir: str, bs: int):
    fio_res = []
    for fio_in in create_fio_inputs(bs, 1):
        fio_out = execute_fio_subprocess(workdir, fio_in)
        fio_res.append((fio_in, fio_out))
    return fio_res


def fio_to_dot(fio_in: FioInput, fio_out: FioOutput):
    ratio = fio_in.rwmixread
    iops = fio_out.rd_iops + fio_out.wr_iops
    return (ratio, iops)


def fio_to_dots(fio_res):
    dots = []
    for res in fio_res:
        (fio_in, fio_out) = res
        (ratio, iops) = fio_to_dot(fio_in, fio_out)
        dots.append((ratio, iops))
    return dots


def dots_max_iops(dots) -> int:
    iops_max = 0
    for dot in dots:
        (ratio, iops) = dot
        iops_max = max(iops_max, iops)
    factor = 1000
    return math.floor((iops_max + factor + 1) / factor) * factor


def plot_fio_results(bs, dots):
    mpl.style.use('seaborn')
    fig, ax = plt.subplots()
    fig.set_facecolor("seashell")
    title = 'Performace (BS={}K)'.format(bs / KILO)
    ax.set(xlabel='%RW', ylabel='IOPS', title=title)
    ax.set(xlim=(0, 100))
    ax.set(ylim=(0, dots_max_iops(dots)))
    ax.grid(True)
    for dot in dots:
        (ratio, iops)=dot
        ax.plot(ratio, iops, 'r.')
    plt.show()


def fio_to_plot(workdir, bs):
    fio_res=collect_fio_results(workdir, bs)
    plot_fio_results(bs, fio_to_dots(fio_res))


def main():
    if len(sys.argv) != 2:
        sys.exit(1)
    workdir=sys.argv[1]
    if not os.path.isdir(workdir):
        print("not a directory: " + workdir)
        sys.exit(2)
    real_workdir=os.path.realpath(workdir)
    bs=64 * KILO
    fio_to_plot(real_workdir, bs)


if __name__ == '__main__':
    main()
