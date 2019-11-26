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
#ifndef CAC_FATAL_H_
#define CAC_FATAL_H_

struct token;

#define cac_assert(expr) \
	do { \
		if (!(expr)) \
			assertion_failure(__FILE__, __LINE__, MAKESTR(expr)); \
	} while (0)


ATTR_NORETURN
void assertion_failure(const char *, unsigned int, const char *);

ATTR_NORETURN
void internal_error(const char *fmt, ...);

ATTR_NORETURN
void fatal_error(const char *fmt, ...);

ATTR_NORETURN
void compile_error(const struct token *tok, const char *fmt, ...);

#endif /* CAC_FATAL_H_ */




