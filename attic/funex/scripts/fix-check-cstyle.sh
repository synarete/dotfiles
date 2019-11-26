#!/bin/bash

self=$(readlink -f $0)
here=$(dirname ${self})
base=$(readlink -f ${here}/../)

find_srcs () {
	for d in "$@"; do
		find ${d} -type f -name '*.[hc]' -or -name '*.cpp' | \
			egrep -v '(git|build|fuse_kernel|config\.h)'
	done
}

srcs="$@"
if [[ ${srcs} = "" ]]; then
	srcs=$(find_srcs ${base}/{apps,include,libfnx,unitests})
fi

${here}/astyle-files.sh ${srcs}
${here}/checkcstyle.py ${srcs}


