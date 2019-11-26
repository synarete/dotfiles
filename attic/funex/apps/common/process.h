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
#ifndef FUNEX_PROCESS_H_
#define FUNEX_PROCESS_H_

#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>


/* Process's first call: verify, setup default and enable panic */
void funex_init_process(int argc, char *argv[]);

/* System setup */
void funex_setup_process(void);

/* Cleanups */
void funex_finalize(void);

/* Cleanups + exit OK */
void funex_finalize_exit(void);

/* Up-time */
void funex_uptime_str(char *, size_t);

/* Gracefully/Failure exit */
FNX_ATTR_NORETURN
void funex_exit(int rc);

FNX_ATTR_NORETURN
void funex_exiterr(void);

FNX_ATTR_NORETURN
void funex_goodbye(void);

/* Halt execution & die (abort)  */
FNX_ATTR_NORETURN
void funex_die(void);

FNX_ATTR_NORETURN
void funex_dief(const char *fmt, ...);

/* Daemon panic call-back */
FNX_ATTR_NORETURN
void funex_fatal_error(int errnum, const char *msg);

/* Process configurations */
char *funex_username(void);

char *funex_selfexe(void);

char *funex_nodename(void);

char *funex_sysconfdir(void);

char *funex_mntusock(void);

/* Daemon */
void funex_daemonize(void);

int funex_forknwait(void (*fn)(void), pid_t *);

void funex_forkd(void (*start)(void));


/* Resource */
void funex_getrlimit_as(struct rlimit *);

void funex_getrlimit_nproc(struct rlimit *);

void funex_setrlimit_core(long);

/* Signals */
typedef int (*funex_sigaction_cb)(int, const siginfo_t *, void *);

void funex_register_sighalt(funex_sigaction_cb);

void funex_report_siginfo(const void *);

void funex_handle_lastsig(int, const void *);

void funex_sigblock(void);

void funex_sigunblock(void);

void funex_register_sigactions(void);

/* Capabilities */
int funex_cap_is_sys_admin(void);

/* Singleton-object */
void *funex_new_sobject(size_t);

void funex_del_sobject(void *, size_t);


#endif /* FUNEX_PROCESS_H_ */



