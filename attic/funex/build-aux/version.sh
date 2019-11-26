#!/bin/bash
set -euo pipefail
IFS=$'\n\t'
self=${BASH_SOURCE[0]}
base=$(dirname ${self})/../
print() { echo -n "$@" | tr -d ' \t\v\n' ; }


version=${FUNEX_VERSION:-1}
if [ -r ${base}/VERSION ]; then
    version=$(head -1 ${base}/VERSION)
fi

release=${FUNEX_RELEASE:-1}
if [ -r ${base}/RELEASE ]; then
    release=$(head -1 ${base}/RELEASE)
fi

revision=$(date +%Y%m%d)
if [ -d ${base}/.git ]; then
    revision=$(cd ${base} && git rev-parse --short=10 HEAD)
elif [ -r ${base}/REVISION ]; then
    revision=$(head -1 ${base}/REVISION)
fi

arg=${1:-}
case "$arg" in
    -h|--help)
	echo ${self} "[--version|--release|--revision]"
	;;
    -v|--version)
	print ${version}
	;;
    -r|--release)
	print ${release}
	;;
    -g|--revision)
	print ${revision}
	;;
    *)
	print ${version}-${release}.${revision}
	echo
  	;;
esac
