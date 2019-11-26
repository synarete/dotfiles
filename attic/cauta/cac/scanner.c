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
#include <string.h>
#include <ctype.h>
#include "lexer.h"

/* Maximum number of nested brackets */
#define CAC_LIMIT_NESTED_BRACKETS_MAX (64)

/* Maximum length of single identifier */
#define CAC_LIMIT_IDENTIFIER_LEN_MAX  (255)


#if 0
#define U_MULTIPLY_SIGN     (0x00D7)
#define U_ASTERISK          (0x002A)
#define U_DIVIDE_SIGN       (0x00F7)
#define U_SLASH             (0x002F)
#endif

static char getchr0(const token_t *tok)
{
	return tok->str->str[0];
}

static bool iswhite(const token_t *tok)
{
	return (token_isatom(tok) && isspace(getchr0(tok)));
}

static bool isstring(const token_t *tok)
{
	return (token_istype(tok, TOK_SQ_STRING) ||
	        token_istype(tok, TOK_DQ_STRING) ||
	        token_istype(tok, TOK_TQ_STRING));
}

static bool iswspaces(const token_t *tok)
{
	return token_istype(tok, TOK_WS);
}

static bool isidentc(const token_t *tok, char c)
{
	const char s[2] = { c, '\0' };
	return (token_istype(tok, TOK_IDENT) && string_isequals(tok->str, s));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Require all atoms to be valid members of basic source character set.
 */
static void verify_charset(const tokseq_t *tseq)
{
	const token_t *tok;
	const char *chset = basic_charset;
	const size_t len = tokseq_len(tseq);

	for (size_t i = 0; i < len; ++i) {
		tok = tokseq_at(tseq, i);
		if (token_isatom(tok)) {
			if (strchr(chset, getchr0(tok)) == NULL) {
				compile_error(tok, "illegal character");
			}
		}
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Concatenate adjacent string-literal tokens (same type), which are delimited
 * by zero or more white spaces.
 */
static void
require_equal_adjacent_strtype(const token_t *tok1, const token_t *tok2)
{
	if (tok1->type != tok2->type) {
		compile_error(tok2, "adjacent strings with different types");
	}
}

static tokseq_t *concat_adjacent_string_literals(tokseq_t *tseq)
{
	tokseq_t *next_tseq;
	token_t *tok, *nxt, *trd;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	while (!tokseq_isempty(tseq)) {
		tok = tokseq_pop_front(tseq);
		if (!isstring(tok)) {
			tokseq_append(next_tseq, tok);
			continue;
		}
		nxt = tokseq_pop_front(tseq);
		if (nxt == NULL) {
			tokseq_append(next_tseq, tok);
			break;
		}
		if (isstring(nxt)) {
			require_equal_adjacent_strtype(tok, nxt);
			tok = token_join(tok, nxt);
			tokseq_push_front(tseq, tok); /* Restore */
			continue;
		}
		trd = tokseq_pop_front(tseq);
		if (trd == NULL) {
			tokseq_append2(next_tseq, tok, nxt);
			break;
		}
		if (iswspaces(nxt) && isstring(trd)) {
			require_equal_adjacent_strtype(tok, trd);
			tok = token_join(tok, trd);
			tokseq_push_front(tseq, tok); /* Restore */
			continue;
		}
		tokseq_append3(next_tseq, tok, nxt, trd);
	}
	return next_tseq;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Transforms multiple space-characters into double white-space tokens and pad
 * the entire sequence with leading and trailing pseudo whites.
 */
static void pad_ws_tokens(tokseq_t *tseq)
{
	token_t *tok, *pad;
	string_t *ws = string_new(" ");

	tok = tokseq_front(tseq);
	pad = token_new(TOK_WS);
	pad->str = ws;
	pad->pos = 0;
	pad->lno = 0;
	pad->off = 0;
	pad->mod = tok->mod;
	tokseq_push_front(tseq, pad);

	tok = tokseq_back(tseq);
	pad = token_new(TOK_WS);
	pad->str = ws;
	pad->pos = tok->pos + 1;
	pad->lno = tok->lno;
	pad->off = tok->off + 1;
	pad->mod = tok->mod;
	tokseq_push_back(tseq, pad);
}

static tokseq_t *scan_ws_tokens(const tokseq_t *tseq)
{
	bool skip_ws = false;
	token_t *tok;
	tokseq_t *new_tsek;
	const size_t len = tokseq_len(tseq);

	new_tsek = tokseq_new(len);
	for (size_t pos = 0; pos < len; ++pos) {
		tok = tokseq_at(tseq, pos);
		if (iswhite(tok)) {
			if (!skip_ws) {
				tok = token_dup2(tok, TOK_WS);
				tokseq_append2(new_tsek, tok, tok);
				skip_ws = true;
			}
		} else {
			tokseq_append(new_tsek, tok);
			skip_ws = false;
		}
	}
	pad_ws_tokens(new_tsek);

	return new_tsek;
}

static tokseq_t *scan_atoms(const tokseq_t *tseq, size_t *pos)
{
	token_t *tok;
	tokseq_t *res;

	res = tokseq_new(0);
	tok = tokseq_at(tseq, *pos);
	while (token_isatom(tok)) {
		tokseq_append(res, tok);
		*pos += 1;
		tok = tokseq_at(tseq, *pos);
	}
	return res;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Match open-to-close parentheses ("{}[]()") using stack algorithm.
 */
static const tokmeta_t *paren_meta(const token_t *tok)
{
	return paren_to_tokmeta(tok->str);
}

static void stack_push(tokseq_t *stack, token_t *tok)
{
	const size_t stack_max = CAC_LIMIT_NESTED_BRACKETS_MAX;

	if (tokseq_len(stack) >= stack_max) {
		compile_error(tok, "too many brackets nesting (max: %lu)",
		              stack_max);
	}
	tokseq_push_back(stack, tok);
}

static token_t *stack_pop(tokseq_t *stack)
{
	return tokseq_pop_back(stack);
}

static tokseq_t *scan_match_parens(const tokseq_t *tseq)
{
	wchar_t pop, chr;
	token_t *tok, *alt, *peer;
	tokseq_t *next_tseq, *stack;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	stack = tokseq_new(len);
	for (size_t pos = 0; pos < len; ++pos) {
		pop = '\0';
		alt = NULL;
		peer = NULL;
		tok = tokseq_at(tseq, pos);

		if (token_ischar(tok, '{')) {
			alt = token_dup5(tok, paren_meta(tok));
			stack_push(stack, alt);
		} else if (token_ischar(tok, '[')) {
			alt = token_dup5(tok, paren_meta(tok));
			stack_push(stack, alt);
		} else if (token_ischar(tok, '(')) {
			alt = token_dup5(tok, paren_meta(tok));
			stack_push(stack, alt);
		} else if (token_ischar(tok, '}')) {
			alt = token_dup5(tok, paren_meta(tok));
			peer = stack_pop(stack);
			pop = '{';
		} else if (token_ischar(tok, ']')) {
			alt = token_dup5(tok, paren_meta(tok));
			peer = stack_pop(stack);
			pop = '[';
		} else if (token_ischar(tok, ')')) {
			alt = token_dup5(tok, paren_meta(tok));
			peer = stack_pop(stack);
			pop = '(';
		}

		if (pop != '\0') {
			if (peer == NULL) {
				compile_error(tok, "unbalanced parans");
			} else if ((chr = token_getchr0(peer)) != pop) {
				compile_error(peer, "unmatched parans");
			}
		}
		if (alt != NULL) {
			tokseq_append(next_tseq, alt);
		} else {
			tokseq_append(next_tseq, tok);
		}
	}
	tok = stack_pop(stack);
	if (tok != NULL) {
		compile_error(tok, "redundent parans");
	}
	tokseq_clear(stack);
	return next_tseq;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static tokseq_t *scan_seperator_delimiters(const tokseq_t *tseq)
{
	size_t dlimcnt = 0;
	token_t *tok;
	tokseq_t *next_tseq;
	const tokmeta_t *tokmeta;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	for (size_t pos = 0; pos < len; ++pos) {
		tokmeta = NULL;
		tok = tokseq_at(tseq, pos);
		if (token_isatom(tok)) {
			tokmeta = special_delim_to_tokmeta(tok->str);
			if (tokmeta != NULL) {
				tok->type = tokmeta->type;
			}
		}
		tokseq_append(next_tseq, tok);

		if (tokmeta == NULL) {
			dlimcnt = 0;
		} else if (++dlimcnt > 1) {
			compile_error(tok, "syntax error");
		}
	}
	return next_tseq;
}

static tokseq_t *scan_whitespace_operators(const tokseq_t *tseq)
{
	size_t pos = 0;
	string_t *str;
	token_t *tok, *nxt, *wop;
	tokseq_t *next_tseq, *atoms;
	const tokmeta_t *tokmeta;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	while (pos < len) {
		tok = tokseq_at(tseq, pos++);
		nxt = tokseq_at(tseq, pos);
		tokseq_append(next_tseq, tok);
		if (!iswspaces(tok) || !token_isatom(nxt)) {
			continue;
		}
		atoms = scan_atoms(tseq, &pos);
		nxt = tokseq_at(tseq, pos);
		if (!iswspaces(nxt)) {
			tokseq_appendn(next_tseq, atoms);
			continue;
		}
		str = tokseq_tostring(atoms);
		tokmeta = whitespace_oper_to_tokmeta(str);
		if (tokmeta == NULL) {
			tokseq_appendn(next_tseq, atoms);
			continue;
		}
		wop = token_dup4(tok, tokmeta, str);
		tokseq_append(next_tseq, wop);
	}
	return next_tseq;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static tokseq_t *scan_special_punct_multi(const tokseq_t *tseq)
{
	size_t pos = 0;
	string_t *str;
	token_t *tok, *nxt;
	tokseq_t *next_tseq;
	const tokmeta_t *tokmeta;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	while (pos < len) {
		tok = tokseq_at(tseq, pos++);
		nxt = tokseq_at(tseq, pos);
		if (token_isapunct(tok) && token_isapunct(nxt)) {
			str = string_join(tok->str, nxt->str);
			tokmeta = special_punct_to_tokmeta2(str);
			if (tokmeta != NULL) {
				tok = token_dup4(tok, tokmeta, str);
				pos += 1;
			}
		}
		tokseq_append(next_tseq, tok);
	}
	return next_tseq;
}

static tokseq_t *scan_special_punct_single(const tokseq_t *tseq)
{
	size_t pos = 0;
	token_t *tok;
	tokseq_t *next_tseq;
	const tokmeta_t *tokmeta;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	while (pos < len) {
		tok = tokseq_at(tseq, pos++);
		if (token_isapunct(tok)) {
			tokmeta = special_punct_to_tokmeta1(tok->str);
			if (tokmeta != NULL) {
				tok = token_dup4(tok, tokmeta, tok->str);
			}
		}
		tokseq_append(next_tseq, tok);
	}
	return next_tseq;
}

static token_t *require_ident(token_t *tok)
{
	if (! token_istype(tok, TOK_IDENT)) {
		compile_error(tok, "syntax error: not an identifier: %s",
		              tok->str->str);
	}
	return tok;
}

static tokseq_t *scan_keywords_and_builtins(const tokseq_t *tseq)
{
	size_t pos = 0;
	token_t *tok;
	tokseq_t *next_tseq;
	const tokmeta_t *tokmeta;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	while (pos < len) {
		tok = tokseq_at(tseq, pos++);
		if (token_istype(tok, TOK_ATSIGN)) {
			tok = require_ident(tokseq_at(tseq, pos++));
			tokmeta = keyword_or_builtin_to_tokmeta(tok->str);
			if (tokmeta == NULL) {
				compile_error(tok, "unknown meta-token");
			}
			tok = token_dup5(tok, tokmeta);
		}
		tokseq_append(next_tseq, tok);
	}
	return next_tseq;
}

/*
 * Converts attributes sequence to well-known tokens
 */
tokseq_t *scan_attributes_list(const tokseq_t *tseq)
{
	token_t *tok;
	tokseq_t *next_tseq;
	const tokmeta_t *tokmeta;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	for (size_t pos = 0; pos < len; ++pos) {
		tok = tokseq_at(tseq, pos);
		if (token_istype(tok, TOK_IDENT)) {
			tokmeta = attribute_to_tokmeta(tok->str);
			if (tokmeta != NULL) {
				tok = token_dup5(tok, tokmeta);
			}
		}
		tokseq_append(next_tseq, tok);
	}
	return next_tseq;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void verify_identifier_len(const token_t *tok)
{
	const string_t *ident = tok->str;

	if (!ident->str || !ident->len) {
		compile_error(tok, "zero length identifier");
	}
	if (ident->len > CAC_LIMIT_IDENTIFIER_LEN_MAX) {
		compile_error(tok, "identifier too long: %s", ident->str);
	}
}

static void verify_identifier_chars(const token_t *tok)
{
	size_t len;
	const char *str;
	const string_t *ident = tok->str;

	str = ident->str;
	len = ident->len;
	if (str[0] == '_') {
		compile_error(tok, "starts with underscore: %s", ident->str);
	}
	if (str[len - 1] == '_') {
		compile_error(tok, "ends with underscore: %s", ident->str);
	}
	if (strstr(str, "__")) {
		compile_error(tok, "multi underscores: %s", ident->str);
	}
	if (strchr("0123456789", str[0])) {
		compile_error(tok, "starts with digit: %s", ident->str);
	}
	for (size_t i = 0; i < len; ++i) {
		if (!strchr(identifier_charset, str[i])) {
			compile_error(tok, "illegal %lu-char: %s",
			              i, ident->str);
		}
	}
}

static void verify_identifier(const token_t *tok)
{
	verify_identifier_len(tok);
	verify_identifier_chars(tok);
}

static token_t *scan_ident(const tokseq_t *tseq, size_t *pos)
{
	token_t *tok;
	tokseq_t *seq;

	seq = tokseq_new(1);
	tok = tokseq_at(tseq, *pos);
	while (token_ischarof(tok, identifier_charset)) {
		tokseq_push_back(seq, tok);
		*pos += 1;
		tok = tokseq_at(tseq, *pos);
	}
	tok = tokseq_meld(seq, TOK_IDENT);
	verify_identifier(tok);
	return tok;
}

static tokseq_t *scan_identifiers(const tokseq_t *tseq)
{
	size_t pos = 0;
	token_t *tok;
	tokseq_t *next_tseq;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	while (pos < len) {
		tok = tokseq_at(tseq, pos);
		if (token_ischarof(tok, identifier_charset)) {
			tok = scan_ident(tseq, &pos);
		} else {
			pos += 1;
		}
		tokseq_append(next_tseq, tok);
	}
	return next_tseq;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void verify_sequence(const tokseq_t *tseq)
{
	const token_t *tok;
	const size_t len = tokseq_len(tseq);

	for (size_t pos = 0; pos < len; ++pos) {
		tok = tokseq_at(tseq, pos);
		if (token_istype(tok, TOK_NUMBERSIGN)) {
			compile_error(tok, "reserved: %s", tok->str->str);
		}
		if (token_istype(tok, TOK_ATSIGN)) {
			compile_error(tok, "missing keyword: %s",
			              tok->str->str);
		}
		if (token_isapunct(tok)) {
			compile_error(tok, "bad punctuation: %s",
			              tok->str->str);
		}
		if (token_isatom(tok)) {
			compile_error(tok, "unused: %s", tok->str->str);
		}
	}
}

static tokseq_t *strip_whitespaces(const tokseq_t *tseq)
{
	tokseq_t *next_tseq;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	for (size_t pos = 0; pos < len; ++pos) {
		token_t *tok = tokseq_at(tseq, pos);
		if (!token_istype(tok, TOK_WS)) {
			tokseq_append(next_tseq, tok);
		}
	}
	return next_tseq;
}

static tokseq_t *scan_prefix_specials(const tokseq_t *tseq)
{
	size_t pos = 0;
	tokseq_t *next_tseq;
	token_t *tok, *nxt;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	while (pos < len) {
		tok = tokseq_at(tseq, pos++);
		nxt = tokseq_at(tseq, pos);
		if (isidentc(tok, 'L') && isstring(nxt)) {
			tok->type = TOK_LPREFIX;
		}
		tokseq_append(next_tseq, tok);
	}
	return next_tseq;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static token_t *new_pseudo_token(const token_t *tok)
{
	token_t *ptok;

	if (token_istype(tok, TOK_LPAREN)) {
		ptok = token_dup2(tok, TOK_PEXPR);
	} else if (token_istype(tok, TOK_LBRACK)) {
		ptok = token_dup2(tok, TOK_BEXPR);
	} else if (token_istype(tok, TOK_LANGLE)) {
		ptok = token_dup2(tok, TOK_AEXPR);
	} else if (token_istype(tok, TOK_LBRACE)) {
		ptok = token_dup2(tok, TOK_BLOCK);
	} else {
		ptok = token_dup2(tok, TOK_SUBSEQ);
	}
	ptok->seq = tokseq_new(0);
	ptok->seq->tok = tok;
	return ptok;
}

static token_t *new_pseudo_token_at(const tokseq_t *tseq)
{
	return new_pseudo_token(tokseq_front(tseq));
}

/*
 * Remove all comment tokens in order to avoid confusion in up-comming parsing
 * phases.
 *
 * NB: Revisit -- comments should be associated with corresponding tokens (?)
 */
static tokseq_t *strip_comments(const tokseq_t *tseq)
{
	tokseq_t *next_tseq;
	const size_t len = tokseq_len(tseq);

	next_tseq = tokseq_new(len);
	for (size_t pos = 0; pos < len; ++pos) {
		token_t *tok = tokseq_at(tseq, pos);
		if (!token_istype(tok, TOK_SL_COMMENT) &&
		    !token_istype(tok, TOK_ML_COMMENT)) {
			tokseq_append(next_tseq, tok);
		}
	}
	return next_tseq;
}

/*
 * Do not allow empty token-sequence after striping
 */
static void verify_nonempty(const tokseq_t *atoms, const tokseq_t *tseq)
{
	if (tokseq_isempty(tseq)) {
		compile_error(tokseq_front(atoms), "empty input");
	}
}

static bool iscloseof(const token_t *tok, const token_t *stok)
{
	toktype_e tt = TOK_NONE;

	if (token_istype(stok, TOK_LPAREN)) {
		tt = TOK_RPAREN;
	} else if (token_istype(stok, TOK_LBRACK)) {
		tt = TOK_RBRACK;
	} else if (token_istype(stok, TOK_LBRACE)) {
		tt = TOK_RBRACE;
	} else if (token_istype(stok, TOK_LANGLE)) {
		tt = TOK_RANGLE;
	} else {
		tt = TOK_NONE;
	}
	return token_istype(tok, tt);
}

static token_t *scan_pexpr(const token_t *stok, tokseq_t *tseq)
{
	token_t *tok, *seq = NULL;
	token_t *pexpr_tok;

	pexpr_tok = new_pseudo_token(stok);
	tok = tokseq_pop_front(tseq);
	while ((tok != NULL) && !iscloseof(tok, stok)) {
		if (token_istype(tok, TOK_LPAREN) ||
		    token_istype(tok, TOK_LBRACK) ||
		    token_istype(tok, TOK_LANGLE)) {
			seq = scan_pexpr(tok, tseq);
			tokseq_append(pexpr_tok->seq, seq);
		} else {
			tokseq_append(pexpr_tok->seq, tok);
		}
		tok = tokseq_pop_front(tseq);
	}
	return pexpr_tok;
}

static token_t *scan_block(const token_t *stok, tokseq_t *tseq)
{
	token_t *tok, *seq;
	token_t *block_tok;

	block_tok = new_pseudo_token(stok);
	block_tok->type = TOK_BLOCK;

	seq = new_pseudo_token_at(tseq);
	tok = tokseq_pop_front(tseq);
	while ((tok != NULL) && !iscloseof(tok, stok)) {
		if (token_istype2(tok, TOK_LPAREN, TOK_LBRACK)) {
			tokseq_append(seq->seq, scan_pexpr(tok, tseq));
		} else if (token_istype(tok, TOK_LBRACE)) {
			tokseq_append(seq->seq, scan_block(tok, tseq));
			tokseq_append(block_tok->seq, seq);
			seq = new_pseudo_token_at(tseq);
		} else if (token_istype(tok, TOK_SEMI)) {
			tokseq_append(block_tok->seq, seq);
			seq = new_pseudo_token_at(tseq);
		} else {
			tokseq_append(seq->seq, tok);
		}
		tok = tokseq_pop_front(tseq);
	}
	if (!tokseq_isempty(seq->seq)) {
		tokseq_append(block_tok->seq, seq);
	}
	return block_tok;
}

/*
 * Converts raw tokens-sequence into structured objects tree.
 */
static token_t *scan_structure(tokseq_t *tseq)
{
	token_t *block_tok;
	token_t *lbrace, *rbrace;

	/* Create dummy block structure */
	lbrace = token_dup2(tokseq_front(tseq), TOK_LBRACE);
	rbrace = token_dup2(tokseq_back(tseq), TOK_RBRACE);
	tokseq_push_back(tseq, rbrace);

	/* Parse recursively */
	block_tok = scan_block(lbrace, tseq);
	cac_assert(block_tok->seq != NULL);
	return block_tok;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

tokseq_t *scan_atoms_to_tokens(tokseq_t *atoms)
{
	tokseq_t *tseq;
	token_t *block;

	/* Phase1 */
	tseq = scan_comments(atoms);
	tseq = scan_string_literals(tseq);
	tseq = concat_adjacent_string_literals(tseq);
	tseq = scan_ws_tokens(tseq);
	verify_charset(tseq);

	/* Phase2 */
	tseq = scan_numaric_literals(tseq);

	/* Phase3 */
	tseq = scan_seperator_delimiters(tseq);
	tseq = scan_whitespace_operators(tseq);
	tseq = scan_match_parens(tseq);
	tseq = scan_special_punct_multi(tseq);
	tseq = scan_special_punct_single(tseq);
	tseq = scan_identifiers(tseq);
	tseq = scan_keywords_and_builtins(tseq);
	verify_sequence(tseq);

	/* Phase4 */
	tseq = strip_whitespaces(tseq);
	tseq = scan_prefix_specials(tseq);
	tseq = strip_comments(tseq);
	verify_nonempty(atoms, tseq);

	/* Phase5 */
	block = scan_structure(tseq);
	tseq = block->seq;

	return tseq;
}

