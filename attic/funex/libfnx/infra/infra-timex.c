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
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "infra-compiler.h"
#include "infra-macros.h"
#include "infra-panic.h"
#include "infra-utility.h"
#include "infra-timex.h"

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_usleep(unsigned long usec)
{
	int rc;
	fnx_timespec_t req = { 0, 0 };
	fnx_timespec_t rem = { 0, 0 };

	fnx_timespec_usecadd(&req, (int64_t)usec);

	errno = 0;
	while (1) {
		rc = nanosleep(&req, &rem);
		if (rc == 0) {
			break;
		}
		if ((rc == -1) && (errno != EINTR)) {
			fnx_panic("rc=%d", rc);
		}
		fnx_bcopy(&req, &rem, sizeof(req));
	}
}
void fnx_msleep(unsigned long millisec)
{
	fnx_usleep(millisec * 1000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_timespec_init(fnx_timespec_t *ts)
{
	ts->tv_sec  = 0;
	ts->tv_nsec = 0;
}

void fnx_timespec_copy(fnx_timespec_t *ts_tgt, const fnx_timespec_t *ts_src)
{
	ts_tgt->tv_sec  = ts_src->tv_sec;
	ts_tgt->tv_nsec = ts_src->tv_nsec;
}

void fnx_timespec_gettimeofday(fnx_timespec_t *ts)
{
	int rc;
	struct timeval tv;

	/*
	 * NB: man (2) gettimeofday on Linux says:
	 * POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of
	 * clock_gettime(2) instead.
	 */
	errno = 0;
	rc = gettimeofday(&tv, NULL);
	if (rc != 0) {
		fnx_panic("gettimeofday-failure rc=%d", rc);
	}
	fnx_timespec_from_timeval(ts, &tv);
}

void fnx_timespec_getmonotime(fnx_timespec_t *ts)
{
	int rc;
	const clockid_t clk_id = CLOCK_MONOTONIC; /* CLOCK_MONOTONIC_COARSE */

	errno = 0;
	rc = clock_gettime(clk_id, ts);
	if (rc != 0) {
		fnx_panic("clock_gettime clk_id=%d rc=%d", clk_id, rc);
	}
}

void fnx_timespec_usecadd(fnx_timespec_t *tp, long n)
{
	const int64_t k = 1000;
	const int64_t m = 1000000;
	const int64_t g = k * m;
	int64_t sec1    = tp->tv_sec;

	if (n >= m) {
		tp->tv_sec += (time_t)(n / m);
		n = n % m;
	}

	tp->tv_nsec += (long)(n * k);
	if (tp->tv_nsec >= g) {
		tp->tv_sec  += 1;
		tp->tv_nsec = (long)(tp->tv_nsec % g);
	}

	if (sec1 > tp->tv_sec) {
		fnx_panic("timespec-wraparound (sec1=%ld tp->tv_sec=%ld)",
		          (long) sec1, (long) tp->tv_sec);
	}
}

void fnx_timespec_msecadd(fnx_timespec_t *tp, long n)
{
	fnx_timespec_usecadd(tp, n * 1000);
}

long fnx_timespec_usecdiff(const fnx_timespec_t *tp1, const fnx_timespec_t *tp2)
{
	const long k = 1000L;
	long dif = 0;

	if (tp2->tv_sec == tp1->tv_sec) {
		dif = tp2->tv_nsec / k - tp1->tv_nsec / k;
	} else {
		dif = (tp2->tv_sec - tp1->tv_sec) * (k * k);
		dif += tp2->tv_nsec / k;
		dif -= tp1->tv_nsec / k;
	}

	return dif;
}

long fnx_timespec_msecdiff(const fnx_timespec_t *tp1,
                           const fnx_timespec_t *tp2)
{
	const long k = 1000;

	return (fnx_timespec_usecdiff(tp1, tp2) + (k / 2)) / k;
}

long fnx_timespec_millisec(const fnx_timespec_t *ts)
{
	const long k = 1000;
	const long m = 1000000;
	int64_t n;

	n = (int64_t) ts->tv_sec * k;
	n += (int64_t) ts->tv_nsec / m;

	if (n < 0) {
		fnx_panic("Timespec-to-millisec convertion failure "\
		          "tp={%lld, %lld} n=%lld",
		          (long long) ts->tv_sec,
		          (long long) ts->tv_nsec,
		          (long long) n);
	}

	return n;
}

long fnx_timespec_microsec(const fnx_timespec_t *ts)
{
	long n;

	n = (int64_t) ts->tv_sec * 1000000;
	n += (int64_t) ts->tv_nsec / 1000;

	if (n < 0) {
		fnx_panic("timespec={%lld, %lld} microsec=%lld",
		          (long long)ts->tv_sec, (long long)ts->tv_nsec, (long long)n);
	}
	return n;
}


void fnx_ts_from_millisec(fnx_timespec_t *ts, long millisec_value)
{
	ts->tv_sec  = (time_t)(millisec_value / 1000);
	ts->tv_nsec = (long int)((millisec_value % 1000) * 1000000);
}

void fnx_ts_from_microsec(fnx_timespec_t *ts, long microsec_value)
{
	ts->tv_sec  = (time_t)(microsec_value / 1000000);
	ts->tv_nsec = (long int)((microsec_value % 1000000) * 1000);
}

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/

void fnx_timespec_to_timeval(const fnx_timespec_t *ts, struct timeval *tv)
{
	tv->tv_sec  = ts->tv_sec;
	tv->tv_usec = ts->tv_nsec / 1000;
}

void fnx_timespec_from_timeval(fnx_timespec_t *ts, const struct timeval *tv)
{
	const long usec = (long)(tv->tv_usec);

	ts->tv_sec  = tv->tv_sec;
	ts->tv_nsec = (long)(usec * 1000);
}

