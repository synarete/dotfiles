#!/bin/bash -e
#
# usage: BS=<4|8|16|32|64..> RUNTIME=<30|60|90..> fio2csv <test-dir>
#

self=$(basename ${BASH_SOURCE[0]})
msg() { echo "$self: $*" >&2; }
die() { msg "$*"; exit 1; }
try() { ( "$@" ) || die "failed: $*"; }
run() { msg "$@" >&2; echo "# $*" && try "$@"; }

export LC_ALL=C
unset CDPATH


KILO=1024
MEGA=$((KILO * KILO))
GIGA=$((MEGA * KILO))
DATASIZE=${GIGA}
RUNTIME=${RUNTIME:-30}
BS=${BS:-4}

# TODO: echo 1 > /sys/block/<dev>/queue/iostats

_fio_minimal() {
  local testdir=$1
  local jobs=$2
  local bs=${BS}
  local bs_size=$((${bs} * 1024))
  local ioengine="psync"
  local size=$((DATASIZE / ${jobs}))
  local base=$(basename ${testdir})
  local name=${base}-bs${bs}-jobs${jobs}
  local filename=${testdir}/${name}

  run fio --name=${name} \
    --filename=${filename} \
    --numjobs=${jobs} \
    --bs=${bs_size} \
    --size=${size} \
    --fallocate=none \
    --rw=randwrite \
    --ioengine=psync \
    --sync=1 \
    --direct=1 \
    --time_based \
    --runtime=${RUNTIME} \
    --thinktime=0 \
    --norandommap \
    --group_reporting \
    --randrepeat=1 \
    --unlink=1 \
    --fsync_on_close=1 \
    --minimal \
    ;
}

_fio_all() {
  local testdir=$(realpath $1)
  local jobs=(1 2 4 8 12 16 24 32)

  for job in ${jobs[@]}; do
    _fio_minimal ${testdir} ${job}
  done
}

_fio2cvs() {
  try which fio > /dev/null

  for testdir in "$@"; do
    [[ -d ${testdir} ]] && _fio_all ${testdir}
  done
}

_fio2cvs "$@"



