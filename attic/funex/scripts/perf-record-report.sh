#!/bin/bash
self="${BASH_SOURCE[0]}"
base=$(dirname ${self})

# Require input prog & args
((!$#)) && echo "usage:" ${self} "<executable> [<args>]"

# Require perf
$(which perf &> /dev/null) || (echo ${self}: "no perf" && exit 1)

# Run prog with perf
perf record -g "$@"

# Run reporter
perf report -g

# NB: Use gprof2dot.py to visualize:
# https://github.com/jrfonseca/gprof2dot
