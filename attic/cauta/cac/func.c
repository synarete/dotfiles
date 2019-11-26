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


static ast_node_t *parse_param_decl(tokseq_t *tseq)
{
	ast_node_t *node;

	node = ast_node_new(AST_PARAM_DECL, tokseq_front(tseq));
	node->u.param_decl.typname = parse_typename(tseq);
	node->u.param_decl.ident = parse_ident(tseq);

	return node;
}

static ast_nodeseq_t *parse_parameters(tokseq_t *tseq)
{
	ast_nodeseq_t *nodeseq;

	nodeseq = ast_nodeseq_new();
	while (!parse_isend(tseq)) {
		ast_nodeseq_append(nodeseq, parse_param_decl(tseq));
		parse_consume_ifnotend(tseq, TOK_COMMA);
	}
	return nodeseq;
}

static ast_node_t *parse_signature(tokseq_t *tseq)
{
	ast_node_t *node;

	node = ast_node_new(AST_SIGNATURE, tokseq_front(tseq));
	node->u.signature.parameters = parse_parameters(parse_pexpr(tseq));
	if (parse_match(tseq, TOK_LONGARROW)) {
		node->u.signature.typename = parse_typename(tseq);
	} else {
		node->u.signature.typename = NULL;
	}
	return node;
}

ast_node_t *parse_node_head(tokseq_t *tseq, toktype_e tt, ast_node_type_e nt)
{
	token_t *tok;
	ast_node_t *node;

	tok = parse_consume(tseq, tt);
	node = ast_node_new(nt, tok);
	return node;
}

ast_node_t *parse_func_decl(tokseq_t *tseq)
{
	tokseq_t *block;
	ast_node_t *node;

	node = parse_node_head(tseq, TOK_FUNC, AST_FUNC_DECL);
	node->u.func_decl.attr = parse_attributes(tseq);
	node->u.func_decl.ident = parse_ident(tseq);
	node->u.func_decl.signature = parse_signature(tseq);


	/* TODO: Parse function body */
	block = parse_block(tseq);
	(void)block;

	return node;
}

ast_node_t *parse_func(tokseq_t *tseq, ast_root_t *ast_root)
{
	ast_node_t *node;

	node = parse_func_decl(tseq);
	ast_nodeseq_append(&ast_root->func_decl_list, node);
	return node;
}


