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
#ifndef CAC_LEXER_H_
#define CAC_LEXER_H_

#include "infra.h"
#include "token.h"

extern const char *basic_charset;

extern const char *identifier_charset;

extern const char *binary_digits;

extern const char *decimal_digits;

extern const char *octal_digits;

extern const char *hexadecimal_digits;


tokseq_t *scan_file_to_atoms(const string_t *path);

tokseq_t *scan_source_to_atoms(const string_t *src);

tokseq_t *scan_comments(const struct tokseq *tseq);

tokseq_t *scan_string_literals(const tokseq_t *tseq);

tokseq_t *scan_numaric_literals(const tokseq_t *tseq);

tokseq_t *scan_atoms_to_tokens(tokseq_t *atoms);

tokseq_t *scan_attributes_list(const tokseq_t *tseq);


#endif /* CAC_LEXER_H_ */


