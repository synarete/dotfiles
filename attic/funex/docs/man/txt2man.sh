#!/bin/bash
set -e
self=$(basename $(readlink -f $0))
base=$(dirname $(readlink -f $0))/


genman() {
	echo "${self}: ${base}"
	cp ${base}/common.txt.in ${base}/common.txt
	man_in=$(find -L ${base} -type f -name '*.txt')
	for s in ${man_in}; do
		t=${s%%.txt}.man
		echo $(basename ${s}) "-->" $(basename ${t})
		rst2man ${s} > ${t}
	done
	rm ${base}/common.txt
}

clean() {
	find ${base} -type f -name '*.man' -exec rm -r {} \;
	find ${base} -type f -name '*~' -o -name '*.pyc' -exec rm -r {} \;
	find ${base} -type f -name '#*#' -exec rm -r {} \;
}

case "$1" in
	--clean)
		clean
		;;
	*)
		genman
		;;
esac
