// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_JXL_BASE_BITS_H_
#define LIB_JXL_BASE_BITS_H_

// Specialized instructions for processing register-sized bit arrays.

#include "lib/jxl/base/compiler_specific.h"
#include "lib/jxl/base/status.h"

#ifdef _MSC_VER
#include <intrin.h>
#endif

#include <stddef.h>
#include <stdint.h>

namespace jxl {

// Empty struct used as a size tag type.
template <size_t N>
struct SizeTag {};

template <typename T>
constexpr bool IsSigned() {
  return T(0) > T(-1);
}

static JXL_INLINE JXL_MAYBE_UNUSED size_t PopCount(SizeTag<4> /* tag */,
                                                   const uint32_t x) {
#if JXL_COMPILER_CLANG || JXL_COMPILER_GCC
  return static_cast<size_t>(__builtin_popcount(x));
#elif JXL_COMPILER_MSVC
  return _mm_popcnt_u32(x);
#else
#error "not supported"
#endif
}
static JXL_INLINE JXL_MAYBE_UNUSED size_t PopCount(SizeTag<8> /* tag */,
                                                   const uint64_t x) {
#if JXL_COMPILER_CLANG || JXL_COMPILER_GCC
  return static_cast<size_t>(__builtin_popcountll(x));
#elif JXL_COMPILER_MSVC
  return _mm_popcnt_u64(x);
#else
#error "not supported"
#endif
}
template <typename T>
static JXL_INLINE JXL_MAYBE_UNUSED size_t PopCount(T x) {
  static_assert(!IsSigned<T>(), "PopCount: use unsigned");
  return PopCount(SizeTag<sizeof(T)>(), x);
}

// Undefined results for x == 0.
static JXL_INLINE JXL_MAYBE_UNUSED size_t
Num0BitsAboveMS1Bit_Nonzero(SizeTag<4> /* tag */, const uint32_t x) {
  JXL_DASSERT(x != 0);
#ifdef _MSC_VER
  unsigned long index;
  _BitScanReverse(&index, x);
  return 31 - index;
#else
  return static_cast<size_t>(__builtin_clz(x));
#endif
}
static JXL_INLINE JXL_MAYBE_UNUSED size_t
Num0BitsAboveMS1Bit_Nonzero(SizeTag<8> /* tag */, const uint64_t x) {
  JXL_DASSERT(x != 0);
#ifdef _MSC_VER
  unsigned long index;
  _BitScanReverse64(&index, x);
  return 63 - index;
#else
  return static_cast<size_t>(__builtin_clzll(x));
#endif
}
template <typename T>
static JXL_INLINE JXL_MAYBE_UNUSED size_t
Num0BitsAboveMS1Bit_Nonzero(const T x) {
  static_assert(!IsSigned<T>(), "Num0BitsAboveMS1Bit_Nonzero: use unsigned");
  return Num0BitsAboveMS1Bit_Nonzero(SizeTag<sizeof(T)>(), x);
}

// Undefined results for x == 0.
static JXL_INLINE JXL_MAYBE_UNUSED size_t
Num0BitsBelowLS1Bit_Nonzero(SizeTag<4> /* tag */, const uint32_t x) {
  JXL_DASSERT(x != 0);
#ifdef _MSC_VER
  unsigned long index;
  _BitScanForward(&index, x);
  return index;
#else
  return static_cast<size_t>(__builtin_ctz(x));
#endif
}
static JXL_INLINE JXL_MAYBE_UNUSED size_t
Num0BitsBelowLS1Bit_Nonzero(SizeTag<8> /* tag */, const uint64_t x) {
  JXL_DASSERT(x != 0);
#ifdef _MSC_VER
  unsigned long index;
  _BitScanForward64(&index, x);
  return index;
#else
  return static_cast<size_t>(__builtin_ctzll(x));
#endif
}
template <typename T>
static JXL_INLINE JXL_MAYBE_UNUSED size_t Num0BitsBelowLS1Bit_Nonzero(T x) {
  static_assert(!IsSigned<T>(), "Num0BitsBelowLS1Bit_Nonzero: use unsigned");
  return Num0BitsBelowLS1Bit_Nonzero(SizeTag<sizeof(T)>(), x);
}

// Returns bit width for x == 0.
template <typename T>
static JXL_INLINE JXL_MAYBE_UNUSED size_t Num0BitsAboveMS1Bit(const T x) {
  return (x == 0) ? sizeof(T) * 8 : Num0BitsAboveMS1Bit_Nonzero(x);
}

// Returns bit width for x == 0.
template <typename T>
static JXL_INLINE JXL_MAYBE_UNUSED size_t Num0BitsBelowLS1Bit(const T x) {
  return (x == 0) ? sizeof(T) * 8 : Num0BitsBelowLS1Bit_Nonzero(x);
}

// Returns base-2 logarithm, rounded down.
template <typename T>
static JXL_INLINE JXL_MAYBE_UNUSED size_t FloorLog2Nonzero(const T x) {
  return (sizeof(T) * 8 - 1) ^ Num0BitsAboveMS1Bit_Nonzero(x);
}

// Returns base-2 logarithm, rounded up.
template <typename T>
static JXL_INLINE JXL_MAYBE_UNUSED size_t CeilLog2Nonzero(const T x) {
  const size_t floor_log2 = FloorLog2Nonzero(x);
  if ((x & (x - 1)) == 0) return floor_log2;  // power of two
  return floor_log2 + 1;
}

}  // namespace jxl

#endif  // LIB_JXL_BASE_BITS_H_
