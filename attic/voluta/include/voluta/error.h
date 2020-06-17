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
#ifndef VOLUTA_ERROR_H_
#define VOLUTA_ERROR_H_


/* compile-time assertions */
#define VOLUTA_STATICASSERT(expr_)       _Static_assert(expr_, #expr_)
#define VOLUTA_STATICASSERT_EQ(a_, b_)   VOLUTA_STATICASSERT(a_ == b_)
#define VOLUTA_STATICASSERT_LE(a_, b_)   VOLUTA_STATICASSERT(a_ <= b_)
#define VOLUTA_STATICASSERT_LT(a_, b_)   VOLUTA_STATICASSERT(a_ < b_)
#define VOLUTA_STATICASSERT_GE(a_, b_)   VOLUTA_STATICASSERT(a_ >= b_)
#define VOLUTA_STATICASSERT_GT(a_, b_)   VOLUTA_STATICASSERT(a_ > b_)

/* run-time assertions*/
#define voluta_assert(cond) \
	voluta_assert_if_((cond), VOLUTA_STR(cond), __FILE__, __LINE__)

#define voluta_assert_eq(a, b) \
	voluta_assert_eq_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_ne(a, b) \
	voluta_assert_ne_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_lt(a, b) \
	voluta_assert_lt_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_le(a, b) \
	voluta_assert_le_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_gt(a, b) \
	voluta_assert_gt_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_ge(a, b) \
	voluta_assert_ge_((long)(a), (long)(b), __FILE__, __LINE__)

#define voluta_assert_not_null(ptr) \
	voluta_assert_not_null_(ptr, __FILE__, __LINE__)

#define voluta_assert_null(ptr) \
	voluta_assert_null_(ptr, __FILE__, __LINE__)

#define voluta_assert_ok(err) \
	voluta_assert_ok_((int)(err), __FILE__, __LINE__)

#define voluta_assert_err(err, exp) \
	voluta_assert_err_((int)(err), (int)(exp), __FILE__, __LINE__)

#define voluta_assert_eqs(s1, s2) \
	voluta_assert_eqs_(s1, s2, __FILE__, __LINE__)

#define voluta_assert_eqm(m1, m2, nn) \
	voluta_assert_eqm_(m1, m2, nn, __FILE__, __LINE__)

void voluta_assert_if_(bool cond, const char *str, const char *file, int line);
void voluta_assert_eq_(long a, long b, const char *file, int line);
void voluta_assert_ne_(long a, long b, const char *file, int line);
void voluta_assert_lt_(long a, long b, const char *file, int line);
void voluta_assert_le_(long a, long b, const char *file, int line);
void voluta_assert_gt_(long a, long b, const char *file, int line);
void voluta_assert_ge_(long a, long b, const char *file, int line);
void voluta_assert_ok_(int err, const char *file, int line);
void voluta_assert_err_(int err, int exp, const char *file, int line);
void voluta_assert_not_null_(const void *ptr, const char *file, int line);
void voluta_assert_null_(const void *ptr, const char *file, int line);
void voluta_assert_eqs_(const char *, const char *, const char *, int);
void voluta_assert_eqm_(const void *, const void *, size_t,
			const char *file, int line);

/* panic */
#define voluta_panic(fmt, ...) \
	voluta_panicf(__FILE__, __LINE__, fmt, __VA_ARGS__)

__attribute__((__noreturn__))
void voluta_panicf(const char *file, int line, const char *fmt, ...);

#endif /* VOLUTA_ERROR_H_ */
