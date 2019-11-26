#!/bin/sh
self="$0"
prog=$(basename ${self})

# Be verbose upon failures
yell() { echo "${prog}: $*" >&2; }
die() { yell "$*"; exit 1; }
try() { "$@" || die "Failed to execute: $*"; }
run() { echo "${prog}: $@" && try $@; }

# Require executable application
$([ -x "$1" ]) || { die "not-executable:" "$1" ; }

# Run the valgrind
run valgrind -v --tool=memcheck --show-reachable=yes --leak-check=full "$@"
