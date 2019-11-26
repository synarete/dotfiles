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
#ifndef FNX_INFRA_WSUBSTR_H_
#define FNX_INFRA_WSUBSTR_H_


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Sub-string: reference to characters-array. When nwr is zero, referring to
 * immutable (read-only) string. In all cases, never overlap writable region.
 * All possible dynamic-allocation must be made explicitly by the user.
 */
struct fnx_wsubstr {
	wchar_t *str;  /* Beginning of characters-array (rd & wr)    */
	size_t len;    /* Number of readable chars (string's length) */
	size_t nwr;    /* Number of writable chars from beginning    */
};
typedef struct fnx_wsubstr    fnx_wsubstr_t;


struct fnx_wsubstr_pair {
	fnx_wsubstr_t first, second;
};
typedef struct fnx_wsubstr_pair  fnx_wsubstr_pair_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Returns the largest possible size a sub-string may have.
 */
size_t fnx_wsubstr_max_size(void);

/*
 * "Not-a-pos" (synonym to fnx_wsubstr_max_size())
 */
size_t fnx_wsubstr_npos(void);


/*
 * Constructors:
 * The first two create read-only substrings, the next two creates a mutable
 * (for write) substring. The last one creates read-only empty string.
 */
void fnx_wsubstr_init(fnx_wsubstr_t *ss, const wchar_t *str);
void fnx_wsubstr_init_rd(fnx_wsubstr_t *ss, const wchar_t *s, size_t n);
void fnx_wsubstr_init_rwa(fnx_wsubstr_t *ss, wchar_t *);
void fnx_wsubstr_init_rw(fnx_wsubstr_t *ss, wchar_t *, size_t nrd, size_t nwr);
void fnx_wsubstr_inits(fnx_wsubstr_t *ss);

/*
 * Shallow-copy constructor (without deep copy).
 */
void fnx_wsubstr_clone(const fnx_wsubstr_t *ss, fnx_wsubstr_t *other);

/*
 * Destructor: zero all
 */
void fnx_wsubstr_destroy(fnx_wsubstr_t *ss);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Returns the string's read-length. Synonym to ss->len.
 */
size_t fnx_wsubstr_size(const fnx_wsubstr_t *ss);

/*
 * Returns the writable-size of sub-string.
 */
size_t fnx_wsubstr_wrsize(const fnx_wsubstr_t *ss);

/*
 * Returns TRUE if sub-string's length is zero.
 */
int fnx_wsubstr_isempty(const fnx_wsubstr_t *ss);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Returns an iterator pointing to the beginning of the characters sequence.
 */
const wchar_t *fnx_wsubstr_begin(const fnx_wsubstr_t *ss);

/*
 * Returns an iterator pointing to the end of the characters sequence.
 */
const wchar_t *fnx_wsubstr_end(const fnx_wsubstr_t *ss);

/*
 * Returns the number of elements between begin() and p. If p is out-of-range,
 * returns npos.
 */
size_t fnx_wsubstr_offset(const fnx_wsubstr_t *ss, const wchar_t *p);

/*
 * Returns pointer to the n'th character. Performs out-of-range check:
 * panics in case n is out of range.
 */
const wchar_t *fnx_wsubstr_at(const fnx_wsubstr_t *substr, size_t n);

/*
 * Returns TRUE if ss->ss_str[i] is a valid substring-index (i < s->ss_len).
 */
int fnx_wsubstr_isvalid_index(const fnx_wsubstr_t *ss, size_t i);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 *  Copy data into buffer, return number of characters assigned. Assign
 *  no more than min(n, size()) bytes.
 *
 *  Returns the number of characters copied to buf, not including possible null
 *  termination character.
 *
 *  NB: Result buf will be null-terminated only if there is enough
 *          room (i.e. n > size()).
 */
size_t fnx_wsubstr_copyto(const fnx_wsubstr_t *ss, wchar_t *buf, size_t n);

/*
 * Three-way lexicographical comparison
 */
int fnx_wsubstr_compare(const fnx_wsubstr_t *ss, const wchar_t *s);
int fnx_wsubstr_ncompare(const fnx_wsubstr_t *ss, const wchar_t *s, size_t n);

/*
 * Returns TRUE in case of equal size and equal data
 */
int fnx_wsubstr_isequal(const fnx_wsubstr_t *ss, const wchar_t *s);
int fnx_wsubstr_nisequal(const fnx_wsubstr_t *ss, const wchar_t *s, size_t n);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Returns the number of (non-overlapping) occurrences of s (or c) as a
 * substring of ss.
 */
size_t fnx_wsubstr_count(const fnx_wsubstr_t *ss, const wchar_t *s);
size_t fnx_wsubstr_ncount(const fnx_wsubstr_t *ss,
                          const wchar_t *s, size_t n);
size_t fnx_wsubstr_count_chr(const fnx_wsubstr_t *ss, wchar_t c);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Searches ss, beginning at position pos, for the first occurrence of s (or
 * single-character c). If search fails, returns npos.
 */
size_t fnx_wsubstr_find(const fnx_wsubstr_t *ss, const wchar_t *str);
size_t fnx_wsubstr_nfind(const fnx_wsubstr_t *ss,
                         size_t pos, const wchar_t *s, size_t n);
size_t fnx_wsubstr_find_chr(const fnx_wsubstr_t *ss, size_t pos, wchar_t c);


/*
 * Searches ss backwards, beginning at position pos, for the last occurrence of
 * s (or single-character c). If search fails, returns npos.
 */
size_t fnx_wsubstr_rfind(const fnx_wsubstr_t *ss, const wchar_t *s);
size_t fnx_wsubstr_nrfind(const fnx_wsubstr_t *ss,
                          size_t pos, const wchar_t *s, size_t n);
size_t fnx_wsubstr_rfind_chr(const fnx_wsubstr_t *ss, size_t pos, wchar_t c);


/*
 * Searches ss, beginning at position pos, for the first character that is
 * equal to any one of the characters of s.
 */
size_t fnx_wsubstr_find_first_of(const fnx_wsubstr_t *ss, const wchar_t *s);
size_t fnx_wsubstr_nfind_first_of(const fnx_wsubstr_t *ss,
                                  size_t pos, const wchar_t *s, size_t n);


/*
 * Searches ss backwards, beginning at position pos, for the last character
 * that is equal to any of the characters of s.
 */
size_t fnx_wsubstr_find_last_of(const fnx_wsubstr_t *ss, const wchar_t *s);
size_t fnx_wsubstr_nfind_last_of(const fnx_wsubstr_t *ss,
                                 size_t pos, const wchar_t *s, size_t n);


/*
 * Searches ss, beginning at position pos, for the first character that is not
 * equal to any of the characters of s.
 */
size_t fnx_wsubstr_find_first_not_of(const fnx_wsubstr_t *ss,
                                     const wchar_t *s);
size_t fnx_wsubstr_nfind_first_not_of(const fnx_wsubstr_t *ss,
                                      size_t pos, const wchar_t *s, size_t n);
size_t fnx_wsubstr_find_first_not(const fnx_wsubstr_t *ss,
                                  size_t pos, wchar_t c);



/*
 * Searches ss backwards, beginning at position pos, for the last character
 * that is not equal to any of the characters of s (or c).
 */
size_t fnx_wsubstr_find_last_not_of(const fnx_wsubstr_t *ss,
                                    const wchar_t *s);
size_t fnx_wsubstr_nfind_last_not_of(const fnx_wsubstr_t *ss,
                                     size_t pos, const wchar_t *s, size_t n);
size_t fnx_wsubstr_find_last_not(const fnx_wsubstr_t *ss,
                                 size_t pos, wchar_t c);



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Creates a substring of ss, which refers to n characters after position i.
 * If i is an invalid index, the result substring is empty. If there are less
 * then n characters after position i, the result substring will refer only to
 * the elements which are members of ss.
 */
void fnx_wsubstr_sub(const fnx_wsubstr_t *ss,
                     size_t i, size_t n,  fnx_wsubstr_t *result);

/*
 * Creates a substring of ss, which refers to the last n chars. The result
 * substring will not refer to more then ss->ss_len elements.
 */
void fnx_wsubstr_rsub(const fnx_wsubstr_t *ss,
                      size_t n, fnx_wsubstr_t *result);

/*
 * Creates a substring with all the characters that are in the range of
 * both s1 and s2. That is, all elements within the range
 * [s1.begin(),s1.end()) which are also in the range
 * [s2.begin(), s2.end()) (i.e. have the same address).
 */
void fnx_wsubstr_intersection(const fnx_wsubstr_t *s1,
                              const fnx_wsubstr_t *s2,
                              fnx_wsubstr_t *result);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Creates a pair of substrings of ss, which are divided by any of the first
 * characters of seps. If none of the characters of seps is in ss, the first
 * first element of the result-pair is equal to ss and the second element is an
 * empty substring.
 *
 *  Examples:
 *  split("root@foo//bar", "/@:")  --> "root", "foo//bar"
 *  split("foo///:bar::zoo", ":/") --> "foo", "bar:zoo"
 *  split("root@foo.bar", ":/")    --> "root@foo.bar", ""
 */
void fnx_wsubstr_split(const fnx_wsubstr_t *ss,
                       const wchar_t *seps, fnx_wsubstr_pair_t *);

void fnx_wsubstr_nsplit(const fnx_wsubstr_t *ss,
                        const wchar_t *seps, size_t n, fnx_wsubstr_pair_t *);

void fnx_wsubstr_split_chr(const fnx_wsubstr_t *ss, wchar_t sep,
                           fnx_wsubstr_pair_t *result);



/*
 * Creates a pair of substrings of ss, which are divided by any of the first n
 * characters of seps, while searching ss backwards. If none of the characters
 * of seps is in ss, the first element of the pair equal to ss and the second
 * element is an empty substring.
 */
void fnx_wsubstr_rsplit(const fnx_wsubstr_t *ss,
                        const wchar_t *seps, fnx_wsubstr_pair_t *result);

void fnx_wsubstr_nrsplit(const fnx_wsubstr_t *ss,
                         const wchar_t *seps, size_t, fnx_wsubstr_pair_t *);

void fnx_wsubstr_rsplit_chr(const fnx_wsubstr_t *ss, wchar_t sep,
                            fnx_wsubstr_pair_t *result);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Creates a substring of ss without the first n leading characters.
 */
void fnx_wsubstr_trim(const fnx_wsubstr_t *ss, size_t n, fnx_wsubstr_t *);


/*
 * Creates a substring of ss without any leading characters which are members
 * of set.
 */
void fnx_wsubstr_trim_any_of(const fnx_wsubstr_t *ss, const wchar_t *set,
                             fnx_wsubstr_t *result);

void fnx_wsubstr_ntrim_any_of(const fnx_wsubstr_t *ss,
                              const wchar_t *set, size_t n, fnx_wsubstr_t *);

void fnx_wsubstr_trim_chr(const fnx_wsubstr_t *substr, wchar_t c,
                          fnx_wsubstr_t *result);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Creates a substring of ss without the last n trailing characters.
 * If n >= ss->ss_len the result substring is empty.
 */
void fnx_wsubstr_chop(const fnx_wsubstr_t *ss, size_t n, fnx_wsubstr_t *);

/*
 * Creates a substring of ss without any trailing characters which are members
 * of set.
 */
void fnx_wsubstr_chop_any_of(const fnx_wsubstr_t *ss,
                             const wchar_t *set, fnx_wsubstr_t *result);

void fnx_wsubstr_nchop_any_of(const fnx_wsubstr_t *ss,
                              const wchar_t *set, size_t n, fnx_wsubstr_t *);

/*
 * Creates a substring of ss without any trailing characters that equals c.
 */
void fnx_wsubstr_chop_chr(const fnx_wsubstr_t *ss, wchar_t c, fnx_wsubstr_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Creates a substring of ss without any leading and trailing characters which
 * are members of set.
 */
void fnx_wsubstr_strip_any_of(const fnx_wsubstr_t *ss,
                              const wchar_t *set, fnx_wsubstr_t *result);

void fnx_wsubstr_nstrip_any_of(const fnx_wsubstr_t *ss,
                               const wchar_t *set, size_t n,
                               fnx_wsubstr_t *result);

/*
 * Creates a substring of substr without any leading and trailing
 * characters which are equal to c.
 */
void fnx_wsubstr_strip_chr(const fnx_wsubstr_t *ss, wchar_t c,
                           fnx_wsubstr_t *result);


/*
 * Strip white-spaces
 */
void fnx_wsubstr_strip_ws(const fnx_wsubstr_t *ss, fnx_wsubstr_t *result);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Finds the first substring of ss that is a token delimited by any of the
 * characters of sep(s).
 */
void fnx_wsubstr_find_token(const fnx_wsubstr_t *ss,
                            const wchar_t *seps, fnx_wsubstr_t *result);

void fnx_wsubstr_nfind_token(const fnx_wsubstr_t *ss,
                             const wchar_t *seps, size_t n,
                             fnx_wsubstr_t *result);

void fnx_wsubstr_find_token_chr(const fnx_wsubstr_t *ss, wchar_t sep,
                                fnx_wsubstr_t *result);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Finds the next token in ss after tok, which is delimeted by any of the
 * characters of sep(s).
 */

void fnx_wsubstr_find_next_token(const fnx_wsubstr_t *ss,
                                 const fnx_wsubstr_t *tok,
                                 const wchar_t *seps,
                                 fnx_wsubstr_t *result);

void fnx_wsubstr_nfind_next_token(const fnx_wsubstr_t *ss,
                                  const fnx_wsubstr_t *tok,
                                  const wchar_t *seps, size_t n,
                                  fnx_wsubstr_t *result);

void fnx_wsubstr_find_next_token_chr(const fnx_wsubstr_t *substr,
                                     const fnx_wsubstr_t *tok, wchar_t sep,
                                     fnx_wsubstr_t *result);

/*
 * Parses the substring ss into tokens, delimited by separators seps and inserts
 * them into tok_list. Inserts no more then max_sz tokens.
 *
 * Returns 0 if all tokens assigned to tok_list, or -1 in case of insufficient
 * space. If p_ntok is not NULL it is set to the number of parsed tokens.
 */
int fnx_wsubstr_tokenize(const fnx_wsubstr_t *ss, const wchar_t *seps,
                         fnx_wsubstr_t tok_list[], size_t list_size,
                         size_t *p_ntok);

int fnx_wsubstr_ntokenize(const fnx_wsubstr_t *ss,
                          const wchar_t *seps, size_t n,
                          fnx_wsubstr_t tok_list[], size_t list_size,
                          size_t *p_ntok);

int fnx_wsubstr_tokenize_chr(const fnx_wsubstr_t *ss, wchar_t sep,
                             fnx_wsubstr_t tok_list[], size_t list_size,
                             size_t *p_ntok);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Scans ss for the longest common prefix with s.
 */
size_t fnx_wsubstr_common_prefix(const fnx_wsubstr_t *ss, const wchar_t *str);
size_t fnx_wsubstr_ncommon_prefix(const fnx_wsubstr_t *ss,
                                  const wchar_t *s, size_t n);

/*
 * Return TRUE if the first character of ss equals c.
 */
int fnx_wsubstr_starts_with(const fnx_wsubstr_t *ss, wchar_t c);


/*
 * Scans ss backwards for the longest common suffix with s.
 */
size_t fnx_wsubstr_common_suffix(const fnx_wsubstr_t *ss,
                                 const wchar_t *s);
size_t fnx_wsubstr_ncommon_suffix(const fnx_wsubstr_t *ss,
                                  const wchar_t *s, size_t n);

/*
 * Return TRUE if the last character of ss equals c.
 */
int fnx_wsubstr_ends_with(const fnx_wsubstr_t *ss, wchar_t c);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Returns pointer to beginning of characters sequence.
 */
wchar_t *fnx_wsubstr_data(const fnx_wsubstr_t *ss);

/*
 * Assigns s, truncates result in case of insufficient room.
 */
void fnx_wsubstr_assign(fnx_wsubstr_t *ss, const wchar_t *s);
void fnx_wsubstr_nassign(fnx_wsubstr_t *ss, const wchar_t *s, size_t len);

/*
 * Assigns n copies of c.
 */
void fnx_wsubstr_assign_chr(fnx_wsubstr_t *ss, size_t n, wchar_t c);


/*
 * Appends s.
 */
void fnx_wsubstr_append(fnx_wsubstr_t *ss, const wchar_t *s);
void fnx_wsubstr_nappend(fnx_wsubstr_t *ss, const wchar_t *s, size_t len);

/*
 * Appends n copies of c.
 */
void fnx_wsubstr_append_chr(fnx_wsubstr_t *ss, size_t n, wchar_t c);

/*
 * Appends single char.
 */
void fnx_wsubstr_push_back(fnx_wsubstr_t *ss, wchar_t c);

/**
 * Inserts s before position pos.
 */
void fnx_wsubstr_insert(fnx_wsubstr_t *ss, size_t pos, const wchar_t *s);
void fnx_wsubstr_ninsert(fnx_wsubstr_t *ss,
                         size_t pos, const wchar_t *s, size_t len);

/*
 * Inserts n copies of c before position pos.
 */
void fnx_wsubstr_insert_chr(fnx_wsubstr_t *ss,
                            size_t pos, size_t n, wchar_t c);

/*
 * Replaces a part of sub-string with the string s.
 */
void fnx_wsubstr_replace(fnx_wsubstr_t *ss,
                         size_t pos, size_t n, const wchar_t *s);
void fnx_wsubstr_nreplace(fnx_wsubstr_t *ss,
                          size_t pos, size_t n, const wchar_t *s, size_t len);

/*
 * Replaces part of sub-string with n2 copies of c.
 */
void fnx_wsubstr_replace_chr(fnx_wsubstr_t *ss,
                             size_t pos, size_t n1, size_t n2, wchar_t c);


/*
 * Erases part of sub-string.
 */
void fnx_wsubstr_erase(fnx_wsubstr_t *ss, size_t pos, size_t n);

/*
 * Reverse the writable portion of sub-string.
 */
void fnx_wsubstr_reverse(fnx_wsubstr_t *ss);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Generic:
 */

/*
 * Returns the index of the first element of ss that satisfy the unary
 * predicate fn (or !fn), or npos if no such element exist.
 */
size_t fnx_wsubstr_find_if(const fnx_wsubstr_t *ss, fnx_wchr_pred fn);
size_t fnx_wsubstr_find_if_not(const fnx_wsubstr_t *ss, fnx_wchr_pred fn);


/*
 * Returns the index of the last element of ss that satisfy the unary
 * predicate fn (or !fn), or npos if no such element exist.
 */
size_t fnx_wsubstr_rfind_if(const fnx_wsubstr_t *ss, fnx_wchr_pred fn);
size_t fnx_wsubstr_rfind_if_not(const fnx_wsubstr_t *ss, fnx_wchr_pred fn);

/*
 * Returns the number of elements in ss that satisfy the unary predicate fn.
 */
size_t fnx_wsubstr_count_if(const fnx_wsubstr_t *ss, fnx_wchr_pred fn);

/*
 * Returns TRUE if all characters of ss satisfy unary predicate fn.
 */
int fnx_wsubstr_test_if(const fnx_wsubstr_t *ss, fnx_wchr_pred fn);


/*
 * Creates a substring of ss without leading characters that satisfy unary
 * predicate fn.
 */
void fnx_wsubstr_trim_if(const fnx_wsubstr_t *ss,
                         fnx_wchr_pred fn, fnx_wsubstr_t *result);

/*
 * Creates a substring of ss without trailing characters that satisfy unary
 * predicate fn.
 */
void fnx_wsubstr_chop_if(const fnx_wsubstr_t *ss,
                         fnx_wchr_pred fn, fnx_wsubstr_t *result);

/*
 * Creates a substring of ss without any leading and trailing characters that
 * satisfy unary predicate fn.
 */
void fnx_wsubstr_strip_if(const fnx_wsubstr_t *ss,
                          fnx_wchr_pred fn, fnx_wsubstr_t *result);


/*
 * Apply fn for every element in sub-string.
 */
void fnx_wsubstr_foreach(fnx_wsubstr_t *ss, fnx_wchr_modify_fn fn);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Returns TRUE if all characters of ss satisfy ctype predicate.
 */
int fnx_wsubstr_isalnum(const fnx_wsubstr_t *ss);
int fnx_wsubstr_isalpha(const fnx_wsubstr_t *ss);
int fnx_wsubstr_isascii(const fnx_wsubstr_t *ss);
int fnx_wsubstr_isblank(const fnx_wsubstr_t *ss);
int fnx_wsubstr_iscntrl(const fnx_wsubstr_t *ss);
int fnx_wsubstr_isdigit(const fnx_wsubstr_t *ss);
int fnx_wsubstr_isgraph(const fnx_wsubstr_t *ss);
int fnx_wsubstr_islower(const fnx_wsubstr_t *ss);
int fnx_wsubstr_isprint(const fnx_wsubstr_t *ss);
int fnx_wsubstr_ispunct(const fnx_wsubstr_t *ss);
int fnx_wsubstr_isspace(const fnx_wsubstr_t *ss);
int fnx_wsubstr_isupper(const fnx_wsubstr_t *ss);
int fnx_wsubstr_isxdigit(const fnx_wsubstr_t *ss);

/*
 * Case sensitive operations:
 */
void fnx_wsubstr_toupper(fnx_wsubstr_t *substr);
void fnx_wsubstr_tolower(fnx_wsubstr_t *substr);
void fnx_wsubstr_capitalize(fnx_wsubstr_t *substr);

#endif /* FNX_INFRA_WSUBSTR_H_ */


