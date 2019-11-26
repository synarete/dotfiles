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
#ifndef CAC_WSTRBUF_H_
#define CAC_WSTRBUF_H_

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Copy the first n characters of s into p. In case of overlap, uses safe copy.
 */
void wstr_copy(wchar_t *p, const wchar_t *s, size_t n);

/*
 * Copy the first n2 characters but no more than n1 characters of s2 into s1.
 * If possible, terminates target buffer with null wchar_tacter.
 */
size_t wstr_ncopy(wchar_t *s1, size_t n1, const wchar_t *s2, size_t n2);

/*
 * Assigns n copies of c to the first n elements of s.
 */
void wstr_fill(wchar_t *s, size_t n, wchar_t c);

/*
 * Assign EOS wchar_tacter ('\0') in s[n]
 */
void wstr_terminate(wchar_t *s, size_t n);

/*
 * Revere the order of characters in s.
 */
void wstr_reverse(wchar_t *s, size_t n);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Inserts the first n2 characters of s to in front of the first n1 characters
 * of buf. In case of insufficient buffer-size, the result string is truncated.
 *
 * buf  Target buffer
 * sz   Size of buffer: number of writable elements after buf.
 * n1   Number of wchar_ts already inp (must be less or equal to sz)
 * s    Source string (may overlap any of the characters of buf)
 * n2   Number of wchar_ts in s
 *
 * Returns the number of characters in p after insertion (always less or equal
 * to sz).
 */
size_t wstr_insert(wchar_t *buf, size_t sz, size_t n1,
                   const wchar_t *s, size_t n2);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Inserts the first n characters of s in front position pos of buf. In case of
 * insufficient buffer-size, the result string is truncated.
 *
 * buf  Target buffer
 * sz   Current number of valid characters in buf
 * wr   Size of buffer: number of writable elements after buf
 * pos  Insert position in buf
 * s    Source string (may overlap any of the characters of buf)
 * n    Number of wchar_ts in s
 *
 * Returns the number of characters in p after insertion
 */
size_t wstr_insert_sub(wchar_t *buf, size_t sz, size_t wr,
                       size_t pos, const wchar_t *s, size_t n);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Inserts n2 copies of c to the front of buf. Tries to insert as many
 * characters as possible, but does not insert more then available writable
 * characters in the buffer.
 *
 * Makes room at the beginning of the buffer: move the current string m steps
 * forward, then fill k c-characters into buf.
 *
 * buf  Target buffer
 * sz   Size of buffer: number of writable elements after p.
 * n1   Number of wchar_ts already in p (must be less or equal to sz)
 * n2   Number of copies of c to insert.
 * c    Fill wchar_tacter.
 *
 * Returns the number of characters in p after insertion (always less or equal
 * to sz).
 */
size_t wstr_insert_chr(wchar_t *buf, size_t sz,
                       size_t n1, size_t n2, wchar_t c);

/*
 * Inserts n copies of c before position pos.
 *
 * buf  Target buffer
 * sz   Current number of valid characters in buf
 * wr   Size of buffer: number of writable elements after buf
 * pos  Insertion position
 * n    Number of chacaters to insert
 * c    Fill wchar_tacter
 *
 * Returns the number of characters in p after insertion.
 */
size_t wstr_insert_fill(wchar_t *buf, size_t sz, size_t wr,
                        size_t pos, size_t n, wchar_t c);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Replaces the first n1 characters of buf with the first n2 characters of s.
 *
 * buf  Target buffer
 * sz   Size of buffer: number of writable elements after buf
 * len  Length of current string in buf
 * n1   Number of wchar_ts to replace (must be less or equal to len)
 * s    Source string (may overlap any of the characters of buf)
 * n2   Number of wchar_ts in s
 *
 * Returns the number of characters in buf after replacement (always less or
 * equal to sz).
 */
size_t wstr_replace(wchar_t *buf, size_t sz, size_t len, size_t n1,
                    const wchar_t *s, size_t n2);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Replaces a sub-range of buf with a copy of s.
 *
 * buf  Beginning of characters range
 * sz   Current size of characters range buf
 * wr   Number of writable characters of buf
 * pos  Start index of sub-range to replace
 * cnt  Number of characters to replace
 * s    Source sequence to use
 * n    Source length
 *
 * Returns the number of valid characters in buf after replacement.
 */
size_t wstr_replace_sub(wchar_t *buf, size_t sz, size_t wr,
                        size_t pos, size_t cnt, const wchar_t *s, size_t n);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Replace the first n2 characters of buf with n2 copies of c.
 *
 * Returns the number of characters in buf after replacement (always less or
 * equal to sz).
 */
size_t wstr_replace_chr(wchar_t *buf, size_t sz, size_t len,
                        size_t n1, size_t n2, wchar_t c);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Replaces a sub-range of buf with a n copies of c.
 *
 * buf  Beginning of characters range
 * sz   Current size of characters range p
 * wr   Number of writable characters of p
 * pos  Start index of sub-range to replace
 * cnt  Number of characters to replace
 * n    Number of occurrences of c to fill
 * c    Fill wchar_tacter
 *
 * Returns the number of valid characters in buf after replacement.
 */
size_t wstr_replace_fill(wchar_t *buf, size_t sz, size_t wr,
                         size_t pos, size_t cnt, size_t n, wchar_t c);


#endif /* CAC_WSTRBUF_H_ */

