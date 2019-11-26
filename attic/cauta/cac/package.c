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
#include "parser.h"



/* Maximum number of libs-namespace nesting */
#define CAC_LIMIT_LIBSNAME_SUB_MAX    (8)

/* Maximum number of sub-packages nesting */
#define CAC_LIMIT_PACKAGE_SUB_MAX     (16)



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static const string_t *
joinwith(const token_t *tok1, char c, const token_t *tok2)
{
	const char sep[2] = { c, '\0' };

	return string_join(string_joins(tok1->str, sep), tok2->str);
}

static ast_ident_t *parse_package_libs_name(tokseq_t *tseq)
{
	size_t cnt, lim;
	token_t *tok, *libs_tok;

	tok = libs_tok = NULL;
	cnt = 0;
	lim = CAC_LIMIT_LIBSNAME_SUB_MAX;
	while (cnt++ < lim) {
		if (cnt == lim) {
			compile_error(tok, "libs name too long");
		}
		tok = parse_consume(tseq, TOK_IDENT);
		if (libs_tok == NULL) {
			libs_tok = token_dup(tok);
		} else {
			libs_tok->str = joinwith(libs_tok, '-', tok);
		}
		if (!parse_match(tseq, TOK_HYPHEN)) {
			break;
		}
	}
	return ast_ident_new(libs_tok);
}

static void parse_package_path_name(tokseq_t *tseq,
                                    ast_ident_t **out1, ast_ident_t **out2)
{
	size_t cnt = 0, lim = CAC_LIMIT_PACKAGE_SUB_MAX;
	token_t *tok, *path_tok, *name_tok;

	tok = path_tok = name_tok = NULL;
	while (cnt++ < lim) {
		if (cnt == lim) {
			compile_error(tok, "too many package subs");
		}
		name_tok = tok = parse_consume(tseq, TOK_IDENT);
		if (path_tok == NULL) {
			path_tok = token_dup(tok);
		} else {
			path_tok->str = joinwith(path_tok, '/', tok);
		}
		if (!parse_match(tseq, TOK_SLASH)) {
			break;
		}
	}
	*out1 = ast_ident_new(path_tok);
	*out2 = ast_ident_new(name_tok);
}

static string_t *hyphens_to_underscores(string_t *str)
{
	for (size_t i = 0; i < str->len; ++i) {
		if (str->str[i] == '-') {
			str->str[i] = '_';
		}
	}
	return str;
}

static ast_ident_t *libs_name(const ast_ident_t *libs)
{
	ast_ident_t *name;

	name = ast_ident_new(libs->token);
	name->name = hyphens_to_underscores(string_dup(libs->name));
	return name;
}

ast_node_t *parse_package_name(tokseq_t *tseq)
{
	ast_ident_t *prefix;
	ast_ident_t *libs = NULL;
	ast_ident_t *path = NULL;
	ast_ident_t *name = NULL;
	ast_node_t *package_name;

	libs = parse_package_libs_name(tseq);
	prefix = libs_name(libs);
	if (parse_isend(tseq)) {
		/* lib only */
		path = libs;
		name = prefix;
	} else {
		/* lib and path-name */
		parse_consume(tseq, TOK_SLASH);
		parse_package_path_name(tseq, &path, &name);
	}
	package_name = ast_node_new(AST_PACKAGE_NAME, tseq->tok);
	package_name->u.package_name.libs = libs;
	package_name->u.package_name.prefix = prefix;
	package_name->u.package_name.path = path;
	package_name->u.package_name.name = name;

	return package_name;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void verify_package_limit(const tokseq_t *tseq)
{
	size_t len, lim;
	const token_t *tok = tseq->tok;

	len = tokseq_len(tseq);
	lim = (CAC_LIMIT_LIBSNAME_SUB_MAX + CAC_LIMIT_PACKAGE_SUB_MAX - 1);
	if ((len < 1) || (len > lim)) {
		compile_error(tok, "illegal package name: %s", tok->str);
	}
}

ast_node_t *parse_package_decl(tokseq_t *tseq)
{
	ast_node_t *package_name, *package_decl;

	parse_consume(tseq, TOK_PACKAGE);
	verify_package_limit(tseq);
	package_name = parse_package_name(tseq);
	parse_end(tseq);
	package_decl = ast_node_new(AST_PACKAGE_DECL, package_name->tok);
	package_decl->u.package_decl.name = package_name;

	return package_decl;
}

ast_node_t *parse_package(tokseq_t *tseq, ast_root_t *ast_root)
{
	ast_node_t *node = NULL;

	if (ast_root->package_decl) {
		compile_error(tokseq_front(tseq), "redundant package");
	}
	node = parse_package_decl(tseq);
	ast_root->package_decl = node;
	return node;
}
