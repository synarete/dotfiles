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
#ifndef FNX_INFRA_MACROS_H_
#define FNX_INFRA_MACROS_H_

#ifdef NDEBUG
#define FNX_DEBUG   0
#else
#if (defined(DEBUG) && (DEBUG > 0))
#define FNX_DEBUG   DEBUG
#elif (defined(_DEBUG) && (_DEBUG > 0))
#define FNX_DEBUG   _DEBUG
#else
#define FNX_DEBUG   0
#endif
#endif

/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Converts the parameter x_ to a string after macro replacement on it has
 * been performed.
 */
#define FNX_MAKESTR(x_)                  FNX_MAKESTR2_(x_)
#define FNX_MAKESTR2_(x_)                #x_


/*
 * The following piece of macro magic joins the two arguments together, even
 * when one of the arguments is itself a macro. The key is that macro
 * expansion of macro arguments does not occur in FNX_JOIN2B_ but does in
 * FNX_JOIN2_.
 */
#define FNX_JOIN(x_,y_)                  FNX_JOIN2_(x_,y_)
#define FNX_JOIN2_(x_,y_)                FNX_JOIN2B_(x_,y_)
#define FNX_JOIN2B_(x_,y_)               x_##y_


/*
 * Evaluates to the number of elements in static array.
 *
 * NB: The size of the array must be known at compile time; *do not* try to
 * use this macro for dynamic-allocated arrays!
 */
#define FNX_NELEMS(X)                    ( sizeof(X) / sizeof(X[0]) )
#define FNX_ARRAYSIZE(X)                 FNX_NELEMS(X)

#define fnx_nelems(x)                    FNX_NELEMS(x)

/*
 * Sizeof field within struct
 */
#define FNX_FIELD_SIZEOF(T, F)           ( sizeof(( (T*)NULL)->F) )
#define FNX_ARRFIELD_NELEMS(T, F)        FNX_ARRAYSIZE( ((T*)(NULL))->F )


/* Common MIN/MAX */
#define FNX_MIN(x,y)                     ( ( (x) < (y) ) ? (x) : (y) )
#define FNX_MAX(x,y)                     ( ( (x) > (y) ) ? (x) : (y) )

/* Bit-masking helpers */
#define FNX_BITFLAG(n)                   (1 << n)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Compile-time assertion.
 *
 * The macros FNX_STATICASSERT(x) and fnx_staticassert(x), generates a compile
 * time error message if the integral-constant-expression 'expr' is not true.
 * In other words, it is the compile time equivalent of the assert macro.
 * Note that if the condition is true, then the macro will generate neither
 * code nor data, and it may be used within function scope.
 */
#define FNX_STATICASSERT_1(expr)             \
	do { switch (0) { case 0: case expr: default: break; } } while (0)

#define FNX_STATICASSERT_LABEL_(tag)         \
	FNX_JOIN(fnx_static_assert_, FNX_JOIN(tag, __LINE__))

#define FNX_STATICASSERT_(tag, expr)         \
	typedef char FNX_STATICASSERT_LABEL_(tag) [ 1 - 2*(!(expr)) ]

#define FNX_STATICASSERT(expr)               \
	FNX_STATICASSERT_(_, expr)

#define FNX_STATICASSERT_EQ(a, b)            \
	FNX_STATICASSERT_(not_equal_, ((a) == (b)))

#define FNX_STATICASSERT_EQSIZEOF(a, b)      \
	FNX_STATICASSERT_(not_equal_sizeof_, (sizeof(a) == sizeof(b)))

#define FNX_STATICASSERT_EQPSIZE(a, b)       \
	FNX_STATICASSERT_(not_equal_pointers_, (sizeof(*a) == sizeof(*b)))


#define fnx_staticassert(expr)      FNX_STATICASSERT(expr)
#define fnx_staticassert_eq(a, b)   FNX_STATICASSERT_EQ(a, b)


/* Compiler-specific support for static-assert */
#if defined(FNX_GCC)
#if (FNX_GCC_VERSION >= 40700)
#undef FNX_STATICASSERT_
#define FNX_STATICASSERT_(tag, expr)         \
	_Static_assert(expr, #expr)
#endif
#elif defined(FNX_CLANG)
#if __has_feature(c_static_assert)
#undef FNX_STATICASSERT_
#define FNX_STATICASSERT_(tag, expr)         \
	_Static_assert(expr, #expr)
#endif
#endif


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Verify two pointers are of equal size
 */
#define FNX_ARGI(i)   FNX_JOIN(FNX_JOIN(_fnx_arg_, __LINE__), i)
#define FNX_EQPTRSIZE(x, y) \
	do { __extension__ __typeof__ ((x)) FNX_ARGI(1) = (x); \
		__extension__ __typeof__ ((y)) FNX_ARGI(2) = (y); \
		FNX_STATICASSERT_EQPSIZE(FNX_ARGI(1), FNX_ARGI(2)); \
	} while(0)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Cast a member of a structure out to the containing structure:
 * NB: Linux kernel use GCC extension to check pointer-types equality.
 */
#define FNX_CONTAINER_OF(ptr, type, member) \
	(type*)((void*)((char*)ptr - offsetof(type, member)))

#define fnx_container_of(ptr, type, member) \
	FNX_CONTAINER_OF(ptr, type, member)


/*. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .*/
/*
 * Suppress compiler warns about unused parameter
 */
#define FNX_UNUSED(x)       ((void)(x))
#define fnx_unused(x)       FNX_UNUSED(x)
#define fnx_unused2(x, y)   do { fnx_unused(x); fnx_unused(y); } while (0)
#define fnx_unused3(x, y, z)   \
	do { fnx_unused(x); fnx_unused(y); fnx_unused(z); } while (0)


/*
 * Array-elements iteration: apply function 'fn' on each member of the array.
 */
#define fnx_foreach_arrelem(arr, fn) \
	do { for (size_t _i = 0; _i < FNX_NELEMS(arr); ++_i) \
			fn(&(arr)[_i]); } while (0)

#define fnx_foreach_arrelem2(arr, fn, arg) \
	do { for (size_t _i = 0; _i < FNX_NELEMS(arr); ++_i) \
			fn(&(arr)[_i], arg); } while (0)


#endif /* FNX_INFRA_MACROS_H_ */

