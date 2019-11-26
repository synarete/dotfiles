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

static ast_node_t *new_null_literal(const token_t *tok)
{
	return ast_node_new(AST_NULL_LITERAL, tok);
}

static ast_node_t *new_file_literal(const token_t *tok)
{
	return ast_node_new(AST_FILE_LITERAL, tok);
}

static ast_node_t *new_line_literal(const token_t *tok)
{
	ast_node_t *node = ast_node_new(AST_LINE_LITERAL, tok);
	node->u.line_literal.line = tok->lno;
	return node;
}

static ast_node_t *new_boolean_literal(const token_t *tok)
{
	ast_node_t *node = ast_node_new(AST_BOOLEAN_LITERAL, tok);
	node->u.boolean_literal.value = (tok->type == TOK_TRUE);
	return node;
}

static ast_node_t *new_character_literal(const token_t *tok, bool wide)
{
	ast_node_t *node = ast_node_new(AST_CHARACTER_LITERAL, tok);
	node->u.character_literal.wide = wide;
	node->u.character_literal.str = tok->str;
	return node;
}

static ast_node_t *new_string_literal(const token_t *tok, bool wide)
{
	ast_node_t *node = ast_node_new(AST_STRING_LITERAL, tok);
	node->u.string_literal.wide = wide;
	node->u.string_literal.str = tok->str;
	return node;
}

static ast_node_t *new_integer_literal(const token_t *tok)
{
	ast_node_t *node = ast_node_new(AST_INTEGER_LITERAL, tok);
	node->u.integer_literal.val = tok->str;
	return node;
}

static ast_node_t *new_float_literal(const token_t *tok)
{
	ast_node_t *node = ast_node_new(AST_FLOAT_LITERAL, tok);
	node->u.float_literal.val = tok->str;
	node->u.float_literal.wide = token_istype(tok, TOK_LFP_LITERAL);
	return node;
}

ast_node_t *parse_literal(tokseq_t *tseq)
{
	bool string_literal = false;
	token_t *tok, *lprefix_tok = NULL;
	ast_node_t *literal = NULL;

	lprefix_tok = parse_match(tseq, TOK_LPREFIX);
	if ((tok = parse_match(tseq, TOK_SQ_STRING)) != NULL) {
		literal = new_character_literal(tok, lprefix_tok != NULL);
		string_literal = true;
	} else if ((tok = parse_match(tseq, TOK_DQ_STRING)) != NULL) {
		literal = new_string_literal(tok, lprefix_tok != NULL);
		string_literal = true;
	} else if ((tok = parse_match(tseq, TOK_NULL)) != NULL) {
		literal = new_null_literal(tok);
	} else if ((tok = parse_match(tseq, TOK_FILE)) != NULL) {
		literal = new_file_literal(tok);
	} else if ((tok = parse_match(tseq, TOK_LINE)) != NULL) {
		literal = new_line_literal(tok);
	} else if ((tok = parse_match(tseq, TOK_TRUE)) != NULL) {
		literal = new_boolean_literal(tok);
	} else if ((tok = parse_match(tseq, TOK_FALSE)) != NULL) {
		literal = new_boolean_literal(tok);
	} else if ((tok = parse_match(tseq, TOK_INT_LITERAL)) != NULL) {
		literal = new_integer_literal(tok);
	} else if ((tok = parse_match(tseq, TOK_FP_LITERAL)) != NULL) {
		literal = new_float_literal(tok);
	} else if ((tok = parse_match(tseq, TOK_LFP_LITERAL)) != NULL) {
		literal = new_float_literal(tok);
	} else {
		compile_error(tseq->tok, "illegal literal");
	}
	if (lprefix_tok && !string_literal) {
		compile_error(lprefix_tok, "missing string literal");
	}
	return literal;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ast_node_t *parse_assign_rhs(tokseq_t *tseq)
{
	return parse_literal(tseq); /* TODO: FIXME */
}

ast_ident_t *parse_ident(tokseq_t *tseq)
{
	return ast_ident_new(parse_consume(tseq, TOK_IDENT));
}

ast_node_t *parse_var_decl(tokseq_t *tseq)
{
	token_t *tok;
	ast_node_t *node;
	ast_var_decl_t *var_decl;

	tok = parse_consume(tseq, TOK_VAR);
	node = ast_node_new(AST_VAR_DECL, tok);
	var_decl = &node->u.var_decl;

	var_decl->attr = parse_attributes(tseq);
	var_decl->typname = parse_typename(tseq);
	var_decl->ident = parse_ident(tseq);

	if ((tok = parse_match(tseq, TOK_ASSIGN)) != NULL) {
		var_decl->rhs = parse_assign_rhs(tseq);
	} else if ((tok = parse_match(tseq, TOK_XASSIGN)) != NULL) {
		var_decl->isfinal = true;
		var_decl->rhs = parse_assign_rhs(tseq);
	}
	parse_end(tseq);

	return node;
}

ast_node_t *parse_global_var(tokseq_t *tseq, ast_root_t *ast_root)
{
	ast_node_t *node;
	const ast_attributes_t *attr;

	node = parse_var_decl(tseq);
	attr = node->u.var_decl.attr;
	if (!attr->attr_global && !attr->attr_static) {
		compile_error(attr->tok, "missing static or global attribute");
	}
	ast_nodeseq_append(&ast_root->staticvar_decl_list, node);
	return node;
}


