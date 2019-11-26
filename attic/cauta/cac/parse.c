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


const token_t *parse_head(const tokseq_t *tseq)
{
	const token_t *tok = tokseq_front(tseq);
	return ((tok != NULL) ? tok : tseq->tok);
}

bool parse_isend(const tokseq_t *tseq)
{
	return tokseq_isempty(tseq);
}

toktype_e parse_getpeek(const tokseq_t *tseq)
{
	const token_t *tok = tokseq_front(tseq);
	return ((tok != NULL) ? token_gettype(tok) : TOK_NONE);
}

bool parse_ispeek(const tokseq_t *tseq, toktype_e tt)
{
	return (parse_getpeek(tseq) == tt);
}

token_t *parse_match(tokseq_t *tseq, toktype_e tt)
{
	token_t *tok;

	tok = tokseq_front(tseq);
	if (tok == NULL) {
		return NULL;
	}
	if (!token_istype(tok, tt)) {
		return NULL;
	}
	tokseq_pop_front(tseq);
	return tok;
}

void parse_end(const tokseq_t *tseq)
{
	const token_t *tok;

	tok = tokseq_front(tseq);
	if (tok != NULL) {
		compile_error(tok, "redundent token: %s", tok->str->str);
	}
}

token_t *parse_consume(tokseq_t *tseq, toktype_e tt)
{
	const token_t *tok = tokseq_front(tseq);
	if (tok == NULL) {
		compile_error(tseq->tok, "missing token");
	}
	if (!token_istype(tok, tt)) {
		compile_error(tok, "unexpected token: %s", tok->str->str);
	}
	return tokseq_pop_front(tseq);
}

token_t *parse_xconsume(tokseq_t *tseq, toktype_e tt1, toktype_e tt2)
{
	parse_consume(tseq, tt1);
	return parse_consume(tseq, tt2);
}

token_t *parse_consume_ifnotend(tokseq_t *tseq, toktype_e tt)
{
	token_t *tok = NULL;

	if (!parse_isend(tseq)) {
		tok = parse_consume(tseq, tt);
	}
	return tok;
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static tokseq_t *pop_tokseqt(tokseq_t *tseq, toktype_e tt)
{
	token_t *tok;

	tok = tokseq_front(tseq);
	if (tok == NULL) {
		compile_error(tseq->tok, "syntax error");
	}
	if (tok->seq == NULL) {
		compile_error(tok, "syntax error");
	}
	tok = parse_consume(tseq, tt);
	return tok->seq;
}

tokseq_t *parse_subseq(tokseq_t *tseq)
{
	return pop_tokseqt(tseq, TOK_SUBSEQ);
}

tokseq_t *parse_block(tokseq_t *tseq)
{
	return pop_tokseqt(tseq, TOK_BLOCK);
}

tokseq_t *parse_pexpr(tokseq_t *tseq)
{
	return pop_tokseqt(tseq, TOK_PEXPR);
}

tokseq_t *parse_bexpr(tokseq_t *tseq)
{
	return pop_tokseqt(tseq, TOK_BEXPR);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static void syntax_error(const tokseq_t *tseq)
{
	compile_error(parse_head(tseq), "syntax error");
}

static ast_node_t *ast_new_comment(const token_t *tok,
                                   bool singleline)
{
	ast_node_t *node;

	node = ast_node_new(AST_COMMENT, tok);
	node->u.comment.text = tok->str;
	node->u.comment.singleline = singleline;
	return node;
}

static ast_node_t *parse_ml_comment(tokseq_t *tseq)
{
	return ast_new_comment(parse_consume(tseq, TOK_ML_COMMENT), false);
}

static ast_node_t *parse_sl_comment(tokseq_t *tseq)
{
	return ast_new_comment(parse_consume(tseq, TOK_SL_COMMENT), true);
}

static ast_node_t *parse_module_ast(ast_root_t *ast_root, tokseq_t *tseq)
{
	toktype_e tt;
	ast_node_t *node = NULL;

	tt = parse_getpeek(tseq);
	switch ((int)tt) {
		case TOK_ML_COMMENT:
			node = parse_ml_comment(tseq);
			break;
		case TOK_SL_COMMENT:
			node = parse_sl_comment(tseq);
			break;
		case TOK_PACKAGE:
			node = parse_package(tseq, ast_root);
			break;
		case TOK_IMPORT:
			node = parse_import(tseq, ast_root);
			break;
		case TOK_CONST:
			node = parse_const(tseq, ast_root);
			break;
		case TOK_VAR:
			node = parse_global_var(tseq, ast_root);
			break;
		case TOK_ENUM:
			node = parse_enum(tseq, ast_root);
			break;
		case TOK_STRUCT:
			node = parse_struct(tseq, ast_root);
			break;
		case TOK_UNION:
			node = parse_union(tseq, ast_root);
			break;
		case TOK_FUNC:
			node = parse_func(tseq, ast_root);
			break;
		default:
			syntax_error(tseq);
			break;
	}
	return node;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ast_root_t *parse_tokens_to_ast(tokseq_t *tseq)
{
	ast_root_t *ast;
	const token_t *tok;

	ast = ast_root_new(tseq->tok->mod);
	tok = tokseq_pop_front(tseq);
	while (tok != NULL) {
		if (tok->type != TOK_SUBSEQ) {
			compile_error(tok, "syntax error");
		}
		cac_assert(tok->seq != NULL);
		parse_module_ast(ast, tok->seq);
		tok = tokseq_pop_front(tseq);
	}
	return ast;
}

static ast_root_t *parse_atoms_to_ast(tokseq_t *atoms)
{
	return parse_tokens_to_ast(scan_atoms_to_tokens(atoms));
}

ast_root_t *path_to_ast(const string_t *path)
{
	return parse_atoms_to_ast(scan_file_to_atoms(path));
}

ast_root_t *text_to_ast(const char *txt)
{
	return parse_atoms_to_ast(scan_source_to_atoms(string_new(txt)));
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


static void ctx_init(context_t *ctx)
{
	ctx->ast_htbl = htbl_new(string_hfns);
	ctx->output = string_new("#include <stdlib.h> \n#include <stdio.h>\n");
	ctx->module = NULL;
}

context_t *new_context(void)
{
	context_t *ctx;

	ctx = (context_t *)gc_malloc(sizeof(*ctx));
	ctx_init(ctx);

	return ctx;
}

static void store_ast(context_t *ctx, ast_root_t *ast, int major)
{
	htbl_insert(ctx->ast_htbl, ast->module->path, ast);
	if (major) {
		ctx->module = ast->module;
	}
}

ast_root_t *find_ast(const context_t *ctx, const string_t *mod_path)
{
	return (ast_root_t *)htbl_find(ctx->ast_htbl, mod_path);
}

ast_root_t *parse_input(context_t *ctx, const string_t *path)
{
	ast_root_t *ast;

	ast = path_to_ast(path);
	store_ast(ctx, ast, 1);
	return ast;
}
