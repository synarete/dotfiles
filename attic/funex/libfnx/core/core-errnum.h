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
#ifndef FNX_CORE_ERRNUM_H_
#define FNX_CORE_ERRNUM_H_


/*
 * Internal status/error-codes:
 */

/* Pend operation */
#define FNX_EPEND           (-10001)

/* Delay reply */
#define FNX_EDELAY          (-10002)

/* Task is in async execution */
#define FNX_EASYNC          (-10003)

/* Marks end-of-stream upon readdir */
#define FNX_EEOS            (-10004)

/* Non-existent of reg-file sub element */
#define FNX_ENOREGS         (-10005)

/* Element not cached */
#define FNX_ECACHEMISS      (-10006)

/* Complex rename-swap need */
#define FNX_ERERENAME       (-10007)


#endif /* FNX_CORE_ERRNUM_H_ */


