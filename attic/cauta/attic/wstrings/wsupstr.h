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
#ifndef CAC_WSUPSTR_H_
#define CAC_WSUPSTR_H_


typedef void (*wchr_modify_fn)(wchar_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Simple buffered-string: reference to writable characters-array buffer.
 * Extends sub-string functionalities.
 */
typedef struct wsupstr {
	/* Base object: read-only sub-string */
	wsubstr_t sub;

	/* Beginning of writable characters-buffer */
	wchar_t  *buf;

	/* Writable buffer size */
	size_t bsz;

} wsupstr_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Constructor: explicit reference to writable characters buffer with initial
 * zero valid characters, and up to nwr writable characters.
 */
void wsupstr_init(wsupstr_t *ss, wchar_t *buf, size_t nwr);

/*
 * Constructor: explicit reference to writable characters buffer with initial
 * nrd valid characters, and up to nwr writable characters.
 */
void wsupstr_ninit(wsupstr_t *ss, wchar_t *buf, size_t nrd, size_t nwr);


/*
 * Destructor: zero all
 */
void wsupstr_destroy(wsupstr_t *ss);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Returns the string's read-length. Synonym to ss->sub.len
 */
size_t wsupstr_size(const wsupstr_t *ss);

/*
 * Returns the writable-size of sub-string.
 */
size_t wsupstr_bufsize(const wsupstr_t *ss);



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 *  Returns pointer to beginning of characters sequence.
 */
wchar_t *wsupstr_data(const wsupstr_t *ss);

/*
 * Assigns s, truncates result in case of insufficient room.
 */
void wsupstr_assign(wsupstr_t *ss, const wchar_t *s);
void wsupstr_nassign(wsupstr_t *ss, const wchar_t *s, size_t len);

/*
 * Assigns n copies of c.
 */
void wsupstr_assign_chr(wsupstr_t *ss, size_t n, wchar_t c);


/*
 * Appends s.
 */
void wsupstr_append(wsupstr_t *ss, const wchar_t *s);
void wsupstr_nappend(wsupstr_t *ss, const wchar_t *s, size_t len);

/*
 * Appends n copies of c.
 */
void wsupstr_append_chr(wsupstr_t *ss, size_t n, wchar_t c);

/*
 * Appends single wchar_t.
 */
void wsupstr_push_back(wsupstr_t *ss, wchar_t c);

/*
 * Inserts s before position pos.
 */
void wsupstr_insert(wsupstr_t *ss, size_t pos, const wchar_t *s);
void wsupstr_ninsert(wsupstr_t *ss,
                     size_t pos, const wchar_t *s, size_t len);

/*
 * Inserts n copies of c before position pos.
 */
void wsupstr_insert_chr(wsupstr_t *ss,
                        size_t pos, size_t n, wchar_t c);

/*
 * Replaces a part of sub-string with the string s.
 */
void wsupstr_replace(wsupstr_t *ss,
                     size_t pos, size_t n, const wchar_t *s);
void wsupstr_nreplace(wsupstr_t *ss,
                      size_t pos, size_t n, const wchar_t *s, size_t len);

/*
 * Replaces part of sub-string with n2 copies of c.
 */
void wsupstr_replace_chr(wsupstr_t *ss,
                         size_t pos, size_t n1, size_t n2, wchar_t c);


/*
 * Erases part of sub-string.
 */
void wsupstr_erase(wsupstr_t *ss, size_t pos, size_t n);

/*
 * Reverse the writable portion of sub-string.
 */
void wsupstr_reverse(wsupstr_t *ss);


/*
 * Apply fn for every element in sub-string.
 */
void wsupstr_foreach(wsupstr_t *ss, wchr_modify_fn fn);



#endif /* CAC_WSUPSTR_H_ */


