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
#ifndef FUNEX_HELPERS_H_
#define FUNEX_HELPERS_H_


#define funex_help_and_goodbye(opt) \
	funex_show_cmdhelp_and_goodbye((const funex_cmdent_t *)(opt->userp))

#define funex_set_debug_level(opt) \
	funex_globals_set_debug(funex_getoptarg(opt, "debug"))


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_vnode_t *funex_newvobj(fnx_vtype_e);

void funex_delvobj(fnx_vnode_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

fnx_bdev_t *funex_open_bdev(const char *, int);

fnx_bdev_t *funex_create_bdev(const char *, fnx_bkcnt_t);

void funex_close_bdev(fnx_bdev_t *);

fnx_dblk_t *funex_get_dblk(fnx_bdev_t *, fnx_lba_t);

void funex_put_dblk(fnx_bdev_t *, const fnx_dblk_t *, int);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_print_times(const fnx_times_t *, const char *);

void funex_print_fsattr(const fnx_fsattr_t *, const char *);

void funex_print_fsstat(const fnx_fsstat_t *, const char *);

void funex_print_iostat(const fnx_iostat_t *, const char *);

void funex_print_opstat(const fnx_opstat_t *, const char *);

void funex_print_super(const fnx_super_t *, const char *);

void funex_print_iattr(const fnx_iattr_t *, const char *);

void funex_print_inode(const fnx_inode_t *, const char *);

void funex_print_dir(const fnx_dir_t *dir, const char *prefix);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_show_pseudo(const char *, const char *,
                       const char *, const char *, int);

void funex_show_pseudofs(const char *);

#endif /* FUNEX_HELPERS_H_ */


