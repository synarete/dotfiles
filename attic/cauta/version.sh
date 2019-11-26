#!/bin/bash
set -euo pipefail
IFS=$'\n\t'
self=$(readlink -f $0)
base=$(dirname ${self})
datenow=$(date +%Y%m%d)

print() { echo -n "$@" | tr -d ' \t\v\n' ; }

cd ${base}
gittop=$(git rev-parse --show-toplevel 2> /dev/null || echo -n "x")

if [ -n "${gittop}" ]; then
  gitrevision=$(git rev-parse --short=10 HEAD)
else
  gitrevision=""
fi

version=${CAUTA_VERSION:-1}
if [ -e ${base}/VERSION ]; then
  version=$(head -1 ${base}/VERSION)
fi

release=${CAUTA_RELEASE:-${datenow}}
if [ -e ${base}/RELEASE ]; then
  release=$(head -1 ${base}/RELEASE)
fi

revision=${CAUTA_REVISION:-1}
if [ -n "${gitrevision}" ]; then
  revision=${gitrevision}
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


