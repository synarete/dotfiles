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
#ifndef FNX_INFRA_TIMEX_H_
#define FNX_INFRA_TIMEX_H_

/* Make standard timespec part of fnx namespace */
struct timeval;
struct timespec;
typedef struct timespec fnx_timespec_t;


/* Wrappers over _sleep */
void fnx_usleep(unsigned long usec);
void fnx_msleep(unsigned long msec);

/* Assign nil values */
void fnx_timespec_init(fnx_timespec_t *);

/* Timespecs copy helper */
void fnx_timespec_copy(fnx_timespec_t *, const fnx_timespec_t *);

/* Fill timespec with current time using 'gettimeofday' */
void fnx_timespec_gettimeofday(fnx_timespec_t *);

/* Fill timespec with current MONOTONIC time using 'clock_gettime' */
void fnx_timespec_getmonotime(fnx_timespec_t *);

/* Adds n usec to tp value's, protect against possible overlap */
void fnx_timespec_usecadd(fnx_timespec_t *, long n);

/* Adds n msec (millisec) to tp value's, protect against possible overlap. */
void fnx_timespec_msecadd(fnx_timespec_t *, long n);

/* Returns the diff in usecs between two timespecs */
long fnx_timespec_usecdiff(const fnx_timespec_t *, const fnx_timespec_t *);

/* Returns the diff in msecs between two timespecs */
long fnx_timespec_msecdiff(const fnx_timespec_t *, const fnx_timespec_t *);

/* Convert timespec to millisec integer */
long fnx_timespec_millisec(const fnx_timespec_t *);

/* Convert timespec to microsec integer */
long fnx_timespec_microsec(const fnx_timespec_t *);

/* Fill timespec from millisec|microsec integer values */
void fnx_ts_from_millisec(fnx_timespec_t *, long millisec_value);
void fnx_ts_from_microsec(fnx_timespec_t *, long microsec_value);


/* Convert timespec <--> timeval */
void fnx_timespec_to_timeval(const fnx_timespec_t *, struct timeval *);

void fnx_timespec_from_timeval(fnx_timespec_t *, const struct timeval *);

#endif /* FNX_INFRA_TIMEX_H_ */



