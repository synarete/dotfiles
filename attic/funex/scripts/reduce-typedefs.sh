#!/bin/bash
self="${BASH_SOURCE[0]}"
base=$(realpath $(dirname ${self})/../)

# Die on first error
set -e

files=$(find ${base}/src/ -type f -name '*.[ch]')

types=$(echo ${files} | xargs egrep 'typedef struct fnx_' |
        sed 's/fnx_packed//g' | awk '{print $3}' | grep fnx_ | sort -u)
for f in ${files}; do
	for t in ${types}; do
		patt="typedef struct ${t} "
		sed -i "/${patt}/d" "$f"
		curr="${t}_t "
		next="struct ${t} "
		sed -i "s/${curr}/${next}/g" "$f"
	done
done

types=$(echo ${files} | xargs egrep 'typedef union fnx_' |
        sed 's/fnx_packed//g' | awk '{print $3}' | grep fnx_ | sort -u)
for f in ${files}; do
	for t in ${types}; do
		patt="typedef union ${t} "
		sed -i "/${patt}/d" "$f"
		curr="${t}_t "
		next="union ${t} "
		sed -i "s/${curr}/${next}/g" "$f"
	done
done

