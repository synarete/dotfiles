#!/bin/bash

# Torture file-system using git-clones, parallel-builds and rm -rf of various
# well-known tools & test-suits
set -e

# Prepare
workdir=$(pwd)
if [ ! -z "$1" ]; then
	workdir="$1"
fi
mkdir -p ${workdir}

# PostgreSQL -- http://www.postgresql.org
cd ${workdir}
git clone git://git.postgresql.org/git/postgresql.git
cd ${workdir}/postgresql && ./configure && make -j4 && make check
cd ${workdir}/postgresql && git clean -fxd
cd ${workdir}
rm -rf ${workdir}/postgresql

# Git SCM -- http://git-scm.com
cd ${workdir}
git clone https://github.com/git/git
cd ${workdir}/git && make test
cd ${workdir}/git && git clean -fxd
cd ${workdir}
rm -rf ${workdir}/git

# GCC
cd ${workdir}
git clone git://gcc.gnu.org/git/gcc.git
mkdir -p ${workdir}/gcc-build
cd  ${workdir}/gcc-build && ${workdir}/gcc/configure --disable-multilib && make && make check
cd ${workdir}
rm -rf ${workdir}/gcc-build
rm -rf ${workdir}/gcc

# Linux kernel
cd ${workdir}
git clone git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git
cd ${workdir}/linux && make defconfig && make -j8 && make modules
cd ${workdir}/linux && git clean -fxd
rm -rf ${workdir}/linux

# SQLite -- http://www.sqlite.org/
# http://repo.or.cz/w/sqlite.git
cd ${workdir}
git clone https://github.com/mackyle/sqlite
mkdir -p ${workdir}/sqlite-build
cd ${workdir}/sqlite
git log -1 --format=format:%ci%n | sed -e 's/ [-+].*$//;s/ /T/;s/^/D /' > manifest
echo $(git log -1 --format=format:%H) > manifest.uuid
cd ${workdir}/sqlite-build
../sqlite/configure
make
make sqlite3.c
make test
cd ${workdir}
rm -rf ${workdir}/sqlite
rm -rf ${workdir}/sqlite-build
