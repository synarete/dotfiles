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
#ifndef CAC_AST_H_
#define CAC_AST_H_

#include "infra.h"
#include "lexer.h"

/*
 * AST nodes types
 */
enum AST_NODE_TYPE {
	AST_NONE,
	AST_BLOCK,
	AST_BREAK,
	AST_CASE,
	AST_COMMAND,
	AST_COMMENT,
	AST_CONST_DECL,
	AST_CONTINUE,
	AST_DEFAULT,
	AST_DOC,
	AST_ELSE,
	AST_ELSIF,
	AST_ENUM_CONSTANT,
	AST_ENUM_DECL,
	AST_FIELD_DECL,
	AST_FUNC_DECL,
	AST_FOR,
	AST_IF,
	AST_IMPORT_DECL,
	AST_BOOLEAN_LITERAL,
	AST_INTEGER_LITERAL,
	AST_NULL_LITERAL,
	AST_CHARACTER_LITERAL,
	AST_STRING_LITERAL,
	AST_FILE_LITERAL,
	AST_LINE_LITERAL,
	AST_FLOAT_LITERAL,
	AST_METHOD_DECL,
	AST_PACKAGE_NAME,
	AST_PACKAGE_DECL,
	AST_PARAM_DECL,
	AST_PROGRAM,
	AST_RETURN,
	AST_SETATTR_DECL,
	AST_SIGNATURE,
	AST_SIZEOF,
	AST_STATIC_DECL,
	AST_STATICVAR_DECL,
	AST_STRUCT_DECL,
	AST_SWITCH,
	AST_TUPLE,
	AST_TYPEOF,
	AST_UNION_DECL,
	AST_WHILE,
	AST_LOCAL_DECL,
	AST_VAR_DECL,
};
typedef enum AST_NODE_TYPE ast_node_type_e;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* AST forward-declarations */
struct ast_node;
struct ast_root;
typedef struct ast_node ast_node_t;
typedef struct ast_root ast_root_t;



/* Identifier */
typedef struct ast_ident {
	const token_t *token;
	const string_t *name;
} ast_ident_t;


/* Pointer descriptor */
typedef struct ast_pointer {
	const token_t *token;
	struct ast_pointer *pointer;
	bool rw;
} ast_pointer_t;


/* Type-name descriptor */
typedef struct ast_typename {
	ast_ident_t *prefix;
	ast_ident_t *name;
	ast_pointer_t *pointer;
	bool primitive;
} ast_typename_t;


/* Attributes set */
typedef struct ast_attributes {
	const token_t *tok;
	bool attr_static;
	bool attr_inline;
	bool attr_global;
} ast_attributes_t;


/* AST nodes-sequence */
typedef struct ast_nodeseq {
	vector_t vec;
} ast_nodeseq_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* AST sub-nodes */

typedef struct ast_comment {
	bool singleline;
	const string_t *text;
} ast_comment_t;


typedef struct ast_file_literal {
	const string_t *file;
} ast_file_literal_t;


typedef struct ast_line_literal {
	size_t line;
} ast_line_literal_t;


typedef struct ast_boolean_literal {
	bool value;
} ast_boolean_literal_t;


typedef struct ast_character_literal {
	const string_t *str;
	bool wide;
} ast_character_literal_t;


typedef struct ast_string_literal {
	const string_t *str;
	bool wide;
} ast_string_literal_t;


typedef struct ast_integer_literal {
	const string_t *val;
} ast_integer_literal_t;


typedef struct ast_float_literal {
	bool wide;
	const string_t *val;
} ast_float_literal_t;


typedef struct ast_param_decl {
	ast_typename_t *typname;
	ast_ident_t *ident;
} ast_param_decl_t;


typedef struct ast_signature {
	ast_nodeseq_t *parameters;
	ast_typename_t *typename;
} ast_signature_t;


typedef struct ast_var_decl {
	ast_attributes_t *attr;
	ast_typename_t *typname;
	ast_ident_t *ident;
	ast_node_t *rhs;
	bool isglobal;
	bool isstatic;
	bool isfinal;
} ast_var_decl_t;


typedef struct ast_package_name {
	ast_ident_t *libs;
	ast_ident_t *prefix;
	ast_ident_t *path;
	ast_ident_t *name;
} ast_package_name_t;


typedef struct ast_package_decl {
	ast_node_t *name;
} ast_package_decl_t;


typedef struct ast_import_decl {
	ast_ident_t *ident;
	ast_node_t *package_name;
} ast_import_decl_t;


typedef struct ast_alias_decl {
	ast_ident_t *ident;
	ast_node_t *literal;
} ast_alias_decl_t;


typedef struct ast_static_decl {
	ast_node_t *var_decl;
} ast_static_decl_t;


typedef struct ast_staticvar_decl {
	ast_node_t *var_decl;
	bool isglobal;
} ast_staticvar_decl_t;


typedef struct ast_enum_constant_decl {
	ast_ident_t *ident;
	ast_node_t *value;
} ast_enum_constant_decl_t;


typedef struct ast_func_decl {
	ast_ident_t *ident;
	ast_attributes_t *attr;
	ast_node_t *signature;
} ast_func_decl_t;


typedef struct ast_method_decl {
	ast_ident_t *ident;
	string_t        *type;
	string_t        *name;
} ast_method_decl_t;


typedef struct ast_setattr_decl {
	ast_ident_t     *attr;
	ast_ident_t     *ident;
} ast_setattr_decl_t;


typedef struct ast_field_decl {
	ast_typename_t *typname;
	ast_ident_t *ident;
} ast_field_decl_t;


typedef struct ast_enum_decl {
	ast_ident_t *ident;
	ast_nodeseq_t *enumerator_list;
} ast_enum_decl_t;


typedef struct ast_struct_decl {
	ast_ident_t *ident;
	ast_nodeseq_t *body;
} ast_struct_decl_t;


typedef struct ast_union_decl {
	ast_ident_t *ident;
	ast_nodeseq_t *body;
} ast_union_decl_t;

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

typedef union ast_node_union {
	ast_comment_t                   comment;
	ast_file_literal_t              file_literal;
	ast_line_literal_t              line_literal;
	ast_boolean_literal_t           boolean_literal;
	ast_character_literal_t         character_literal;
	ast_string_literal_t            string_literal;
	ast_integer_literal_t           integer_literal;
	ast_float_literal_t             float_literal;
	ast_signature_t                 signature;
	ast_param_decl_t                param_decl;
	ast_var_decl_t                  var_decl;
	ast_package_name_t              package_name;
	ast_package_decl_t              package_decl;
	ast_import_decl_t               import_decl;
	ast_alias_decl_t                alias_decl;
	ast_static_decl_t               static_decl;
	ast_enum_constant_decl_t        enum_constant_decl;
	ast_enum_decl_t                 enum_decl;
	ast_func_decl_t                 func_decl;
	ast_method_decl_t               method_decl;
	ast_setattr_decl_t              setattr_decl;
	ast_struct_decl_t               struct_decl;
	ast_union_decl_t                union_decl;
	ast_field_decl_t                field_decl;
	ast_staticvar_decl_t            staticvar_decl;

} ast_node_union_t;


/* Super-set of all sub-nodes within AST tree */
struct ast_node {
	ast_node_type_e  type;
	ast_node_union_t u;
	const token_t   *tok;
};


/* Abstract Syntax Tree (AST) per module */
struct ast_root {
	const module_t  *module;
	ast_node_t      *comment;
	ast_node_t      *package_decl;
	ast_nodeseq_t   import_decl_list;
	ast_nodeseq_t   const_decl_list;
	ast_nodeseq_t   staticvar_decl_list;
	ast_nodeseq_t   enum_decl_list;
	ast_nodeseq_t   struct_decl_list;
	ast_nodeseq_t   union_decl_list;
	ast_nodeseq_t   func_decl_list;
};


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

ast_nodeseq_t *ast_nodeseq_new(void);

void ast_nodeseq_init(ast_nodeseq_t *);

void ast_nodeseq_append(ast_nodeseq_t *, ast_node_t *);

size_t ast_nodeseq_size(const ast_nodeseq_t *);

ast_node_t *ast_nodeseq_at(const ast_nodeseq_t *, size_t);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

ast_root_t *ast_root_new(const module_t *);

ast_node_t *ast_node_new(ast_node_type_e, const token_t *);

ast_ident_t *ast_ident_new(const token_t *tok);

bool ast_ident_isequal(const ast_ident_t *, const ast_ident_t *);


#endif /* CAC_AST_H_ */

