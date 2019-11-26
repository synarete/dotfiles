/*
 * This file is part of CaC
 *
 * Copyright (C) 2016,2017 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CaC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <iconv.h>
#include <wchar.h>
#include <errno.h>

#include "infra.h"
#include "iconvu.h"

static void cac_iconv_check(void)
{
	cac_staticassert_eq(sizeof(wchar_t), 4);
}

static int cac_iconv_open(const char *tocode,
                          const char *fromcode, iconv_t *out)
{
	int rc = 0;
	iconv_t cd;

	cd = iconv_open(tocode, fromcode);
	if (cd == (iconv_t)(-1)) {
		rc = -errno;
	} else {
		*out = cd;
	}
	return rc;
}

static int cac_iconv(iconv_t cd, char **inbuf, size_t *inbytesleft,
                     char **outbuf, size_t *outbytesleft)
{
	int rc = 0;
	size_t res;

	res = iconv(cd, inbuf, inbytesleft, outbuf, outbytesleft);
	if (res == (size_t)(-1)) {
		rc = -errno;
	}
	return rc;
}

static int cac_iconv_close(iconv_t cd)
{
	int rc;

	rc = iconv_close(cd);
	if (rc != 0) {
		rc = -errno;
	}
	return rc;
}

int cac_iconv_utf8_to_wchar(const char *utf8, size_t len,
                            wchar_t **out_dat, size_t *out_len)
{
	int rc;
	char *in, *out, *buf;
	iconv_t cd = NULL;
	size_t inbytes, outbytes, outbufsz, outlen;

	cac_iconv_check();
	in = (char *)utf8;
	inbytes = len * sizeof(in[0]);
	outbufsz = ((inbytes + 1) * sizeof(wchar_t));
	buf = out = (char *)gc_malloc(outbufsz);
	outbytes = outbufsz;

	rc = cac_iconv_open("WCHAR_T", "UTF-8", &cd);
	if (rc != 0) {
		return rc;
	}
	rc = cac_iconv(cd, &in, &inbytes, &buf, &outbytes);
	if (rc != 0) {
		cac_iconv_close(cd);
		return rc;
	}
	rc = cac_iconv_close(cd);
	if (rc != 0) {
		return rc;
	}
	outlen = (outbufsz - outbytes) / sizeof(wchar_t);

	*out_dat = (wchar_t *)out;
	*out_len = outlen;
	return 0;
}

