#!/bin/bash

base=$(dirname ${BASH_SOURCE[0]})
root=$(readlink -f ${base}/../)
srcdir=${root}/src
config_h=${srcdir}/config.h

# Must have cppcheck
$(which cppcheck &> /dev/null) || (echo "no cppcheck" && exit 1)

# Assuming gcc on RedHat(Fedora)
gcc_versnum=$(gcc --version | grep gcc | awk '{print $3}')

# Run cppcheck, use only stderr-output
touch ${config_h}
cppcheck -I/usr/include -I/usr/include/linux/ -I/${srcdir} \
         -I/usr/lib/gcc/x86_64-redhat-linux/${gcc_versnum}/include/ \
         -I/usr/include/c++/${gcc_versnum} \
         --enable=all --platform=unix64 --relative-paths \
         --template='{file}:{line}: [{severity}:{id}] {message}' \
         --force \
         ${srcdir}
rm ${config_h}
