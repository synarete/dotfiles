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
#ifndef FNX_INFRA_LOGGER_H_
#define FNX_INFRA_LOGGER_H_

/*
 * Logger macros API;
 * Always prefer those macro interfaces over direct function-call.
 *
 * NB: FNX_TRACEF used to be a wrapper with FNX_FUNCTION; does not worth it..
 */

#define FNX_CHECKF(logger, fmt, ...) \
	do { if (fnx_unlikely(fnx_check_format) && \
		         fnx_unlikely(logger == NULL)) \
			fprintf(stdout, fmt "\n", __VA_ARGS__); } while (0)

#define FNX_LOGF2(logger, level, fn, fmt, ...) \
	do { \
		fnx_logf(logger, level, __FILE__, __LINE__, fn, fmt, __VA_ARGS__); \
	} while (0)

#define FNX_LOGF(logger, level, fmt, ...) \
	do { \
		FNX_CHECKF(logger, fmt, __VA_ARGS__);\
		FNX_LOGF2(logger, level, NULL, fmt, __VA_ARGS__); \
	} while (0)

#define FNX_TRACEF(logger, level, fmt, ...) \
	FNX_LOGF(logger, level, fmt, __VA_ARGS__)


#define fnx_log_trace3(lg, fmt, ...) \
	FNX_TRACEF(lg, FNX_LL_TRACE3, fmt, __VA_ARGS__)
#define fnx_log_trace2(lg, fmt, ...) \
	FNX_TRACEF(lg, FNX_LL_TRACE2, fmt, __VA_ARGS__)
#define fnx_log_trace(lg, fmt, ...) \
	FNX_TRACEF(lg, FNX_LL_TRACE1, fmt, __VA_ARGS__)
#define fnx_log_info(lg, fmt, ...) \
	FNX_LOGF(lg, FNX_LL_INFO,  fmt, __VA_ARGS__)
#define fnx_log_warn(lg, fmt, ...) \
	FNX_LOGF(lg, FNX_LL_WARN,fmt, __VA_ARGS__)
#define fnx_log_error(lg, fmt, ...) \
	FNX_LOGF(lg, FNX_LL_ERROR, fmt, __VA_ARGS__)
#define fnx_log_critical(lg, fmt, ...) \
	FNX_LOGF(lg, FNX_LL_CRIT, fmt, __VA_ARGS__)

#define fnx_trace3(fmt, ...) \
	fnx_log_trace3(fnx_default_logger, fmt, __VA_ARGS__)
#define fnx_trace2(fmt, ...) \
	fnx_log_trace2(fnx_default_logger, fmt, __VA_ARGS__)
#define fnx_trace1(fmt, ...) \
	fnx_log_trace(fnx_default_logger, fmt, __VA_ARGS__)
#define fnx_info(fmt, ...) \
	fnx_log_info(fnx_default_logger, fmt, __VA_ARGS__)
#define fnx_warn(fmt, ...) \
	fnx_log_warn(fnx_default_logger, fmt, __VA_ARGS__)
#define fnx_error(fmt, ...) \
	fnx_log_error(fnx_default_logger, fmt, __VA_ARGS__)
#define fnx_critical(fmt, ...) \
	fnx_log_critical(fnx_default_logger, fmt, __VA_ARGS__)

/*
 * Log Levels.
 */
#define FNX_LL_TRACE3           (-3)
#define FNX_LL_TRACE2           (-2)
#define FNX_LL_TRACE1           (-1)
#define FNX_LL_NONE             (0)
#define FNX_LL_INFO             (2)
#define FNX_LL_WARN             (3)
#define FNX_LL_ERROR            (4)
#define FNX_LL_CRIT             (5)

/*
 * Log control flags:
 */
#define FNX_LF_STDOUT           FNX_BITFLAG(1)
#define FNX_LF_SYSLOG           FNX_BITFLAG(2)
#define FNX_LF_PROGNAME         FNX_BITFLAG(3)
#define FNX_LF_SEVERITY         FNX_BITFLAG(4)
#define FNX_LF_FILINE           FNX_BITFLAG(5)
#define FNX_LF_FUNC             FNX_BITFLAG(6)
#define FNX_LF_TIMESTAMP        FNX_BITFLAG(7)
#define FNX_LF_TIMEHUMAN        FNX_BITFLAG(8)
#define FNX_LF_ERRNOSTR         FNX_BITFLAG(9)

#define FNX_LF_DEFAULT_         (FNX_LF_STDOUT      |   \
                                 FNX_LF_SYSLOG      |   \
                                 FNX_LF_FILINE      |   \
                                 FNX_LF_FUNC        |   \
                                 FNX_LF_TIMESTAMP)

#define FNX_LL_DEFAULT          (FNX_LL_INFO)
#define FNX_LF_DEFAULT          \
	(FNX_LF_STDOUT | FNX_LF_PROGNAME | FNX_LF_SEVERITY)

/* Default log-rate (messages per second) */
#define FNX_LR_DEFAULT          (1024)
#define FNX_LR_UNLIMITED        (0)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Interface log-message.
 */
struct fnx_logmsg {
	int lm_level;
	int lm_errno;
	int lm_line;
	int lm_pad;
	const char *lm_file;
	const char *lm_func;
	const char *lm_ident;
	const char *lm_msgstr;
};
typedef struct fnx_logmsg   fnx_logmsg_t;

/*
 * Logging interface.
 */
struct fnx_logger {
	/* Control flags */
	int log_flags;

	/* Normal/debug threshold level */
	int log_level;
	int log_trace;

	/*
	 * Basic rate-limit mechanism: drop non-critical messages in case of
	 * overflow. Should be effective mainly in cases if logging diarrhea.
	 * Zero log_rate indicates disabling; log_count and log_last are used by
	 * internal logger implementation.
	 */
	size_t log_rate;
	size_t log_count;
	time_t log_last;

	/* Optional ident-prefix */
	const char *log_ident;

	/*
	 * Test-message hook. Returns status code: 0 where message should be
	 * sent, non-zero where message should be discarded.
	 */
	int (*log_testmsg)(struct fnx_logger *, const struct fnx_logmsg *);

	/*
	 * Send-message hook.
	 */
	void (*log_sendmsg)(struct fnx_logger *, const struct fnx_logmsg *);
};
typedef struct fnx_logger   fnx_logger_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Initialize logger with default hooks & flags. */
void fnx_logger_init(fnx_logger_t *logger);

/* Destructor */
void fnx_logger_destroy(fnx_logger_t *logger);

/*
 * Initialize logging facility: Limits the number of logging-messages to @rate
 * per sec, or don't use rate-limit mechanism if @rate is zero. Sets @flags
 * for control, allow only messages above @level.
 */
void fnx_logger_setup(fnx_logger_t *logger, const char *ident,
                      int level, int debug, int flags, size_t rate);


/* Logging facility API. Prefer the above macros. */
void fnx_logf(fnx_logger_t *logger, int level,
              const char *file, int line, const char *func,
              const char *fmt, ...);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/* Default logger instance: */
extern fnx_logger_t *fnx_default_logger;

/* Foramt checker flags -- keep zero */
extern short fnx_check_format;


#endif /* FNX_INFRA_LOGGER_H_ */




