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
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <fnxserv.h>
#include "logging.h"
#include "process.h"

/* Local functions */
static const char *funex_str_signum(int signum);



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Signal operations:
 *
 * Divides signals into 4 groups: ignore, term, abort and no-catch:
 *
 * ignore:   catch and do nothing
 * term:     terminate process
 * abort:    abort
 * no-catch: nothing
 *
 * In case of term signals, apply call-back hook (if set) for proper cleanup.
 * The callback hook is executed from the context of signal handler; should
 * return non-zero value in order to re-raise the signal.
 */
static funex_sigaction_cb s_halt_callback = NULL;

/*
 * Signal-action which simply terminates the process. See example in GNU libc
 * documentations (24.4.2).
 */
static void funex_sigaction_halt_handler(int signum, siginfo_t *si, void *p)
{
	int reraise = 1;
	funex_sigaction_cb user_hook = s_halt_callback;
	static volatile sig_atomic_t s_halt_in_progress = 0;

	/*
	 * Since this handler is established for more than one kind of signal, it
	 * might still get invoked recursively by delivery of some other kind of
	 * signal. Using a static variable to keep track of that.
	 */
	if (s_halt_in_progress) {
		raise(signum);
		return;
	}

	/* Do actual cleanups via call-back hook */
	s_halt_in_progress = 1;
	if (user_hook != NULL) {
		reraise = user_hook(signum, si, p);
	}
	s_halt_in_progress = 0;

	/*
	 * Re-raise the signal. We reactivate the signal's default handling, which
	 * is to terminate the process. We could just call exit or abort, but
	 * re-raising the signal sets the return status from the process correctly.
	 */
	if (reraise) {
		signal(signum, SIG_DFL);
		raise(signum);
	}
}

/*
 * Signal-action which forces abort.
 */
static void funex_sigaction_abort_handler(int signum, siginfo_t *si, void *p)
{
	(void)si;
	(void)signum;
	(void)p;
	abort();
}


static struct sigaction s_sigaction_ign = {
	.sa_handler     = SIG_IGN
};

static struct sigaction s_sigaction_halt = {
	.sa_sigaction   = funex_sigaction_halt_handler,
	.sa_flags       = SA_SIGINFO
};

static struct sigaction s_sigaction_abort = {
	.sa_sigaction   = funex_sigaction_abort_handler,
	.sa_flags       = SA_SIGINFO
};


/*
 * Register signal-action handlers.
 */
static void register_sigaction(int signum, const struct sigaction *act)
{
	int rc;

	rc = sigaction(signum, act, NULL);
	if (rc != 0) {
		fnx_panic("sigaction-failed signum=%d rc=%d", signum, rc);
	}
}

static void funex_sigaction_ignore(int signum)
{
	fnx_info("Sigaction <%s>: ignore", funex_str_signum(signum));
	register_sigaction(signum,  &s_sigaction_ign);
}

static void funex_sigaction_halt(int signum)
{
	fnx_info("Sigaction <%s>: halt", funex_str_signum(signum));
	register_sigaction(signum,  &s_sigaction_halt);
}

static void funex_sigaction_abort(int signum)
{
	fnx_info("Sigaction <%s>: abort", funex_str_signum(signum));
	register_sigaction(signum,  &s_sigaction_abort);
}

static void funex_sigaction_nocatch(int signum)
{
	fnx_info("Sigaction <%s>: nocatch", funex_str_signum(signum));
}

void funex_register_sigactions(void)
{
	/*
	 * NB: SIGCHLD
	 * FUSE mount depends on SIGCHLD; do not ignore.
	 */
	funex_sigaction_ignore(SIGHUP);
	funex_sigaction_halt(SIGINT);
	funex_sigaction_halt(SIGQUIT);
	funex_sigaction_abort(SIGILL);
	funex_sigaction_ignore(SIGTRAP);
	funex_sigaction_nocatch(SIGABRT); /* Do not catch -- abort is abort! */
	funex_sigaction_halt(SIGBUS);
	funex_sigaction_nocatch(SIGFPE);
	funex_sigaction_nocatch(SIGKILL);
	funex_sigaction_ignore(SIGUSR1);
	funex_sigaction_abort(SIGSEGV);
	funex_sigaction_ignore(SIGUSR2);
	funex_sigaction_ignore(SIGPIPE);
	funex_sigaction_ignore(SIGALRM);
	funex_sigaction_halt(SIGTERM);
	funex_sigaction_abort(SIGSTKFLT);
	/*fnx_sigaction_ignore(SIGCHLD);*/ /* FIXME */
	funex_sigaction_ignore(SIGCONT);
	funex_sigaction_nocatch(SIGSTOP);
	funex_sigaction_halt(SIGTSTP);
	funex_sigaction_halt(SIGTTIN);
	funex_sigaction_halt(SIGTTOU);
	funex_sigaction_ignore(SIGURG);
	funex_sigaction_halt(SIGXCPU);
	funex_sigaction_halt(SIGXFSZ);
	funex_sigaction_halt(SIGVTALRM);
	funex_sigaction_nocatch(SIGPROF);
	funex_sigaction_ignore(SIGWINCH);
	funex_sigaction_ignore(SIGIO);
	funex_sigaction_halt(SIGPWR);
	funex_sigaction_halt(SIGSYS);
}

/*
 * Sets signal-blocking mask for calling thread (main).
 */
void funex_sigblock(void)
{
	int rc;
	sigset_t sigset_th;

	sigemptyset(&sigset_th);
	sigaddset(&sigset_th, SIGHUP);
	sigaddset(&sigset_th, SIGINT);
	sigaddset(&sigset_th, SIGQUIT);
	sigaddset(&sigset_th, SIGTERM);
	sigaddset(&sigset_th, SIGTRAP);
	sigaddset(&sigset_th, SIGUSR1);
	sigaddset(&sigset_th, SIGUSR2);
	sigaddset(&sigset_th, SIGPIPE);
	sigaddset(&sigset_th, SIGALRM);
	sigaddset(&sigset_th, SIGCHLD);
	sigaddset(&sigset_th, SIGCONT);
	sigaddset(&sigset_th, SIGWINCH);
	sigaddset(&sigset_th, SIGIO);

	rc = pthread_sigmask(SIG_BLOCK, &sigset_th, NULL);
	if (rc != 0) {
		fnx_panic("sigmask-failure rc=%d", rc);
	}
}

void funex_sigunblock(void)
{
	int rc;
	sigset_t sigset_th;

	sigfillset(&sigset_th);

	rc = pthread_sigmask(SIG_UNBLOCK, &sigset_th, NULL);
	if (rc != 0) {
		fnx_panic("sigmask-failure rc=%d", rc);
	}
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void funex_register_sighalt(funex_sigaction_cb cb)
{
	s_halt_callback = cb;
}

void funex_report_siginfo(const void *ptr)
{
	const siginfo_t *si = (const siginfo_t *)ptr;

	fnx_info("si_signo=%d si_errno=%d si_code=%d si_pid=%d si_uid=%d",
	         si->si_signo, si->si_errno, si->si_code,
	         si->si_pid, si->si_uid);
}

/* Report last signal, abort if fatal */
void funex_handle_lastsig(int signum, const void *ptr)
{
	if (signum != 0) {
		funex_report_siginfo(ptr);
	}
	switch (signum) {
		case SIGBUS:
		case SIGFPE:
		case SIGSEGV:
			abort();
		default:
			break;
	}
}

/*
 * Converts signal-number to human readable string:
 */
static const char *s_signum_str[] = {
	[SIGHUP]        = "SIGHUP",
	[SIGINT]        = "SIGINT",
	[SIGQUIT]       = "SIGQUIT",
	[SIGILL]        = "SIGILL",
	[SIGTRAP]       = "SIGTRAP",
	[SIGABRT]       = "SIGABRT",
	[SIGBUS]        = "SIGBUS",
	[SIGFPE]        = "SIGFPE",
	[SIGKILL]       = "SIGKILL",
	[SIGUSR1]       = "SIGUSR1",
	[SIGSEGV]       = "SIGSEGV",
	[SIGUSR2]       = "SIGUSR2",
	[SIGPIPE]       = "SIGPIPE",
	[SIGALRM]       = "SIGALRM",
	[SIGTERM]       = "SIGTERM",
	[SIGSTKFLT]     = "SIGSTKFLT",
	[SIGCHLD]       = "SIGCHLD",
	[SIGCONT]       = "SIGCONT",
	[SIGSTOP]       = "SIGSTOP",
	[SIGTSTP]       = "SIGTSTP",
	[SIGTTIN]       = "SIGTTIN",
	[SIGTTOU]       = "SIGTTOU",
	[SIGURG]        = "SIGURG",
	[SIGXCPU]       = "SIGXCPU",
	[SIGXFSZ]       = "SIGXFSZ",
	[SIGVTALRM]     = "SIGVTALRM",
	[SIGPROF]       = "SIGPROF",
	[SIGWINCH]      = "SIGWINCH",
	[SIGIO]         = "SIGIO",
	[SIGPWR]        = "SIGPWR",
	[SIGSYS]        = "SIGSYS",
};

static const char *funex_str_signum(int signum)
{
	int nelems;
	const char *s;

	nelems = (int)FNX_ARRAYSIZE(s_signum_str);
	if ((signum > 0) && (signum < nelems)) {
		s = s_signum_str[signum];
	} else {
		s = "SIGXXX";
	}
	return s;
}

