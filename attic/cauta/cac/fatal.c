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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <error.h>
#include <err.h>

#include "macros.h"
#include "strutils.h"
#include "vector.h"
#include "token.h"
#include "fatal.h"


ATTR_NORETURN
void assertion_failure(const char *file, unsigned int line, const char *expr)
{
	error(EXIT_FAILURE, 0,
	      "Assertion-failure `%s' (%s:%u)", expr, file, line);
	abort();
}

ATTR_NORETURN
void internal_error(const char *fmt, ...)
{
	va_list ap;
	FILE *fp = stderr;

	fprintf(fp, "%s: ", program_invocation_short_name);
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
	abort();
}

ATTR_NORETURN
void fatal_error(const char *fmt, ...)
{
	va_list ap;

	program_invocation_name = program_invocation_short_name;
	va_start(ap, fmt);
	verrx(EXIT_FAILURE, fmt, ap);
	va_end(ap);
}

ATTR_NORETURN
void compile_error(const struct token *tok, const char *fmt, ...)
{
	va_list ap;
	FILE *fp = stderr;

	fprintf(fp, "%s: ", program_invocation_short_name);
	if (tok != NULL) {
		fprintf(fp, "%s:", tok->mod->path->str);
		if (tok->lno > 0) {
			fprintf(fp, "%lu:", tok->lno);
			if (tok->off > 0) {
				fprintf(fp, "%lu:", tok->off);
			}
		}
		fputs(" ", fp);
	}

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fputs("\n", fp);
	fflush(fp);
	exit(EXIT_FAILURE);
}


