#!/bin/bash -e
self=$(basename ${BASH_SOURCE[0]})
msg() { echo "$self: $*" >&2; }
die() { msg "$*"; exit 1; }
try() { ( "$@" ) || die "failed: $*"; }
run() { echo "$self: $@" >&2; try "$@"; }

export LC_ALL=C
unset CDPATH

name=voluta
selfdir=$(realpath $(dirname ${BASH_SOURCE[0]}))
basedir=$(realpath ${selfdir}/../../)
version_sh=${basedir}/version.sh
version=$(try ${version_sh} --version)
release=$(try ${version_sh} --release)
revision=$(try ${version_sh} --revision)
archive_tgz=${name}-${version}.tar.gz

builddir=${basedir}/build
rpmhomedir=${builddir}/rpm
autotoolsdir=${rpmhomedir}/autotools/

rpmsourcedir=${selfdir}/rpm
rpmbuilddir=${rpmhomedir}/rpmbuild
rpmvardir=${rpmbuilddir}/var
rpmtmpdir=${rpmvardir}/tmp
rpmdate=$(date +"%a %b %d %Y")
rpmspec_in=${rpmsourcedir}/${name}.spec.in
rpmspec_out=${rpmbuilddir}/SPECS/${name}.spec

# Prerequisites checks
run which aclocal
run which automake
run which libtoolize
run which rst2man
run which basename
run which rpmbuild

# Autotools build
run mkdir -p ${autotoolsdir}
cd ${autotoolsdir}
run ${basedir}/bootstrap
run ${basedir}/configure
run make
run make distcheck

# Pre rpmbuild
unset HOME
export HOME=${rpmhomedir}

# Prepare rpm tree
run mkdir -p ${rpmtmpdir}
run mkdir -p ${rpmbuilddir}
run mkdir -p ${rpmbuilddir}/RPMS
run mkdir -p ${rpmbuilddir}/SOURCES
run mkdir -p ${rpmbuilddir}/SPECS
run mkdir -p ${rpmbuilddir}/SRPMS
run mkdir -p ${rpmbuilddir}/BUILDROOT

# Generate spec
run sed \
  -e "s,[@]NAME[@],${name},g" \
  -e "s,[@]VERSION[@],${version},g" \
  -e "s,[@]RELEASE[@],${release},g" \
  -e "s,[@]REVISION[@],${revision},g" \
  -e "s,[@]RPMDATE[@],${rpmdate},g" \
  ${rpmspec_in} > ${rpmspec_out}

# Copy dist archive
run cp ${autotoolsdir}/${archive_tgz} ${rpmbuilddir}/SOURCES

# Execute rpmbuild
run cd ${rpmbuilddir}
run rpmbuild -bb --clean \
  --define "_topdir ${rpmbuilddir}" \
  --define "_var ${rpmvardir}" \
  ${rpmspec_out}

# Copy rpms to root of build-dir
run find ${rpmbuilddir}/RPMS/ ${rpmbuilddir}/SRPMS/ \
  -type f -name ${name}'*.rpm' \
  -exec cp {} ${builddir} \;

# Cleanup build staging area
cd ${basedir}
run rm -rf ${rpmhomedir}

# Show result rpm files
run find ${builddir} -type f -name ${name}'*.rpm' -exec basename {} \;

# Bye ;)
exit 0



