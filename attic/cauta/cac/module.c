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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "infra.h"
#include "token.h"
#include "lexer.h"

/* Maximum bytes-size of single module (16M) */
#define CAC_LIMIT_MODULE_SIZE_MAX     (1ULL << 24)

/* Maximum number of lines in module */
#define CAC_LIMIT_MODULE_LINES_MAX    (CAC_LIMIT_MODULE_SIZE_MAX / 128)



const char *basic_charset =
        "\t \v \f \n " \
        "a b c d e f g h i j k l m n o p q r s t u v w x y z " \
        "A B C D E F G H I J K L M N O P Q R S T U V W X Y Z " \
        "0 1 2 3 4 5 6 7 8 9 " \
        "_ { } [ ] # ( ) < > % : ; . ? * + - / ^ & | ~ ! = , " \
        "\\ \" ' @ $ ";

const char *identifier_charset =
        "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

const char *binary_digits = "01";

const char *decimal_digits = "0123456789";

const char *octal_digits = "01234567";

const char *hexadecimal_digits = "0123456789ABCDEFabcdef";




/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static module_t *
module_new(const string_t *path, const string_t *src)
{
	module_t *module;

	module = (module_t *)gc_malloc(sizeof(*module));
	module->path = string_dup(path);
	module->pkg = NULL;
	module->src = src;

	return module;
}

static token_t *module2tok(const module_t *module)
{
	token_t *token;

	token = token_new(TOK_NONE);
	token->mod = module;
	return token;
}


static token_t *path2tok(const string_t *path)
{
	return module2tok(module_new(path, string_nil));
}

static string_t *resolve_realpath(const string_t *path)
{
	char *resp;
	string_t *real;

	resp = realpath(path->str, NULL);
	if (resp == NULL) {
		compile_error(path2tok(path), "no realpath: %s", strerror(errno));
	}
	real = string_new(resp);
	free(resp);
	return real;
}

static size_t resolve_module_size(const string_t *path)
{
	size_t size;
	struct stat st;
	token_t *tok = path2tok(path);

	if (stat(path->str, &st) != 0) {
		compile_error(tok, "stat failed: %s", strerror(errno));
	}
	if (!S_ISREG(st.st_mode)) {
		compile_error(tok, "not regular file: %s", path->str);
	}
	size = (size_t)st.st_size;
	if ((size == 0) || (size > CAC_LIMIT_MODULE_SIZE_MAX)) {
		compile_error(tok, "illegal module size: %lu", size);
	}
	return size;
}

static string_t *read_stream(const string_t *path, size_t sz)
{
	int fd;
	char *buf;
	ssize_t nrd;
	token_t *tok = path2tok(path);

	fd = open(path->str, O_RDONLY);
	if (fd < 0) {
		compile_error(tok, "open failure: %s", strerror(errno));
	}
	buf = gc_malloc(sz);
	nrd = read(fd, buf, sz);
	if (nrd != (ssize_t)sz) {
		compile_error(tok, "read failure: %s", strerror(errno));
	}
	close(fd);
	return string_nnew(buf, sz);
}

/*
 * Load module object from single source file as raw data.
 */
static string_t *load_stream(const string_t *path)
{
	size_t size;
	string_t *real, *data;

	real = resolve_realpath(path);
	size = resolve_module_size(real);
	data = read_stream(real, size);
	return data;
}

/*
 * Use input stream to instantiate module object
 */
static module_t *stream_to_module(const string_t *path, const string_t *src)
{
	return module_new(path, src);
}

/*
 * Transform moudle's source stream into sequence of atom tokens.
 */
static tokseq_t *module_to_atoms(const module_t *module)
{
	char chr;
	size_t off = 0, line_num = 1;
	token_t *tok;
	tokseq_t *atoms;
	const string_t *src = module->src;
	const size_t llimit = CAC_LIMIT_MODULE_LINES_MAX;

	atoms = tokseq_new(src->len);
	for (size_t pos = 0; pos < src->len; ++pos) {
		chr = src->str[pos];
		tok = token_new_atom(chr);
		tok->pos = pos;
		tok->lno = line_num;
		tok->off = ++off;
		tok->mod = module;
		tokseq_append(atoms, tok);

		if (chr == '\n') {
			off = 0;
			if (++line_num > llimit) {
				compile_error(tok, "more than %lu lines", llimit);
			}
		}
	}
	return atoms;
}

/*
 * Load moudle's source file and transform into a sequence of atom tokens.
 */
tokseq_t *scan_file_to_atoms(const string_t *path)
{
	module_t *module = NULL;
	tokseq_t *atoms = NULL;

	module = stream_to_module(path, load_stream(path));
	atoms = module_to_atoms(module);
	return atoms;
}

/*
 * Transforms input source into a sequence of atom tokens.
 */
tokseq_t *scan_source_to_atoms(const string_t *src)
{
	module_t *module;
	tokseq_t *atoms;
	const string_t *path;

	path = string_new(":");
	module = stream_to_module(path, src);
	atoms = module_to_atoms(module);
	return atoms;
}

