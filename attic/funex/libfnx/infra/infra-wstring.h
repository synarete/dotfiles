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
#ifndef FNX_INFRA_WSTRING_H_
#define FNX_INFRA_WSTRING_H_


typedef int (*fnx_wchr_pred)(wchar_t);
typedef void (*fnx_wchr_modify_fn)(wchar_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * String Operations:
 */
/*
 * Returns the number of characters in s before the first null character.
 */
size_t fnx_wstr_length(const wchar_t *s);

/*
 * Three way lexicographic compare of two characters-arrays.
 */
int fnx_wstr_compare(const wchar_t *s1, const wchar_t *s2, size_t n);
int fnx_wstr_ncompare(const wchar_t *s1, size_t n1,
                      const wchar_t *s2, size_t n2);

/*
 * Returns the first occurrence of s2 as a substring of s1, or null if no such
 * substring.
 */
const wchar_t *fnx_wstr_find(const wchar_t *s1, size_t n1,
                             const wchar_t *s2, size_t n2);


/*
 * Returns the last occurrence of s2 as substring of s1.
 */
const wchar_t *fnx_wstr_rfind(const wchar_t *s1, size_t n1,
                              const wchar_t *s2, size_t n2);


const wchar_t *fnx_wstr_find_chr(const wchar_t *s, size_t n, wchar_t a);


/*
 * Returns the last occurrence of c within the first n characters of s
 */
const wchar_t *fnx_wstr_rfind_chr(const wchar_t *s, size_t n, wchar_t c);

/*
 * Returns the first occurrence of any of the characters of s2 in s1.
 */
const wchar_t *fnx_wstr_find_first_of(const wchar_t *s1, size_t n1,
                                      const wchar_t *s2, size_t n2);

/*
 * Returns the first occurrence of any of the wchar_t of s2 which is not in s1.
 */
const wchar_t *
fnx_wstr_find_first_not_of(const wchar_t *s1, size_t n1,
                           const wchar_t *s2, size_t n2);

/*
 * Returns the first character in s which is not equal to c.
 */
const wchar_t *
fnx_wstr_find_first_not_eq(const wchar_t *s, size_t n, wchar_t c);

/*
 * Returns the last occurrence of any of the characters of s2 within the first
 * n1 characters of s1.
 */
const wchar_t *fnx_wstr_find_last_of(const wchar_t *s1, size_t n1,
                                     const wchar_t *s2, size_t n2);

/*
 * Returns the last occurrence of any of the characters of s2 which is not in
 * the first n1 characters of s1.
 */
const wchar_t *fnx_wstr_find_last_not_of(const wchar_t *s1, size_t n1,
        const wchar_t *s2, size_t n2);

/*
 * Returns the last character within the first n characters of s which is not
 * equal to c.
 */
const wchar_t *
fnx_wstr_find_last_not_eq(const wchar_t *s, size_t n, wchar_t c);

/*
 * Returns the number of matching equal characters from the first n characters
 * of s1 and s2.
 */
size_t fnx_wstr_common_prefix(const wchar_t *s1, const wchar_t *s2, size_t n);

/*
 * Returns the number of matching equal characters from the last n characters
 * of s1 and s2.
 */
size_t fnx_wstr_common_suffix(const wchar_t *s1, const wchar_t *s2, size_t n);


/*
 * Returns the number of of characters from the first n1 elements of s1 that
 * overlaps any of the characters of the first n2 elements of s2.
 */
size_t fnx_wstr_overlaps(const wchar_t *s1, size_t n1,
                         const wchar_t *s2, size_t n2);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Copy the first n characters of s into p. In case of overlap, uses safe copy.
 */
void fnx_wstr_copy(wchar_t *p, const wchar_t *s, size_t n);


/*
 * Assigns n copies of c to the first n elements of s.
 */
void fnx_wstr_fill(wchar_t *s, size_t n, wchar_t c);

/*
 * Assign EOS character ('\0') in s[n]
 */
void fnx_wstr_terminate(wchar_t *s, size_t n);

/*
 * Revere the order of characters in s.
 */
void fnx_wstr_reverse(wchar_t *s, size_t n);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Inserts the first n2 characters of s to in front of the first n1 characters
 * of p. In case of insufficient buffer-size, the result string is truncated.
 *
 * p   Target buffer
 * sz  Size of buffer: number of writable elements after p.
 * n1  Number of chars already inp (must be less or equal to sz)
 * s   Source string (may overlap any of the characters of p)
 * n2  Number of chars in s
 *
 * Returns the number of characters in p after insertion (always less or equal
 * to sz).
 */
size_t fnx_wstr_insert(wchar_t *p, size_t sz, size_t n1,
                       const wchar_t *s, size_t n2);


/*
 * Inserts n2 copies of c to the front of p. Tries to insert as many characters
 * as possible, but does not insert more then available writable characters
 * in the buffer.
 *
 * Makes room at the beginning of the buffer: move the current string m steps
 * forward, then fill k c-characters into p.
 *
 * p   Target buffer
 * sz  Size of buffer: number of writable elements after p.
 * n1  Number of chars already in p (must be less or equal to sz)
 * n2  Number of copies of c to insert.
 * c   Fill character.
 *
 * Returns the number of characters in p after insertion (always less or equal
 * to sz).
 */
size_t fnx_wstr_insert_chr(wchar_t *p, size_t sz,
                           size_t n1, size_t n2, wchar_t c);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Replaces the first n1 characters of p with the first n2 characters of s.
 *
 * p   Target buffer
 * sz  Size of buffer: number of writable elements after p.
 * len Length of current string in p.
 * n1  Number of chars to replace (must be less or equal to len).
 * s   Source string (may overlap any of the characters of p)
 * n2  Number of chars in s
 *
 * Returns the number of characters in p after replacement (always less or
 * equal to sz).
 */
size_t fnx_wstr_replace(wchar_t *p, size_t sz, size_t len, size_t n1,
                        const wchar_t *s, size_t n2);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Replace the first n2 characters of p with n2 copies of c.
 *
 * Returns the number of characters in p after replacement (always less or
 * equal to sz).
 */
size_t fnx_wstr_replace_chr(wchar_t *p, size_t sz, size_t len,
                            size_t n1, size_t n2, wchar_t c);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Char-Traits Functions:
 */
int fnx_wchr_isalnum(wchar_t c);
int fnx_wchr_isalpha(wchar_t c);
int fnx_wchr_isblank(wchar_t c);
int fnx_wchr_iscntrl(wchar_t c);
int fnx_wchr_isdigit(wchar_t c);
int fnx_wchr_isgraph(wchar_t c);
int fnx_wchr_islower(wchar_t c);
int fnx_wchr_isprint(wchar_t c);
int fnx_wchr_ispunct(wchar_t c);
int fnx_wchr_isspace(wchar_t c);
int fnx_wchr_isupper(wchar_t c);
int fnx_wchr_isxdigit(wchar_t c);

wchar_t fnx_wchr_toupper(wchar_t c);
wchar_t fnx_wchr_tolower(wchar_t c);

#endif /* FNX_INFRA_WSTRING_H_ */

