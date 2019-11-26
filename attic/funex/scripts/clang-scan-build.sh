#!/bin/bash
#
# Wrapper over Clang's scan-build static-code analyzer (via autotools build).
#
self="${BASH_SOURCE[0]}"
prog=$(basename ${self})
prog="${prog%.*}"
base=$(dirname ${BASH_SOURCE[0]})
root=$(readlink -f ${base}/../)
bdir=${root}/build
html=${bdir}/html
cwd=${pwd}

# Be verbose upon failures
yell() { echo "$self: $*" >&2; }
die() { yell "$*"; exit 1; }
try() { "$@" || die "Failed to execute: $*"; }
require() { which "$@" > /dev/null 2>&1  || die "Missing: $*"; }

# Required tools
require clang
require scan-build

# Checkers lists
clang_sub_checkers() {
	echo $(try clang -cc1 -analyzer-checker-help | awk '{print $1}' | egrep -v osx | egrep '\.' | egrep '^'${1})
}
core_checkers=$(clang_sub_checkers core)
unix_checkers=$(clang_sub_checkers unix)
deadcode_checkers=$(clang_sub_checkers deadcode)
security_checkers=$(clang_sub_checkers security)
debug_checkers=$(clang_sub_checkers debug)
alpha_checkers=$(clang_sub_checkers alpha)

# Select checkers-set
checkers=${core_checkers}" "${unix_checkers}" "${deadcode_checkers}" "${security_checkers}
case "$1" in
	--debug)
		checkers=${checkers}" "${debug_checkers}" "
		;;
	--alpha)
		checkers=${checkers}" "${debug_checkers}" "${alpha_checkers}" "
		;;
	-h|--help)
		echo ${self} "[--debug|--alpha]"
		exit
esac

list_enable_checkers() {
	for c in ${checkers}; do
		echo -n -enable-checker ${c} ""
	done
}

# Run bootstrap
try ${root}/bootstrap

# Run clang's analyzer
echo "$prog":
for c in ${checkers}; do
	echo "  " ${c}
done
mkdir -p ${bdir}
cd ${bdir}
$(make clean &> /dev/null)
mkdir -p ${html}
export CCC_ANALYZER_CPLUSPLUS=1
export CCC_CC=clang
export CCC_CXX=clang++
scan-build --use-analyzer=$(which clang) ../configure
scan-build --use-analyzer=$(which clang)  -k -v -V -o ${html} $(list_enable_checkers) make all
cd ${cwd}
