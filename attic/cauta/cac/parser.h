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
#ifndef CAC_PARSER_H_
#define CAC_PARSER_H_

#include "infra.h"
#include "lexer.h"
#include "ast.h"


/*
 * Parser & code-generation execution context.
 */
typedef struct context {

	/* Major module */
	const module_t *module;

	/* ASTs hash-table by module */
	htbl_t   *ast_htbl;

	/* Output result */
	string_t *output;

} context_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

context_t *new_context(void);

ast_root_t *parse_input(context_t *, const string_t *);

ast_root_t *find_ast(const context_t *, const string_t *);

ast_root_t *path_to_ast(const string_t *);

ast_root_t *text_to_ast(const char *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

const token_t *parse_head(const tokseq_t *tseq);

bool parse_isend(const tokseq_t *tseq);

toktype_e parse_getpeek(const tokseq_t *tseq);

bool parse_ispeek(const tokseq_t *tseq, toktype_e);

token_t *parse_match(tokseq_t *tseq, toktype_e);

token_t *parse_consume(tokseq_t *tseq, toktype_e);

token_t *parse_xconsume(tokseq_t *tseq, toktype_e, toktype_e);

token_t *parse_consume_ifnotend(tokseq_t *tseq, toktype_e tt);

void parse_end(const tokseq_t *tseq);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

tokseq_t *parse_subseq(tokseq_t *tseq);

tokseq_t *parse_block(tokseq_t *tseq);

tokseq_t *parse_pexpr(tokseq_t *tseq);

tokseq_t *parse_bexpr(tokseq_t *tseq);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

ast_typename_t *parse_typename(tokseq_t *tseq);

ast_attributes_t *parse_attributes(tokseq_t *tseq);

ast_ident_t *parse_ident(tokseq_t *tseq);

ast_node_t *parse_node_head(tokseq_t *tseq, toktype_e tt, ast_node_type_e nt);


ast_node_t *parse_setattr(tokseq_t *tseq);

ast_node_t *parse_package_name(tokseq_t *tseq);

ast_node_t *parse_literal(tokseq_t *tseq);


ast_node_t *parse_package(tokseq_t *tseq, ast_root_t *ast_root);

ast_node_t *parse_package_decl(tokseq_t *tseq);

ast_node_t *parse_import(tokseq_t *tseq, ast_root_t *ast_root);

ast_node_t *parse_import_decl(tokseq_t *tseq);

ast_node_t *parse_const(tokseq_t *tseq, ast_root_t *ast_root);

ast_node_t *parse_const_decl(tokseq_t *tseq);

ast_node_t *parse_enum(tokseq_t *tseq, ast_root_t *ast_root);

ast_node_t *parse_enum_decl(tokseq_t *tseq);

ast_node_t *parse_func(tokseq_t *tseq, ast_root_t *ast_root);

ast_node_t *parse_func_decl(tokseq_t *tseq);

ast_node_t *parse_global_var(tokseq_t *tseq, ast_root_t *ast_root);

ast_node_t *parse_var_decl(tokseq_t *tseq);

ast_node_t *parse_struct(tokseq_t *tseq, ast_root_t *ast_root);

ast_node_t *parse_struct_decl(tokseq_t *tseq);

ast_node_t *parse_union(tokseq_t *tseq, ast_root_t *ast_root);

ast_node_t *parse_union_decl(tokseq_t *tseq);


ast_node_t *parse_conditional_expression(tokseq_t *tseq);

ast_node_t *parse_contant_expression(tokseq_t *tseq);



#endif /* CAC_PARSER_H_ */


