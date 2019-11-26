#!/bin/bash
root=$(readlink -f $(dirname ${BASH_SOURCE[0]})/../)

$(which python3-pylint > /dev/null 2>&1) || (echo "no python3-pylint" && exit 1)

pys=$@
if [[ $pys == "" ]]; then
	pys=$(find  ${root} -type f -name '*.py' | egrep -v '(git|build)')
fi

python3-pylint \
	--output-format=colorized \
	--reports=no \
	--max-line-length=80 \
	--max-args=8 \
	--max-locals=13 \
	--max-returns=4 \
	--max-parents=4 \
	--max-attributes=12 \
	--min-public-methods=0  \
	--max-public-methods=40 \
	--disable=C0103 \
	--disable=C0111 \
	--disable=W0613 \
        $pys
