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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

#include <fnxserv.h>
#include "parse.h"


static int isequal(const char *s1, const char *s2)
{
	return (s1 && s2 && (strcasecmp(s1, s2) == 0));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

int funex_parse_bool(const char *str, int *p_bool)
{
	int rc = 0;
	if (isequal(str, "1") || isequal(str, "true")) {
		*p_bool = FNX_TRUE;
	} else if (isequal(str, "0") || isequal(str, "false")) {
		*p_bool = FNX_FALSE;
	} else {
		rc = -1;
	}
	return rc;
}

int funex_parse_loff(const char *str, loff_t *loff)
{
	char *endptr = NULL;
	long double val, mul, iz;

	mul = 1.0F;
	errno = 0;
	val = strtold(str, &endptr);
	if ((endptr == str) || (errno == ERANGE)) {
		return -1;
	}
	if (strlen(endptr) > 1) {
		return -1;
	}
	switch (toupper(*endptr)) {
		case 'K':
			mul = (double)((long)FNX_KILO);
			break;
		case 'M':
			mul = (double)((long)FNX_MEGA);
			break;
		case 'G':
			mul = (double)((long)FNX_GIGA);
			break;
		case 'T':
			mul = (double)((long)FNX_TERA);
			break;
		case 'P':
			mul = (double)((long)FNX_PETA);
			break;
		case '\0':
			break;
		default:
			return -1;
			break;
	}
	val *= mul;
	modfl(val, &iz);
	*loff = (loff_t)iz;
	return 0;
}

int funex_parse_int(const char *str, int *p_num)
{
	int rc;
	long num;

	rc = funex_parse_long(str, &num);
	if (rc != 0) {
		return rc;
	}
	if ((num > INT_MAX) || (num < INT_MIN)) {
		return -1;
	}
	*p_num = (int)num;
	return 0;
}

int funex_parse_long(const char *str, long *p_num)
{
	char *endptr = NULL;
	long val;

	errno = 0;
	val = strtol(str, &endptr, 0);
	if ((endptr == str) || (errno == ERANGE)) {
		return -1;
	}
	if (strlen(endptr) > 1) {
		return -1;
	}

	*p_num  = val;
	return 0;
}

int funex_parse_uid(const char *str, fnx_uid_t *p_uid)
{
	int rc;
	long val = -1;
	fnx_uid_t uid_max;

	rc = funex_parse_long(str, &val);
	if (rc != 0) {
		return -1;
	}

	uid_max = (uid_t)(-1);
	if ((val < 0) || (val > (long)uid_max)) {
		return -1;
	}

	*p_uid = (uid_t)val;
	return 0;
}

int funex_parse_gid(const char *str, fnx_gid_t *p_gid)
{
	int rc;
	long val = -1;
	fnx_gid_t gid_max;

	rc = funex_parse_long(str, &val);
	if (rc != 0) {
		return -1;
	}

	gid_max = (gid_t)(-1);
	if ((val < 0) || (val > (long)gid_max)) {
		return -1;
	}

	*p_gid = (gid_t)val;
	return 0;
}

int funex_parse_uuid(const char *str, fnx_uuid_t *p_uuid)
{
	return fnx_uuid_parse(p_uuid, str);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Config-file line-parsing:
 */

static void chop_semicolon(fnx_substr_t *s)
{
	char const *sc = ";";

	fnx_substr_chop_any_of(s, sc, s);
}

static void strip_whitespaces(fnx_substr_t *s)
{
	fnx_substr_strip_ws(s, s);
}

static void init_strref(fnx_substr_t *s, const char *str)
{
	fnx_substr_init(s, str ? str : "");
	strip_whitespaces(s);
}

static void clone_strref(fnx_substr_t *s, const fnx_substr_t *other)
{
	fnx_substr_clone(other, s);
	strip_whitespaces(s);
}

static int is_comment_line(const fnx_substr_t *s)
{
	return fnx_substr_starts_with(s, '#');
}

static int isalnum_or_ispunct(char c)
{
	return (fnx_chr_isalnum(c) || fnx_chr_ispunct(c));
}

static int is_valid_token(const fnx_substr_t *s)
{
	return fnx_substr_test_if(s, isalnum_or_ispunct);
}

static void
split_key_value(const fnx_substr_t *s, fnx_substr_t *key, fnx_substr_t *val)
{
	char const *ws;
	fnx_substr_pair_t split_pair;

	ws = fnx_ascii_whitespaces();
	fnx_substr_split(s, "=", &split_pair);
	fnx_substr_strip_any_of(&split_pair.first, ws, key);
	fnx_substr_strip_any_of(&split_pair.second, ws, val);
	chop_semicolon(val);
	strip_whitespaces(val);
}

static int
parse_key_value(const fnx_substr_t *line, fnx_substr_t *key, fnx_substr_t *val)
{
	fnx_substr_t ss;
	fnx_substr_t *s = &ss;

	clone_strref(s, line);
	strip_whitespaces(s);
	split_key_value(s, key, val);
	if (!is_valid_token(key)) {
		return -1;
	}
	if (!is_valid_token(val)) {
		return -1;
	}
	return 0;
}

int funex_parse_kv(const char *line, char *key, size_t key_len,
                   char *val, size_t val_len)
{
	int rc = 0;
	fnx_substr_t sstr, skey, sval;

	init_strref(&sstr, line);
	init_strref(&skey, NULL);
	init_strref(&sval, NULL);

	if (is_comment_line(&sstr)) {
		return 0;
	}
	rc = parse_key_value(&sstr, &skey, &sval);
	if (rc != 0) {
		return rc;
	}
	if ((skey.len >= key_len) ||
	    (sval.len >= val_len)) {
		return -1;
	}
	fnx_substr_copyto(&skey, key, key_len);
	fnx_substr_copyto(&sval, val, val_len);
	return 0;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

enum FUNEX_MNTOPT {
	FUNEX_MNTOPT_RDONLY,
	FUNEX_MNTOPT_NOSUID,
	FUNEX_MNTOPT_NODEV,
	FUNEX_MNTOPT_NOEXEC,
	FUNEX_MNTOPT_MANDLOCK,
	FUNEX_MNTOPT_DIRSYNC,
	FUNEX_MNTOPT_NOATIME,
	FUNEX_MNTOPT_NODIRATIME,
	FUNEX_MNTOPT_RELATIME,
};

int funex_parse_mntops(const char *str, fnx_mntf_t *mntf)
{
	int rc;
	size_t len, sz;
	char buf[512];
	char *subopts, *value;

	char *const token[] = {
		[FUNEX_MNTOPT_RDONLY]       = (char *)"ro",
		[FUNEX_MNTOPT_NOSUID]       = (char *)"nosuid",
		[FUNEX_MNTOPT_NODEV]        = (char *)"nodev",
		[FUNEX_MNTOPT_NOEXEC]       = (char *)"noexec",
		[FUNEX_MNTOPT_MANDLOCK]     = (char *)"mandlock",
		[FUNEX_MNTOPT_DIRSYNC]      = (char *)"dirsync",
		[FUNEX_MNTOPT_NOATIME]      = (char *)"noatime",
		[FUNEX_MNTOPT_NODIRATIME]   = (char *)"nodiratime",
		[FUNEX_MNTOPT_RELATIME]     = (char *)"relatime",
		NULL
	};

	*mntf = FNX_MNTF_DEFAULT;

	len = strlen(str);
	sz  = sizeof(buf);
	if (len >= sz) {
		return -1;
	}
	strncpy(buf, str, sz);

	rc = 0;
	subopts = buf;
	while (*subopts != '\0') {
		value = NULL;
		switch (getsubopt(&subopts, token, &value)) {
			case FUNEX_MNTOPT_RDONLY:
				*mntf |= FNX_MNTF_RDONLY;
				break;
			case FUNEX_MNTOPT_NOSUID:
				*mntf |= FNX_MNTF_NOSUID;
				break;
			case FUNEX_MNTOPT_NODEV:
				*mntf |= FNX_MNTF_NODEV;
				break;
			case FUNEX_MNTOPT_NOEXEC:
				*mntf |= FNX_MNTF_NOEXEC;
				break;
			case FUNEX_MNTOPT_MANDLOCK:
				*mntf |= FNX_MNTF_MANDLOCK;
				break;
			case FUNEX_MNTOPT_DIRSYNC:
				*mntf |= FNX_MNTF_DIRSYNC;
				break;
			case FUNEX_MNTOPT_NOATIME:
				*mntf |= FNX_MNTF_NOATIME;
				break;
			case FUNEX_MNTOPT_NODIRATIME:
				*mntf |= FNX_MNTF_NODIRATIME;
				break;
			case FUNEX_MNTOPT_RELATIME:
				*mntf |= FNX_MNTF_RELATIME;
				break;
			default:
				rc = -1;
				break;
		}
	}
	return rc;
}
