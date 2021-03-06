/*
 * Copyright 2010 Andrea Mazzoleni. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ANDREA MAZZOLENI AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ANDREA MAZZOLENI OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/** \file
 * Generic types.
 */

#ifndef __TOMMYTYPES_H
#define __TOMMYTYPES_H

/******************************************************************************/
/* types */

#if defined(_MSC_VER)
#include <stddef.h>
typedef unsigned tommy_uint32_t; /**< Generic uint32_t type. */
typedef unsigned _int64 tommy_uint64_t; /**< Generic uint64_t type. */
typedef size_t tommy_size_t; /**< Generic size_t type. */
typedef size_t tommy_uintptr_t; /**< Generic uintptr_t type. */
#else
#include <stdint.h>
typedef uint32_t tommy_uint32_t; /**< Generic uint32_t type. */
typedef uint64_t tommy_uint64_t; /**< Generic uint64_t type. */
typedef uintptr_t tommy_size_t; /**< Generic size_t type. */
typedef uintptr_t tommy_uintptr_t; /**< Generic uintptr_t type. */
#endif
typedef int tommy_bool_t; /**< Generic boolean type. */

/** \internal
 * Type cast required for the C++ compilation.
 * When compiling in C++ we cannot convert a void* pointer to another pointer.
 * In such case we need an explicit cast.
 */
#ifdef __cplusplus
#define tommy_cast(type, value) static_cast<type>(value)
#else
#define tommy_cast(type, value) (value)
#endif

/******************************************************************************/
/* heap */

/* by default uses malloc/realloc/free */

/**
 * Generic malloc(), realloc() and free() functions.
 * Redefine them to what you need. By default they map to the C malloc(), realloc() and free().
 */
#if !defined(tommy_malloc) && !defined(tommy_realloc) && !defined(tommy_free)
#include <stdlib.h>
#define tommy_malloc malloc
#define tommy_realloc realloc
#define tommy_free free
#endif

/******************************************************************************/
/* modificators */

/** \internal
 * Definition of the inline keyword if available.
 */
#if !defined(tommy_inline)
/*
#if __STDC_VERSION__ >= 199901L || defined(__cplusplus)
#define tommy_inline inline
#elif defined(_MSC_VER) || defined(__GNUC__)
#define tommy_inline static __inline
#else
#define tommy_inline static
#endif*/
#define tommy_inline static __attribute__((unused))
#endif

/** \internal
 * Definition of the always_inline keyword if available.
 */
#if !defined(tommy_always_inline)
#if defined(__GNUC__)
#define tommy_always_inline static __attribute__((always_inline)) __inline
#else
#define tommy_always_inline tommy_inline
#endif
#endif

/** \internal
 * Definition of the restrict keyword if available.
 */
#if !defined(tommy_restrict)
#if __STDC_VERSION__ >= 199901L
#define tommy_restrict restrict
#elif defined(_MSC_VER) || defined(__GNUC__)
#define tommy_restrict __restrict
#else
#define tommy_restrict
#endif
#endif

/** \internal
 * Hints the compiler that a condition is likely true.
 */
#if !defined(tommy_likely)
#if defined(__GNUC__)
#define tommy_likely(x) __builtin_expect(!!(x),1)
#else
#define tommy_likely(x) (x)
#endif
#endif

/** \internal
 * Hints the compiler that a condition is likely false.
 */
#if !defined(tommy_unlikely)
#if defined(__GNUC__)
#define tommy_unlikely(x) __builtin_expect(!!(x),0)
#else
#define tommy_unlikely(x) (x)
#endif
#endif

/******************************************************************************/
/* key */

/**
 * Key type used in indexed data structures to store the key or the hash value.
 */
typedef tommy_uint32_t tommy_key_t;

/**
 * Bits into the ::tommy_key_t type.
 */
#define TOMMY_KEY_BIT (sizeof(tommy_key_t) * 8)

/******************************************************************************/
/* node */

/**
 * Data structure node.
 * This node type is shared between all the data structures and used to store some
 * info directly into the objects you want to store.
 *
 * A typical declaration is:
 * \code
 * struct object {
 *     tommy_node node;
 *     // other fields
 * };
 * \endcode
 */
typedef struct tommy_node_struct {
	/**
	 * Next node.
	 * The tail node has it at 0, like a 0 terminated list.
	 */
	struct tommy_node_struct* next;

	/**
	 * Previous node.
	 * The head node points to the tail node, like a circular list.
	 */
	struct tommy_node_struct* prev;

	/**
	 * Pointer at the object containing the node.
	 * This field is initialized when inserting nodes into a data structure.
	 */
	void* data;

	/**
	 * Key used to store the node.
	 * With hashtables this field is used to store the hash value.
	 * With lists this field is not used.
	 */
	tommy_key_t key;
} tommy_node;

/******************************************************************************/
/* compare */

/**
 * Compare function for elements.
 */
typedef int tommy_compare_func(const void* void_a, const void* void_b);

/**
 * Foreach function.
 */
typedef void tommy_foreach_func(void* void_a);

/**
 * Foreach function with an argument.
 * The argument is always passed as first parameter.
 */
typedef void tommy_foreach_arg_func(void* void_arg, void* void_a);

/******************************************************************************/
/* bit hacks */

#if defined(_MSC_VER) && !defined(__cplusplus)
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
#endif

/** \internal
 * Integer log2 for constants.
 * You can use it only for exact power of 2 up to 256.
 */
#define TOMMY_ILOG2(value) ((value) == 256 ? 8 : (value) == 128 ? 7 :(value) == 64 ? 6 : (value) == 32 ? 5 : (value) == 16 ? 4 : (value) == 8 ? 3 : (value) == 4 ? 2 : (value) == 2 ? 1 : 0)

/**
 * Bit scan reverse or integer log2.
 * Return the bit index of the most significant 1 bit.
 *
 * If no bit is set, the result is undefined.
 * To force a return 0 in this case, you can use tommy_ilog2(value | 1).
 *
 * Other interesting ways for bitscan can be found at:
 * 
 * Bit Twiddling Hacks 
 * http://graphics.stanford.edu/~seander/bithacks.html 
 *
 * Chess Programming BitScan
 * http://chessprogramming.wikispaces.com/BitScan
 *
 * \param value Value to scan. 0 is not allowed.
 * \return The index of the most significan bit set.
 */
tommy_inline unsigned tommy_ilog2_u32(tommy_uint32_t value)
{
#if defined(_MSC_VER)
	unsigned long count;
	_BitScanReverse(&count, value);
	return count;
#elif defined(__GNUC__)
	/*
	 * GCC implements __builtin_clz(x) as "__builtin_clz(x) = bsr(x) ^ 31"
	 *
	 * Where "x ^ 31 = 31 - x", but gcc does not optimize "31 - __builtin_clz(x)" to bsr(x),
	 * but generates 31 - (bsr(x) xor 31).
	 *
	 * So we write "__builtin_clz(x) ^ 31" instead of "31 - __builtin_clz(x)".
	 */
	return __builtin_clz(value) ^ 31;
#else
	/* Find the log base 2 of an N-bit integer in O(lg(N)) operations with multiply and lookup */
	/* from http://graphics.stanford.edu/~seander/bithacks.html */
	static const int TOMMY_DE_BRUIJN_INDEX_ILOG2[32] = {
		0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
		8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
	};

	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;

	return TOMMY_DE_BRUIJN_INDEX_ILOG2[(tommy_uint32_t)(value * 0x07C4ACDDU) >> 27];
#endif
}

/**
 * Bit scan forward or trailing zero count.
 * Return the bit index of the least significant 1 bit.
 *
 * If no bit is set, the result is undefined.
 * \param value Value to scan. 0 is not allowed.
 * \return The index of the least significan bit set.
 */
tommy_inline unsigned tommy_ctz_u32(tommy_uint32_t value)
{
#if defined(_MSC_VER)
	unsigned long count;
	_BitScanForward(&count, value);
	return count;
#elif defined(__GNUC__)
	return __builtin_ctz(value);
#else
	/* Count the consecutive zero bits (trailing) on the right with multiply and lookup */
	/* from http://graphics.stanford.edu/~seander/bithacks.html */
	static const tommy_uint32_t TOMMY_DE_BRUIJN_INDEX_CTZ[32] = {
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};

	return TOMMY_DE_BRUIJN_INDEX_CTZ[(tommy_uint32_t)(((value & -value) * 0x077CB531U)) >> 27];
#endif
}

/**
 * Rounds up to the next power of 2.
 * For the value 0, the result is undefined.
 * \return The smallest power of 2 not less than the specified value.
 */
tommy_inline tommy_uint32_t tommy_roundup_pow2_u32(tommy_uint32_t value)
{
	/* Round up to the next highest power of 2 */
	/* from http://www-graphics.stanford.edu/~seander/bithacks.html */

	--value;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	++value;

	return value;
}
#endif

