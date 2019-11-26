#!/bin/bash
#
# Run fio file-based benchmarks + verifications. Mimics StorageReview's
# 70/30 test. See:
# http://www.storagereview.com/fio_flexible_i_o_tester_synthetic_benchmark
#
self="$0"
base=$(dirname ${self})
workdir=$([ -d "$1" ] && echo "$1" || echo $(pwd))
testname=fiotest-8k7030-$(date +%Y%m%dT%H%M)
filepath=${workdir}/${testname}

# Be verbose upon failures
yell() { echo "$self: $*" >&2; }
die() { yell "$*"; exit 1; }
try() { "$@" || die "Failed to execute: $*"; }

# Require fio + prepare and cleanup upon exit
try fio --version
try mkdir -p ${workdir}
cleanups () {
	rm -f ${filepath}
	rm -f ${workdir}/local-${testname}*-verify.state
}
trap cleanups EXIT

# Actual fio run
echo ${filepath}
try fio \
	--filename=${filepath} \
	--size=1G \
	--direct=0 \
	--rw=randrw \
	--refill_buffers \
	--norandommap \
	--randrepeat=0 \
	--ioengine=libaio \
	--bs=8k \
	--rwmixread=70  \
	--iodepth=16 \
	--numjobs=16 \
	--runtime=60 \
	--group_reporting \
	--verify=crc32c \
	--name=${testname}

