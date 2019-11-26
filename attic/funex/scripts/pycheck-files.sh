#!/bin/sh
root=$(readlink -f $(dirname $0)/../)

$(which pychecker > /dev/null 2>&1) || (echo "no pychecker" && exit 1)

pys=$@
if [ $pys = "" ]; then
	pys=$(find  ${root} -type f -name '*.py' | egrep -v '(git|build)')
fi

export PYTHONPATH=/usr/lib/scons/:${PYTHONPATH}
for f in $pys; do
	pychecker --limit 40 \
	          --var      \
	          --initattr \
	          --callattr \
	          --selfused \
	          --changetypes \
	          --exec \
	          --stdlib \
	          --maxlines 80 \
	          --maxbranches 10 \
	          --maxargs 8 \
	          --maxlocals 20 \
	          --blacklist SCons \
	          $f
done

