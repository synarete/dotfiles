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
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <syscall.h>
#include <sys/prctl.h>

#include "infra-compiler.h"
#include "infra-macros.h"
#include "infra-utility.h"
#include "infra-panic.h"
#include "infra-timex.h"
#include "infra-thread.h"


/*
 * Controls mutex type: for development ('-g -O0') use non-portable error
 * checking, otherwise use default.
 */
#if (FNX_DEBUG > 0) && (FNX_OPTIMIZE == 0)
#define FNX_MUTEX_KIND PTHREAD_MUTEX_ERRORCHECK_NP
#else
/* #define FNX_MUTEX_KIND PTHREAD_MUTEX_DEFAULT */
#define FNX_MUTEX_KIND PTHREAD_MUTEX_ADAPTIVE_NP
#endif

/*
 * Panic in cases of failure, where rc is returned value from one of
 * pthread's functions
 */
#define fnx_check_pthread_rc(rc) \
	do { if (rc != 0) \
			fnx_panic("%s: rc=%d", FNX_FUNCTION, rc); } while (0)

/*
 * Global threading mutex
 */
pthread_mutex_t s_thread_mutex = PTHREAD_MUTEX_INITIALIZER;



/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Mutex */

#define FNX_MUTEX_MAGIC (0xAA01BB13)

#define fnx_mutex_check(mutex) \
	do { if (fnx_unlikely(mutex->magic != mutex_magic(mutex))) \
			fnx_panic("mutex=%p magic=%lx",  \
			          (void*)mutex, (long)(mutex->magic)); \
	} while (0)


static unsigned long mutex_magic(const fnx_mutex_t *mutex)
{
	const unsigned long addr = (unsigned long)((const void *)mutex);
	return (addr ^ FNX_MUTEX_MAGIC);
}

void fnx_mutex_init(fnx_mutex_t *mutex)
{
	int rc, kind = FNX_MUTEX_KIND;
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	rc = pthread_mutexattr_settype(&attr, kind);
	fnx_check_pthread_rc(rc);

	pthread_mutex_init(&mutex->mutex, &attr);
	pthread_mutexattr_destroy(&attr);

	mutex->magic = mutex_magic(mutex);
}

void fnx_mutex_destroy(fnx_mutex_t *mutex)
{
	int rc;

	fnx_mutex_check(mutex);
	rc = pthread_mutex_destroy(&mutex->mutex);
	fnx_check_pthread_rc(rc);

	memset(mutex, 0xEE, sizeof(*mutex));
}

void fnx_mutex_lock(fnx_mutex_t *mutex)
{
	int rc;

	fnx_mutex_check(mutex);
	rc = pthread_mutex_lock(&mutex->mutex);
	fnx_check_pthread_rc(rc);
}

int fnx_mutex_trylock(fnx_mutex_t *mutex)
{
	int rc, status;

	rc = pthread_mutex_trylock(&mutex->mutex);
	if (rc == 0) {
		status = 0;
	} else {
		status = -rc;
		if (rc == EBUSY) {
			errno = rc;
		} else {
			fnx_check_pthread_rc(rc);
		}
	}
	return status;
}

void fnx_mutex_unlock(fnx_mutex_t *mutex)
{
	int rc;

	fnx_mutex_check(mutex);
	rc = pthread_mutex_unlock(&mutex->mutex);
	fnx_check_pthread_rc(rc);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/* Condition variable */

#define FNX_COND_MAGIC (0xCC0011DD)

#define fnx_cond_check(cond) \
	do { if (fnx_unlikely(cond->magic != cond_magic(cond))) \
			fnx_panic("cond=%p magic=%lx", \
			          (void*)cond, (long)(cond->magic));\
	} while (0)


static unsigned long cond_magic(const fnx_cond_t *cond)
{
	const unsigned long addr = (unsigned long)((const void *)cond);
	return (addr ^ FNX_COND_MAGIC);
}

void fnx_cond_init(fnx_cond_t *cond)
{
	int rc;
	pthread_condattr_t attr;

	rc = pthread_condattr_init(&attr);
	fnx_check_pthread_rc(rc);

	rc = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
	fnx_check_pthread_rc(rc);

	rc = pthread_cond_init(&cond->cond, &attr);
	fnx_check_pthread_rc(rc);

	rc = pthread_condattr_destroy(&attr);
	fnx_check_pthread_rc(rc);

	cond->magic = cond_magic(cond);
}

void fnx_cond_destroy(fnx_cond_t *cond)
{
	int rc;

	rc = pthread_cond_destroy(&cond->cond);
	fnx_check_pthread_rc(rc);

	memset(cond, 0xC0, sizeof(*cond));
}

void fnx_cond_wait(fnx_cond_t *cond, fnx_mutex_t *mutex)
{
	int rc;

	fnx_cond_check(cond);
	rc = pthread_cond_wait(&cond->cond, &mutex->mutex);
	fnx_check_pthread_rc(rc);
}

int fnx_cond_timedwait(fnx_cond_t *cond, fnx_mutex_t *mutex,
                       const fnx_timespec_t *abstime)
{
	int rc, status;

	fnx_mutex_check(mutex);
	fnx_cond_check(cond);
	rc = pthread_cond_timedwait(&cond->cond, &mutex->mutex, abstime);
	if (rc == 0) {
		status = 0;
	} else {
		status = -rc;
		if (rc == ETIMEDOUT) {
			errno = rc;
		} else if (rc == EINTR) {
			errno = rc;
		} else {
			fnx_check_pthread_rc(rc);
		}
	}
	return status;
}

void fnx_cond_signal(fnx_cond_t *cond)
{
	int rc;

	fnx_cond_check(cond);
	rc = pthread_cond_signal(&cond->cond);
	fnx_check_pthread_rc(rc);
}

void fnx_cond_broadcast(fnx_cond_t *cond)
{
	int rc;

	fnx_cond_check(cond);
	rc = pthread_cond_broadcast(&cond->cond);
	fnx_check_pthread_rc(rc);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Lock (Mutex + Condition):
 */

void fnx_lock_init(fnx_lock_t *lock)
{
	fnx_mutex_init(&lock->mutex);
	fnx_cond_init(&lock->cond);
}

void fnx_lock_destroy(fnx_lock_t *lock)
{
	fnx_mutex_destroy(&lock->mutex);
	fnx_cond_destroy(&lock->cond);
}

void fnx_lock_acquire(fnx_lock_t *lock)
{
	fnx_mutex_lock(&lock->mutex);
}

void fnx_lock_release(fnx_lock_t *lock)
{
	fnx_mutex_unlock(&lock->mutex);
}

void fnx_lock_wait(fnx_lock_t *lock)
{
	fnx_cond_wait(&lock->cond, &lock->mutex);
}

int fnx_lock_timedwait(fnx_lock_t *lock, const fnx_timespec_t *ts)
{
	return fnx_cond_timedwait(&lock->cond, &lock->mutex, ts);
}

void fnx_lock_signal(fnx_lock_t *lock)
{
	fnx_cond_signal(&lock->cond);
}

void fnx_lock_broadcast(fnx_lock_t *lock)
{
	fnx_cond_broadcast(&lock->cond);
}


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Read-Write Lock:
 */
void fnx_rwlock_init(fnx_rwlock_t *rw)
{
	int rc;
	pthread_rwlockattr_t attr;

	rc = pthread_rwlockattr_init(&attr);
	fnx_check_pthread_rc(rc);

	rc = pthread_rwlock_init(&rw->rwlock, &attr);
	fnx_check_pthread_rc(rc);

	rc = pthread_rwlockattr_destroy(&attr);
	fnx_check_pthread_rc(rc);
}

void fnx_rwlock_destroy(fnx_rwlock_t *rw)
{
	int rc;

	rc = pthread_rwlock_destroy(&rw->rwlock);
	fnx_check_pthread_rc(rc);

	memset(rw, 0xFE, sizeof(*rw));
}

void fnx_rwlock_rdlock(fnx_rwlock_t *rw)
{
	int rc;

	rc = pthread_rwlock_rdlock(&rw->rwlock);
	fnx_check_pthread_rc(rc);
}

void fnx_rwlock_wrlock(fnx_rwlock_t *rw)
{
	int rc;

	rc = pthread_rwlock_wrlock(&rw->rwlock);
	fnx_check_pthread_rc(rc);
}

int fnx_rwlock_tryrdlock(fnx_rwlock_t *rw)
{
	int rc, status;

	rc = pthread_rwlock_tryrdlock(&rw->rwlock);
	if (rc == 0) {
		status = 0;
	} else {
		status = -rc;
		if (rc == EBUSY) {
			errno = rc;
		} else {
			fnx_check_pthread_rc(rc);
		}
	}

	return status;
}

int fnx_rwlock_trywrlock(fnx_rwlock_t *rw)
{
	int rc, status;

	rc = pthread_rwlock_trywrlock(&rw->rwlock);
	if (rc == 0) {
		status = 0;
	} else {
		status = -rc;
		if (rc == EBUSY) {
			errno  = rc;
		} else {
			fnx_check_pthread_rc(rc);
		}
	}
	return status;
}

int fnx_rwlock_timedrdlock(fnx_rwlock_t *rw, size_t usec_timeout)
{
	int rc, status;
	fnx_timespec_t ts;

	fnx_timespec_getmonotime(&ts);
	fnx_timespec_usecadd(&ts, (long)usec_timeout);

	rc = pthread_rwlock_timedrdlock(&rw->rwlock, &ts);
	if (rc == 0) {
		status = 0;
	} else {
		status = -rc;
		if (rc == ETIMEDOUT) {
			errno  = rc;
		} else {
			fnx_check_pthread_rc(rc);
		}
	}
	return status;
}

int fnx_rwlock_timedwrlock(fnx_rwlock_t *rw, size_t usec_timeout)
{
	int rc, status = 0;
	fnx_timespec_t ts;

	fnx_timespec_getmonotime(&ts);
	fnx_timespec_usecadd(&ts, (long)usec_timeout);

	rc = pthread_rwlock_timedwrlock(&rw->rwlock, &ts);
	if (rc == 0) {
		status = 0;
	} else {
		status = -rc;
		if (rc == ETIMEDOUT) {
			errno = rc;
		} else {
			fnx_check_pthread_rc(rc);
		}
	}
	return status;
}

void fnx_rwlock_unlock(fnx_rwlock_t *rw)
{
	int rc;

	rc = pthread_rwlock_unlock(&rw->rwlock);
	fnx_check_pthread_rc(rc);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * SpinLock:
 */
void fnx_spinlock_init(fnx_spinlock_t *s)
{
	int rc, pshared = 0;

	rc = pthread_spin_init(&s->spinlock, pshared);
	fnx_check_pthread_rc(rc);
}

void fnx_spinlock_destroy(fnx_spinlock_t *s)
{
	int rc;

	rc = pthread_spin_destroy(&s->spinlock);
	fnx_check_pthread_rc(rc);

	memset(s, 0x77, sizeof(*s));
}

void fnx_spinlock_lock(fnx_spinlock_t *s)
{
	int rc;

	rc = pthread_spin_lock(&s->spinlock);
	fnx_check_pthread_rc(rc);
}

int fnx_spinlock_trylock(fnx_spinlock_t *s)
{
	int rc, status = 0;

	rc = pthread_spin_trylock(&s->spinlock);
	if (rc == 0) {
		status = 0;
	} else {
		status = -rc;
		if (rc == EBUSY) {
			errno = rc;
		} else {
			fnx_check_pthread_rc(rc);
		}
	}
	return status;
}

void fnx_spinlock_unlock(fnx_spinlock_t *s)
{
	int rc;

	rc = pthread_spin_unlock(&s->spinlock);
	fnx_check_pthread_rc(rc);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Threads:
 */
void fnx_thread_init(fnx_thread_t *th)
{
	fnx_bzero(th, sizeof(*th));

	th->th_exec = NULL;
	th->th_arg  = NULL;
	th->th_tid  = (pid_t)(-1);
	th->th_blocksig = 1;
}

void fnx_thread_destroy(fnx_thread_t *th)
{
	fnx_bzero(th, sizeof(*th));
	th->th_tid = (pid_t)(-1);
}

/* N.B. Must be called before start */
void fnx_thread_setname(fnx_thread_t *th, const char *name)
{
	size_t sz;

	sz = sizeof(th->th_name);
	fnx_bzero(th->th_name, sz);
	if (name != NULL) {
		strncpy(th->th_name, name, sz - 1);
	}
}


/* Wrapper for gettid (Linux specific) */
static pid_t fnx_gettid(void)
{
#if defined(SYS_gettid)
	return (pid_t)syscall(SYS_gettid);
#else
	return getpid();
#endif
}

static void thread_wait_tid(const fnx_thread_t *th)
{
	int rc, has_pid = 0;

	while (!has_pid) {
		rc = pthread_mutex_lock(&s_thread_mutex);
		fnx_check_pthread_rc(rc);
		has_pid = (th->th_tid > 0);
		rc = pthread_mutex_unlock(&s_thread_mutex);
		fnx_check_pthread_rc(rc);
		if (!has_pid) {
			fnx_msleep(10);
		}
	}
}

static void thread_fill_tid(fnx_thread_t *th)
{
	int rc;

	rc = pthread_mutex_lock(&s_thread_mutex);
	fnx_check_pthread_rc(rc);
	th->th_tid = fnx_gettid();
	rc = pthread_mutex_unlock(&s_thread_mutex);
	fnx_check_pthread_rc(rc);
}

/* Export thread's name to the system (Linux specific, for 'top') */
static void thread_set_title(const fnx_thread_t *th)
{
	int rc;

	if (strlen(th->th_name)) {
		rc = pthread_setname_np(th->th_id, th->th_name);
		fnx_check_pthread_rc(rc);
	}
	/* fnx_setproctitle(th->th_name); */
}

/* Disable some signals to thread */
static void thread_block_signals(const fnx_thread_t *th)
{
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
	pthread_sigmask(SIG_BLOCK, &sigset_th, NULL);
	fnx_unused(th);
}

/* Disable/Enable current thread cancellation */
static void thread_disable_cancel(const fnx_thread_t *th)
{
	int rc;
	long long tid;

	rc = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	if (rc != 0) {
		tid = (long long)th->th_tid;
		fnx_panic("pthread-cancel-disable: rc=%d tid=%#llx", rc, tid);
	}
}

static void thread_enable_cancel(const fnx_thread_t *th)
{
	int rc;
	long long tid;

	rc = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (rc != 0) {
		tid = (long long)th->th_tid;
		fnx_panic("pthread-cancel-enable: rc=%d tid=%#llx", rc, tid);
	}
}


/*
 * Entry point for Funex threads. Disable cancellation while executing user's
 * call. The thread_name and thread_pid args are used just for debugger
 * back-tarcing readability.
 */
static void fnx_thread_exec(const fnx_thread_t *th,
                            const char *thread_name, pid_t thread_pid)
{
	void *arg = th->th_arg;

	/* Prepare */
	if (th->th_blocksig) {
		thread_block_signals(th);
	}

	/* Execute (user's hook) */
	thread_disable_cancel(th);
	th->th_exec(arg);
	thread_enable_cancel(th);

	(void)thread_name;
	(void)thread_pid;
}

static void *fnx_start_thread(void *p)
{
	fnx_thread_t *th = (fnx_thread_t *)p;

	/* Export threads name */
	thread_set_title(th);

	/* Get system tid */
	thread_fill_tid(th);

	/* Execute user's call. */
	fnx_thread_exec(th, th->th_name, th->th_tid);

	/* Done */
	pthread_exit(NULL);
	return NULL;
}

/*
 * Creates new thread (start execution).
 */
void fnx_thread_start(fnx_thread_t *th, fnx_thread_fn fn, void *arg)
{
	int rc;
	pthread_attr_t attr;

	th->th_exec = fn;
	th->th_arg  = arg;
	th->th_tid  = (pid_t)(-1);

	errno = 0;
	rc = pthread_attr_init(&attr);
	fnx_check_pthread_rc(rc);

	rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	fnx_check_pthread_rc(rc);

	rc = pthread_create(&th->th_id, &attr, fnx_start_thread, th);
	fnx_check_pthread_rc(rc);

	pthread_attr_destroy(&attr);

	/* Do not return until sub-thread has start running */
	thread_wait_tid(th);
}

/*
 * Threads-join wrapper
 */
int fnx_thread_join(const fnx_thread_t *other, int flags)
{
	int rc;
	void *retval = NULL;
	const char *name;

	errno  = 0;
	name = other->th_name;
	rc = pthread_join(other->th_id, &retval);
	if (rc != 0) {
		if ((rc == ESRCH) && !(flags & FNX_NOFAIL)) {
			return -rc;
		}
		switch (rc) {
			case ESRCH:
			case EDEADLK:
			case EINVAL:
			default:
				fnx_panic("no-pthread-join name=%s th_id=%#lx rc=%d",
				          name, (unsigned long)other->th_id, rc);
				return -rc;
		}
	}
	return 0;
}

void fnx_setproctitle(const char *name)
{
	if (name && strlen(name)) {
		prctl(PR_SET_NAME, name, 0, 0, 0);
	}
}


