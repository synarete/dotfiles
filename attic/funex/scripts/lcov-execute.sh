#!/bin/bash
#
# Wrapper over application execution (compile with 'scons COVERAGE=1') + lcov
#
# Usage: lcov-execute.sh <app> [args]
#

self="${BASH_SOURCE[0]}"
prog=$(basename ${self})
base=$(dirname ${BASH_SOURCE[0]})
root=$(readlink -f ${base}/../)
app="$1"
appv="$@"
outdir=${root}/_build/lcov
outfile=${root}/_build/lcov/app.info


# Be verbose upon failures
yell() { echo "${prog}: $*" >&2; }
die() { yell "$*"; exit 1; }
try() { "$@" || die "Failed to execute: $*"; }
run() { echo "${prog}: $@" && try $@; }


# Make sure application is executable and linked with gcov
$([ -x "${app}" ]) || { die "not-executable:" ${app}; }
gcov_syms=$(nm -g --defined-only ${app} | grep "T gcov_" | wc -l)
$([ "${gcov_syms}" -gt "1" ]) || { die "no-gcov-symbols:" ${app}; }

# Prepare
run rm -rf ${outdir}
run mkdir -p ${outdir}

# Reset counters
run lcov --directory ${root} --zerocounters

# Run application
run ${appv}

# Capturing the current coverage state to a file
run lcov --directory ${root} --capture --output-file ${outfile}

# Generate HTML output
run genhtml --frames --show-details --output-directory ${outdir} ${outfile}

# Report
echo "Generated HTML output in:" ${outdir}


