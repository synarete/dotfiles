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
#include <fnxconfig.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <unistd.h>
#include <sys/mount.h>

#include <fnxhash.h>
#include <fnxinfra.h>
#include <fnxcore.h>

#define expect_eq(blk1, blk2) \
	fnx_assert(memcmp(blk1, blk2, sizeof(*blk1)) == 0)

static fnx_alloc_t s_alloc;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const fnx_vinfo_t *vinfo_of(const fnx_vnode_t *vnode)
{
	const fnx_vinfo_t *vinfo;

	vinfo = fnx_get_vinfo(fnx_vnode_vtype(vnode));
	fnx_assert(vinfo != NULL);
	fnx_assert(vinfo->init != NULL);
	fnx_assert(vinfo->destroy != NULL);
	fnx_assert(vinfo->clone != NULL);
	fnx_assert(vinfo->dexport != NULL);
	fnx_assert(vinfo->dimport != NULL);
	fnx_assert(vinfo->dcheck != NULL);
	fnx_assert(vinfo->dsize <= FNX_BLKSIZE);
	return vinfo;
}

static fnx_blk_t *new_blk(void)
{
	fnx_blk_t *blk = (fnx_blk_t *)fnx_mmap_bks(1);
	fnx_assert(blk != NULL);
	fnx_bzero(blk, sizeof(*blk));

	return blk;
}

static void del_blk(fnx_blk_t *blk)
{
	fnx_bff(blk, sizeof(*blk));
	fnx_munmap_bks(blk, 1);
}

static fnx_vnode_t *new_vobj(fnx_vtype_e vtype)
{
	fnx_vnode_t *vnode;
	const fnx_vinfo_t *vinfo;

	vinfo = fnx_get_vinfo(vtype);
	fnx_assert(vinfo != NULL);
	fnx_assert(vinfo->init != NULL);
	vnode = fnx_alloc_new_vobj(&s_alloc, vtype);

	return vnode;
}

static void del_vobj(fnx_vnode_t *vnode)
{
	const fnx_vinfo_t *vinfo = vinfo_of(vnode);
	fnx_assert(vinfo->destroy != NULL);

	fnx_alloc_del_vobj(&s_alloc, vnode);
}

static void clone_vobj(const fnx_vnode_t *vnode, fnx_vnode_t *other)
{
	const fnx_vinfo_t *vinfo = vinfo_of(vnode);
	vinfo->clone(vnode, other);
}

static void dexport_vobj(const fnx_vnode_t *vnode, fnx_blk_t *blk)
{
	const fnx_vinfo_t *vinfo = vinfo_of(vnode);
	vinfo->dexport(vnode, (fnx_header_t *)blk);
}

static void dimport_vobj(fnx_vnode_t *vnode, fnx_blk_t *const blk)
{
	const fnx_vinfo_t *vinfo = vinfo_of(vnode);
	vinfo->dimport(vnode, (const fnx_header_t *)blk);
}

static void utest_converts(const fnx_vnode_t *vnode1)
{
	fnx_vtype_e vtype;
	fnx_blk_t *blk1, *blk2, *blk3;
	fnx_vnode_t *vnode2, *vnode3;

	/* Prepare memory */
	vtype   = fnx_vnode_vtype(vnode1);
	vnode2  = new_vobj(vtype);
	vnode3  = new_vobj(vtype);
	blk1    = new_blk();
	blk2    = new_blk();
	blk3    = new_blk();

	/* Do bidirectional conversions */
	dexport_vobj(vnode1, blk1);
	clone_vobj(vnode1, vnode2);
	dexport_vobj(vnode2, blk2);
	expect_eq(blk1, blk2);
	dimport_vobj(vnode3, blk2);
	dexport_vobj(vnode3, blk3);
	expect_eq(blk1, blk3);
	dimport_vobj(vnode3, blk3);
	clone_vobj(vnode3, vnode2);
	dexport_vobj(vnode2, blk2);
	expect_eq(blk2, blk3);

	/* Cleanups */
	del_blk(blk1);
	del_blk(blk2);
	del_blk(blk3);
	del_vobj(vnode2);
	del_vobj(vnode3);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const fnx_uctx_t *get_uctx(void)
{
	static fnx_uctx_t s_uctx;

	if (s_uctx.u_cred.cr_pid == 0) {
		fnx_uctx_init(&s_uctx);
		s_uctx.u_cred.cr_pid = getpid();
		s_uctx.u_cred.cr_uid = getuid();
		s_uctx.u_cred.cr_gid = getgid();
	}
	return &s_uctx;
}

static void fill_pbuf(char *buf, size_t bsz, size_t *p_len)
{
	size_t len;
	fnx_uuid_t uuid;

	fnx_bzero(buf, bsz);
	fnx_uuid_generate(&uuid);
	while ((len = strlen(buf)) < (bsz - 1)) {
		snprintf(buf + len, bsz - len, "%s", uuid.str);
	}
	*p_len = len;
}

static const fnx_name_t *get_name(void)
{
	static fnx_name_t s_name;

	fnx_name_init(&s_name);
	fill_pbuf(s_name.str, sizeof(s_name.str), &s_name.len);
	return &s_name;
}

static const fnx_path_t *get_path(size_t len)
{
	static fnx_path_t s_path;

	fnx_path_init(&s_path);
	fill_pbuf(s_path.str, len, &s_path.len);
	for (size_t i = 0; i < len; i = (2 * (i + 3)) + 1) {
		s_path.str[i] = '/';
	}
	return &s_path;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void utest_super(void)
{
	fnx_ino_t ino;
	fnx_vnode_t *vnode;
	fnx_super_t *super;
	fnx_fsstat_t *fsstat;

	vnode  = new_vobj(FNX_VTYPE_SUPER);
	super  = fnx_vnode_to_super(vnode);

	fsstat = &super->su_fsinfo.fs_stat;
	fnx_super_setup(super, "utest-super", "0.0.1");
	fnx_super_settimes(super, NULL, FNX_BAMCTIME_NOW);
	fnx_super_devise_fs(super, FNX_TERA);
	fnx_fsstat_next_ino(fsstat, &ino);
	fnx_fsstat_stamp_ino(fsstat, ino);
	fnx_fsstat_account(fsstat, FNX_VTYPE_DIR, 1);

	utest_converts(vnode);
	del_vobj(vnode);
}

static void utest_spmap(void)
{
	fnx_vaddr_t vaddr;
	fnx_baddr_t baddr;
	fnx_vnode_t *vnode;
	fnx_spmap_t *spmap;

	vnode = new_vobj(FNX_VTYPE_SPMAP);
	spmap = fnx_vnode_to_spmap(vnode);

	fnx_vaddr_for_spmap(&vaddr, 2017);
	fnx_baddr_setup(&baddr, 1, 2017 * FNX_BCKTNBK, 0);
	fnx_spmap_setup(spmap, &vaddr, &baddr, NULL);

	fnx_vaddr_for_dir(&vaddr, 100);
	fnx_spmap_insert(spmap, &vaddr, &baddr);
	fnx_vaddr_for_dir(&vaddr, 101);
	fnx_spmap_insert(spmap, &vaddr, &baddr);

	utest_converts(vnode);
	del_vobj(vnode);
}

static void utest_dir(void)
{
	fnx_ino_t ino = 19223;
	fnx_vnode_t *vnode;
	fnx_dir_t   *dir;

	vnode = new_vobj(FNX_VTYPE_DIR);
	dir   = fnx_vnode_to_dir(vnode);

	fnx_dir_setup(dir, get_uctx(), 0756);
	fnx_vaddr_for_dir(&dir->d_inode.i_vnode.v_vaddr, ino);
	fnx_inode_associate(&dir->d_inode, ino, get_name());

	utest_converts(vnode);
	del_vobj(vnode);
}

static void utest_dirseg(void)
{
	fnx_ino_t dino = 0x33A9FC;
	fnx_off_t dseg = 7661;
	fnx_vnode_t  *vnode;
	fnx_dirseg_t *dirseg;

	vnode  = new_vobj(FNX_VTYPE_DIRSEG);
	dirseg = fnx_vnode_to_dirseg(vnode);

	fnx_vaddr_for_dirseg(&dirseg->ds_vnode.v_vaddr, dino, dseg);
	fnx_dirseg_setup(dirseg, dseg);

	utest_converts(vnode);
	del_vobj(vnode);
}

static void utest_special(fnx_vtype_e vtype)
{
	fnx_ino_t ino;
	fnx_vnode_t *vnode;
	fnx_inode_t *inode;

	vnode = new_vobj(vtype);
	inode = fnx_vnode_to_inode(vnode);
	ino   = ULONG_MAX >> 11;

	fnx_vaddr_for_inode(&inode->i_vnode.v_vaddr, vtype, ino);
	fnx_inode_setup(inode, get_uctx(), 0123, 0);
	fnx_inode_setitime(inode, FNX_BAMCTIME_NOW);
	fnx_inode_associate(inode, ino, get_name());

	utest_converts(vnode);
	del_vobj(vnode);
}

static void utest_chrdev(void)
{
	utest_special(FNX_VTYPE_CHRDEV);
}

static void utest_blkdev(void)
{
	utest_special(FNX_VTYPE_BLKDEV);
}

static void utest_sock(void)
{
	utest_special(FNX_VTYPE_SOCK);
}

static void utest_fifo(void)
{
	utest_special(FNX_VTYPE_FIFO);
}

static void utest_reflnk(void)
{
	fnx_ino_t ino, refino;
	fnx_vnode_t *vnode;
	fnx_inode_t *inode;

	vnode = new_vobj(FNX_VTYPE_REFLNK);
	inode = fnx_vnode_to_inode(vnode);
	ino   = UINT_MAX;
	refino = ino + 1;

	fnx_vaddr_for_inode(&inode->i_vnode.v_vaddr, FNX_VTYPE_REFLNK, ino);
	fnx_inode_associate(inode, ino, get_name());
	inode->i_refino = refino;

	utest_converts(vnode);
	del_vobj(vnode);
}

static void utest_symlnk(fnx_vtype_e vtype, size_t len)
{
	fnx_vnode_t *vnode;
	fnx_symlnk_t *symlnk;

	vnode = new_vobj(vtype);
	symlnk = fnx_vnode_to_symlnk(vnode);

	fnx_symlnk_setup(symlnk, get_uctx(), get_path(len));

	utest_converts(vnode);
	del_vobj(vnode);
}

static void utest_symlnk1(void)
{
	utest_symlnk(FNX_VTYPE_SYMLNK1, FNX_SYMLNK1_MAX / 2);
	utest_symlnk(FNX_VTYPE_SYMLNK1, FNX_SYMLNK1_MAX);
}

static void utest_symlnk2(void)
{
	utest_symlnk(FNX_VTYPE_SYMLNK2, FNX_SYMLNK2_MAX / 2);
	utest_symlnk(FNX_VTYPE_SYMLNK2, FNX_SYMLNK2_MAX);
}

static void utest_symlnk3(void)
{
	utest_symlnk(FNX_VTYPE_SYMLNK3, FNX_SYMLNK3_MAX / 2);
	utest_symlnk(FNX_VTYPE_SYMLNK3, FNX_SYMLNK3_MAX);
}

static void utest_reg(void)
{
	fnx_ino_t ino = 1922371;
	fnx_vnode_t *vnode;
	fnx_reg_t   *reg;

	vnode = new_vobj(FNX_VTYPE_REG);
	reg   = fnx_vnode_to_reg(vnode);

	fnx_vaddr_for_reg(&reg->r_inode.i_vnode.v_vaddr, ino);
	fnx_inode_associate(&reg->r_inode, ino, get_name());
	fnx_reg_markseg(reg, FNX_RSEGSIZE - 1);
	fnx_reg_marksec(reg, FNX_RSECSIZE + 1);
	fnx_reg_wmore(reg, 77, 1, 0);

	utest_converts(vnode);
	del_vobj(vnode);
}

static void utest_regseg(void)
{
	fnx_ino_t ino = 111;
	fnx_off_t off = FNX_REGSIZE_MAX / 2;
	fnx_vnode_t  *vnode;
	fnx_regseg_t *regseg;

	vnode  = new_vobj(FNX_VTYPE_REGSEG);
	regseg = fnx_vnode_to_regseg(vnode);

	fnx_vaddr_for_regseg(&regseg->rs_vnode.v_vaddr, ino, off);
	fnx_regseg_setup(regseg, off);

	utest_converts(vnode);
	del_vobj(vnode);
}

static void utest_regsec(void)
{
	fnx_ino_t ino = 11111;
	fnx_off_t off = 5 * FNX_RSECSIZE;
	fnx_vnode_t  *vnode;
	fnx_regsec_t *regsec;

	vnode  = new_vobj(FNX_VTYPE_REGSEC);
	regsec = fnx_vnode_to_regsec(vnode);

	fnx_vaddr_for_regsec(&regsec->rc_vnode.v_vaddr, ino, off);
	fnx_regsec_markseg(regsec, off + 5);

	utest_converts(vnode);
	del_vobj(vnode);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void swap(void **arr, int pos1, int pos2)
{
	void *ptr = arr[pos1];
	arr[pos1] = arr[pos2];
	arr[pos2] = ptr;
}

static void random_shuffle(void **arr, size_t nelems)
{
	int *pos;
	size_t msz;

	msz = sizeof(*pos) * nelems;
	pos = (int *)fnx_xmalloc(msz, FNX_NOFAIL);
	blgn_prandom_tseq(pos, nelems, 0);
	for (size_t i = 0; i < nelems; ++i) {
		swap(arr, pos[i], pos[(nelems - i) - 1]);
	}
	fnx_free(pos, msz);
}

static void utest_alloc_vobjs(void)
{
	size_t i, memsz, nobjs = 10000;
	fnx_vnode_t *vnode;
	fnx_vnode_t **vnodes;
	fnx_vtype_e vtype = FNX_VTYPE_NONE;
	const fnx_vinfo_t *vinfo = NULL;
	fnx_alloc_t *alloc = &s_alloc;

	memsz  = nobjs * sizeof(*vnodes);
	vnodes = (fnx_vnode_t **)fnx_xmalloc(memsz, FNX_NOFAIL);
	for (i = 0; i < nobjs; ++i) {
		while (!vinfo || !vinfo->size) {
			vtype = (vtype + 1) % 256;
			vinfo = fnx_get_vinfo(vtype);
		}
		vnode = fnx_alloc_new_vobj(alloc, vtype);
		fnx_assert(vnode != NULL);
		vnodes[i] = vnode;
		vinfo = NULL;
	}
	random_shuffle((void **)vnodes, nobjs);
	for (i = 0; i < nobjs; ++i) {
		vnode = vnodes[i];
		fnx_alloc_del_vobj(alloc, vnode);
	}
	fnx_free(vnodes, memsz);
}


static void utest_alloc_bks(void)
{
	size_t i, memsz, nbks = 10000;
	fnx_baddr_t baddr;
	fnx_bkref_t *bkref;
	fnx_bkref_t **bkrefs;
	fnx_alloc_t *alloc = &s_alloc;

	memsz  = nbks * sizeof(*bkrefs);
	bkrefs = (fnx_bkref_t **)fnx_xmalloc(memsz, FNX_NOFAIL);
	for (i = 0; i < nbks; ++i) {
		fnx_baddr_setup2(&baddr, 1, i, 0);
		bkref = fnx_alloc_new_bk(alloc, &baddr);
		fnx_assert(bkref != NULL);
		bkrefs[i] = bkref;
	}
	random_shuffle((void **)bkrefs, nbks);
	for (i = 0; i < nbks; ++i) {
		bkref = bkrefs[i];
		fnx_alloc_del_bk(alloc, bkref);
	}
	fnx_free(bkrefs, memsz);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void utest_vobjs(void)
{
	utest_super();
	utest_spmap();
	utest_dir();
	utest_dirseg();
	utest_chrdev();
	utest_blkdev();
	utest_sock();
	utest_fifo();
	utest_reflnk();
	utest_symlnk1();
	utest_symlnk2();
	utest_symlnk3();
	utest_reg();
	utest_regseg();
	utest_regsec();
}

int main(void)
{
	int rc;
	fnx_alloc_t *alloc = &s_alloc;

	rc = fnx_verify_core();
	fnx_assert(rc == 0);

	fnx_verify_sinfos();
	fnx_alloc_init(alloc);
	utest_alloc_vobjs();
	utest_alloc_bks();
	utest_vobjs();
	fnx_alloc_clear(alloc);
	fnx_alloc_destroy(alloc);

	return EXIT_SUCCESS;
}
