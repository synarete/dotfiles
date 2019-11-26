#!/bin/bash
#
# Create docker image for Funex build-test on debian-based machine.
# Uses template Dockerfile.in file.
#
# Based on: https://github.com/dockerfile/ubuntu
#
base=$(dirname ${BASH_SOURCE[0]})
cd ${base}

# Die on first error
set -e

# Be more verbose upon failures
yell() { echo "$0: $*" >&2; }
die() { yell "$*"; exit 111; }
try() { "$@" || die "Failed to execute: $*"; }

# Have a docker
try docker --version

# Generate Dockerfile from template
dockerfile=${base}/Dockerfile
try sed -e "s,[@]NAME[@],`id -nu`,g" \
    -e "s,[@]UID[@],`id -u`,g" \
    -e "s,[@]GID[@],`id -g`,g" \
    -e "s,[@]GROUP[@],`id -ng`,g" \
    -e "s,[@]HOME[@],`echo $HOME`,g" \
    ${base}/Dockerfile.in > ${dockerfile}

# Run the docker
try docker build -t fubuntu .

# Report
try docker images

# Cleanups
try rm ${dockerfile}

# Hint the user what to do next:
echo
echo "Simple run: "
echo "  docker run --rm -i -t <image-id>  /bin/bash --login"
echo

# Run with mount-bind to home:
# docker run --rm -t -i -u ${UID} -v ${HOME}:${HOME} -w ${HOME} <image-id> /bin/bash --login


