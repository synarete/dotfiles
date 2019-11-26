#!/bin/bash
base=$(dirname ${BASH_SOURCE[0]})

case "$1" in
"--gpl")
	notice=${base}/gpl-notice
	shift
	;;
"--lgpl")
	notice=${base}/lgpl-notice
	shift
	;;
*)
	exit 1
  	;;
esac

for f in "$@"; do
	{ tail -n 23 ${notice}; tail -n +23 ${f}; } | sponge ${f}
done
