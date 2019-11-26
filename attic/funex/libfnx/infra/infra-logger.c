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
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <error.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/time.h>

#include "infra-compiler.h"
#include "infra-macros.h"
#include "infra-utility.h"
#include "infra-logger.h"


/* Converts logging-level to human readable string */
static const char *log_str_level(int level)
{
	const char *str;

	switch (level) {
		case FNX_LL_TRACE3:
		case FNX_LL_TRACE2:
		case FNX_LL_TRACE1:
			str = "debug";
			break;
		case FNX_LL_INFO:
			str = "info";
			break;
		case FNX_LL_WARN:
			str = "warn";
			break;
		case FNX_LL_ERROR:
			str = "error";
			break;
		case FNX_LL_CRIT:
			str = "critical";
			break;
		default:
			str = "";
			break;
	}
	return str;
}

/* Converts logging-level to syslog priority */
static int log_syslog_priority(int level)
{
	int priority;

	switch (level) {
		case FNX_LL_TRACE3:
		case FNX_LL_TRACE2:
		case FNX_LL_TRACE1:
			priority = LOG_DEBUG;
			break;
		case FNX_LL_INFO:
			priority = LOG_INFO;
			break;
		case FNX_LL_WARN:
			priority = LOG_WARNING;
			break;
		case FNX_LL_ERROR:
			priority = LOG_ERR;
			break;
		case FNX_LL_CRIT:
			priority = LOG_CRIT;
			break;
		default:
			priority = LOG_DEBUG;
			break;
	}
	return priority;
}

/* Strip path-prefix from filename */
static const char *log_basename(const char *path)
{
	const char *s;

	s = strrchr(path, '/');
	if (s != NULL) {
		s = s + 1;
	} else {
		s = path;
	}
	return s;
}

/* Generate human-readable time-stamp for now */
static void log_strtime(char *buf, size_t sz, const struct timeval *tv)
{
	int n;
	struct tm t_tm;

	memset(&t_tm, 0, sizeof(t_tm));
	localtime_r(&tv->tv_sec, &t_tm);

	n = (int)strftime(buf, sz, "%FT%T", &t_tm);
	if (tv && (n > 0) && (n < (int)sz)) {
		sz = (sz - (size_t) n);
		snprintf(buf + n, sz, ".%06lu", tv->tv_usec);
	}
}

static void log_strtimes(char *buf, size_t sz, const struct timeval *tv)
{
	snprintf(buf, sz, "%09lu.%06lu", tv->tv_sec, tv->tv_usec);
}

static void log_timestamp_now(char *buf, size_t sz, int human)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	if (human) {
		log_strtime(buf, sz, &tv);
	} else {
		log_strtimes(buf, sz, &tv);
	}
}

/* Have current errno-string */
static void log_strerrno(int errnum, int have_msg, char *buf, size_t sz)
{
	size_t len;
	char msg[256];
	char *str = msg;

	len = 0;
	if (have_msg) {
		fnx_bzero(msg, sizeof(msg));
		str = strerror_r(errnum, msg, sizeof(msg) - 1);
		len = strlen(str);
	}

	if ((len == 0) || (len >= sz)) {
		snprintf(buf, sz, "errno=%d", errnum);
	} else {
		strncpy(buf, str, len + 1);
	}
}

/* Generate <file:line func> marker-prefix */
static void log_prefix(const char *ident, const char *file, int line,
                       const char *func, char *buf, size_t sz)
{
	size_t n;
	char *p;
	char dlm = ':';

	p = buf;
	if (ident != NULL) {
		n = (size_t)snprintf(p, sz, "%s%c", ident, dlm);
		n = fnx_min(n, sz - 1);
		p  += n;
		sz -= n;
	}
	if (file != NULL) {
		n = (size_t)snprintf(p, sz, " %s%c%d", file, dlm, line);
		n = fnx_min(n, sz - 1);
		p  += n;
		sz -= n;
	}
	if (func != NULL) {
		n = (size_t)snprintf(p, sz, " %s", func);
		n = fnx_min(n, sz - 1);
		p  += n;
		sz -= n;
	}

	if ((p != buf) && (sz > 1)) {
		*p++ = dlm;
	}

	*p = '\0';
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Log to STDOUT */
static void log_stdout(const char *msg,
                       const char *progname,
                       const char *ident,
                       const char *severity,
                       const char *file, int line,
                       const char *func,
                       const char *timestamp,
                       const char *errnostr)
{
	FILE *fp = stdout;

	flockfile(fp);

	if (severity != NULL) {
		fprintf(fp, "[%s] ", severity);
	}
	if (timestamp != NULL) {
		fprintf(fp, "[%s] ", timestamp);
	}
	if (progname != NULL) {
		fprintf(fp, "%s: ", progname);
	}
	if (ident != NULL) {
		fprintf(fp, "%s: ", ident);
	}
	if (file != NULL) {
		fprintf(fp, "%s:%d ", file, line);
	}
	if (func != NULL) {
		fprintf(fp, "%s: ", func);
	}
	if ((errnostr != NULL) && strlen(errnostr)) {
		fprintf(fp, "%s <%s>\n", msg, errnostr);
	} else {
		fprintf(fp, "%s\n", msg);
	}

	funlockfile(fp);
}

/* Log via SYSLOG */
static void log_syslog(const char *msg, const char *ident, int level,
                       const char *file, int line, const char *func,
                       const char *errstr)
{
	int pr;
	char prefix[1024];

	log_prefix(ident, file, line, func, prefix, sizeof(prefix));

	pr = log_syslog_priority(level);
	if (prefix[0]) {
		syslog(pr, "%s %s %s", prefix, msg, errstr);
	} else {
		syslog(pr, "%s %s", msg, errstr);
	}
}

/* Default logging hook */
static void log_logmsg(const fnx_logmsg_t *msg, int flags)
{
	int level, line;
	const char *errstr;
	const char *tmstamp;
	const char *progname;
	const char *ident;
	const char *severity;
	const char *file;
	const char *func;
	const char *msgstr;
	char tms[80];
	char ers[80];

	level   = msg->lm_level;
	ident   = msg->lm_ident;
	msgstr  = msg->lm_msgstr;

	/* In case of error, force output */
	if (level >= FNX_LL_ERROR) {
		flags |= FNX_LF_STDOUT | FNX_LF_SEVERITY;
	}
	/* Show non-zero errno if warn or more */
	errstr = "";
	if (msg->lm_errno != 0) {
		if (level <= FNX_LL_TRACE1) {
			if (flags & FNX_LF_ERRNOSTR) {
				log_strerrno(msg->lm_errno, 1, ers, sizeof(ers));
				errstr = ers;
			} else {
				errstr = fnx_errno_to_string(msg->lm_errno);
			}
		}
	}
	/* Show program name */
	progname = NULL;
	if (flags & FNX_LF_PROGNAME) {
		progname = program_invocation_short_name;
	}
	/* Set logging metadata by level + flags */
	file = NULL;
	line = 0;
	if (msg->lm_file != NULL) {
		if ((flags & FNX_LF_FILINE) && (level <= FNX_LL_TRACE1)) {
			file = log_basename(msg->lm_file);
			line = msg->lm_line;
		}
	}
	func = NULL;
	if (msg->lm_func != NULL) {
		if ((flags & FNX_LF_FUNC) && (level <= FNX_LL_TRACE1)) {
			func = msg->lm_func;
		}
	}
	tmstamp = NULL;
	if (flags & FNX_LF_TIMESTAMP) {
		log_timestamp_now(tms, sizeof(tms), (flags & FNX_LF_TIMEHUMAN));
		tmstamp = tms;
	}
	severity = NULL;
	if (flags & FNX_LF_SEVERITY) {
		severity = log_str_level(level);
	}

	/* Do actual logging */
	if (flags & FNX_LF_STDOUT) {
		log_stdout(msgstr, progname, ident, severity,
		           file, line, func, tmstamp, errstr);
	}
	if (flags & FNX_LF_SYSLOG) {
		log_syslog(msgstr, ident, level, file, line, func, errstr);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Logger:
 */
static int logger_testmsg(fnx_logger_t *, const fnx_logmsg_t *);
static void logger_sendmsg(fnx_logger_t *, const fnx_logmsg_t *);


void fnx_logger_init(fnx_logger_t *logger)
{
	fnx_bzero(logger, sizeof(*logger));

	logger->log_flags   = FNX_LF_DEFAULT;
	logger->log_level   = FNX_LL_DEFAULT;
	logger->log_trace   = FNX_LL_DEFAULT;
	logger->log_rate    = FNX_LR_DEFAULT;
	logger->log_count   = 0;
	logger->log_last    = 0;
	logger->log_testmsg = logger_testmsg;
	logger->log_sendmsg = logger_sendmsg;
}

void fnx_logger_destroy(fnx_logger_t *logger)
{
	logger->log_flags   = 0;
	logger->log_level   = 0;
	logger->log_rate    = 0;
	logger->log_count   = 0;
	logger->log_last    = 0;
	logger->log_sendmsg = NULL;
	logger->log_testmsg = NULL;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

static int logger_testmsg(fnx_logger_t *logger, const fnx_logmsg_t *logmsg)
{
	int msg_level;

	msg_level = logmsg->lm_level;

	/* Do nothing if no control flags set */
	if (!logger->log_flags) {
		return -1;
	}
	/* Force logging for anything more then error */
	if (msg_level >= FNX_LL_ERROR) {
		return 0;
	}
	/* Test for normal logging */
	if ((msg_level >= FNX_LL_INFO) &&
	    (msg_level >= logger->log_level)) {
		return 0;
	}
	/* Test for debug logging */
	if ((msg_level <= FNX_LL_TRACE1) &&
	    (msg_level >= logger->log_trace)) {
		return 0;
	}

	/* No logging for this message */
	return -1;
}

static void logger_sendmsg(fnx_logger_t *logger, const fnx_logmsg_t *logmsg)
{
	log_logmsg(logmsg, logger->log_flags);
}

static int logger_do_testmsg(fnx_logger_t *logger, int level, int errnum)
{
	int rc;
	fnx_logmsg_t msg;

	if (logger->log_testmsg != NULL) {
		msg.lm_errno   = errnum;
		msg.lm_level   = level;
		msg.lm_line    = 0;
		msg.lm_file    = NULL;
		msg.lm_func    = NULL;
		msg.lm_msgstr  = "";

		rc = logger->log_testmsg(logger, &msg);
	} else {
		rc = 0;
	}

	return rc;
}

/*
 * Roughly check rate within last second. Try to delay the time system call
 * until just needed + handle cases where time has changed and moved back.
 *
 * Note that we don't care for reentrant and don't use mutex/spinlock or any
 * other synchronization mechanism; rate-limit is crude.
 *
 * Error-Critical messages are excluded from rate-limit. Zero-rate indicate no
 * limit.
 */
static int logger_probe_update_rate(fnx_logger_t *logger, int level)
{
	int rc = 0;
	time_t now;

	if ((level < FNX_LL_ERROR) && (logger->log_rate > 0)) {
		logger->log_count++;
		if (logger->log_count > logger->log_rate) {
			now = time(NULL);
			if ((now > logger->log_last) ||
			    (now < logger->log_last)) {
				logger->log_last    = now;
				logger->log_count   = 0;
			} else {
				rc = -1;
			}
		}
	}
	return rc;
}

static void logger_do_sendmsg(fnx_logger_t *logger, int level, int errnum,
                              const char *file, int line, const char *func,
                              const char *fmt, va_list ap)
{
	int rc;
	size_t nn, sz;
	char str[1024];
	fnx_logmsg_t msg;

	/* Set formatted message */
	sz = sizeof(str);
	rc = vsnprintf(str, sz - 1, fmt, ap);
	nn = fnx_min(sz - 1, (size_t)rc);
	str[nn] = '\0';

	/* Send it via provided hook */
	msg.lm_level    = level;
	msg.lm_errno    = errnum;
	msg.lm_file     = file;
	msg.lm_line     = line;
	msg.lm_func     = func;
	msg.lm_ident    = logger->log_ident;
	msg.lm_msgstr   = str;
	logger->log_sendmsg(logger, &msg);
}

void fnx_logf(fnx_logger_t *logger, int level,
              const char *file, int line, const char *func,
              const char *fmt, ...)
{
	int rc, saved_errno;
	va_list ap;


	/* Save errno */
	saved_errno = errno;
	/* If no user-provided logger, use default */
	if (logger == NULL) {
		logger = fnx_default_logger;
	}
	/* If no logging support for this app, no op */
	if (logger == NULL) {
		goto out;
	}
	/* Must have send-hook */
	if (logger->log_sendmsg == NULL) {
		goto out;
	}
	/* Test for send using user-provided hook */
	rc = logger_do_testmsg(logger, level, saved_errno);
	if (rc != 0) {
		goto out;
	}
	/* Check rate-limit */
	rc = logger_probe_update_rate(logger, level);
	if (rc != 0) {
		goto out;
	}
	/* All is OK, send the message */
	va_start(ap, fmt);
	logger_do_sendmsg(logger, level, saved_errno,
	                  file, line, func, fmt, ap);
	va_end(ap);
out:
	errno = saved_errno;
}


void fnx_logger_setup(fnx_logger_t *logger, const char *ident,
                      int level, int trace, int flags, size_t rate)
{
	logger->log_ident = ident;
	logger->log_level = level;
	logger->log_trace = trace;
	logger->log_flags = flags;
	logger->log_rate  = rate;
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Default global logger instance. */
static fnx_logger_t fnx_s_default_logger = {
	.log_flags   = FNX_LF_DEFAULT,
	.log_level   = FNX_LL_DEFAULT,
	.log_trace   = FNX_LL_DEFAULT,
	.log_rate    = FNX_LR_DEFAULT,
	.log_count   = 0,
	.log_last    = 0,
	.log_ident   = NULL,
	.log_testmsg = logger_testmsg,
	.log_sendmsg = logger_sendmsg
};

fnx_logger_t *fnx_default_logger = &fnx_s_default_logger;

/* Foramt checker flags -- keep zero */
short fnx_check_format = 0;



