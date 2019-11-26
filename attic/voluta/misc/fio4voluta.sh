#!/bin/bash
self=$(basename ${BASH_SOURCE[0]})
msg() { echo "$self: $*" >&2; }
die() { msg "$*"; exit 1; }
try() { ( "$@" ) || die "failed: $*"; }
run() { echo "$@" >&2; try "$@"; }

workdir=$(realpath ${1:-$(pwd)})
filename=${workdir}/fio_rw.$$
njobs=1
bksize=256k
rwmode=randrw
size=1G
name="${rwmode}${bksize}${size}"

try which fio > /dev/null
try ls "${workdir}" > /dev/null

run fio \
  --filename=${filename} \
  --readwrite=${rwmode} \
  --rwmixread=70 \
  --gtod_reduce=1 \
  --refill_buffers \
  --norandommap \
  --randrepeat=1 \
  --ioengine=pvsync \
  --bs=${bksize} \
  --iodepth=16 \
  --numjobs=${njobs} \
  --runtime=60 \
  --loops=4 \
  --group_reporting \
  --size=${size} \
  --name=${name}

try unlink ${filename}
