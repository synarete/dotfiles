/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * This file is part of libvoluta
 *
 * Copyright (C) 2020 Shachar Sharon
 *
 * Libvoluta is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Libvoluta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */
#define _GNU_SOURCE 1
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "voluta-lib.h"

int voluta_thread_sigblock_common(void)
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
	sigaddset(&sigset_th, SIGURG);
	sigaddset(&sigset_th, SIGPROF);
	sigaddset(&sigset_th, SIGWINCH);
	sigaddset(&sigset_th, SIGIO);

	return pthread_sigmask(SIG_BLOCK, &sigset_th, NULL);
}

static void *voluta_thread_start(void *arg)
{
	struct voluta_thread *thread = (struct voluta_thread *)arg;

	thread->finish_time = 0;
	thread->start_time = time(NULL);
	thread->exec(thread);
	thread->finish_time = time(NULL);

	return thread;
}

int voluta_thread_create(struct voluta_thread *thread)
{
	int err;
	pthread_attr_t attr;
	void *(*start)(void *arg) = voluta_thread_start;

	if (thread->thread || !thread->exec) {
		return -EINVAL;
	}

	err = pthread_attr_init(&attr);
	if (!err) {
		thread->start_time = thread->finish_time = 0;
		err = pthread_create(&thread->thread, &attr, start, thread);
		pthread_attr_destroy(&attr);
	}
	return err;
}

int voluta_thread_join(struct voluta_thread *thread)
{
	return pthread_join(thread->thread, NULL);
}


int voluta_mutex_init(struct voluta_mutex *mutex)
{
	int err, kind = PTHREAD_MUTEX_NORMAL;
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
	err = pthread_mutexattr_settype(&attr, kind);
	if (err) {
		return err;
	}
	err = pthread_mutex_init(&mutex->mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	if (err) {
		return err;
	}
	return 0;
}

void voluta_mutex_destroy(struct voluta_mutex *mutex)
{
	int err;

	err = pthread_mutex_destroy(&mutex->mutex);
	if (err) {
		voluta_panic("pthread_mutex_destroy: %d", err);
	}
}

void voluta_mutex_lock(struct voluta_mutex *mutex)
{
	int err;

	err = pthread_mutex_lock(&mutex->mutex);
	if (err) {
		voluta_panic("pthread_mutex_lock: %d", err);
	}
}

bool voluta_mutex_rylock(struct voluta_mutex *mutex)
{
	int err;
	bool status = false;

	err = pthread_mutex_trylock(&mutex->mutex);
	if (err == 0) {
		status = true;
	} else if (err == EBUSY) {
		status = false;
	} else {
		voluta_panic("pthread_mutex_trylock: %d", err);
	}
	return status;
}

void voluta_mutex_unlock(struct voluta_mutex *mutex)
{
	int err;

	err = pthread_mutex_unlock(&mutex->mutex);
	if (err) {
		voluta_panic("pthread_mutex_unlock: %d", err);
	}
}
