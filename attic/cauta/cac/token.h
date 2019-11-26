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
#ifndef CAC_TOKEN_H_
#define CAC_TOKEN_H_

#include <stdlib.h>
#include "strutils.h"
#include "vector.h"


/*
 * Forward declarations
 */
struct module;
struct tokmeta;
struct token;
struct tokseq;


/*
 * Token types
 */
enum CAC_TOK_TYPE {
	/* Default: unknown token type */
	TOK_NONE,
	/* Single character */
	TOK_ATOM,
	/* White-spaces characters */
	TOK_WS,
	/* Identifier & type-identifier */
	TOK_IDENT,
	/* Multi-line comment */
	TOK_ML_COMMENT,
	/* Single-line comment */
	TOK_SL_COMMENT,
	/* Single-quoted string */
	TOK_SQ_STRING,
	/* Double-quoted string */
	TOK_DQ_STRING,
	/* Triple-quoted (apostrophes) string */
	TOK_TQ_STRING,
	/* String wide-encoding prefix (L) */
	TOK_LPREFIX,
	/* Integer & floating-point literals */
	TOK_INT_LITERAL,
	TOK_FP_LITERAL,
	TOK_LFP_LITERAL,
	/* Delimiter: Brace parenthesis ("curly", {}) */
	TOK_LBRACE,
	TOK_RBRACE,
	/* Delimiter: Parenthesis ("rounded", ()) */
	TOK_LPAREN,
	TOK_RPAREN,
	/* Delimiter: Brackets parenthesis ("square", []) */
	TOK_LBRACK,
	TOK_RBRACK,
	/* Delimiter: Angle parenthesis ("pointy", <>) */
	TOK_LANGLE,
	TOK_RANGLE,
	/* Separators: Semicolon & comma (;,) */
	TOK_SEMI,
	TOK_COMMA,
	/* Operators (+, -, *, /, %, |, &, ~, ^) */
	TOK_PLUS,
	TOK_MINUS,
	TOK_MUL,
	TOK_DIV,
	TOK_MOD,
	TOK_OR,
	TOK_AND,
	TOK_NOT,
	TOK_XOR,
	/* Operators (<<, >>, ||, &&, !, <, <=, >, >=, ==, !=) */
	TOK_LSHIFT,
	TOK_RSHIFT,
	TOK_LOR,
	TOK_LAND,
	TOK_LNOT,
	TOK_LT,
	TOK_LE,
	TOK_GT,
	TOK_GE,
	TOK_EQ,
	TOK_NE,
	/* Assignment operators (=, ::=) */
	TOK_ASSIGN,
	TOK_XASSIGN,
	/* Assignments (+=, -=, *=, /=, %=, <<=, >>=, &=, ^=, |=) */
	TOK_PLUSASSIGN,
	TOK_MINUSASSIGN,
	TOK_MULASSIGN,
	TOK_DIVASSIGN,
	TOK_MODASSIGN,
	TOK_LSHIFTASSIGN,
	TOK_RSHIFTASSIGN,
	TOK_ANDASSIGN,
	TOK_XORASSIGN,
	TOK_ORASSIGN,
	/* Increment/decrement (++, --) */
	TOK_INCREMENT,
	TOK_DECREMENT,
	/* Special markers double (->, ~>) */
	TOK_ARROW,
	TOK_XARROW,
	/* Special markers single (@, #, $, :, ., &, /, -) */
	TOK_ATSIGN,
	TOK_NUMBERSIGN,
	TOK_DOLLAR,
	TOK_COLON,
	TOK_PERIOD,
	TOK_ADDRESSOF,
	TOK_SLASH,
	TOK_HYPHEN,
	/* Long arrow (-->) */
	TOK_LONGARROW,
	/* Ternary condition (?, :) */
	TOK_TERNARYIF,
	TOK_TERNARYELSE,
	/* Reinterpret, casting (<>, >-) */
	TOK_REINTERPRET,
	TOK_CAST,
	/* Pointer tags (*, ^) */
	TOK_POINTER,
	TOK_RPOINTER,
	/* Built-in commands/keywords */
	TOK_BASEOF,
	TOK_BREAK,
	TOK_CASE,
	TOK_CONST,
	TOK_CONTAINEROF,
	TOK_CONTINUE,
	TOK_DEFAULT,
	TOK_DOC,
	TOK_ENUM,
	TOK_ELSE,
	TOK_ELSIF,
	TOK_FUNC,
	TOK_FOR,
	TOK_GLOBAL,
	TOK_IF,
	TOK_IMPORT,
	TOK_INLINE,
	TOK_METHOD,
	TOK_NELEMS,
	TOK_OFFSETOF,
	TOK_PACKAGE,
	TOK_RETURN,
	TOK_SETATTR,
	TOK_SIZEOF,
	TOK_STATIC,
	TOK_STRUCT,
	TOK_SWITCH,
	TOK_TYPEOF,
	TOK_UNCONST,
	TOK_UNION,
	TOK_VAR,
	TOK_WHILE,
	/* Meta built-ins */
	TOK_NULL,
	TOK_TRUE,
	TOK_FALSE,
	TOK_FILE,
	TOK_LINE,
	/* Primitive types */
	TOK_TYPE,
	TOK_VOID,
	TOK_OPAQUE,
	TOK_BOOL,
	TOK_BYTE,
	TOK_CHAR,
	TOK_WCHAR,
	TOK_SHORT,
	TOK_USHORT,
	TOK_INT,
	TOK_UINT,
	TOK_LONG,
	TOK_ULONG,
	TOK_SIZE,
	TOK_INT8,
	TOK_UINT8,
	TOK_INT16,
	TOK_UINT16,
	TOK_INT32,
	TOK_UINT32,
	TOK_INT64,
	TOK_UINT64,
	TOK_INT128,
	TOK_UINT128,
	TOK_FLOAT32,
	TOK_FLOAT64,
	/* Pseudo-tokens */
	TOK_SUBSEQ,
	TOK_BLOCK,      /* { ... } */
	TOK_PEXPR,      /* ( ... ) */
	TOK_BEXPR,      /* [ ... ] */
	TOK_AEXPR,      /* < ... > */
	/* End-of-enum marker; keep last */
	TOK_LAST,
};
typedef enum CAC_TOK_TYPE toktype_e;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


/*
 * Module: input stream (typically, source-file).
 */
typedef struct module
{
	/* Input file path-name */
	string_t *path;

	/* Parent package name */
	string_t *pkg;

	/* Source data (raw) */
	const string_t *src;
} module_t;


/*
 * Numeral value embedded within integer and floating-point tokens
 */
typedef union toknumval {
	int64_t     i;
	uint64_t    u;
	double   f;
} toknumval_t;


typedef struct tokvalue {
	toknumval_t num;
	bool  wide;
	bool  uns;
} tokvalue_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Token meta-data descriptor
 */
typedef struct tokmeta {
	/* Code key */
	const char      *key;

	/* Internal name (debug) */
	const char      *name;

	/* Token's type */
	toktype_e       type;
} tokmeta_t;


/*
 * Tokens sequence
 */
typedef struct tokseq {
	/* Parent token */
	const struct token *tok;

	/* Tokens vector */
	vector_t vec;
} tokseq_t;


/*
 * Scanned token
 */
typedef struct token {
	/* Token's type */
	toktype_e       type;

	/* Position within module */
	size_t          pos;

	/* Line number */
	size_t          lno;

	/* Offset within line */
	size_t          off;

	/* String value */
	const string_t *str;

	/* Parent module */
	const module_t  *mod;

	/* Sub-sequence (optional) */
	tokseq_t        *seq;

	/* Numerical value (optional) */
	const string_t *num;
} token_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const tokmeta_t *paren_to_tokmeta(const string_t *str);

const tokmeta_t *special_delim_to_tokmeta(const string_t *str);

const tokmeta_t *keyword_or_builtin_to_tokmeta(const string_t *str);

const tokmeta_t *attribute_to_tokmeta(const string_t *str);

const tokmeta_t *whitespace_oper_to_tokmeta(const string_t *str);

const tokmeta_t *special_punct_to_tokmeta1(const string_t *str);

const tokmeta_t *special_punct_to_tokmeta2(const string_t *str);

const tokmeta_t *primitive_to_tokmeta(const string_t *str);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


token_t *token_dup(const token_t *tok);

token_t *token_dup2(const token_t *tok, toktype_e);

token_t *token_dup3(const token_t *tok, toktype_e, const string_t *);

token_t *token_dup4(const token_t *tok, const tokmeta_t *, const string_t *);

token_t *token_dup5(const token_t *tok, const tokmeta_t *);


token_t *token_new(toktype_e);

token_t *token_new2(const tokmeta_t *tok);

token_t *token_new_atom(char ch);

token_t *token_join(const token_t *, const token_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

size_t token_len(const token_t *);

bool token_haslen(const token_t *, size_t);

toktype_e token_gettype(const token_t *);

bool token_istype(const token_t *, toktype_e);

bool token_istype2(const token_t *, toktype_e, toktype_e);

bool token_isprimitive(const token_t *);

bool token_isatom(const token_t *);

char token_getchr0(const token_t *);

bool token_ischar(const token_t *, char);

bool token_ischarof(const token_t *, const char *);

bool token_isapunct(const token_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

tokseq_t *tokseq_new(size_t);

tokseq_t *tokseq_dup(const tokseq_t *);

size_t tokseq_len(const tokseq_t *);

bool tokseq_isempty(const tokseq_t *);

tokseq_t *tokseq_tail(const tokseq_t *, size_t);

tokseq_t *tokseq_sub(const tokseq_t *, size_t, size_t);

void tokseq_clear(tokseq_t *);

token_t *tokseq_at(const tokseq_t *, size_t);

void tokseq_append(tokseq_t *, token_t *);

void tokseq_append2(tokseq_t *, token_t *, token_t *);

void tokseq_append3(tokseq_t *, token_t *, token_t *, token_t *);

void tokseq_appendn(tokseq_t *, const tokseq_t *);

token_t *tokseq_front(const tokseq_t *);

token_t *tokseq_back(const tokseq_t *);

token_t *tokseq_pop_front(tokseq_t *);

void tokseq_push_front(tokseq_t *, token_t *);

token_t *tokseq_pop_back(tokseq_t *);

void tokseq_push_back(tokseq_t *, token_t *);

void tokseq_erase(tokseq_t *, size_t);

tokseq_t *tokseq_popn_front(tokseq_t *, size_t);

tokseq_t *tokseq_join(tokseq_t *, tokseq_t *);

token_t *tokseq_meld(const tokseq_t *, toktype_e);

string_t *tokseq_tostring(const tokseq_t *);


#endif /* CAC_TOKEN_H_ */

