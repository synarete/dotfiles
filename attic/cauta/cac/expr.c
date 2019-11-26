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


ast_node_t *parse_setattr(tokseq_t *tseq)
{
	token_t *tok;
	tokseq_t *pexpr = NULL;
	tokseq_t *block = NULL;
	ast_node_t *fn_decl;

	tok = parse_consume(tseq, TOK_SETATTR);
	tok = parse_consume(tseq, TOK_IDENT);

	fn_decl = ast_node_new(AST_SETATTR_DECL, tok);
	fn_decl->u.func_decl.ident = ast_ident_new(tok);

	/* TODO: Parse function args */
	pexpr = parse_pexpr(tseq);
	(void)pexpr;

	block = parse_block(tseq);
	(void)block;

	return fn_decl;
}


ast_node_t *parse_conditional_expression(tokseq_t *tseq)
{
	return parse_literal(tseq);
}

ast_node_t *parse_contant_expression(tokseq_t *tseq)
{
	return parse_conditional_expression(tseq);
}
