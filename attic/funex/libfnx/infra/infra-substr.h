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
#ifndef FNX_INFRA_SUBSTR_H_
#define FNX_INFRA_SUBSTR_H_


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Sub-string: reference to characters-array. When nwr is zero, referring to
 * immutable (read-only) string. In all cases, never overlap writable region.
 * All possible dynamic-allocation must be made explicitly by the user.
 */
struct fnx_substr {
	char  *str; /* Beginning of characters-array (rd & wr)    */
	size_t len; /* Number of readable chars (string's length) */
	size_t nwr; /* Number of writable chars from beginning    */
};
typedef struct fnx_substr    fnx_substr_t;


struct fnx_substr_pair {
	fnx_substr_t first, second;
};
typedef struct fnx_substr_pair  fnx_substr_pair_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Returns the largest possible size a sub-string may have.
 */
size_t fnx_substr_max_size(void);

/*
 * "Not-a-pos" (synonym to fnx_substr_max_size())
 */
size_t fnx_substr_npos(void);


/*
 * Constructors:
 * The first two create read-only substrings, the next two creates a mutable
 * (for write) substring. The last one creates read-only empty string.
 */
void fnx_substr_init(fnx_substr_t *ss, const char *str);
void fnx_substr_init_rd(fnx_substr_t *ss, const char *s, size_t n);
void fnx_substr_init_rwa(fnx_substr_t *ss, char *);
void fnx_substr_init_rw(fnx_substr_t *ss, char *, size_t nrd, size_t nwr);
void fnx_substr_inits(fnx_substr_t *ss);

/*
 * Shallow-copy constructor (without deep copy).
 */
void fnx_substr_clone(const fnx_substr_t *ss, fnx_substr_t *other);

/*
 * Destructor: zero all
 */
void fnx_substr_destroy(fnx_substr_t *ss);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Returns the string's read-length. Synonym to ss->len.
 */
size_t fnx_substr_size(const fnx_substr_t *ss);

/*
 * Returns the writable-size of sub-string.
 */
size_t fnx_substr_wrsize(const fnx_substr_t *ss);

/*
 * Returns TRUE if sub-string's length is zero.
 */
int fnx_substr_isempty(const fnx_substr_t *ss);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Returns an iterator pointing to the beginning of the characters sequence.
 */
const char *fnx_substr_begin(const fnx_substr_t *ss);

/*
 * Returns an iterator pointing to the end of the characters sequence.
 */
const char *fnx_substr_end(const fnx_substr_t *ss);

/*
 * Returns the number of elements between begin() and p. If p is out-of-range,
 * returns npos.
 */
size_t fnx_substr_offset(const fnx_substr_t *ss, const char *p);

/*
 * Returns pointer to the n'th character. Performs out-of-range check:
 * panics in case n is out of range.
 */
const char *fnx_substr_at(const fnx_substr_t *substr, size_t n);

/*
 * Returns TRUE if ss->ss_str[i] is a valid substring-index (i < s->ss_len).
 */
int fnx_substr_isvalid_index(const fnx_substr_t *ss, size_t i);


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
size_t fnx_substr_copyto(const fnx_substr_t *ss, char *buf, size_t n);

/*
 * Three-way lexicographical comparison
 */
int fnx_substr_compare(const fnx_substr_t *ss, const char *s);
int fnx_substr_ncompare(const fnx_substr_t *ss, const char *s, size_t n);

/*
 * Returns TRUE in case of equal size and equal data
 */
int fnx_substr_isequal(const fnx_substr_t *ss, const char *s);
int fnx_substr_nisequal(const fnx_substr_t *ss, const char *s, size_t n);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Returns the number of (non-overlapping) occurrences of s (or c) as a
 * substring of ss.
 */
size_t fnx_substr_count(const fnx_substr_t *ss, const char *s);
size_t fnx_substr_ncount(const fnx_substr_t *ss,
                         const char *s, size_t n);
size_t fnx_substr_count_chr(const fnx_substr_t *ss, char c);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Searches ss, beginning at position pos, for the first occurrence of s (or
 * single-character c). If search fails, returns npos.
 */
size_t fnx_substr_find(const fnx_substr_t *ss, const char *str);
size_t fnx_substr_nfind(const fnx_substr_t *ss,
                        size_t pos, const char *s, size_t n);
size_t fnx_substr_find_chr(const fnx_substr_t *ss, size_t pos, char c);


/*
 * Searches ss backwards, beginning at position pos, for the last occurrence of
 * s (or single-character c). If search fails, returns npos.
 */
size_t fnx_substr_rfind(const fnx_substr_t *ss, const char *s);
size_t fnx_substr_nrfind(const fnx_substr_t *ss,
                         size_t pos, const char *s, size_t n);
size_t fnx_substr_rfind_chr(const fnx_substr_t *ss, size_t pos, char c);


/*
 * Searches ss, beginning at position pos, for the first character that is
 * equal to any one of the characters of s.
 */
size_t fnx_substr_find_first_of(const fnx_substr_t *ss, const char *s);
size_t fnx_substr_nfind_first_of(const fnx_substr_t *ss,
                                 size_t pos, const char *s, size_t n);


/*
 * Searches ss backwards, beginning at position pos, for the last character
 * that is equal to any of the characters of s.
 */
size_t fnx_substr_find_last_of(const fnx_substr_t *ss, const char *s);
size_t fnx_substr_nfind_last_of(const fnx_substr_t *ss,
                                size_t pos, const char *s, size_t n);


/*
 * Searches ss, beginning at position pos, for the first character that is not
 * equal to any of the characters of s.
 */
size_t fnx_substr_find_first_not_of(const fnx_substr_t *ss,
                                    const char *s);
size_t fnx_substr_nfind_first_not_of(const fnx_substr_t *ss,
                                     size_t pos, const char *s, size_t n);
size_t fnx_substr_find_first_not(const fnx_substr_t *ss,
                                 size_t pos, char c);



/*
 * Searches ss backwards, beginning at position pos, for the last character
 * that is not equal to any of the characters of s (or c).
 */
size_t fnx_substr_find_last_not_of(const fnx_substr_t *ss,
                                   const char *s);
size_t fnx_substr_nfind_last_not_of(const fnx_substr_t *ss,
                                    size_t pos, const char *s, size_t n);
size_t fnx_substr_find_last_not(const fnx_substr_t *ss,
                                size_t pos, char c);



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Creates a substring of ss, which refers to n characters after position i.
 * If i is an invalid index, the result substring is empty. If there are less
 * then n characters after position i, the result substring will refer only to
 * the elements which are members of ss.
 */
void fnx_substr_sub(const fnx_substr_t *ss,
                    size_t i, size_t n,  fnx_substr_t *result);

/*
 * Creates a substring of ss, which refers to the last n chars. The result
 * substring will not refer to more then ss->ss_len elements.
 */
void fnx_substr_rsub(const fnx_substr_t *ss,
                     size_t n, fnx_substr_t *result);

/*
 * Creates a substring with all the characters that are in the range of
 * both s1 and s2. That is, all elements within the range
 * [s1.begin(),s1.end()) which are also in the range
 * [s2.begin(), s2.end()) (i.e. have the same address).
 */
void fnx_substr_intersection(const fnx_substr_t *s1,
                             const fnx_substr_t *s2,
                             fnx_substr_t *result);


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
void fnx_substr_split(const fnx_substr_t *ss,
                      const char *seps, fnx_substr_pair_t *);

void fnx_substr_nsplit(const fnx_substr_t *ss,
                       const char *seps, size_t n, fnx_substr_pair_t *);

void fnx_substr_split_chr(const fnx_substr_t *ss, char sep,
                          fnx_substr_pair_t *result);



/*
 * Creates a pair of substrings of ss, which are divided by any of the first n
 * characters of seps, while searching ss backwards. If none of the characters
 * of seps is in ss, the first element of the pair equal to ss and the second
 * element is an empty substring.
 */
void fnx_substr_rsplit(const fnx_substr_t *ss,
                       const char *seps, fnx_substr_pair_t *result);

void fnx_substr_nrsplit(const fnx_substr_t *ss,
                        const char *seps, size_t, fnx_substr_pair_t *);

void fnx_substr_rsplit_chr(const fnx_substr_t *ss, char sep,
                           fnx_substr_pair_t *result);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Creates a substring of ss without the first n leading characters.
 */
void fnx_substr_trim(const fnx_substr_t *ss, size_t n, fnx_substr_t *);


/*
 * Creates a substring of ss without any leading characters which are members
 * of set.
 */
void fnx_substr_trim_any_of(const fnx_substr_t *ss, const char *set,
                            fnx_substr_t *result);

void fnx_substr_ntrim_any_of(const fnx_substr_t *ss,
                             const char *set, size_t n, fnx_substr_t *);

void fnx_substr_trim_chr(const fnx_substr_t *substr, char c,
                         fnx_substr_t *result);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Creates a substring of ss without the last n trailing characters.
 * If n >= ss->ss_len the result substring is empty.
 */
void fnx_substr_chop(const fnx_substr_t *ss, size_t n, fnx_substr_t *);

/*
 * Creates a substring of ss without any trailing characters which are members
 * of set.
 */
void fnx_substr_chop_any_of(const fnx_substr_t *ss,
                            const char *set, fnx_substr_t *result);

void fnx_substr_nchop_any_of(const fnx_substr_t *ss,
                             const char *set, size_t n, fnx_substr_t *);

/*
 * Creates a substring of ss without any trailing characters that equals c.
 */
void fnx_substr_chop_chr(const fnx_substr_t *ss, char c, fnx_substr_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Creates a substring of ss without any leading and trailing characters which
 * are members of set.
 */
void fnx_substr_strip_any_of(const fnx_substr_t *ss,
                             const char *set, fnx_substr_t *result);

void fnx_substr_nstrip_any_of(const fnx_substr_t *ss,
                              const char *set, size_t n,
                              fnx_substr_t *result);

/*
 * Creates a substring of substr without any leading and trailing
 * characters which are equal to c.
 */
void fnx_substr_strip_chr(const fnx_substr_t *ss, char c,
                          fnx_substr_t *result);


/*
 * Strip white-spaces
 */
void fnx_substr_strip_ws(const fnx_substr_t *ss, fnx_substr_t *result);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Finds the first substring of ss that is a token delimited by any of the
 * characters of sep(s).
 */
void fnx_substr_find_token(const fnx_substr_t *ss,
                           const char *seps, fnx_substr_t *result);

void fnx_substr_nfind_token(const fnx_substr_t *ss,
                            const char *seps, size_t n,
                            fnx_substr_t *result);

void fnx_substr_find_token_chr(const fnx_substr_t *ss, char sep,
                               fnx_substr_t *result);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Finds the next token in ss after tok, which is delimeted by any of the
 * characters of sep(s).
 */

void fnx_substr_find_next_token(const fnx_substr_t *ss,
                                const fnx_substr_t *tok,
                                const char *seps,
                                fnx_substr_t *result);

void fnx_substr_nfind_next_token(const fnx_substr_t *ss,
                                 const fnx_substr_t *tok,
                                 const char *seps, size_t n,
                                 fnx_substr_t *result);

void fnx_substr_find_next_token_chr(const fnx_substr_t *substr,
                                    const fnx_substr_t *tok, char sep,
                                    fnx_substr_t *result);

/*
 * Parses the substring ss into tokens, delimited by separators seps and inserts
 * them into tok_list. Inserts no more then max_sz tokens.
 *
 * Returns 0 if all tokens assigned to tok_list, or -1 in case of insufficient
 * space. If p_ntok is not NULL it is set to the number of parsed tokens.
 */
int fnx_substr_tokenize(const fnx_substr_t *ss, const char *seps,
                        fnx_substr_t tok_list[], size_t list_size,
                        size_t *p_ntok);

int fnx_substr_ntokenize(const fnx_substr_t *ss,
                         const char *seps, size_t n,
                         fnx_substr_t tok_list[], size_t list_size,
                         size_t *p_ntok);

int fnx_substr_tokenize_chr(const fnx_substr_t *ss, char sep,
                            fnx_substr_t tok_list[], size_t list_size,
                            size_t *p_ntok);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Scans ss for the longest common prefix with s.
 */
size_t fnx_substr_common_prefix(const fnx_substr_t *ss, const char *str);
size_t fnx_substr_ncommon_prefix(const fnx_substr_t *ss,
                                 const char *s, size_t n);

/*
 * Return TRUE if the first character of ss equals c.
 */
int fnx_substr_starts_with(const fnx_substr_t *ss, char c);


/*
 * Scans ss backwards for the longest common suffix with s.
 */
size_t fnx_substr_common_suffix(const fnx_substr_t *ss,
                                const char *s);
size_t fnx_substr_ncommon_suffix(const fnx_substr_t *ss,
                                 const char *s, size_t n);

/*
 * Return TRUE if the last character of ss equals c.
 */
int fnx_substr_ends_with(const fnx_substr_t *ss, char c);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 *  Returns pointer to beginning of characters sequence.
 */
char *fnx_substr_data(const fnx_substr_t *ss);

/*
 * Assigns s, truncates result in case of insufficient room.
 */
void fnx_substr_assign(fnx_substr_t *ss, const char *s);
void fnx_substr_nassign(fnx_substr_t *ss, const char *s, size_t len);

/*
 * Assigns n copies of c.
 */
void fnx_substr_assign_chr(fnx_substr_t *ss, size_t n, char c);


/*
 * Appends s.
 */
void fnx_substr_append(fnx_substr_t *ss, const char *s);
void fnx_substr_nappend(fnx_substr_t *ss, const char *s, size_t len);

/*
 * Appends n copies of c.
 */
void fnx_substr_append_chr(fnx_substr_t *ss, size_t n, char c);

/*
 * Appends single char.
 */
void fnx_substr_push_back(fnx_substr_t *ss, char c);

/*
 * Inserts s before position pos.
 */
void fnx_substr_insert(fnx_substr_t *ss, size_t pos, const char *s);
void fnx_substr_ninsert(fnx_substr_t *ss,
                        size_t pos, const char *s, size_t len);

/*
 * Inserts n copies of c before position pos.
 */
void fnx_substr_insert_chr(fnx_substr_t *ss,
                           size_t pos, size_t n, char c);

/*
 * Replaces a part of sub-string with the string s.
 */
void fnx_substr_replace(fnx_substr_t *ss,
                        size_t pos, size_t n, const char *s);
void fnx_substr_nreplace(fnx_substr_t *ss,
                         size_t pos, size_t n, const char *s, size_t len);

/*
 * Replaces part of sub-string with n2 copies of c.
 */
void fnx_substr_replace_chr(fnx_substr_t *ss,
                            size_t pos, size_t n1, size_t n2, char c);


/*
 * Erases part of sub-string.
 */
void fnx_substr_erase(fnx_substr_t *ss, size_t pos, size_t n);

/*
 * Reverse the writable portion of sub-string.
 */
void fnx_substr_reverse(fnx_substr_t *ss);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Generic:
 */

/*
 * Returns the index of the first element of ss that satisfy the unary
 * predicate fn (or !fn), or npos if no such element exist.
 */
size_t fnx_substr_find_if(const fnx_substr_t *ss, fnx_chr_pred fn);
size_t fnx_substr_find_if_not(const fnx_substr_t *ss, fnx_chr_pred fn);


/*
 * Returns the index of the last element of ss that satisfy the unary
 * predicate fn (or !fn), or npos if no such element exist.
 */
size_t fnx_substr_rfind_if(const fnx_substr_t *ss, fnx_chr_pred fn);
size_t fnx_substr_rfind_if_not(const fnx_substr_t *ss, fnx_chr_pred fn);

/*
 * Returns the number of elements in ss that satisfy the unary predicate fn.
 */
size_t fnx_substr_count_if(const fnx_substr_t *ss, fnx_chr_pred fn);

/*
 * Returns TRUE if all characters of ss satisfy unary predicate fn.
 */
int fnx_substr_test_if(const fnx_substr_t *ss, fnx_chr_pred fn);


/*
 * Creates a substring of ss without leading characters that satisfy unary
 * predicate fn.
 */
void fnx_substr_trim_if(const fnx_substr_t *ss,
                        fnx_chr_pred fn, fnx_substr_t *result);

/*
 * Creates a substring of ss without trailing characters that satisfy unary
 * predicate fn.
 */
void fnx_substr_chop_if(const fnx_substr_t *ss,
                        fnx_chr_pred fn, fnx_substr_t *result);

/*
 * Creates a substring of ss without any leading and trailing characters that
 * satisfy unary predicate fn.
 */
void fnx_substr_strip_if(const fnx_substr_t *ss,
                         fnx_chr_pred fn, fnx_substr_t *result);


/*
 * Apply fn for every element in sub-string.
 */
void fnx_substr_foreach(fnx_substr_t *ss, fnx_chr_modify_fn fn);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Returns TRUE if all characters of ss satisfy ctype predicate.
 */
int fnx_substr_isalnum(const fnx_substr_t *ss);
int fnx_substr_isalpha(const fnx_substr_t *ss);
int fnx_substr_isascii(const fnx_substr_t *ss);
int fnx_substr_isblank(const fnx_substr_t *ss);
int fnx_substr_iscntrl(const fnx_substr_t *ss);
int fnx_substr_isdigit(const fnx_substr_t *ss);
int fnx_substr_isgraph(const fnx_substr_t *ss);
int fnx_substr_islower(const fnx_substr_t *ss);
int fnx_substr_isprint(const fnx_substr_t *ss);
int fnx_substr_ispunct(const fnx_substr_t *ss);
int fnx_substr_isspace(const fnx_substr_t *ss);
int fnx_substr_isupper(const fnx_substr_t *ss);
int fnx_substr_isxdigit(const fnx_substr_t *ss);

/*
 * Case sensitive operations:
 */
void fnx_substr_toupper(fnx_substr_t *substr);
void fnx_substr_tolower(fnx_substr_t *substr);
void fnx_substr_capitalize(fnx_substr_t *substr);

#endif /* FNX_INFRA_SUBSTR_H_ */


