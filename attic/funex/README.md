The Funex Filesystem
====================

Funex is a FUSE based filesystem for GNU/Linux, designed to store large files
(images, videos, databases, vmdks etc.) over modern storage devices with
multiple I/O-workloads.

__NOTE__: Funex is a young project and its code-base is under heavy
development. The filesystem on-disk format is unstable and likely to change;
it should be considered experimental and __should not__ be used to store
critical data.


Build & Install
---------------

    $ git clone https://github.com/synarete/funex
    $ cd funex
    $ ./bootstrap
    $ cd build
    $ ../configure
    ...
    $ make
    ...
    $ sudo make install


    $ # On RH/Fedora:
    $ make rpm
    ...


Quick Howto (tmpfs)
-------------------
Funex is a _FUSE_ based file-system; make sure that the _fuse.ko_ module is
loaded, and that the _user\_allow\_other_ flag is enabled in /etc/fuse.conf

    $ sudo modinfo fuse
    filename:       /lib/modules/.../kernel/fs/fuse/fuse.ko
    ...
    $ cat /etc/fuse.conf
    # mount_max = 1000
    user_allow_other


Create working area in tmpfs:

    $ mkdir -p /tmp/funex/mnt /tmp/funex/dat


Format new filesystem volume (1G):

    $ funex mkfs --size=1G /tmp/funex/dat/volume
    ...
    $ stat -c %s /tmp/funex/dat/volume
    1073741824


Mount the newly create funex filesystem:

    $ funex mount /tmp/funex/dat/volume /tmp/funex/mnt
    $ funex heads
    /tmp/funex/mnt


Try it:

    $ ps -e | grep funex
    8310 ?        00:00:00 funex-fsd

    $ echo "hello, world" > /tmp/funex/mnt/hello
    $ cat /tmp/funex/mnt/hello
    hello, world


Cleanup: unmount the funex filesystem and remove its storage volume:

    $ funex umount /tmp/funex/mnt
    $ rm -rf /tmp/funex


__Note__: the actual process which serves the filesystem is called _funex-fsd_.
The _funex_ command is just a front-end utility.


License
-------
GPLv3.
