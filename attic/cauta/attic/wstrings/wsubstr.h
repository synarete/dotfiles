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
#ifndef CAC_WSUBSTR_H_
#define CAC_WSUBSTR_H_


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Sub-string: reference to read-only characters-array. The actual referenced
 * memory region is provided and maintained by user.
 */
typedef struct wsubstr {
	/* Beginning of read-only characters-array */
	const wchar_t *str;

	/* Number of readable wchar_ts (string's length) */
	size_t len;

} wsubstr_t;


typedef struct wsubstr_pair {
	wsubstr_t first, second;

} wsubstr_pair_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Returns the largest possible size a sub-string may have.
 */
size_t wsubstr_max_size(void);

/*
 * "Not-a-pos" (synonym to wsubstr_max_size())
 */
size_t wsubstr_npos(void);


/*
 * Constructor: create a read-only sub-strings over user-provided characters
 * sequence.
 */
void wsubstr_init(wsubstr_t *ss, const wchar_t *str);
void wsubstr_ninit(wsubstr_t *ss, const wchar_t *s, size_t n);

/*
 * Shallow-copy constructor (without deep copy).
 */
void wsubstr_clone(const wsubstr_t *ss, wsubstr_t *other);

/*
 * Destructor: zero all
 */
void wsubstr_destroy(wsubstr_t *ss);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Returns the string's read-length. Synonym to ss->len.
 */
size_t wsubstr_size(const wsubstr_t *ss);

/*
 * Returns TRUE if sub-string's length is zero.
 */
int wsubstr_isempty(const wsubstr_t *ss);

/*
 *  Returns pointer to beginning of characters sequence.
 */
const wchar_t *wsubstr_data(const wsubstr_t *ss);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Returns an iterator pointing to the beginning of the characters sequence.
 */
const wchar_t *wsubstr_begin(const wsubstr_t *ss);

/*
 * Returns an iterator pointing to the end of the characters sequence.
 */
const wchar_t *wsubstr_end(const wsubstr_t *ss);

/*
 * Returns the number of elements between begin() and p. If p is out-of-range,
 * returns npos.
 */
size_t wsubstr_offset(const wsubstr_t *ss, const wchar_t *p);

/*
 * Returns pointer to the n'th character. Performs out-of-range check:
 * panics in case n is out of range.
 */
const wchar_t *wsubstr_at(const wsubstr_t *wsubstr, size_t n);

/*
 * Returns TRUE if ss->ss_str[i] is a valid sub-string-index (i < s->ss_len).
 */
int wsubstr_isvalid_index(const wsubstr_t *ss, size_t i);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 *  Copy data into buffer, return number of characters assigned. Assign
 *  no more than min(n, size()) bytes.
 *
 *  Returns the number of characters copied to buf, not including possible null
 *  termination character.
 *
 *  NB: Result buf will be null-terminated only if there is enough room
 *  in target buffer (i.e. n > size()).
 */
size_t wsubstr_copyto(const wsubstr_t *ss, wchar_t *buf, size_t n);

/*
 * Three-way lexicographical comparison
 */
int wsubstr_compare(const wsubstr_t *ss, const wchar_t *s);
int wsubstr_ncompare(const wsubstr_t *ss, const wchar_t *s, size_t n);

/*
 * Returns TRUE in case of equal size and equal data
 */
int wsubstr_isequal(const wsubstr_t *ss, const wchar_t *s);
int wsubstr_isequals(const wsubstr_t *ss1, const wsubstr_t *ss2);
int wsubstr_nisequal(const wsubstr_t *ss, const wchar_t *s, size_t n);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Returns the number of (non-overlapping) occurrences of s (or c) as a
 * sub-string of ss.
 */
size_t wsubstr_count(const wsubstr_t *ss, const wchar_t *s);
size_t wsubstr_ncount(const wsubstr_t *ss,
                      const wchar_t *s, size_t n);
size_t wsubstr_count_chr(const wsubstr_t *ss, wchar_t c);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Searches ss, beginning at position pos, for the first occurrence of s (or
 * single-character c). If search fails, returns npos.
 *
 * Compatible with C++ STL: empty string always matches
 */
size_t wsubstr_find(const wsubstr_t *ss, const wchar_t *str);
size_t wsubstr_nfind(const wsubstr_t *ss,
                     size_t pos, const wchar_t *s, size_t n);
size_t wsubstr_find_chr(const wsubstr_t *ss, size_t pos, wchar_t c);


/*
 * Searches ss backwards, beginning at position pos, for the last occurrence of
 * s (or single-character c). If search fails, returns npos.
 */
size_t wsubstr_rfind(const wsubstr_t *ss, const wchar_t *s);
size_t wsubstr_nrfind(const wsubstr_t *ss,
                      size_t pos, const wchar_t *s, size_t n);
size_t wsubstr_rfind_chr(const wsubstr_t *ss, size_t pos, wchar_t c);


/*
 * Searches ss, beginning at position pos, for the first character that is
 * equal to any one of the characters of s.
 */
size_t wsubstr_find_first_of(const wsubstr_t *ss, const wchar_t *s);
size_t wsubstr_nfind_first_of(const wsubstr_t *ss,
                              size_t pos, const wchar_t *s, size_t n);


/*
 * Searches ss backwards, beginning at position pos, for the last character
 * that is equal to any of the characters of s.
 */
size_t wsubstr_find_last_of(const wsubstr_t *ss, const wchar_t *s);
size_t wsubstr_nfind_last_of(const wsubstr_t *ss,
                             size_t pos, const wchar_t *s, size_t n);


/*
 * Searches ss, beginning at position pos, for the first character that is not
 * equal to any of the characters of s.
 */
size_t wsubstr_find_first_not_of(const wsubstr_t *ss,
                                 const wchar_t *s);
size_t wsubstr_nfind_first_not_of(const wsubstr_t *ss,
                                  size_t pos, const wchar_t *s, size_t n);
size_t wsubstr_find_first_not(const wsubstr_t *ss,
                              size_t pos, wchar_t c);



/*
 * Searches ss backwards, beginning at position pos, for the last character
 * that is not equal to any of the characters of s (or c).
 */
size_t wsubstr_find_last_not_of(const wsubstr_t *ss,
                                const wchar_t *s);
size_t wsubstr_nfind_last_not_of(const wsubstr_t *ss,
                                 size_t pos, const wchar_t *s, size_t n);
size_t wsubstr_find_last_not(const wsubstr_t *ss,
                             size_t pos, wchar_t c);



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Creates a sub-string of ss, which refers to n characters after position i.
 * If i is an invalid index, the result sub-string is empty. If there are less
 * then n characters after position i, the result sub-string will refer only to
 * the elements which are members of ss.
 */
void wsubstr_sub(const wsubstr_t *ss,
                 size_t i, size_t n,  wsubstr_t *result);

/*
 * Creates a sub-string of ss, which refers to the last n wchar_ts. The result
 * sub-string will not refer to more then ss->ss_len elements.
 */
void wsubstr_rsub(const wsubstr_t *ss,
                  size_t n, wsubstr_t *result);

/*
 * Creates a sub-string with all the characters that are in the range of
 * both s1 and s2. That is, all elements within the range
 * [s1.begin(),s1.end()) which are also in the range
 * [s2.begin(), s2.end()) (i.e. have the same address).
 */
void wsubstr_intersection(const wsubstr_t *s1,
                          const wsubstr_t *s2,
                          wsubstr_t *result);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Creates a pair of sub-strings of ss, which are divided by any of the first
 * characters of s. If none of the characters of s is in ss, the first
 * first element of the result-pair is equal to ss and the second element is an
 * empty sub-string.
 *
 *  Examples:
 *  split("root@foo//bar", "/@:")  --> "root", "foo//bar"
 *  split("foo///:bar::zoo", ":/") --> "foo", "bar:zoo"
 *  split("root@foo.bar", ":/")    --> "root@foo.bar", ""
 */
void wsubstr_split(const wsubstr_t *ss,
                   const wchar_t *s, wsubstr_pair_t *);

void wsubstr_nsplit(const wsubstr_t *ss,
                    const wchar_t *s, size_t n, wsubstr_pair_t *);

void wsubstr_split_chr(const wsubstr_t *ss, wchar_t sep,
                       wsubstr_pair_t *result);



/*
 * Creates a pair of sub-strings of ss, which are divided by any of the first n
 * characters of s, while searching ss backwards. If none of the characters
 * of s is in ss, the first element of the pair equal to ss and the second
 * element is an empty sub-string.
 */
void wsubstr_rsplit(const wsubstr_t *ss,
                    const wchar_t *s, wsubstr_pair_t *);

void wsubstr_nrsplit(const wsubstr_t *ss,
                     const wchar_t *s, size_t, wsubstr_pair_t *);

void wsubstr_rsplit_chr(const wsubstr_t *ss, wchar_t c,
                        wsubstr_pair_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Creates a sub-string of ss without the first n leading characters.
 */
void wsubstr_trim(const wsubstr_t *ss, size_t n, wsubstr_t *);


/*
 * Creates a sub-string of ss without any leading characters which are members
 * of s.
 */
void wsubstr_trim_any_of(const wsubstr_t *ss, const wchar_t *set,
                         wsubstr_t *);

void wsubstr_ntrim_any_of(const wsubstr_t *ss,
                          const wchar_t *s, size_t n, wsubstr_t *);

void wsubstr_trim_chr(const wsubstr_t *wsubstr, wchar_t c,
                      wsubstr_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Creates a sub-string of ss without the last n trailing characters.
 * If n >= ss->ss_len the result sub-string is empty.
 */
void wsubstr_chop(const wsubstr_t *ss, size_t n, wsubstr_t *);

/*
 * Creates a sub-string of ss without any trailing characters which are members
 * of n.
 */
void wsubstr_chop_any_of(const wsubstr_t *ss,
                         const wchar_t *s, wsubstr_t *);

void wsubstr_nchop_any_of(const wsubstr_t *ss,
                          const wchar_t *s, size_t n, wsubstr_t *);

/*
 * Creates a sub-string of ss without any trailing characters that equals c.
 */
void wsubstr_chop_chr(const wsubstr_t *ss, wchar_t c, wsubstr_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Creates a sub-string of ss without any leading and trailing characters which
 * are members of s.
 */
void wsubstr_strip_any_of(const wsubstr_t *ss,
                          const wchar_t *s, wsubstr_t *);

void wsubstr_nstrip_any_of(const wsubstr_t *ss,
                           const wchar_t *s, size_t n, wsubstr_t *);

/*
 * Creates a sub-string of ss without any leading and trailing characters which
 * are equal to c.
 */
void wsubstr_strip_chr(const wsubstr_t *ss, wchar_t c, wsubstr_t *);


/*
 * Strip white-spaces
 */
void wsubstr_strip_ws(const wsubstr_t *ss, wsubstr_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Finds the first sub-string of ss that is a token delimited by any of the
 * characters of s.
 */
void wsubstr_find_token(const wsubstr_t *ss,
                        const wchar_t *s, wsubstr_t *);

void wsubstr_nfind_token(const wsubstr_t *ss,
                         const wchar_t *s, size_t n, wsubstr_t *);

void wsubstr_find_token_chr(const wsubstr_t *ss, wchar_t c,
                            wsubstr_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Finds the next token in ss after tok, which is delimited by any of the
 * characters of s.
 */

void wsubstr_find_next_token(const wsubstr_t *ss,
                             const wsubstr_t *tok,
                             const wchar_t *s,
                             wsubstr_t *);

void wsubstr_nfind_next_token(const wsubstr_t *ss,
                              const wsubstr_t *tok,
                              const wchar_t *s, size_t n,
                              wsubstr_t *);

void wsubstr_find_next_token_chr(const wsubstr_t *wsubstr,
                                 const wsubstr_t *tok, wchar_t sep,
                                 wsubstr_t *);

/*
 * Parses the sub-string ss into tokens, delimited by separators s and inserts
 * them into tok_list. Inserts no more then max_sz tokens.
 *
 * Returns 0 if all tokens assigned to tok_list, or -1 in case of insufficient
 * space. If p_ntok is not NULL it is set to the number of parsed tokens.
 */
int wsubstr_tokenize(const wsubstr_t *ss, const wchar_t *s,
                     wsubstr_t tok_list[], size_t list_size,
                     size_t *p_ntok);

int wsubstr_ntokenize(const wsubstr_t *ss,
                      const wchar_t *s, size_t n,
                      wsubstr_t tok_list[], size_t list_size,
                      size_t *p_ntok);

int wsubstr_tokenize_chr(const wsubstr_t *ss, wchar_t sep,
                         wsubstr_t tok_list[], size_t list_size,
                         size_t *p_ntok);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Scans ss for the longest common prefix with s.
 */
size_t wsubstr_common_prefix(const wsubstr_t *ss, const wchar_t *str);
size_t wsubstr_ncommon_prefix(const wsubstr_t *ss,
                              const wchar_t *s, size_t n);

/*
 * Return TRUE if the first character of ss equals c.
 */
int wsubstr_starts_with(const wsubstr_t *ss, wchar_t c);


/*
 * Scans ss backwards for the longest common suffix with s.
 */
size_t wsubstr_common_suffix(const wsubstr_t *ss,
                             const wchar_t *s);
size_t wsubstr_ncommon_suffix(const wsubstr_t *ss,
                              const wchar_t *s, size_t n);

/*
 * Return TRUE if the last character of ss equals c.
 */
int wsubstr_ends_with(const wsubstr_t *ss, wchar_t c);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Generic:
 */

/*
 * Returns the index of the first element of ss that satisfy the unary
 * predicate fn (or !fn), or npos if no such element exist.
 */
size_t wsubstr_find_if(const wsubstr_t *ss, wchr_fn fn);
size_t wsubstr_find_if_not(const wsubstr_t *ss, wchr_fn fn);


/*
 * Returns the index of the last element of ss that satisfy the unary
 * predicate fn (or !fn), or npos if no such element exist.
 */
size_t wsubstr_rfind_if(const wsubstr_t *ss, wchr_fn fn);
size_t wsubstr_rfind_if_not(const wsubstr_t *ss, wchr_fn fn);

/*
 * Returns the number of elements in ss that satisfy the unary predicate fn.
 */
size_t wsubstr_count_if(const wsubstr_t *ss, wchr_fn fn);

/*
 * Returns TRUE if all characters of ss satisfy unary predicate fn.
 */
int wsubstr_test_if(const wsubstr_t *ss, wchr_fn fn);


/*
 * Creates a sub-string of ss without leading characters that satisfy unary
 * predicate fn.
 */
void wsubstr_trim_if(const wsubstr_t *ss,
                     wchr_fn fn, wsubstr_t *);

/*
 * Creates a sub-string of ss without trailing characters that satisfy unary
 * predicate fn.
 */
void wsubstr_chop_if(const wsubstr_t *ss,
                     wchr_fn fn, wsubstr_t *);

/*
 * Creates a sub-string of ss without any leading and trailing characters that
 * satisfy unary predicate fn.
 */
void wsubstr_strip_if(const wsubstr_t *ss,
                      wchr_fn fn, wsubstr_t *);


#endif /* CAC_WSUBSTR_H_ */


