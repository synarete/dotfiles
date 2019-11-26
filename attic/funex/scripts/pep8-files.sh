#!/bin/sh
root=$(readlink -f $(dirname $0)/../)

$(which pep8 > /dev/null 2>&1) || (echo "no pep8" && exit 1)

pys=$@
if [[ $pys = "" ]]; then
	pys=$(find  ${root} -type f -name '*.py' | egrep -v '(git|build)')
fi

pep8 --ignore=E127 $pys


