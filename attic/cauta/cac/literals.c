/*
 * CaC: Cauta-to-C Compiler
 *
 * Copyright (C) 2016,2017,2018 Shachar Sharon
 *
 * CaC is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CaC is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CaC. If not, see <https://www.gnu.org/licenses/gpl>.
 */
#include "lexer.h"

static bool ismember(const char *str, char chr)
{
	return (strchr(str, chr) != NULL);
}

static char getchr(const token_t *tok)
{
	return ((tok && tok->str) ? tok->str->str[0] : '\0');
}

static bool isslash(const token_t *tok)
{
	return token_ischar(tok, '/');
}

static bool isstar(const token_t *tok)
{
	return token_ischar(tok, '*');
}

static bool isnewline(const token_t *tok)
{
	return token_ischar(tok, '\n');
}

static bool isbackslash(const token_t *tok)
{
	return token_ischar(tok, '\\');
}

static bool isdquot(const token_t *tok)
{
	return token_ischar(tok, '"');
}

static bool isquot(const token_t *tok)
{
	return token_ischar(tok, '\'');
}

static bool iszero(const token_t *tok)
{
	return token_ischar(tok, '0');
}

static bool isxhex(const token_t *tok)
{
	return token_ischar(tok, 'x') || token_ischar(tok, 'X');
}

static bool isbbin(const token_t *tok)
{
	return token_ischar(tok, 'b') || token_ischar(tok, 'B');
}

static bool isdot(const token_t *tok)
{
	return token_ischar(tok, '.');
}

static bool isexpind(const token_t *tok)
{
	return token_ischar(tok, 'e') || token_ischar(tok, 'E');
}

static bool isexpsign(const token_t *tok)
{
	return token_ischar(tok, '+') || token_ischar(tok, '-');
}

static bool islsuffix(const token_t *tok)
{
	return token_ischar(tok, 'l') || token_ischar(tok, 'L');
}

static bool isfsuffix(const token_t *tok)
{
	return token_ischar(tok, 'f') ||
	       token_ischar(tok, 'F') || islsuffix(tok);
}

static bool ischarofs(const token_t *tok, const char *set)
{
	return (token_isatom(tok) && ismember(set, getchr(tok)));
}

static bool isbinary(const token_t *tok)
{
	return ischarofs(tok, binary_digits);
}

static bool isdecimal(const token_t *tok)
{
	return ischarofs(tok, decimal_digits);
}

static bool isoctal(const token_t *tok)
{
	return ischarofs(tok, octal_digits);
}

static bool ishexadec(const token_t *tok)
{
	return ischarofs(tok, hexadecimal_digits);
}

static bool isidentchr(const token_t *tok)
{
	return ischarofs(tok, identifier_charset);
}

static bool isprenum(const token_t *tok)
{
	return (token_istype(tok, TOK_WS) ||
	        token_istype(tok, TOK_LPAREN) ||
	        token_istype(tok, TOK_LBRACK) ||
	        token_istype(tok, TOK_COMMA) ||
	        token_istype(tok, TOK_SEMI));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void require_atom(const token_t *tok)
{
	if (tok && !token_isatom(tok)) {
		compile_error(tok, "not an atom");
	}
}

static void
require_atoms(const token_t *tok1, const token_t *tok2, const token_t *tok3)
{
	require_atom(tok1);
	require_atom(tok2);
	require_atom(tok3);
}

static void require_validlen(const token_t *tok, size_t min_len, size_t max_len)
{
	const string_t *str = tok->str;

	if ((str->len < min_len) || (max_len < str->len)) {
		compile_error(tok, "illegal numeral: %s", str->str);
	}
}

static token_t *dup_int_literal(const token_t *base_tok, const string_t *str,
                                size_t min_len, size_t max_len)
{
	token_t *tok;

	tok = token_dup3(base_tok, TOK_INT_LITERAL, str);
	require_validlen(tok, min_len, max_len);
	return tok;
}


/*
 * Scans double-quoted string
 */
static token_t *scan_double_quoted_string(const tokseq_t *tseq, size_t *pos)
{
	bool term = false;
	token_t *tok, *nxt;
	tokseq_t *dq_tseq;
	const size_t len = tokseq_len(tseq);

	dq_tseq = tokseq_new(len);
	tok = tokseq_at(tseq, (*pos)++);
	tokseq_append(dq_tseq, tok);
	while ((*pos < len) && !term) {
		tok = tokseq_at(tseq, *pos);
		nxt = tokseq_at(tseq, *pos + 1);
		require_atoms(tok, nxt, NULL);

		if (isbackslash(tok)) {
			*pos += 2;
			tokseq_append2(dq_tseq, tok, nxt);
		} else {
			tok = tokseq_at(tseq, (*pos)++);
			tokseq_append(dq_tseq, tok);
			term = isdquot(tok);
		}
	}
	if (!term) {
		tok = tokseq_front(tseq);
		compile_error(tok, "unterminated double-quoted string");
	}
	nxt = tokseq_at(tseq, *pos);
	if (isdquot(nxt)) {
		compile_error(nxt, "duplicated double-quoted string");
	}
	return tokseq_meld(dq_tseq, TOK_DQ_STRING);
}

static token_t *scan_single_quoted_string(const tokseq_t *tseq, size_t *pos)
{
	bool term = false;
	token_t *tok, *nxt;
	tokseq_t *sq_tseq;
	const size_t len = tokseq_len(tseq);

	sq_tseq = tokseq_new(len);
	tok = tokseq_at(tseq, (*pos)++);
	tokseq_append(sq_tseq, tok);
	while ((*pos < len) && !term) {
		tok = tokseq_at(tseq, *pos);
		nxt = tokseq_at(tseq, *pos + 1);
		require_atoms(tok, nxt, NULL);

		if (isbackslash(tok)) {
			*pos += 2;
			tokseq_append2(sq_tseq, tok, nxt);
		} else {
			tok = tokseq_at(tseq, (*pos)++);
			tokseq_append(sq_tseq, tok);
			term = isquot(tok);
		}
	}
	tok = tokseq_meld(sq_tseq, TOK_SQ_STRING);
	if (!term) {
		compile_error(tok, "unterminated single quoted string");
	}
	nxt = tokseq_at(tseq, *pos);
	if (isquot(nxt)) {
		compile_error(nxt, "duplicated quotes");
	}
	return tok;
}

static token_t *scan_triple_quoted_string(const tokseq_t *tseq, size_t *pos)
{
	bool term = false;
	token_t *tok, *nxt, *trd;
	tokseq_t *triq_tseq;
	const size_t len = tokseq_len(tseq);

	triq_tseq = tokseq_new(len);
	tok = tokseq_at(tseq, (*pos)++);
	nxt = tokseq_at(tseq, (*pos)++);
	trd = tokseq_at(tseq, (*pos)++);
	tokseq_append3(triq_tseq, tok, nxt, trd);
	while ((*pos < len) && !term) {
		tok = tokseq_at(tseq, *pos);
		nxt = tokseq_at(tseq, *pos + 1);
		trd = tokseq_at(tseq, *pos + 2);
		require_atoms(tok, nxt, trd);

		if (isbackslash(tok)) {
			*pos += 2;
			tokseq_append2(triq_tseq, tok, nxt);
		} else if (isquot(tok) && isquot(nxt) && isquot(trd)) {
			*pos += 3;
			tokseq_append3(triq_tseq, tok, nxt, trd);
			term = true;
		} else {
			*pos += 1;
			tokseq_append(triq_tseq, tok);
		}
	}
	if (!term) {
		tok = tokseq_front(triq_tseq);
		compile_error(tok, "unterminated triple quoted string");
	}
	nxt = tokseq_at(tseq, *pos);
	if (isquot(nxt)) {
		compile_error(nxt, "duplicated quotes");
	}
	return tokseq_meld(triq_tseq, TOK_TQ_STRING);
}

tokseq_t *scan_string_literals(const tokseq_t *tseq)
{
	size_t pos = 0;
	tokseq_t *next_tseq;
	token_t *tok, *nxt, *trd;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	while (pos < len) {
		tok = tokseq_at(tseq, pos);
		nxt = tokseq_at(tseq, pos + 1);
		trd = tokseq_at(tseq, pos + 2);
		if (isquot(tok) && isquot(nxt) && isquot(trd)) {
			/* Triple-quoted string */
			tok = scan_triple_quoted_string(tseq, &pos);
		} else if (isdquot(tok)) {
			/* Double-quoted string */
			tok = scan_double_quoted_string(tseq, &pos);
		} else if (isquot(tok)) {
			/* Single-quoted string */
			tok = scan_single_quoted_string(tseq, &pos);
		} else {
			pos += 1;
		}
		tokseq_append(next_tseq, tok);
	}
	return next_tseq;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static token_t *scan_hex_literal(const tokseq_t *tseq, size_t *pos)
{
	int lsf = 0, hex = 1;
	string_t *str;
	token_t *tok, *tkz, *tkx;
	const size_t len = tokseq_len(tseq);

	tkz = tokseq_at(tseq, (*pos)++);
	tkx = tokseq_at(tseq, (*pos)++);
	str = string_join(tkz->str, tkx->str);
	while ((*pos < len) && hex && !lsf) {
		tok = tokseq_at(tseq, *pos);
		hex = ishexadec(tok);
		lsf = islsuffix(tok);
		if (hex || lsf) {
			str = string_join(str, tok->str);
			*pos += 1;
		}
	}
	return dup_int_literal(tkz, str, 3, 64);
}

static token_t *
scan_bin_literal(const tokseq_t *tseq, size_t *pos)
{
	int lsf = 0, bin = 1;
	string_t *str;
	token_t *tkz, *tkb, *tok = NULL;
	const size_t len = tokseq_len(tseq);

	tkz = tokseq_at(tseq, (*pos)++);
	tkb = tokseq_at(tseq, (*pos)++);
	str = string_join(tkz->str, tkb->str);
	while ((*pos < len) && bin && !lsf) {
		tok = tokseq_at(tseq, *pos);
		bin = isbinary(tok);
		lsf = islsuffix(tok);
		if (bin || lsf) {
			str = string_join(str, tok->str);
			*pos += 1;
		}
	}
	return dup_int_literal(tkz, str, 3, 64);
}

static token_t *scan_oct_literal(const tokseq_t *tseq, size_t *pos)
{
	int lsf = 0, oct = 1;
	string_t *str;
	token_t *tko, *tok;
	const size_t len = tokseq_len(tseq);

	tko = tokseq_at(tseq, (*pos)++);
	str = string_dup(tko->str);
	while ((*pos < len) && oct && !lsf) {
		tok = tokseq_at(tseq, *pos);
		oct = isoctal(tok);
		lsf = islsuffix(tok);
		if (oct || lsf) {
			str = string_join(str, tok->str);
			*pos += 1;
		}
	}
	return dup_int_literal(tko, str, 2, 64);
}

static token_t *scan_dec_literal(const tokseq_t *tseq, size_t *pos)
{
	int lsf = 0, dec = 1;
	string_t *str;
	token_t *tkd, *tok = NULL;
	const size_t len = tokseq_len(tseq);

	tkd = tokseq_at(tseq, *pos);
	str = string_new(NULL);
	while ((*pos < len) && dec && !lsf) {
		tok = tokseq_at(tseq, *pos);
		dec = isdecimal(tok);
		lsf = islsuffix(tok);
		if (dec || lsf) {
			str = string_join(str, tok->str);
			*pos += 1;
		}
	}
	return dup_int_literal(tkd, str, 1, 64);
}

static token_t *scan_zero_literal(const tokseq_t *tseq, size_t *pos)
{
	token_t *num;
	const token_t *tok;
	const token_t *nxt;

	tok = tokseq_at(tseq, (*pos)++);
	nxt = tokseq_at(tseq, *pos);
	if (ishexadec(nxt)) {
		compile_error(tok, "illegal zero numeral");
	}
	num = token_dup2(tok, TOK_INT_LITERAL);
	num->num = tok->str;
	return num;
}

static token_t *scan_int_literal(const tokseq_t *tseq, size_t *pos)
{
	token_t *tok, *nxt;

	tok = tokseq_at(tseq, *pos);
	nxt = tokseq_at(tseq, *pos + 1);
	if (!iszero(tok)) {
		tok = scan_dec_literal(tseq, pos);
	} else {
		if (isbbin(nxt)) {
			tok = scan_bin_literal(tseq, pos);
		} else if (isxhex(nxt)) {
			tok = scan_hex_literal(tseq, pos);
		} else if (isoctal(nxt)) {
			tok = scan_oct_literal(tseq, pos);
		} else {
			tok = scan_zero_literal(tseq, pos);
		}
	}
	nxt = tokseq_at(tseq, *pos + 1);
	if (ishexadec(nxt)) {
		compile_error(nxt, "illegal numeral here");
	}
	return tok;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * View forward into tokens stream which begins with decimal digit, check if
 * it has a trailing dot, exponent-part or float-type-suffix, which implies
 * that this is the beginning of floating-point-literal.
 */
static bool isfpseq(const tokseq_t *tseq, size_t beg)
{
	size_t pos = beg;
	const token_t *tok = NULL;
	const size_t len = tokseq_len(tseq);

	while (pos < len) {
		tok = tokseq_at(tseq, pos);
		if (!isdecimal(tok)) {
			break;
		}
		++pos;
	}
	return (pos > beg) && (isdot(tok) || isexpind(tok) || isfsuffix(tok));
}

static token_t *try_scan_dot(const tokseq_t *tseq, size_t *pos)
{
	token_t *tok;

	tok = tokseq_at(tseq, *pos);
	if (isdot(tok)) {
		*pos += 1;
	} else {
		tok = NULL;
	}
	return tok;
}

static token_t *try_scan_expind(const tokseq_t *tseq, size_t *pos)
{
	token_t *tok;

	tok = tokseq_at(tseq, *pos);
	if (isexpind(tok)) {
		*pos += 1;
	} else {
		tok = NULL;
	}
	return tok;
}

static token_t *try_scan_expsign(const tokseq_t *tseq, size_t *pos)
{
	token_t *tok;

	tok = tokseq_at(tseq, *pos);
	if (isexpsign(tok)) {
		*pos += 1;
	} else {
		tok = NULL;
	}
	return tok;
}

static token_t *try_scan_lsuffix(const tokseq_t *tseq, size_t *pos)
{
	token_t *tok;

	tok = tokseq_at(tseq, *pos);
	if (islsuffix(tok)) {
		*pos += 1;
	} else {
		tok = NULL;
	}
	return tok;
}

static token_t *
scan_decimal_floating_point_literal(const tokseq_t *tseq, size_t *pos)
{
	token_t *tok;
	token_t *fpt;
	string_t *str;
	toktype_e toktype;

	fpt = tokseq_at(tseq, *pos);
	tok = scan_dec_literal(tseq, pos);
	str = string_dup(tok->str);
	tok = try_scan_dot(tseq, pos);
	if (tok != NULL) {
		str = string_join(str, tok->str);
		tok = scan_dec_literal(tseq, pos);
		str = string_join(str, tok->str);
	}
	tok = try_scan_expind(tseq, pos);
	if (tok != NULL) {
		str = string_join(str, tok->str);
		tok = try_scan_expsign(tseq, pos);
		if (tok != NULL) {
			str = string_join(str, tok->str);
		}
		tok = scan_dec_literal(tseq, pos);
		str = string_join(str, tok->str);
	}
	tok = try_scan_lsuffix(tseq, pos);
	if (tok != NULL) {
		toktype = TOK_LFP_LITERAL;
		str = string_join(str, tok->str);
	} else {
		toktype = TOK_FP_LITERAL;
	}
	return token_dup3(fpt, toktype, str);
}

static token_t *scan_fp_literal(const tokseq_t *tseq, size_t *pos)
{
	token_t *nxt, *tok = NULL;

	tok = scan_decimal_floating_point_literal(tseq, pos);
	nxt = tokseq_at(tseq, *pos);
	if (isidentchr(tok)) {
		compile_error(nxt, "bad floating point literla");
	}
	return tok;
}

tokseq_t *scan_numaric_literals(const tokseq_t *tseq)
{
	size_t pos = 0;
	token_t *tok;
	tokseq_t *next_tseq;
	const token_t *prv = NULL;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	while (pos < len) {
		tok = tokseq_at(tseq, pos);
		if (isprenum(prv) && isdecimal(tok)) {
			if (isfpseq(tseq, pos)) {
				tok = scan_fp_literal(tseq, &pos);
			} else {
				tok = scan_int_literal(tseq, &pos);
			}
			prv = NULL;
		} else {
			pos += 1;
			prv = tok;
		}
		tokseq_append(next_tseq, tok);
	}
	return next_tseq;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static token_t *scan_sl_comment(const tokseq_t *tseq, size_t *pos)
{
	bool nl = false;
	token_t *tok;
	tokseq_t *sl_comment_tseq;
	const size_t len = tokseq_len(tseq);

	sl_comment_tseq = tokseq_new(len);
	while (*pos < len) {
		tok = tokseq_at(tseq, *pos);
		if (!token_isatom(tok)) {
			compile_error(tok, "not-an-atom");
		}
		if ((nl = isnewline(tok)) == true) {
			break;
		}
		tokseq_append(sl_comment_tseq, tok);
		*pos += 1;
	}
	tok = tokseq_meld(sl_comment_tseq, TOK_SL_COMMENT);
	if (!nl) {
		compile_error(tok, "missing new-line after comment");
	}
	return tok;
}

static token_t *scan_ml_comment(const tokseq_t *tseq, size_t *pos)
{
	bool term = false;
	token_t *nxt, *tok = NULL;
	tokseq_t *ml_comment_tseq;
	const size_t len = tokseq_len(tseq);

	ml_comment_tseq = tokseq_new(len);
	while ((*pos < len) && !term) {
		tok = tokseq_at(tseq, *pos);
		nxt = tokseq_at(tseq, *pos + 1);
		if (!token_isatom(tok) || !token_isatom(nxt)) {
			compile_error(tok, "not-an-atom");
		}
		if (isstar(tok) && isslash(nxt)) {
			*pos += 2;
			term = true;
		} else {
			*pos += 1;
			tokseq_append(ml_comment_tseq, tok);
		}
	}
	tok = tokseq_meld(ml_comment_tseq, TOK_ML_COMMENT);
	if (!term) {
		compile_error(tok, "malformed multi line comment");
	}
	return tok;
}

tokseq_t *scan_comments(const tokseq_t *tseq)
{
	int strmode = 0;
	size_t pos = 0;
	tokseq_t *next_tseq;
	token_t *tok, *nxt;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	while (pos < len) {
		tok = tokseq_at(tseq, pos);
		nxt = tokseq_at(tseq, pos + 1);

		if (strmode) {
			tokseq_append(next_tseq, tok);
			pos += 1;
			if (isbackslash(tok)) {
				tokseq_append(next_tseq, nxt);
				pos += 1;
			} else if ((strmode == 1) && isquot(tok)) {
				strmode = 0;
			} else if ((strmode == 2) && isdquot(tok)) {
				strmode = 0;
			}
		} else {
			if (isslash(tok) && isslash(nxt)) {
				pos += 2;
				tok = scan_sl_comment(tseq, &pos);
			} else if (isslash(tok) && isstar(nxt)) {
				pos += 2;
				tok = scan_ml_comment(tseq, &pos);
			} else {
				if (isquot(tok)) {
					strmode = 1;
				} else if (isdquot(tok)) {
					strmode = 2;
				}
				pos += 1;
			}
			tokseq_append(next_tseq, tok);
		}
	}
	return next_tseq;
}


