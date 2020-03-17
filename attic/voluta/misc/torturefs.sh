#!/bin/bash
#
# Torture file-system using various well-known tools. Using environment
# variable LOCAL_GIT_REPO_HOME as root directory for local git repositories,
# or else, got to the web.
#
self=$(basename ${BASH_SOURCE[0]})

TOKYOCABINET_GIT_URL="${LOCAL_GIT_REPO_HOME}/tokyocabinet"
RSYNC_GIT_URL="${LOCAL_GIT_REPO_HOME}/rsync"
POSTGRESQL_GIT_URL="${LOCAL_GIT_REPO_HOME}/postgresql"
SQLITE_GIT_URL="${LOCAL_GIT_REPO_HOME}/sqlite"
COREUTILS_GIT_GRL="${LOCAL_GIT_REPO_HOME}/coreutils"
GITSCM_GIT_URL="${LOCAL_GIT_REPO_HOME}/git"
SUBVERSION_GIT_URL="${LOCAL_GIT_REPO_HOME}/subversion"
DIFFUTILS_GIT_URL="${LOCAL_GIT_REPO_HOME}/diffutils"

if [[ -z "${LOCAL_GIT_REPO_HOME}" ]]; then
  TOKYOCABINET_GIT_URL="https://github.com/maiha/tokyocabinet"
  RSYNC_GIT_URL="git://git.samba.org/rsync.git"
  POSTGRESQL_GIT_URL="git://git.postgresql.org/git/postgresql.git"
  SQLITE_GIT_URL="https://github.com/mackyle/sqlite.git"
  COREUTILS_GIT_GRL="https://github.com/coreutils/coreutils"
  GITSCM_GIT_URL="https://github.com/git/git.git"
  SUBVERSION_GIT_URL="https://github.com/apache/subversion"
  DIFFUTILS_GIT_URL="git://git.savannah.gnu.org/diffutils"
fi


msg() { echo "$self: $*" >&2; }
die() { msg "$*"; exit 1; }
try() { echo "$self: $@" >&2; ( "$@" ) || msg "failed: $*"; }
run() { echo "$self: $@" >&2; ( "$@" ) || die "failed: $*"; }

git_clone() {
  url="$1"
  workdir="$2"

  run rm -rf ${workdir}
  run git clone ${url} ${workdir}
}

git_clean_fxd() {
  run git clean -fxd
}


# GNU coreutils
do_coreutils_check() {
  local currdir=$(pwd)
  local workdir="$1/coreutils"

  git_clone ${COREUTILS_GIT_GRL} ${workdir}

  cd ${workdir}
  run ./bootstrap
  run ./configure
  run make
  try make check
  git_clean_fxd

  cd ${currdir}
  run rm -rf ${workdir}
}

# TokyoCabinet
do_tokyocabinet_check() {
  local currdir=$(pwd)
  local workdir="$1/tokyocabinet"

  git_clone ${TOKYOCABINET_GIT_URL} ${workdir}

  cd ${workdir}
  run ./configure
  run make
  run make check
  git_clean_fxd

  cd ${currdir}
  run rm -rf ${workdir}
}

# Rsync
do_rsync_check() {
  local currdir=$(pwd)
  local workdir="$1/rsync"

  git_clone ${RSYNC_GIT_URL} ${workdir}

  cd ${workdir}
  run ./configure
  run make
  try make check
  git_clean_fxd

  cd ${currdir}
  run rm -rf ${workdir}
}

# Git-SCM
do_gitscm_check() {
  local currdir=$(pwd)
  local workdir="$1/git"

  git_clone ${GITSCM_GIT_URL} ${workdir}

  cd ${workdir}
  run make
  try make test
  git_clean_fxd

  cd ${currdir}
  run rm -rf ${workdir}
}

# PostgreSQL
do_postgresql_check() {
  local currdir=$(pwd)
  local workdir="$1/postgresql"

  git_clone ${POSTGRESQL_GIT_URL} ${workdir}

  cd ${workdir}
  run ./configure
  run make
  run make check
  git_clean_fxd

  cd ${currdir}
  run rm -rf ${workdir}
}

# Diffutils
do_diffutils_check() {
  local currdir=$(pwd)
  local workdir="$1/diffutils"

  git_clone ${DIFFUTILS_GIT_URL} ${workdir}

  cd ${workdir}
  run ./bootstrap
  run ./configure
  run make
  run make check
  git_clean_fxd

  cd ${currdir}
  run rm -rf ${workdir}
}

# Subversion
do_subversion_check() {
  local currdir=$(pwd)
  local workdir="$1/subversion"

  git_clone ${SUBVERSION_GIT_URL} ${workdir}

  cd ${workdir}
  run ./autogen.sh
  run ./configure
  run make
  run make check
  git_clean_fxd

  cd ${currdir}
  run rm -rf ${workdir}
}

# Sqlite
do_sqlite_check() {
  local currdir=$(pwd)
  local workdir="$1/sqlite"
  local builddir="$1/sqlite-build"

  git_clone ${SQLITE_GIT_URL} ${workdir}
  mkdir -p ${builddir}

  cd ${workdir}
  git log -1 --format=format:%ci%n \
    | sed -e 's/ [-+].*$//;s/ /T/;s/^/D /' > manifest
  echo $(git log -1 --format=format:%H) > manifest.uuid
  cd builddir
  run ../sqlite/configure
  run make
  run make sqlite3.c
  try make test
  git_clean_fxd

  cd ${currdir}
  run rm -rf ${workdir}
  run rm -rf ${builddir}
}

# All-in-one
do_all_checks() {
  local workdir="$1"

  do_tokyocabinet_check ${workdir}
  do_rsync_check ${workdir}
  do_postgresql_check ${workdir}
  do_coreutils_check ${workdir}
  do_diffutils_check ${workdir}
  do_sqlite_check ${workdir}
  do_gitscm_check ${workdir}
  do_subversion_check ${workdir}
}


show_usage() {
  echo ${self}": generate heavy load on file-system via other tools"
  echo
  echo "  -c|--coreutils        (${COREUTILS_GIT_GRL})"
  echo "  -d|--diffutils        (${DIFFUTILS_GIT_URL})"
  echo "  -t|--tokyocabinet     (${TOKYOCABINET_GIT_URL})"
  echo "  -r|--rsync            (${RSYNC_GIT_URL})"
  echo "  -p|--postgres         (${POSTGRESQL_GIT_URL})"
  echo "  -s|--subversion       (${SUBVERSION_GIT_URL})"
  echo "  -q|--sqlite           (${SQLITE_GIT_URL})"
  echo "  -g|--gitscm           (${GITSCM_GIT_URL})"
  echo "  -a|--all"
  echo
}

# Main
arg=${1:-"-a"}
wd=${2:-"$(pwd)"}
mkdir -p ${wd}
case "$arg" in
  -c|--coreutils)
    do_coreutils_check ${wd}
    ;;
  -d|--diffutils)
    do_diffutils_check ${wd}
    ;;
  -g|--gitscm)
    do_gitscm_check ${wd}
    ;;
  -t|--tokyocabinet)
    do_tokyocabinet_check ${wd}
    ;;
  -r|--rsync)
    do_rsync_check ${wd}
    ;;
  -p|--postgres)
    do_postgresql_check ${wd}
    ;;
  -s|--subversion)
    do_subversion_check ${wd}
    ;;
  -q|--sqlite)
    do_sqlite_check ${wd}
    ;;
  -a|--all)
    do_all_checks ${wd}
    ;;
  *)
    show_usage
    ;;
esac





