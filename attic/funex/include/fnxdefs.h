/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#ifndef FNXDEFS_H_
#define FNXDEFS_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Generic common defines */

/* Booleans */
#define FNX_TRUE                (1)
#define FNX_FALSE               (0)

/* Common power-of-2 sizes */
#define FNX_KILO                (1ULL << 10)
#define FNX_MEGA                (1ULL << 20)
#define FNX_GIGA                (1ULL << 30)
#define FNX_TERA                (1ULL << 40)
#define FNX_PETA                (1ULL << 50)
#define FNX_EXA                 (1ULL << 60)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Filsystem's base-constants */

/* Unique File-System type magic-number (see statfs(2) or kernel's magic.h) */
#define FNX_FSMAGIC             (0x16121973) /* Birthday */

/* File-system version number: */
#define FNX_FSVERSION           (1)


/* Frangmet-size: The size in bytes of the minimum allocation unit */
#define FNX_FRGSIZE             (512)

/* Block-size: I/O data unit size (bytes) */
#define FNX_BLKSIZE             (8192)

/* Number of fragments within block */
#define FNX_BLKNFRG             (16)


/* Bucket-size: Sub-volume allocation unit size (bytes) */
#define FNX_BCKTSIZE            (1UL << 20) /* 1M */

/* Number of blocks within volume-bucket unit */
#define FNX_BCKTNBK             (128)

/* Number of fragments within volume-bucket unit */
#define FNX_BCKTNFRG            (2048)

/* Number of v-object references in single bucket (max) */
#define FNX_BCKTNVREF           (163)


/* Regular-file sub-mapping segment size (bytes) */
#define FNX_RSEGSIZE            (1UL << 19) /* 512K */

/* Number of blocks within segment */
#define FNX_RSEGNBK             (64)

/* Number of fragments within segment */
#define FNX_RSEGNFRG            (1024)

/* Regular-file section size (bytes) */
#define FNX_RSECSIZE            (1UL << 30) /* 1G */

/* Number of segments within section */
#define FNX_RSECNSEG            (2048)

/* Number of blocks within section */
#define FNX_RSECNBK             (131072)

/* Number of virtual sections in regular file (including first) */
#define FNX_REGNSEC             (4096)

/* Number of virtual segments in regular file */
#define FNX_REGNSEG             (8388608UL)


/* Number of dir-entries in directory head node */
#define FNX_DIRHNDENT           (223)

/* Number of possible (potential) dir-segments */
#define FNX_DIRNSEGS            (32768)

/* Number of dir-entries in each directory segment */
#define FNX_DSEGNDENT           (29)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Limits: */

/* Maximum number of simultaneous Funex mounts */
#define FNX_MOUNT_MAX           (64)

/* Maximum file-length (not including null terminator) */
#define FNX_NAME_MAX            (255)

/* Maximum path length (including NUL) */
#define FNX_PATH_MAX            (4096)

/* Maximum hard-links to inode */
#define FNX_LINK_MAX            (32768)

/* Maximum number of simultaneous active users */
#define FNX_USERS_MAX           (64)

/* Maximum groups per user */
#define FNX_GROUPS_MAX          (32)

/* Maximum number of simultaneous open file-handles */
#define FNX_OPENF_MAX           (8192)

/* Volume's min/max size (bytes) */
#define FNX_VOLSIZE_MIN         (1UL << 21)
#define FNX_VOLSIZE_MAX         (1UL << 51) /* 2P */

/* Maximum number of entries in single directory */
/* NB: Can be 256K, but is it needed? */
#define FNX_DIRCHILD_MAX        (1UL << 17) /* 128K */

/* Regular-file's max size */
#define FNX_REGSIZE_MAX         (1UL << 42) /* 4T */

/* Maximum length for symbolic-link values (including NUL) */
#define FNX_SYMLNK1_MAX         (512)
#define FNX_SYMLNK2_MAX         (1536)
#define FNX_SYMLNK3_MAX         (4096) /* FNX_PATH_MAX */


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Special IDs: */

/* Non-valid offset value */
#define FNX_OFF_NULL            (-1)

/* Special UID/GID values */
#define FNX_UID_NULL            (-1)
#define FNX_UID_MAX             (1 << 24)
#define FNX_GID_NULL            (-1)

/* Special volume ids */
#define FNX_VOL_NULL            (0)
#define FNX_VOL_ROOT            (1)
#define FNX_VOL_DATA            (2)

/* Special LBA values */
#define FNX_LBA_NULL            (0)
#define FNX_LBA_SUPER           (1)
#define FNX_LBA_APEX0           (1ULL << 63)
#define FNX_LBA_VNULL           (FNX_LBA_NULL | FNX_LBA_APEX0)
#define FNX_LBA_MAX             (FNX_LBA_APEX0 - 1ULL)
#define FNX_LBA_VMAX            (FNX_LBA_MAX | FNX_LBA_APEX0)

/* Special ino numbers */
#define FNX_INO_NULL            (0)
#define FNX_INO_ROOT            (1)
#define FNX_INO_APEX0           (0x1000)
#define FNX_INO_ANY             (~(0ULL))
#define FNX_INO_PMASK           (1ULL << 56)
#define FNX_INO_MAX             (FNX_INO_PMASK - 1)
#define FNX_XNO_NULL            (FNX_INO_NULL)
#define FNX_XNO_ANY             (FNX_INO_ANY)

/* Directory special offset values */
#define FNX_DOFF_NONE           (FNX_OFF_NULL)
#define FNX_DOFF_SELF           (0)
#define FNX_DOFF_PARENT         (1)
#define FNX_DOFF_BEGIN          (2)
#define FNX_DOFF_BEGINS         (FNX_DIRHNDENT)
#define FNX_DOFF_PROOT          (FNX_DOFF_END + 1)
#define FNX_DOFF_END            (FNX_DOFF_BEGINS + \
                                 (FNX_DIRNSEGS * FNX_DSEGNDENT))

/* Regular-file special offset values */
#define FNX_ROFF_NONE           (FNX_OFF_NULL)
#define FNX_ROFF_BEGIN          (0)
#define FNX_ROFF_END            (FNX_REGSIZE_MAX)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Mount options */
#define FNX_MNTF_RDONLY         (1 << 0)
#define FNX_MNTF_NOSUID         (1 << 1)
#define FNX_MNTF_NODEV          (1 << 2)
#define FNX_MNTF_NOEXEC         (1 << 3)
#define FNX_MNTF_MANDLOCK       (1 << 6)
#define FNX_MNTF_DIRSYNC        (1 << 7)
#define FNX_MNTF_NOATIME        (1 << 10)
#define FNX_MNTF_NODIRATIME     (1 << 11)
#define FNX_MNTF_RELATIME       (1 << 21)

#define FNX_MNTF_STRICT         (1ULL << 33)

#define FNX_MNTF_DEFAULT  \
	(FNX_MNTF_NOATIME | FNX_MNTF_NODIRATIME)


/* Capability flags */
#define FNX_CAPF_CHOWN          (1 << 0)
#define FNX_CAPF_FOWNER         (1 << 3)
#define FNX_CAPF_FSETID         (1 << 4)
#define FNX_CAPF_ADMIN          (1 << 21)
#define FNX_CAPF_MKNOD          (1 << 27)

#define FNX_CAPF_ALL \
	(FNX_CAPF_CHOWN | FNX_CAPF_FOWNER | \
	 FNX_CAPF_FSETID | FNX_CAPF_ADMIN | FNX_CAPF_MKNOD)

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Setattr control flags (0-8 same as in FUSE) */
#define FNX_SETATTR_MODE        (1 << 0)
#define FNX_SETATTR_UID         (1 << 1)
#define FNX_SETATTR_GID         (1 << 2)
#define FNX_SETATTR_SIZE        (1 << 3)
#define FNX_SETATTR_ATIME       (1 << 4)
#define FNX_SETATTR_MTIME       (1 << 5)
#define FNX_SETATTR_ATIME_NOW   (1 << 7)
#define FNX_SETATTR_MTIME_NOW   (1 << 8)
#define FNX_SETATTR_BTIME       (1 << 9)
#define FNX_SETATTR_CTIME       (1 << 10)
#define FNX_SETATTR_BTIME_NOW   (1 << 11)
#define FNX_SETATTR_CTIME_NOW   (1 << 12)


/* Time specific control flags */
#define FNX_BTIME               FNX_SETATTR_BTIME
#define FNX_BTIME_NOW           FNX_SETATTR_BTIME_NOW
#define FNX_ATIME               FNX_SETATTR_ATIME
#define FNX_ATIME_NOW           FNX_SETATTR_ATIME_NOW
#define FNX_MTIME               FNX_SETATTR_MTIME
#define FNX_MTIME_NOW           FNX_SETATTR_MTIME_NOW
#define FNX_CTIME               FNX_SETATTR_CTIME
#define FNX_CTIME_NOW           FNX_SETATTR_CTIME_NOW

#define FNX_AMTIME              (FNX_ATIME | FNX_MTIME)
#define FNX_AMCTIME             (FNX_ATIME | FNX_MTIME | FNX_CTIME)
#define FNX_BAMCTIME            (FNX_BTIME | FNX_ATIME | FNX_MTIME | FNX_CTIME)
#define FNX_AMTIME_NOW          (FNX_ATIME_NOW | FNX_MTIME_NOW)
#define FNX_ACTIME_NOW          (FNX_ATIME_NOW | FNX_CTIME_NOW)
#define FNX_MCTIME_NOW          (FNX_MTIME_NOW | FNX_CTIME_NOW)
#define FNX_AMCTIME_NOW         (FNX_ATIME_NOW | FNX_MCTIME_NOW)
#define FNX_BAMCTIME_NOW        (FNX_BTIME_NOW | FNX_AMCTIME_NOW)
#define FNX_SETATTR_AMTIME_ANY  (FNX_AMTIME | FNX_AMTIME_NOW)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * On-disk magic numbers
 */
#define FNX_MAGIC1              (161803398)     /* Golden ratio */
#define FNX_MAGIC2              (271828182)     /* Euler's number e */
#define FNX_MAGIC3              (787499699)     /* Chaitin's Constant */
#define FNX_MAGIC4              (141421356)     /* sqrt(2) */
#define FNX_MAGIC5              (179424691)     /* 10000001st prime */
#define FNX_MAGIC6              (314159265)     /* Pi */
#define FNX_MAGIC7              (466920160)     /* Feigenbaum C1 */
#define FNX_MAGIC8              (0x15320B74)    /* Plastic number */
#define FNX_MAGIC9              (0x93C467E3)    /* Euler-Mascheroni */

#define FNX_SUPER_MAGIC         FNX_MAGIC1
#define FNX_SPMAP_MAGIC         FNX_MAGIC2
#define FNX_VNODE_MAGIC         FNX_MAGIC3
#define FNX_INODE_MAGIC         FNX_MAGIC4
#define FNX_DIR_MAGIC           FNX_MAGIC5
#define FNX_DIRSEG_MAGIC        FNX_MAGIC6


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Names:
 */
/* Mount info */
#define FNX_FSNAME              "funex"

/* Fs-type string in /proc/self/mountinfo */
#define FNX_MNTFSTYPE           "fuse.funex"

/* Mounting-daemon IPC socket */
#define FNX_MNTSOCK             "funexmnt.sock"


/*
 * Pseudo-namespace names:
 */
/* Pseudo filesystem's root-dir name */
#define FNX_PSROOTNAME          ".funex"

/* Pseudo filename for ioctls */
#define FNX_PSIOCTLNAME         "ioctl"

/* Pseudo filename to control fs-halt */
#define FNX_PSHALTNAME          "halt"

/* Pseudo dirname for super-block repr */
#define FNX_PSSUPERDNAME        "super"

/* Pseudo filename to query meta-data stats */
#define FNX_PSFSSTATNAME        "fsstat"

/* Pseudo filename to query operations stats */
#define FNX_PSOPSTATSNAME       "opstat"

/* Pseudo filename to query io-counters */
#define FNX_PSIOSTATSNAME       "iostat"

/* Pseudo-fs cache entries */
#define FNX_PSCACHEDNAME        "cache"
#define FNX_PSCSTATSNAME        "stats"

/* Pseudo-fs fuse-interface entries */
#define FNX_PSFUSEIDNAME        "fusei"
#define FNX_PSATTRTIMEOUTNAME   "attr_timeout"
#define FNX_PSENTRYTIMEOUTNAME  "entry_timeout"


#endif /* FNXDEFS_H_ */


