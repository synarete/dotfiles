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
#define _GNU_SOURCE 1
#include "ast.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

ast_nodeseq_t *ast_nodeseq_new(void)
{
	ast_nodeseq_t *nseq;

	nseq = (ast_nodeseq_t *)gc_malloc(sizeof(*nseq));
	ast_nodeseq_init(nseq);
	return nseq;
}

void ast_nodeseq_init(ast_nodeseq_t *nseq)
{
	vector_init(&nseq->vec, 0);
}

void ast_nodeseq_append(ast_nodeseq_t *nseq, ast_node_t *node)
{
	vector_append(&nseq->vec, node);
}

size_t ast_nodeseq_size(const ast_nodeseq_t *nseq)
{
	return vector_size(&nseq->vec);
}

ast_node_t *ast_nodeseq_at(const ast_nodeseq_t *nseq, size_t pos)
{
	return (ast_node_t *)vector_at(&nseq->vec, pos);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

ast_root_t *ast_root_new(const module_t *module)
{
	ast_root_t *ast_root;

	ast_root = gc_malloc(sizeof(*ast_root));
	ast_nodeseq_init(&ast_root->import_decl_list);
	ast_nodeseq_init(&ast_root->const_decl_list);
	ast_nodeseq_init(&ast_root->staticvar_decl_list);
	ast_nodeseq_init(&ast_root->enum_decl_list);
	ast_nodeseq_init(&ast_root->struct_decl_list);
	ast_nodeseq_init(&ast_root->union_decl_list);
	ast_nodeseq_init(&ast_root->func_decl_list);
	ast_root->module = module;
	ast_root->package_decl = NULL;
	ast_root->comment = NULL;
	return ast_root;
}

ast_node_t *ast_node_new(ast_node_type_e type, const token_t *tok)
{
	ast_node_t *ast_node;

	ast_node = gc_malloc(sizeof(*ast_node));
	ast_node->type = type;
	ast_node->tok = tok;
	return ast_node;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

ast_ident_t *ast_ident_new(const token_t *tok)
{
	ast_ident_t *ident;

	ident = gc_malloc(sizeof(*ident));
	ident->token = tok;
	ident->name = tok->str;
	return ident;
}

static int ast_ident_compare(const ast_ident_t *id1, const ast_ident_t *id2)
{
	return string_compare(id1->name, id2->name);
}

bool ast_ident_isequal(const ast_ident_t *id1, const ast_ident_t *id2)
{
	return (ast_ident_compare(id1, id2) == 0);
}
