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
#ifndef FNX_INFRA_THREAD_H_
#define FNX_INFRA_THREAD_H_


/*
 * Max thread name. See man PRCTL(2), PR_SET_NAME
 */
#define FNX_THREAD_NAME_MAXLEN     15


/*
 * Error-checking wapperes over POSIX-threads (pthread) functionality;
 * Panic in case lib-pthread returns error.
 */
struct fnx_mutex {
	unsigned long   magic;
	pthread_mutex_t mutex;
};
typedef struct fnx_mutex        fnx_mutex_t;


struct fnx_cond {
	unsigned long   magic;
	pthread_cond_t  cond;
};
typedef struct fnx_cond         fnx_cond_t;


struct fnx_rwlock {
	pthread_rwlock_t rwlock;
};
typedef struct fnx_rwlock       fnx_rwlock_t;


struct fnx_spinlock {
	pthread_spinlock_t spinlock;
};
typedef struct fnx_spinlock     fnx_spinlock_t;


struct fnx_lock {
	fnx_mutex_t  mutex;
	fnx_cond_t   cond;
};
typedef struct fnx_lock fnx_lock_t;


/*
 * Wrapper over pthread threading. Should not exit in the middle, may not be
 * cancelated.
 */
typedef void (*fnx_thread_fn)(void *);

struct fnx_thread {
	fnx_thread_fn   th_exec;        /* Entry point          */
	pthread_t       th_id;          /* PThread ID           */
	pid_t           th_tid;         /* Linux gettid         */
	void           *th_arg;         /* Execution arg        */
	char            th_blocksig;    /* Bool: block signals  */
	char            th_name[16];    /* Optional name        */
};
typedef struct fnx_thread    fnx_thread_t;


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Mutex wrapper. Always using error-checking mutex. Lock is blocking.
 * Trylock returns 0 upon success, or -1 upon failure.
 */
void fnx_mutex_init(fnx_mutex_t *m);
void fnx_mutex_destroy(fnx_mutex_t *m);
void fnx_mutex_lock(fnx_mutex_t *m);
int  fnx_mutex_trylock(fnx_mutex_t *m);
void fnx_mutex_unlock(fnx_mutex_t *m);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Condition wrapper. In case of cond-wait and cond-timedwait, it is up to the
 * user to explicitly lock/unlock the associated mutex. The returned value of
 * cond-timedwait is 0 upon success; non-zero value (-ETIMEDOUT) upon
 * timeout.
 */
void fnx_cond_init(fnx_cond_t *c);
void fnx_cond_destroy(fnx_cond_t *c);
void fnx_cond_wait(fnx_cond_t *c, fnx_mutex_t *);
int  fnx_cond_timedwait(fnx_cond_t *c, fnx_mutex_t *, const fnx_timespec_t *);
void fnx_cond_signal(fnx_cond_t *c);
void fnx_cond_broadcast(fnx_cond_t *c);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Lock wrapper: a condition + mutex pair.
 */
void fnx_lock_init(fnx_lock_t *lk);
void fnx_lock_destroy(fnx_lock_t *lk);
void fnx_lock_acquire(fnx_lock_t *lk);
void fnx_lock_release(fnx_lock_t *lk);
void fnx_lock_wait(fnx_lock_t *lk);
int  fnx_lock_timedwait(fnx_lock_t *lk, const fnx_timespec_t *);
void fnx_lock_signal(fnx_lock_t *lk);
void fnx_lock_broadcast(fnx_lock_t *lk);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Read-Write lock wrapper. Try-rdlock returns 0 upon success, -1
 * upon failure. Timed-rdlock and Timed-wrlock returns 0 upon success,
 * non-zero value (-ETIMEDOUT) upon timeout.
 */
void fnx_rwlock_init(fnx_rwlock_t *rw);
void fnx_rwlock_destroy(fnx_rwlock_t *rw);
void fnx_rwlock_rdlock(fnx_rwlock_t *rw);
void fnx_rwlock_wrlock(fnx_rwlock_t *rw);
int  fnx_rwlock_tryrdlock(fnx_rwlock_t *rw);
int  fnx_rwlock_trywrlock(fnx_rwlock_t *rw);
int  fnx_rwlock_timedrdlock(fnx_rwlock_t *rw, size_t usec_timeout);
int  fnx_rwlock_timedwrlock(fnx_rwlock_t *rw, size_t usec_timeout);
void fnx_rwlock_unlock(fnx_rwlock_t *rw);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Spinlock wrapper. Try-lock returns 0 upon success, and -1 upon
 * failure.
 */
void fnx_spinlock_init(fnx_spinlock_t *s);
void fnx_spinlock_destroy(fnx_spinlock_t *s);
void fnx_spinlock_lock(fnx_spinlock_t *s);
int  fnx_spinlock_trylock(fnx_spinlock_t *s);
void fnx_spinlock_unlock(fnx_spinlock_t *s);

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

/*
 * Initialize or destroy thread-container object with defaults.
 */
void fnx_thread_init(fnx_thread_t *);

void fnx_thread_destroy(fnx_thread_t *);

/*
 * Associate short-name string with thread's execution context.
 * N.B. Must be calles before start.
 */
void fnx_thread_setname(fnx_thread_t *, const char *);

/*
 * Creates new thread and starts execution. Will not return until sub-thread
 * has start running
 */
void fnx_thread_start(fnx_thread_t *, void (*fn)(void *), void *arg);

/*
 * Join a thread. Returns 0 upon success, -1 if no thread-ID was found and flags
 * is zero. If flags is FNX_NOFAIL, panics upon failure.
 */
int fnx_thread_join(const fnx_thread_t *, int flags);

/*
 * Wrapper over 'prctl' to implement BSD-like 'setproctitle'. Linux specific.
 */
void fnx_setproctitle(const char *);


#endif /* FNX_INFRA_THREAD_H_ */

