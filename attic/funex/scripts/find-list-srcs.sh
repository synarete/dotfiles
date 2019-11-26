#!/bin/bash
# Helper script to list source files into Makefile.am

for d in "$@"; do
    find ${d} -maxdepth 1 -type f -name '*.[ch]' | \
        sed 's/${d}//g' | \
        sed 's/\.\///g' | \
        awk '{print $1" \\"}' | \
        sort -u
done
