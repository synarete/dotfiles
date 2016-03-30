#!/bin/bash
#
# Poor man's way of creating bootable ubuntu image and running within light
# weight container using systemd-nspawn:
#
#  1. Run this script; will use debootstrap to install ubuntu system.
#  2. Copy result image (a.k.a. /tmp/ubuntu-wily-amd64.img) to your dir.
#  3. Do:
#       $ systemctl start systemd-networkd
#       $ systemctl start systemd-resolved
#  4. Login into container and change root password:
#       $ systemd-nspawn -i /root/ubuntu-wily-amd64.img
#  5. Within container, may need to update /etc/resolv.conf with host's params
#  6. Boot into container:
#       $ systemd-nspawn -i ubuntu-wily-amd64.img -b --machine=ubuntu-wily
#

set -euo pipefail
IFS=$'\n\t'
export LC_ALL=C
self=${BASH_SOURCE[0]}
base=$(dirname ${self})
prog=$(basename ${self})

# Be verbose upon failures
yell() { echo "${prog}: $*" >&2; }
die() { yell "$*"; exit 1; }
try() { "$@" || die "Failed to execute: $*"; }
run() { echo "${prog}: $@" && try $@; }

# Where stuff goes
img=/tmp/ubuntu-wily-amd64.img
dev=/dev/loop7
part=${dev}p1
mnt=/mnt/test/

# Actions
run mkdir -p ${mnt}
run truncate -s 32G ${img}
run parted ${img} mklabel gpt
run parted ${img} mkpart primary xfs 2048s 100%
run parted ${img} align-check optimal 1
run losetup --partscan ${dev} ${img}
run mkfs -t ext4 ${part}
run mount ${part} ${mnt}
run debootstrap --arch=amd64 wily ${mnt} http://archive.ubuntu.com/ubuntu/
run sync
run umount ${mnt}
run losetup -d ${dev}
run rmdir ${mnt}
echo ${img}












