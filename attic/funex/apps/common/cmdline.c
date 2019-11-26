/*
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                             *
 *                            Funex File-System                                *
 *                                                                             *
 *  Copyright (C) 2014,2015 Synarete                                           *
 *                                                                             *
 *  Funex is a free software; you can redistribute it and/or modify it under   *
 *  the terms of the GNU General Public License as published by the Free       *
 *  Software Foundation, either version 3 of the License, or (at your option)  *
 *  any later version.                                                         *
 *                                                                             *
 *  Funex is distributed in the hope that it will be useful, but WITHOUT ANY   *
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  *
 *  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more      *
 *  details.                                                                   *
 *                                                                             *
 *  You should have received a copy of the GNU General Public License along    *
 *  with this program. If not, see <http://www.gnu.org/licenses/>              *
 *                                                                             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 */
#include <fnxconfig.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <fnxserv.h>

#include "version.h"
#include "process.h"
#include "parse.h"
#include "globals.h"
#include "cmdline.h"

#define globals (funex_globals)

/* Local functions */
static void funex_getopts_init(funex_getopts_t *, int, char *[],
                               const char *, void *);
static void funex_getopts_destroy(funex_getopts_t *);
static int funex_getopts_lookupnextopt(funex_getopts_t *, int *);
static int funex_getopts_lookupnextarg(funex_getopts_t *, char const **);
static const char *funex_getopts_lookuparg(const funex_getopts_t *);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_show_cmdhelp_usage(const funex_cmdent_t *cmdent, int pad)
{
	int c, n, has_newline = 0;
	const char *s;
	const char *prog = globals.prog.name;

	if (cmdent->usage == NULL) {
		return;
	}

	n = 0;
	s = cmdent->usage;
	while ((c = *s++) != '\0') {
		if (c == '@') {
			if (n++ > 0) {
				printf("%*c\n", pad, ' ');
			}
			printf("%s", prog);
		} else if (c == '%') {
			printf("%s", prog);
		} else {
			printf("%c", c);
		}
		if (c == '\n') {
			has_newline = 1;
		}
	}
	if (!has_newline) {
		printf("%s\n", "");
	}
}

static void funex_show_help(const funex_cmdent_t *cmdent)
{
	const char *s;

	if (cmdent->usage != NULL) {
		funex_show_cmdhelp_usage(cmdent, 1);
	}
	if ((s = cmdent->helps) != NULL) {
		printf("\n%s\n", s);
	}
}

void funex_show_cmdhelp_and_goodbye(const funex_getopts_t *opt)
{
	const funex_cmdent_t *cmdent;

	cmdent = (const funex_cmdent_t *)(opt->userp);
	if (cmdent != NULL) {
		funex_show_help(cmdent);
	}
	funex_goodbye();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Helper: prettify build-time string into static buffer */
static const char *funex_buildtime_str(void)
{
	int c, dash = 0;
	size_t len  = 0;
	const char *str;
	static char buf[256];

	str = funex_buildtime();
	while (((c = *str++) != '\0') && (len < sizeof(buf))) {
		if (isspace(c) || !isascii(c) || !isprint(c)) {
			dash += 1;
			if (dash == 1) {
				buf[len++] = '-';
			}
		} else {
			dash = 0;
			buf[len++] = (char)c;
		}
	}
	return buf;
}

void funex_show_version(int detailed)
{
	const char *prog_name;
	const char *build_time;

	if (detailed) {
		prog_name  = program_invocation_name;
		build_time = funex_buildtime_str();
		printf("%s %s %s\n", prog_name, funex_version(), build_time);
	} else {
		prog_name  = globals.prog.name;
		printf("%s %s\n", prog_name, funex_version());
	}
}

void funex_show_version_and_goodbye(void)
{
	funex_show_version(globals.opt.all);
	funex_goodbye();
}

void funex_show_license(void)
{
	printf("\n%s\n", funex_license());
}

void funex_show_license_and_goodbye(void)
{
	funex_show_license();
	funex_goodbye();
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/


int funex_getnextopt(funex_getopts_t *opt)
{
	int rc, c = 0;

	rc = funex_getopts_lookupnextopt(opt, &c);
	if (rc != 0) {
		c = 0;
	} else {
		if (c == '?') {
			funex_die_unknown_opt(opt, c);
		} else if (c == ':') {
			funex_die_missing_arg(NULL);
		}
	}
	return c;
}

const char *funex_getoptarg(funex_getopts_t *opt, const char *argname)
{
	const char *arg;

	arg = funex_getopts_lookuparg(opt);
	if (arg == NULL) {
		funex_die_missing_arg(argname);
	}
	return arg;
}

const char *funex_getnextarg(funex_getopts_t *opt,
                             const char *argname, int flags)
{
	int rc;
	const char *arg = NULL;
	const char *arg_extra = NULL;

	rc = funex_getopts_lookupnextarg(opt, &arg);
	if (rc == 0) {
		if (flags & FUNEX_ARG_NONE) {
			funex_die_redundant_arg(arg);
		}
		if (flags & FUNEX_ARG_LAST) {
			rc = funex_getopts_lookupnextarg(opt, &arg_extra);
			if (rc == 0) {
				funex_die_redundant_arg(arg_extra);
			}
		}
	} else {
		if (flags & FUNEX_ARG_REQ) {
			funex_die_missing_arg(argname);
		}
	}
	return arg;
}

void funex_parse_cmdargs(int argc, char *argv[],
                         const funex_cmdent_t *cmdent,
                         funex_getopts_fn getopt_hook)
{
	funex_getopts_t opt;

	funex_getopts_init(&opt, argc, argv, cmdent->optdef, (void *)cmdent);
	getopt_hook(&opt);
	funex_getopts_destroy(&opt);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_die_unimplemented(const char *str)
{
	funex_dief("unimplemented: %s", str);
}

void funex_die_missing_arg(const char *arg_name)
{
	if (arg_name != NULL) {
		funex_dief("missing: %s", arg_name);
	} else {
		funex_dief("missing-arg");
	}
}

void funex_die_illegal_arg(const char *arg_name, const char *arg_value)
{
	if (arg_value != NULL) {
		funex_dief("illegal: %s %s", arg_name, arg_value);
	} else {
		funex_dief("illegal: %s", arg_name);
	}
}

void funex_die_redundant_arg(const char *arg_name)
{
	if (arg_name != NULL) {
		funex_dief("redundant-arg: %s", arg_name);
	} else {
		funex_dief("redundant-arg");
	}
}

void funex_die_unknown_opt(const funex_getopts_t *opt, int c)
{
	char oc;
	const char *ext = "try `--help' for more information";

	oc = (char)opt->optopt;
	if (((c == ':') || (c == '?')) && fnx_chr_isprint(oc)) {
		funex_dief("unknown option: -%c\n%s", oc, ext);
	} else {
		funex_dief("illegal or missing option\n%s", ext);
	}
}

void funex_die_illegal_volsize(const char *arg, loff_t sz)
{
	const loff_t minsz = (loff_t)FNX_VOLSIZE_MIN;
	const loff_t maxsz = (loff_t)FNX_VOLSIZE_MAX;
	const loff_t minm  = (minsz / (loff_t)FNX_MEGA);
	const loff_t maxt  = (maxsz / (loff_t)FNX_TERA);

	if (arg == NULL) {
		funex_dief("illegal vol-size: %ld (min=%ldM max=%ldT)", sz, minm, maxt);
	} else {
		funex_dief("illegal vol-size: %s (min=%ldM max=%ldT)", arg, minm, maxt);
	}
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Getopt wrappers
 */
/* Special chars helpers */
static int is_delimchr(int c)
{
	return (strchr(FUNEX_GETOPT_DELIMCHRS, c) != NULL);
}

static int is_withargchr(int c)
{
	return (strchr(FUNEX_GETOPT_HASARGCHRS, c) != NULL);
}

/* Convert long-opts to opt-string */
static void make_optstring(const struct option *longopts, char *s, size_t len)
{
	const char *end = (s + len) - 3;

	memset(s, 0, len);
	while (s < end) {
		if (!longopts->name || (longopts->val <= 0)) {
			break;
		}
		*s++ = (char)longopts->val;
		if (longopts->has_arg) {
			*s++ = ':';
		}
		*s = '\0';
		++longopts;
	}
}

/* Refresh with getopt globals */
static void getopt_refresh(funex_getopts_t *gopt)
{
	gopt->optarg = optarg;
	gopt->optind = optind;
	gopt->opterr = opterr;
	gopt->optopt = optopt;
}


/* Construct from user provided description string */
static void funex_getopts_init(funex_getopts_t *gopt, int argc, char *argv[],
                               const char *optdef, void *userp)
{
	int rc;
	size_t i, n;
	fnx_substr_t ss;
	fnx_substr_t toks[FUNEX_GETOPT_MAX];
	fnx_substr_t *tok = NULL;
	char *p;
	size_t sz;
	struct option *p_option;

	if (optdef == NULL) {
		optdef = "";
	}

	/* Reset getopt globals */
	memset(gopt, 0, sizeof(*gopt));
	optarg = NULL;
	optind = 1;
	optopt = 0;
	opterr = 0;

	/* Set defaults */
	gopt->userp = userp;
	gopt->argc  = argc;
	gopt->argv  = argv;
	gopt->optdefs = strdup(optdef);
	getopt_refresh(gopt);

	/* Parse optdef string into getopt long repr */
	fnx_substr_init_rwa(&ss, gopt->optdefs);
	rc = fnx_substr_tokenize_chr(&ss, ' ', toks, FNX_ARRAYSIZE(toks), &n);
	if (rc != 0) {
		funex_dief("internal-error: optdefs=%s", gopt->optdefs);
	}

	for (i = 0; i < n; ++i) {
		tok = &toks[i];
		p   = fnx_substr_data(tok);
		sz  = fnx_substr_size(tok);

		p_option = &gopt->longopts[gopt->optcount++];

		p_option->val = (int)p[0];

		if (is_delimchr(p[1])) {
			p += 2;
			sz -= 2;
		}

		if (sz && is_withargchr(p[sz - 1])) {
			p_option->has_arg = required_argument;
			p[sz - 1] = '\0';
		} else {
			p_option->has_arg = no_argument;
			p[sz] = '\0';
		}

		p_option->name = p;
	}

	sz = sizeof(gopt->optstring);
	make_optstring(gopt->longopts, gopt->optstring, sz - 1);
}

static int funex_getopts_lookupnextopt(funex_getopts_t *gopt, int *c)
{
	gopt->optchr = getopt_long(gopt->argc,
	                           gopt->argv,
	                           gopt->optstring,
	                           gopt->longopts,
	                           &gopt->longindex);
	getopt_refresh(gopt);
	*c = gopt->optchr;
	return (gopt->optchr != EOF) ? 0 : -1;
}

/* Parse non-option ARGV-elements: */
static int funex_getopts_lookupnextarg(funex_getopts_t *gopt, char const **arg)
{
	int rc = -1;

	if (gopt->optind < gopt->argc) {
		*arg = gopt->argv[gopt->optind];
		optind += 1;
		getopt_refresh(gopt);
		rc = 0;
	}
	return rc;
}

static const char *funex_getopts_lookuparg(const funex_getopts_t *gopt)
{
	return gopt->optarg;
}

static void funex_getopts_destroy(funex_getopts_t *gopt)
{
	if (gopt->optdefs != NULL) {
		free(gopt->optdefs);
		gopt->optdefs = NULL;
	}
}



