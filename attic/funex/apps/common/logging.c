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
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/types.h>

#include <fnxserv.h>
#include "version.h"
#include "globals.h"
#include "process.h"
#include "logging.h"

#define globals (funex_globals)

/*
 * Resolves logging facility configurations, based on global settings.
 * Either as default, command-line utility (printer) or daemon process.
 */
static void resolve_logger_params(int *p_flags, int *p_level,
                                  int *p_trace, size_t *p_rate)
{
	int log_flags, log_level, log_trace;
	size_t log_rate;

	log_flags   = 0;
	log_level   = FNX_LL_INFO;
	log_trace   = FNX_LL_NONE;

	/* Log rate confuses valgrind-helgrind -- need fix */
	log_rate    = FNX_LR_UNLIMITED;

	if (globals.proc.utility) {
		log_flags |= FNX_LF_STDOUT;
	} else if (globals.proc.service) {
		log_flags |= FNX_LF_SEVERITY;
		if (globals.proc.nodaemon) {
			log_flags |= FNX_LF_STDOUT | FNX_LF_PROGNAME;
		}
	}
	if (globals.proc.debug > 0) {
		log_flags |= FNX_LF_FILINE;
	}
	if (globals.log.trace > 0) {
		log_trace = FNX_LL_TRACE1;
	}
	if (globals.log.trace > 1) {
		log_trace = FNX_LL_TRACE2;
	}
	if (globals.log.trace > 2) {
		log_trace = FNX_LL_TRACE3;
	}
	if (globals.log.showfunc) {
		log_flags |= FNX_LF_FUNC;
	}
	if (globals.log.showloc) {
		log_flags |= FNX_LF_FILINE;
	}
	if (globals.log.syslog) {
		log_flags |= FNX_LF_SYSLOG;
	}
	if (globals.log.showprog) {
		log_flags |= FNX_LF_PROGNAME;
	}
	if (globals.log.showtimes) {
		log_flags |= FNX_LF_TIMESTAMP;
	}
	if (globals.log.verbose) {
		log_level = FNX_MIN(log_level, FNX_LL_INFO);
		log_flags |= FNX_LF_TIMEHUMAN;
	}
	if (globals.log.quiet) {
		log_level = FNX_LL_ERROR;
	}

	*p_flags = log_flags;
	*p_level = log_level;
	*p_trace = log_trace;
	*p_rate  = log_rate;
}

static void log_first_line(const fnx_logger_t *logger)
{
	pid_t pid;
	const char *prog;
	const char *vers;
	const char *hdrs = "++++++";

	pid  = getpid();
	prog = funex_globals.prog.name;
	vers = funex_version();

	fnx_info("%s %s %s %s", hdrs, prog, vers, hdrs);
	fnx_info("pid=%d", pid);
	fnx_info("log_level=%d log_trace=%d log_flags=%#x log_rate=%ld",
	         logger->log_level, logger->log_trace,
	         logger->log_flags, (long)logger->log_rate);
}

/* Logger's default setting */
void funex_setup_logger(void)
{
	size_t log_rate;
	int log_flags, log_level, log_trace;
	const char *ident = globals.prog.name;
	fnx_logger_t *logger = fnx_default_logger;

	resolve_logger_params(&log_flags, &log_level, &log_trace, &log_rate);
	fnx_logger_setup(logger, ident, log_level, log_trace, log_flags, log_rate);
}

/* Logger for printing utility */
void funex_setup_plogger(void)
{
	size_t log_rate;
	int log_flags, log_level, log_trace;
	fnx_logger_t *logger = fnx_default_logger;

	resolve_logger_params(&log_flags, &log_level, &log_trace, &log_rate);
	fnx_logger_setup(logger, NULL, log_level, log_trace, log_flags, log_rate);
}


/*
 * Logging sub-system. Daemon's runtime logging uses syslog.
 * Here we initialize global logger object, sub-components my define and use
 * derived loggers.
 */
static char s_syslog_open = FNX_FALSE;

void funex_open_syslog(void)
{
	int option, facility;
	const char *ident;

	ident    = funex_globals.prog.name;
	option   = LOG_NDELAY | LOG_PID;
	facility = LOG_USER;

	openlog(ident, option, facility);
	setlogmask(LOG_UPTO(LOG_DEBUG));
	atexit(funex_close_syslog);

	s_syslog_open = FNX_TRUE;
}

void funex_close_syslog(void)
{
	if (s_syslog_open) {
		fflush(stdout);
		fflush(stderr);
		closelog();
		s_syslog_open = FNX_FALSE;
	}
}

/* Logger setting for daemon process */
void funex_setup_dlogger(void)
{
	size_t log_rate;
	int log_flags, log_level, log_trace;
	fnx_logger_t *logger = fnx_default_logger;

	if (globals.log.syslog) {
		funex_open_syslog();
	}
	resolve_logger_params(&log_flags, &log_level, &log_trace, &log_rate);
	fnx_logger_setup(logger, NULL, log_level, log_trace, log_flags, log_rate);
	log_first_line(logger);
}
