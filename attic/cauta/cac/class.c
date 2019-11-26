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
#include "token.h"
#include "lexer.h"
#include "parser.h"


static ast_attributes_t *new_attributes(const token_t *tok)
{
	ast_attributes_t *attr;

	attr = gc_malloc(sizeof(*attr));
	attr->tok = tok;
	return attr;
}

static void parse_attributes_list(tokseq_t *tseq, ast_attributes_t *attributes)
{
	token_t *tok;
	const string_t *str;

	while (!parse_isend(tseq)) {
		if ((tok = parse_match(tseq, TOK_STATIC)) != NULL) {
			attributes->attr_static = true;
		} else if ((tok = parse_match(tseq, TOK_INLINE)) != NULL) {
			attributes->attr_inline = true;
		} else if ((tok = parse_match(tseq, TOK_GLOBAL)) != NULL) {
			attributes->attr_global = true;
		} else {
			tok = tokseq_front(tseq);
			str = tok->str;
			compile_error(tok, "unknown attribute: %s", str->str);
		}
		parse_consume_ifnotend(tseq, TOK_COMMA);
	}
}

ast_attributes_t *parse_attributes(tokseq_t *tseq)
{
	token_t *tok;
	tokseq_t *attr_tseq;
	ast_attributes_t *attributes;

	attributes = new_attributes(tokseq_front(tseq));
	tok = parse_match(tseq, TOK_BEXPR);
	if (tok) {
		cac_assert(tok->seq);
		attributes = new_attributes(tok);
		attr_tseq = scan_attributes_list(tok->seq);
		parse_attributes_list(attr_tseq, attributes);
	}
	return attributes;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ast_typename_t *ast_new_typename(void)
{
	ast_typename_t *typename;

	typename = gc_malloc(sizeof(*typename));
	typename->name = NULL;
	typename->pointer = NULL;
	typename->prefix = NULL;
	typename->primitive = false;

	return typename;
}

static ast_typename_t *parse_typename_head(tokseq_t *tseq)
{
	token_t *tok;
	const tokmeta_t *tokmeta;
	ast_ident_t *ident;
	ast_typename_t *typename;

	tok = parse_consume(tseq, TOK_IDENT);
	ident = ast_ident_new(tok);

	typename = ast_new_typename();
	if (parse_match(tseq, TOK_PERIOD)) {
		tok = parse_consume(tseq, TOK_IDENT);
		typename->prefix = ident;
		typename->name = ast_ident_new(tok);
	} else {
		tokmeta = primitive_to_tokmeta(tok->str);
		if (tokmeta != NULL) {
			tok->type = tokmeta->type;
			typename->primitive = true;
		}
		typename->name = ident;
	}
	return typename;
}

static token_t *parse_match_pointer_tag(tokseq_t *tseq)
{
	token_t *tok;

	tok = parse_match(tseq, TOK_POINTER);
	if (tok != NULL) {
		return tok;
	}
	tok = parse_match(tseq, TOK_RPOINTER);
	if (tok != NULL) {
		return tok;
	}
	return NULL;
}

static ast_pointer_t *parse_pointer(tokseq_t *tseq)
{
	size_t cnt = 0;
	token_t *tok = NULL;
	ast_pointer_t *ptr, *cur, *prv;

	ptr = cur = prv = NULL;
	tok = parse_match_pointer_tag(tseq);
	while (tok != NULL) {
		cur = (ast_pointer_t *)gc_malloc(sizeof(*cur));
		cur->pointer = NULL;
		cur->token = tok;
		cur->rw = token_istype(tok, TOK_POINTER);

		if (prv) {
			prv->pointer = cur;
		}
		if (!ptr) {
			ptr = cur;
		}
		prv = cur;
		++cnt;
		tok = parse_match_pointer_tag(tseq);
	}
	if (cnt > 2) {
		compile_error(tseq->tok, "too many pointer tags");
	}
	return ptr;
}

ast_typename_t *parse_typename(tokseq_t *tseq)
{
	ast_typename_t *typename;

	typename = parse_typename_head(tseq);
	if (parse_ispeek(tseq, TOK_COLON)) {
		parse_consume(tseq, TOK_COLON);
	} else if (parse_ispeek(tseq, TOK_POINTER)) {
		typename->pointer = parse_pointer(tseq);
	} else if (parse_ispeek(tseq, TOK_RPOINTER)) {
		typename->pointer = parse_pointer(tseq);
	} else {
		compile_error(tokseq_front(tseq), "missing type tag");
	}
	return typename;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ast_node_t *parse_field_decl(tokseq_t *tseq)
{
	ast_node_t *node;

	node = ast_node_new(AST_FIELD_DECL, tokseq_front(tseq));
	node->u.field_decl.typname = parse_typename(tseq);
	node->u.field_decl.ident = parse_ident(tseq);

	return node;
}

static ast_nodeseq_t *parse_struct_body(tokseq_t *block)
{
	ast_node_t *field_decl;
	ast_nodeseq_t *nodeseq;

	nodeseq = ast_nodeseq_new();
	while (!parse_isend(block)) {
		field_decl = parse_field_decl(parse_subseq(block));
		ast_nodeseq_append(nodeseq, field_decl);
	}
	if (!nodeseq->vec.len) {
		compile_error(block->tok, "empty struct body");
	}
	return nodeseq;
}

ast_node_t *parse_struct_decl(tokseq_t *tseq)
{
	ast_node_t *node;

	node = parse_node_head(tseq, TOK_STRUCT, AST_STRUCT_DECL);
	node->u.struct_decl.ident = parse_ident(tseq);
	node->u.struct_decl.body = parse_struct_body(parse_block(tseq));
	return node;
}

ast_node_t *parse_struct(tokseq_t *tseq, ast_root_t *ast_root)
{
	ast_node_t *node;

	node = parse_struct_decl(tseq);
	ast_nodeseq_append(&ast_root->struct_decl_list, node);
	return node;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ast_nodeseq_t *parse_union_body(tokseq_t *block)
{
	ast_node_t *field_decl;
	ast_nodeseq_t *nodeseq;

	nodeseq = ast_nodeseq_new();
	while (!parse_isend(block)) {
		field_decl = parse_field_decl(parse_subseq(block));
		ast_nodeseq_append(nodeseq, field_decl);
	}
	if (!nodeseq->vec.len) {
		compile_error(block->tok, "empty union body");
	}
	return nodeseq;
}

ast_node_t *parse_union_decl(tokseq_t *tseq)
{
	ast_node_t *node;

	node = parse_node_head(tseq, TOK_UNION, AST_UNION_DECL);
	node->u.union_decl.ident = parse_ident(tseq);
	node->u.union_decl.body = parse_union_body(parse_block(tseq));
	return node;
}

ast_node_t *parse_union(tokseq_t *tseq, ast_root_t *ast_root)
{
	ast_node_t *node;

	node = parse_union_decl(tseq);
	ast_nodeseq_append(&ast_root->union_decl_list, node);
	return node;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static ast_node_t *parse_enum_constant(tokseq_t *tseq)
{
	ast_node_t *node;

	node = parse_node_head(tseq, TOK_IDENT, AST_ENUM_CONSTANT);
	node->u.enum_constant_decl.ident = ast_ident_new(node->tok);

	if (!parse_isend(tseq) && !parse_ispeek(tseq, TOK_COMMA)) {
		parse_consume(tseq, TOK_ASSIGN);
		node->u.enum_constant_decl.value =
		        parse_contant_expression(tseq);
	}
	return node;
}

static ast_nodeseq_t *parse_enumerator_list(tokseq_t *tseq)
{
	ast_nodeseq_t *nodeseq;

	nodeseq = ast_nodeseq_new();
	while (!parse_isend(tseq)) {
		ast_nodeseq_append(nodeseq, parse_enum_constant(tseq));
		parse_consume_ifnotend(tseq, TOK_COMMA);
	}
	return nodeseq;
}

static tokseq_t *parse_enum_decl_body(tokseq_t *tseq)
{
	tokseq_t *block, *body;

	block = parse_block(tseq);
	body = parse_subseq(block);
	parse_end(block);
	return body;
}

ast_node_t *parse_enum_decl(tokseq_t *tseq)
{
	ast_node_t *node;

	node = parse_node_head(tseq, TOK_ENUM, AST_ENUM_DECL);
	node->u.enum_decl.ident = parse_ident(tseq);
	node->u.enum_decl.enumerator_list =
	        parse_enumerator_list(parse_enum_decl_body(tseq));
	return node;
}

ast_node_t *parse_enum(tokseq_t *tseq, ast_root_t *ast_root)
{
	ast_node_t *node;

	node = parse_enum_decl(tseq);
	ast_nodeseq_append(&ast_root->enum_decl_list, node);
	return node;
}


