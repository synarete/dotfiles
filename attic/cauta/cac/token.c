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
#include "infra.h"
#include "token.h"



#define TOKMETA(k, t) \
	{ k, MAKESTR(t), t }


static const tokmeta_t s_tokmeta_parens[] = {
	TOKMETA("{",            TOK_LBRACE),
	TOKMETA("}",            TOK_RBRACE),
	TOKMETA("(",            TOK_LPAREN),
	TOKMETA(")",            TOK_RPAREN),
	TOKMETA("[",            TOK_LBRACK),
	TOKMETA("]",            TOK_RBRACK),
	TOKMETA("<",            TOK_LANGLE),
	TOKMETA(">",            TOK_RANGLE),

};

static const tokmeta_t s_tokmeta_sdelim[] = {
	TOKMETA(";",            TOK_SEMI),
	TOKMETA(",",            TOK_COMMA),
};

static const tokmeta_t s_tokmeta_wsoper[] = {
	TOKMETA("+",            TOK_PLUS),
	TOKMETA("-",            TOK_MINUS),
	TOKMETA("*",            TOK_MUL),
	TOKMETA("/",            TOK_DIV),
	TOKMETA("%",            TOK_MOD),
	TOKMETA("|",            TOK_OR),
	TOKMETA("&",            TOK_AND),
	TOKMETA("^",            TOK_XOR),
	TOKMETA("<<",           TOK_LSHIFT),
	TOKMETA(">>",           TOK_RSHIFT),
	TOKMETA("||",           TOK_LOR),
	TOKMETA("&&",           TOK_LAND),
	TOKMETA("<",            TOK_LT),
	TOKMETA("<=",           TOK_LE),
	TOKMETA(">",            TOK_GT),
	TOKMETA(">=",           TOK_GE),
	TOKMETA("==",           TOK_EQ),
	TOKMETA("!=",           TOK_NE),
	TOKMETA("=",            TOK_ASSIGN),
	TOKMETA("::=",          TOK_XASSIGN),
	TOKMETA("+=",           TOK_PLUSASSIGN),
	TOKMETA("-=",           TOK_MINUSASSIGN),
	TOKMETA("*=",           TOK_MULASSIGN),
	TOKMETA("/=",           TOK_DIVASSIGN),
	TOKMETA("%=",           TOK_MODASSIGN),
	TOKMETA("<<=",          TOK_LSHIFTASSIGN),
	TOKMETA(">>=",          TOK_RSHIFTASSIGN),
	TOKMETA("&=",           TOK_ANDASSIGN),
	TOKMETA("^=",           TOK_XORASSIGN),
	TOKMETA("|=",           TOK_ORASSIGN),
	TOKMETA("-->",          TOK_LONGARROW),
	TOKMETA("?",            TOK_TERNARYIF),
	TOKMETA(":",            TOK_TERNARYELSE),
	TOKMETA("<>",           TOK_REINTERPRET),
	TOKMETA(">-",           TOK_CAST),
};

static const tokmeta_t s_tokmeta_spunct1[] = {
	TOKMETA("@",            TOK_ATSIGN),
	TOKMETA("#",            TOK_NUMBERSIGN),
	TOKMETA("$",            TOK_DOLLAR),
	TOKMETA("/",            TOK_SLASH),
	TOKMETA("-",            TOK_HYPHEN),
	TOKMETA(":",            TOK_COLON),
	TOKMETA("*",            TOK_POINTER),
	TOKMETA("^",            TOK_RPOINTER),
	TOKMETA("&",            TOK_ADDRESSOF),
	TOKMETA(".",            TOK_PERIOD),
	TOKMETA("~",            TOK_NOT),
	TOKMETA("!",            TOK_LNOT),
};

static const tokmeta_t s_tokmeta_spunct2[] = {
	TOKMETA("++",           TOK_INCREMENT),
	TOKMETA("--",           TOK_DECREMENT),
	TOKMETA("->",           TOK_ARROW),
	TOKMETA("~>",           TOK_XARROW),
};

static const tokmeta_t s_tokmeta_primitives[] = {
	TOKMETA("type",         TOK_TYPE),
	TOKMETA("void",         TOK_VOID),
	TOKMETA("opaque",       TOK_OPAQUE),
	TOKMETA("bool",         TOK_BOOL),
	TOKMETA("byte",         TOK_BYTE),
	TOKMETA("char",         TOK_CHAR),
	TOKMETA("wchar",        TOK_WCHAR),
	TOKMETA("short",        TOK_SHORT),
	TOKMETA("ushort",       TOK_USHORT),
	TOKMETA("int",          TOK_INT),
	TOKMETA("uint",         TOK_UINT),
	TOKMETA("long",         TOK_LONG),
	TOKMETA("ulong",        TOK_ULONG),
	TOKMETA("size",         TOK_SIZE),
	TOKMETA("int8",         TOK_INT8),
	TOKMETA("uint8",        TOK_UINT8),
	TOKMETA("int16",        TOK_INT16),
	TOKMETA("uint16",       TOK_UINT16),
	TOKMETA("int32",        TOK_INT32),
	TOKMETA("uint32",       TOK_UINT32),
	TOKMETA("int64",        TOK_INT64),
	TOKMETA("uint64",       TOK_UINT64),
	TOKMETA("int128",       TOK_INT128),
	TOKMETA("uint128",      TOK_UINT128),
	TOKMETA("float32",      TOK_FLOAT32),
	TOKMETA("float64",      TOK_FLOAT64),
};

static const tokmeta_t s_tokmeta_keyword[] = {
	TOKMETA("baseof",       TOK_BASEOF),
	TOKMETA("break",        TOK_BREAK),
	TOKMETA("case",         TOK_CASE),
	TOKMETA("const",        TOK_CONST),
	TOKMETA("containerof",  TOK_CONTAINEROF),
	TOKMETA("continue",     TOK_CONTINUE),
	TOKMETA("default",      TOK_DEFAULT),
	TOKMETA("doc",          TOK_DOC),
	TOKMETA("enum",         TOK_ENUM),
	TOKMETA("else",         TOK_ELSE),
	TOKMETA("elsif",        TOK_ELSIF),
	TOKMETA("func",         TOK_FUNC),
	TOKMETA("for",          TOK_FOR),
	TOKMETA("if",           TOK_IF),
	TOKMETA("import",       TOK_IMPORT),
	TOKMETA("nelems",       TOK_NELEMS),
	TOKMETA("offsetof",     TOK_OFFSETOF),
	TOKMETA("package",      TOK_PACKAGE),
	TOKMETA("return",       TOK_RETURN),
	TOKMETA("setattr",      TOK_SETATTR),
	TOKMETA("sizeof",       TOK_SIZEOF),
	TOKMETA("struct",       TOK_STRUCT),
	TOKMETA("switch",       TOK_SWITCH),
	TOKMETA("typeof",       TOK_TYPEOF),
	TOKMETA("unconst",      TOK_UNCONST),
	TOKMETA("union",        TOK_UNION),
	TOKMETA("var",          TOK_VAR),
	TOKMETA("while",        TOK_WHILE),
};

static const tokmeta_t s_tokmeta_builtin[] = {
	TOKMETA("F",            TOK_FALSE),
	TOKMETA("false",        TOK_FALSE),
	TOKMETA("null",         TOK_NULL),
	TOKMETA("NULL",         TOK_NULL),
	TOKMETA("T",            TOK_TRUE),
	TOKMETA("true",         TOK_TRUE),
	TOKMETA("file",         TOK_FILE),
	TOKMETA("line",         TOK_LINE),
};

static const tokmeta_t s_tokmeta_attributes[] = {
	TOKMETA("global",       TOK_GLOBAL),
	TOKMETA("inline",       TOK_INLINE),
	TOKMETA("static",       TOK_STATIC),
	TOKMETA("method",       TOK_METHOD),
};

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#define find_tokmeta_of(tbl, str) find_tokmeta(str, tbl, ARRAYSIZE(tbl))

static bool isdesc(const tokmeta_t *td, const string_t *str)
{
	return string_isequals(str, td->key);
}

static const tokmeta_t *
find_tokmeta(const string_t *str, const tokmeta_t *tbl, size_t tblsz)
{
	const tokmeta_t *tokmeta = NULL;

	for (size_t i = 0; i < tblsz; ++i) {
		tokmeta = &tbl[i];
		if (isdesc(tokmeta, str)) {
			break;
		}
		tokmeta = NULL;
	}
	return tokmeta;
}

const tokmeta_t *paren_to_tokmeta(const string_t *str)
{
	return find_tokmeta_of(s_tokmeta_parens, str);
}

const tokmeta_t *special_delim_to_tokmeta(const string_t *str)
{
	return find_tokmeta_of(s_tokmeta_sdelim, str);
}

static const tokmeta_t *keyword_to_tokmeta(const string_t *str)
{
	return find_tokmeta_of(s_tokmeta_keyword, str);
}

static const tokmeta_t *builtin_to_tokmeta(const string_t *str)
{
	return find_tokmeta_of(s_tokmeta_builtin, str);
}

const tokmeta_t *keyword_or_builtin_to_tokmeta(const string_t *str)
{
	const tokmeta_t *tokmeta;

	tokmeta = keyword_to_tokmeta(str);
	if (tokmeta == NULL) {
		tokmeta = builtin_to_tokmeta(str);
	}
	return tokmeta;
}

const tokmeta_t *attribute_to_tokmeta(const string_t *str)
{
	return find_tokmeta_of(s_tokmeta_attributes, str);
}

const tokmeta_t *whitespace_oper_to_tokmeta(const string_t *str)
{
	return find_tokmeta_of(s_tokmeta_wsoper, str);
}

const tokmeta_t *special_punct_to_tokmeta1(const string_t *str)
{
	return find_tokmeta_of(s_tokmeta_spunct1, str);
}

const tokmeta_t *special_punct_to_tokmeta2(const string_t *str)
{
	return find_tokmeta_of(s_tokmeta_spunct2, str);
}

const tokmeta_t *primitive_to_tokmeta(const string_t *str)
{
	return find_tokmeta_of(s_tokmeta_primitives, str);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void token_init(token_t *tok, toktype_e tok_type)
{
	tok->type = tok_type;
	tok->pos = 0;
	tok->lno = 0;
	tok->off = 0;
	tok->str = string_nil;
	tok->mod = NULL;
	tok->seq = NULL;
	tok->num = string_nil;
}

static void token_clone(token_t *tok, const token_t *other)
{
	tok->type = other->type;
	tok->pos = other->pos;
	tok->lno = other->lno;
	tok->off = other->off;
	tok->str = string_dup(other->str);
	tok->mod = other->mod;
	tok->seq = other->seq ? tokseq_dup(other->seq) : NULL;
	tok->num = string_dup(other->num);
}

token_t *token_new(toktype_e tok_type)
{
	token_t *tok;

	tok = gc_malloc(sizeof(*tok));
	token_init(tok, tok_type);
	return tok;
}

token_t *token_new2(const tokmeta_t *tokmeta)
{
	token_t *tok;

	tok = token_new(tokmeta->type);
	tok->str = string_new(tokmeta->key);
	return tok;
}

token_t *token_new_atom(char ch)
{
	token_t *tok;

	tok = token_new(TOK_ATOM);
	tok->str = string_nnew(&ch, 1);
	return tok;
}

token_t *token_join(const token_t *tok1, const token_t *tok2)
{
	token_t *tok;

	tok = token_new(tok1->type);
	tok->pos = tok1->pos;
	tok->lno = tok1->lno;
	tok->off = tok1->off;
	tok->str = string_join(tok1->str, tok2->str);
	tok->mod = tok1->mod;
	return tok;
}

token_t *token_dup(const token_t *other)
{
	token_t *tok = token_new(other->type);
	token_clone(tok, other);
	return tok;
}

token_t *token_dup2(const token_t *other, toktype_e newtype)
{
	token_t *tok = token_dup(other);
	tok->type = newtype;
	return tok;
}

token_t *token_dup3(const token_t *other,
                    toktype_e newtype, const string_t *str)
{
	token_t *tok = token_dup2(other, newtype);
	tok->str = string_dup(str);
	return tok;
}

token_t *token_dup4(const token_t *other,
                    const tokmeta_t *meta, const string_t  *str)
{
	return token_dup3(other, meta->type, str);
}

token_t *token_dup5(const token_t *other, const tokmeta_t *meta)
{
	return token_dup4(other, meta, string_new(meta->key));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t token_len(const token_t *tok)
{
	return (tok && tok->str) ? tok->str->len : 0;
}

bool token_haslen(const token_t *tok, size_t len)
{
	return (token_len(tok) == len);
}

toktype_e token_gettype(const token_t *tok)
{
	return ((tok != NULL) ? tok->type : TOK_NONE);
}

bool token_istype(const token_t *tok, toktype_e tt)
{
	return (token_gettype(tok) == tt);
}

bool token_istype2(const token_t *tok, toktype_e tt1, toktype_e tt2)
{
	return (token_istype(tok, tt1) || token_istype(tok, tt2));
}

bool token_isprimitive(const token_t *tok)
{
	bool res = false;
	const size_t nelems = ARRAYSIZE(s_tokmeta_primitives);

	for (size_t i = 0; !res && (i < nelems); ++i) {
		res = token_istype(tok, s_tokmeta_primitives[i].type);
	}
	return res;
}

bool token_isatom(const token_t *tok)
{
	return (token_istype(tok, TOK_ATOM) && token_haslen(tok, 1));
}

char token_getchr0(const token_t *tok)
{
	return tok->str->str[0];
}

bool token_ischar(const token_t *tok, char ch)
{
	return token_isatom(tok) && (token_getchr0(tok) == ch);
}

bool token_ischarof(const token_t *tok, const char *set)
{
	return (token_isatom(tok) && (strchr(set, token_getchr0(tok)) != NULL));
}

bool token_isapunct(const token_t *tok)
{
	return (token_isatom(tok) && ispunct(token_getchr0(tok)));
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void tokseq_init(tokseq_t *tseq, size_t cap)
{
	vector_init(&tseq->vec, cap);
	tseq->tok = NULL;
}

tokseq_t *tokseq_new(size_t initial_cap)
{
	tokseq_t *tseq;

	tseq = (tokseq_t *)gc_malloc(sizeof(*tseq));
	tokseq_init(tseq, initial_cap);
	return tseq;
}


static tokseq_t *tokseq_new_by_vec(const vector_t *vec)
{
	tokseq_t *tseq;

	tseq = (tokseq_t *)gc_malloc(sizeof(*tseq));
	memcpy(&tseq->vec, vec, sizeof(tseq->vec));
	return tseq;
}

tokseq_t *tokseq_tail(const tokseq_t *tseq, size_t pos)
{
	return tokseq_new_by_vec(vector_tail(&tseq->vec, pos));
}

tokseq_t *tokseq_sub(const tokseq_t *tseq, size_t pos, size_t cnt)
{
	return tokseq_new_by_vec(vector_sub(&tseq->vec, pos, cnt));
}

tokseq_t *tokseq_dup(const tokseq_t *tseq)
{
	return tokseq_sub(tseq, 0, tokseq_len(tseq));
}

size_t tokseq_len(const tokseq_t *tseq)
{
	return tseq->vec.len;
}

bool tokseq_isempty(const tokseq_t *tseq)
{
	return (tokseq_len(tseq) == 0);
}

void tokseq_clear(tokseq_t *tseq)
{
	vector_clear(&tseq->vec);
}

token_t *tokseq_at(const tokseq_t *tseq, size_t pos)
{
	return (token_t *)vector_at(&tseq->vec, pos);
}

void tokseq_append(tokseq_t *tseq, token_t *tok)
{
	cac_assert(tseq != NULL);

	if (tok != NULL) {
		tokseq_push_back(tseq, tok);
	}
}

void tokseq_append2(tokseq_t *tseq, token_t *tok1, token_t *tok2)
{
	tokseq_append(tseq, tok1);
	tokseq_append(tseq, tok2);
}

void tokseq_append3(tokseq_t *tseq, token_t *tok1, token_t *tok2, token_t *tok3)
{
	tokseq_append(tseq, tok1);
	tokseq_append(tseq, tok2);
	tokseq_append(tseq, tok3);
}


void tokseq_appendn(tokseq_t *tseq, const tokseq_t *other)
{
	token_t *tok;

	for (size_t i = 0; i < other->vec.len; ++i) {
		tok = tokseq_at(other, i);
		tokseq_append(tseq, tok);
	}
}

token_t *tokseq_front(const tokseq_t *tseq)
{
	return (token_t *)vector_front(&tseq->vec);
}

token_t *tokseq_back(const tokseq_t *tseq)
{
	return (token_t *)vector_back(&tseq->vec);
}

token_t *tokseq_pop_front(tokseq_t *tseq)
{
	return (token_t *)vector_pop_front(&tseq->vec);
}

tokseq_t *tokseq_popn_front(tokseq_t *tseq, size_t n)
{
	token_t *tok;
	tokseq_t *res = tokseq_new(n);

	while (n-- > 0) {
		tok = tokseq_pop_front(tseq);
		if (tok == NULL) {
			break;
		}
		tokseq_push_back(res, tok);
	}
	return res;
}

void tokseq_push_front(tokseq_t *tseq, token_t *tok)
{
	vector_push_front(&tseq->vec, tok);
}

token_t *tokseq_pop_back(tokseq_t *tseq)
{
	return (token_t *)vector_pop_back(&tseq->vec);
}

void tokseq_push_back(tokseq_t *tseq, token_t *tok)
{
	vector_push_back(&tseq->vec, tok);
}

void tokseq_erase(tokseq_t *tseq, size_t pos)
{
	vector_erase(&tseq->vec, pos);
}

tokseq_t *tokseq_join(tokseq_t *tseq1, tokseq_t *tseq2)
{
	return tokseq_new_by_vec(vector_join(&tseq1->vec, &tseq2->vec));
}

token_t *tokseq_meld(const tokseq_t *tseq, toktype_e tok_type)
{
	token_t *tok, *sub;

	tok = token_new(tok_type);
	if (tseq->vec.len > 0) {
		sub = tokseq_at(tseq, 0);
		tok->lno = sub->lno;
		tok->pos = sub->pos;
		tok->off = sub->off;
		tok->mod = sub->mod;
	}
	for (size_t i = 0; i < tseq->vec.len; ++i) {
		sub = tokseq_at(tseq, i);
		tok->str = string_join(tok->str, sub->str);
	}
	return tok;
}

string_t *tokseq_tostring(const tokseq_t *tseq)
{
	size_t len, cnt = 0;
	string_t *str;
	const token_t *tok;
	char buf[512] = "";
	const size_t bsz = sizeof(buf);

	str = string_new("");
	for (size_t i = 0; i < tseq->vec.len; ++i) {
		tok = tokseq_at(tseq, i);
		len = tok->str->len;
		if (len < (bsz - cnt)) {
			strncpy(buf + cnt, tok->str->str, bsz - cnt);
			cnt += len;
		} else {
			if (cnt > 0) {
				str = string_joinn(str, buf, cnt);
				cnt = 0;
			}
			str = string_join(str, tok->str);
		}
	}
	if (cnt > 0) {
		str = string_joinn(str, buf, cnt);
	}
	return str;
}

