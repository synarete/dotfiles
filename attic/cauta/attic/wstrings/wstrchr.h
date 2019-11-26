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
#ifndef CAC_WSTRCHR_H_
#define CAC_WSTRCHR_H_


typedef int (*wchr_fn)(wchar_t);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * String Operations:
 */
/*
 * Returns the number of characters in s before the first null wchar_tacter.
 */
size_t wstr_length(const wchar_t *s);

/*
 * Three way lexicographic compare of two characters-arrays.
 */
int wstr_compare(const wchar_t *s1, const wchar_t *s2, size_t n);
int wstr_ncompare(const wchar_t *s1, size_t n1,
                  const wchar_t *s2, size_t n2);

/*
 * Returns the first occurrence of s2 as a substring of s1, or null if no such
 * substring.
 */
const wchar_t *wstr_find(const wchar_t *s1, size_t n1,
                         const wchar_t *s2, size_t n2);


/*
 * Returns the number of non-overlapping occurrences of s2 in s1.
 */
size_t wstr_count(const wchar_t *s1, size_t n1, const wchar_t *s2,
                  size_t n2);

/*
 * Returns the last occurrence of s2 as substring of s1.
 */
const wchar_t *wstr_rfind(const wchar_t *s1, size_t n1,
                          const wchar_t *s2, size_t n2);


const wchar_t *wstr_find_chr(const wchar_t *s, size_t n, wchar_t a);


/*
 * Returns the number of occurrences of c in s.
 */
size_t wstr_count_chr(const wchar_t *s, size_t n, wchar_t c);


/*
 * Returns the last occurrence of c within the first n characters of s
 */
const wchar_t *wstr_rfind_chr(const wchar_t *s, size_t n, wchar_t c);

/*
 * Returns the first occurrence of any of the characters of s2 in s1.
 */
const wchar_t *wstr_find_first_of(const wchar_t *s1, size_t n1,
                                  const wchar_t *s2, size_t n2);

/*
 * Returns the first occurrence of any of the wchar_t of s2 which is not in s1.
 */
const wchar_t *wstr_find_first_not_of(const wchar_t *s1, size_t n1,
                                      const wchar_t *s2, size_t n2);

/*
 * Returns the first wchar_tacter in s which is not equal to c.
 */
const wchar_t *wstr_find_first_not_eq(const wchar_t *s, size_t n,
                                      wchar_t c);

/*
 * Returns the last occurrence of any of the characters of s2 within the first
 * n1 characters of s1.
 */
const wchar_t *wstr_find_last_of(const wchar_t *s1, size_t n1,
                                 const wchar_t *s2, size_t n2);

/*
 * Returns the last occurrence of any of the characters of s2 which is not in
 * the first n1 characters of s1.
 */
const wchar_t *
wstr_find_last_not_of(const wchar_t *s1, size_t n1,
                      const wchar_t *s2, size_t n2);

/*
 * Returns the last wchar_tacter within the first n characters of s which is not
 * equal to c.
 */
const wchar_t *
wstr_find_last_not_eq(const wchar_t *s, size_t n, wchar_t c);

/*
 * Returns the maximal number of matching equal characters from the first
 * characters of s1 and s2.
 */
size_t wstr_common_prefix(const wchar_t *s1, size_t n1,
                          const wchar_t *s2, size_t n2);

/*
 * Returns the maximal number of matching equal characters from the last
 * characters of s1 and s2.
 */
size_t wstr_common_suffix(const wchar_t *s1, size_t n1,
                          const wchar_t *s2, size_t n2);

/*
 * Returns the number of of characters from the first n1 elements of s1 that
 * overlaps any of the characters of the first n2 elements of s2.
 */
size_t wstr_overlaps(const wchar_t *s1, size_t n1,
                     const wchar_t *s2, size_t n2);



/*
 * Returns pointer to the first element which satisfies predicate fn, or NULL
 * pointer if none exists within range [beg, end).
 */
const wchar_t *wstr_find_if(const wchar_t *beg, const wchar_t *end,
                            wchr_fn fn, int u);


/*
 * Returns pointer to the element element which satisfies predicate fn, or NULL
 * pointer if none exists within range [beg, end).
 */
const wchar_t *wstr_rfind_if(const wchar_t *beg, const wchar_t *end,
                             wchr_fn fn, int u);


/*
 * Returns TRUE if all characters within the range [beg, end) satisfy unary
 * predicate fn.
 */
int wstr_test_if(const wchar_t *beg, const wchar_t *end, wchr_fn fn);


/*
 * Returns the number of characters within the range [beg, end) that satisfy
 * unary predicate fn.
 */
size_t wstr_count_if(const wchar_t *beg, const wchar_t *end,
                     wchr_fn fn);

#endif /* CAC_WSTRCHR_H_ */

