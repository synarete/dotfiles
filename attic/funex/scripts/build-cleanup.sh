#!/bin/bash
# Cleanup auto-generated files/dirs by build-systems.

self=${BASH_SOURCE[0]}
scriptdir=$(dirname ${self})
basedir=$(realpath ${scriptdir}/../)

rm -f ${basedir}/.sconsign*
rm -f ${basedir}/configure
rm -f ${basedir}/include/config.h*
rm -rf ${basedir}/autom4te.cache
rm -rf ${basedir}/aclocal.m4
rm -rf ${basedir}/build/*
rm -rf ${basedir}/_build/*
rm -f ${basedir}/config.log
rm -f ${basedir}/config.status
rm -f ${basedir}/common.mk
rm -f ${basedir}/libtool
rm -f ${basedir}/include/stamp-h1
rm -f ${basedir}/funex.pc
rm -f ${basedir}/funex*.tar.gz

find ${basedir}/ -type f -name 'Makefile.in' -exec rm -r {} \;
find ${basedir}/ -type f -name 'Makefile' -exec rm -r {} \;
find ${basedir}/ -type d -name '\.deps' | xargs rm -rf
find ${basedir}/ -type f -name '\.dirstamp' -exec rm -f {} \;
find ${basedir}/ -type f -name '*~' -o -name '*.pyc' -exec rm -r {} \;
find ${basedir}/ -type f -name '#*#' -exec rm -r {} \;

dirty=$(git --git-dir=${basedir}/.git diff-index --name-only HEAD --)
[ -z "${dirty}" ] || echo ${self}":" "Dirty git-repo:" ${basedir}
