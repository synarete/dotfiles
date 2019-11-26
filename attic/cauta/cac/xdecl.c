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

ast_node_t *parse_import_decl(tokseq_t *tseq)
{
	ast_node_t *node;
	ast_import_decl_t *import_decl;

	node = parse_node_head(tseq, TOK_IMPORT, AST_IMPORT_DECL);
	import_decl = &node->u.import_decl;
	import_decl->ident = parse_ident(tseq);
	parse_consume(tseq, TOK_XASSIGN);
	import_decl->package_name = parse_package_name(tseq);
	parse_end(tseq);

	return node;
}

ast_node_t *parse_import(tokseq_t *tseq, ast_root_t *ast_root)
{
	ast_node_t *node;

	node = parse_import_decl(tseq);
	ast_nodeseq_append(&ast_root->import_decl_list, node);
	return node;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

ast_node_t *parse_const_decl(tokseq_t *tseq)
{
	ast_node_t *node;

	node = parse_node_head(tseq, TOK_CONST, AST_CONST_DECL);
	node->u.alias_decl.ident = parse_ident(tseq);
	parse_consume(tseq, TOK_XASSIGN);
	node->u.alias_decl.literal = parse_literal(tseq);
	parse_end(tseq);

	return node;
}

ast_node_t *parse_const(tokseq_t *tseq, ast_root_t *ast_root)
{
	ast_node_t *node;

	node = parse_const_decl(tseq);
	ast_nodeseq_append(&ast_root->const_decl_list, node);
	return node;
}
