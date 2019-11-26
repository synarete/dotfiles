#!/usr/bin/env python3

import sys
import os
import subprocess
import shlex
from typing import List
from typing import Tuple

KILO = 1024
MEGA = KILO * KILO
GIGA = KILO * KILO * KILO
SIZE = 2 * GIGA
BLKSIZE = 64 * KILO
RUNTIME = 60


class FioInput:

    def __init__(self, workdir: str, numjobs: int, size: int) -> None:
        self.base = ''
        self.name = 'fio-'
        self.tag = ''
        self.workdir = os.path.realpath(workdir)
        self.bs = BLKSIZE
        self.numjobs = numjobs
        self.rw = 'readwrite'
        self.size = size
        self.ioengine = 'psync'
        self.runtime = RUNTIME
        self.cmd = 'fio '


class FioOutput:

    def __init__(self, tag, jobs, output) -> None:
        fileds = ['0'] + output.split(';')
        self.tag = str(tag)
        self.jobs = int(jobs)
        self.rd_iops = fileds[8]
        self.rd_bw = fileds[7]
        self.rd_lat = fileds[40]
        self.wr_iops = fileds[49]
        self.wr_bw = fileds[48]
        self.wr_lat = fileds[81]


def update_fio_input(fio_in: FioInput) -> None:
    fio_in.base = os.path.basename(fio_in.workdir)
    fio_in.name = 'fio-{}-{}-{}'.format(fio_in.rw,
                                        fio_in.ioengine, fio_in.base)
    fio_in.tag = '{}_{}'.format(fio_in.base, fio_in.rw)


def assign_fio_command(fio_in: FioInput) -> None:
    cmd = 'fio '
    cmd = cmd + '--fsync_on_close=1 '
    cmd = cmd + '--thinktime=0 '
    cmd = cmd + '--norandommap --fallocate=none '
    cmd = cmd + '--group_reporting --minimal '
    cmd = cmd + '--numjobs={} '.format(fio_in.numjobs)
    cmd = cmd + '--name={} '.format(fio_in.name)
    cmd = cmd + '--directory={} '.format(fio_in.workdir)
    cmd = cmd + '--bs={} '.format(fio_in.bs)
    cmd = cmd + '--size={} '.format(fio_in.size)
    cmd = cmd + '--rw={} '.format(fio_in.rw)
    cmd = cmd + '--ioengine={} '.format(fio_in.ioengine)
    cmd = cmd + '--runtime={} '.format(fio_in.runtime)
    fio_in.cmd = cmd


def create_all_fio_inputs(workdir: str) -> List[FioInput]:
    fio_ins = []
    nproc = min(int(os.cpu_count()), 4)
    jobs = [1 + cpu for cpu in range(0, nproc)]
    for job in jobs:
        fio_in = FioInput(workdir, job, int(SIZE / int(job)))
        update_fio_input(fio_in)
        assign_fio_command(fio_in)
        fio_ins.append(fio_in)
    return fio_ins


def execute_fio_subprocess(fio_in: FioInput) -> FioOutput:
    pipe = subprocess.PIPE
    env = os.environ.copy()
    cmd = fio_in.cmd
    pipes = subprocess.Popen(shlex.split(cmd), stdout=pipe, stderr=pipe,
                             cwd=fio_in.workdir, shell=False, env=env)
    std_out, std_err = pipes.communicate()
    return FioOutput(fio_in.tag, fio_in.numjobs, str(std_out.decode('UTF-8')))


def fios_to_csv(workdir: str) -> str:
    fmt = ' {:<9} , {:9} , {:9} , {:12} , {:9} , {:9} , {:12} \n'
    csv = fmt.format('threads',
                     'wr_iops', 'wr_bw', 'wr_lat',
                     'rd_iops', 'rd_bw', 'rd_lat')
    fio_ins = create_all_fio_inputs(workdir)
    for fio_in in fio_ins:
        fio_out = execute_fio_subprocess(fio_in)
        line = fmt.format(fio_out.jobs,
                          fio_out.wr_iops, fio_out.wr_bw, fio_out.wr_lat,
                          fio_out.rd_iops, fio_out.rd_bw, fio_out.rd_lat)
        csv = csv + line
    return csv


def main():
    for workdir in sys.argv[1:]:
        real_workdir = os.path.realpath(workdir)
        if os.path.isdir(real_workdir):
            print(real_workdir)
            csv_data = fios_to_csv(real_workdir)
            print(csv_data)


if __name__ == '__main__':
    main()
