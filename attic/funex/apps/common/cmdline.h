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
#ifndef FUNEX_CMDLINE_H_
#define FUNEX_CMDLINE_H_

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

/* Get-opt extensions */
#define FUNEX_GETOPT_MAX         (64)
#define FUNEX_GETOPT_DELIMCHRS   "_,:"
#define FUNEX_GETOPT_HASARGCHRS  ":="

/* Arguments control flags */
#define FUNEX_ARG_OPT       FNX_BITFLAG(0)
#define FUNEX_ARG_REQ       FNX_BITFLAG(1)
#define FUNEX_ARG_LAST      FNX_BITFLAG(2)
#define FUNEX_ARG_NONE      FNX_BITFLAG(3)
#define FUNEX_ARG_REQLAST   FUNEX_ARG_REQ | FUNEX_ARG_LAST

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * State-full wrapper over 'getopt_long' functionality
 */
struct funex_getopts {
	void   *userp;          /* User's private pointer */
	int     argc;           /* Parsed arguments */
	char  **argv;
	char   *optarg;         /* References to getopt globals */
	int     optind;
	int     opterr;
	int     optopt;
	int     longindex;
	int     optchr;         /* Last returned value */
	char   *optdefs;        /* User provided def string */
	size_t  optcount;       /* Decoded desc into getopt representation */
	char    optstring[FUNEX_GETOPT_MAX * 4];
	struct option longopts[FUNEX_GETOPT_MAX];
};
typedef struct funex_getopts  funex_getopts_t;


/*
 * Sub-commands definition + reference entry
 */
struct funex_cmdent {
	const char *optdef;     /* Options definitions for getopt wrapper */
	const char *usage;      /* Program's usage description string */
	const char *helps;      /* Help string (detailed) */
};
typedef struct funex_cmdent  funex_cmdent_t;


/* Get-opts hook */
typedef void (*funex_getopts_fn)(funex_getopts_t *);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_show_cmdhelp_usage(const funex_cmdent_t *, int);

void funex_show_cmdhelp_options(const funex_cmdent_t *);

void funex_show_cmdhelp_and_goodbye1(const funex_cmdent_t *);

void funex_show_cmdhelp_and_goodbye(const funex_getopts_t *);



void funex_show_version(int all);

void funex_show_version_and_goodbye(void);

void funex_show_license(void);

void funex_show_license_and_goodbye(void);

void funex_parse_cmdargs(int, char *[], const funex_cmdent_t *,
                         funex_getopts_fn);


int funex_getnextopt(struct funex_getopts *);

const char *funex_getoptarg(funex_getopts_t *, const char *);

const char *funex_getnextarg(funex_getopts_t *, const char *, int);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_die_unimplemented(const char *);

void funex_die_missing_arg(const char *);

void funex_die_illegal_arg(const char *, const char *);

void funex_die_redundant_arg(const char *);

void funex_die_unknown_opt(const struct funex_getopts *, int c);

void funex_die_illegal_volsize(const char *, loff_t);


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

#endif /* FUNEX_CMDLINE_H_ */


