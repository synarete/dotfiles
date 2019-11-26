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
#ifndef CAC_WSTRING_H_
#define CAC_WSTRING_H_

struct htblfns;

typedef wsubstr_t wstring_t;

extern wstring_t *wstring_nil;

extern const struct htblfns *wstring_hfns;


/*
 * Strings pair
 */
typedef struct wstrpair {
	wstring_t *first;
	wstring_t *second;
} wstrpair_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

wstring_t *wstring_new(const wchar_t *);

wstring_t *wstring_nnew(const wchar_t *, size_t);

wstring_t *wstring_cnew(wchar_t);


wstring_t *wstring_dup(const wstring_t *);

wsupstr_t *wstring_xdup(const wstring_t *);

wstring_t *wstring_join(const wstring_t *, const wstring_t *);

wstring_t *wstring_joins(const wstring_t *, const wchar_t *);

wstring_t *wstring_joinn(const wstring_t *, const wchar_t *,
                         size_t);

wstring_t *wstring_joinss(const wstring_t *,
                          const wchar_t *, const wstring_t *);


wstrpair_t *wstring_split(const wstring_t *, size_t, size_t);

wstrpair_t *wstring_split_chr(const wstring_t *, wchar_t);

wstrpair_t *wstring_split_str(const wstring_t *, const wchar_t *);

wstring_t *wstring_fmt(const wchar_t *fmt, ...);

bool wstring_isequal(const wstring_t *, const wstring_t *);

uint64_t wstring_hash(const wstring_t *);

int wstring_compare(const wstring_t *, const wstring_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

wstrpair_t *cac_wstrpair_new(void);

wstrpair_t *cac_wstrpair_new2(wstring_t *, wstring_t *);


#endif /* CAC_WSTRING_H_ */

