#pragma once

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <cassert>

namespace mg {

using uint    = unsigned int;
using byte    = uint8_t;
using int8    = int8_t;
using i8      = int8;
using int16   = int16_t;
using i16     = int16;
using int32   = int32_t;
using i32     = int32;
using int64   = int64_t;
using i64     = int64;
using uint8   = uint8_t;
using u8      = uint8;
using uint16  = uint16_t;
using u16     = uint16;
using uint32  = uint32_t;
using u32     = uint32;
using uint64  = uint64_t;
using u64     = uint64;
using float32 = float;
using f32     = float32;
using float64 = double;
using f64     = float64;

template <typename t> t Min(const t& a, const t& b) { return b < a ? b : a; }
template <typename t> t Max(const t& a, const t& b) { return a < b ? b : a; }

struct buffer {
  byte* Data = nullptr;
  i64 Bytes = 0;
  buffer() = default;
  buffer(const byte* DataIn, i64 BytesIn) : Data(const_cast<byte*>(DataIn)), Bytes(BytesIn) {}
  template <typename t, int N> buffer(t (&Arr)[N])  : Data((byte*)const_cast<t*>(&Arr[0])), Bytes(sizeof(Arr)) {}
  byte& operator[](i64 Idx) const { assert(Idx < Bytes); return const_cast<byte&>(Data[Idx]); }
  explicit operator bool() const { return this->Data && this->Bytes; }
};
inline bool operator==(const buffer& Buf1, const buffer& Buf2) { return Buf1.Data == Buf2.Data && Buf1.Bytes == Buf2.Bytes; }

static constexpr int Pow4[] = { 1, 4, 16, 64, 256, 1024, 4096 };

template <typename t>
struct traits {
  // using signed_t =
  // using unsigned_t =
  // using integral_t =
  // static constexpr uint NBinaryMask =
  // static constexpr int ExpBits
  // static constexpr int ExpBias
};

template <>
struct traits<i8> {
  using signed_t   = i8;
  using unsigned_t = u8;
  static constexpr u8 NBinaryMask = 0xaa;
  static constexpr i8 Min = -(1 << 7);
  static constexpr i8 Max = (1 << 7) - 1;
};

template <>
struct traits<u8> {
  using signed_t   = i8;
  using unsigned_t = u8;
  static constexpr u8 NBinaryMask = 0xaa;
  static constexpr u8 Min = 0;
  static constexpr u8 Max = (1 << 8) - 1;
};

template <>
struct traits<i16> {
  using signed_t   = i16;
  using unsigned_t = u16;
  static constexpr u16 NBinaryMask = 0xaaaa;
  static constexpr i16 Min = -(1 << 15);
  static constexpr i16 Max = (1 << 15) - 1;
};

template <>
struct traits<u16> {
  using signed_t   = i16;
  using unsigned_t = u16;
  static constexpr u16 NBinaryMask = 0xaaaa;
  static constexpr u16 Min = 0;
  static constexpr u16 Max = (1 << 16) - 1;
};

template <>
struct traits<i32> {
  using signed_t   = i32;
  using unsigned_t = u32;
  using floating_t = f32;
  static constexpr u32 NBinaryMask = 0xaaaaaaaa;
  static constexpr i32 Min = i32(0x80000000);
  static constexpr i32 Max = 0x7fffffff;
};

template <>
struct traits<u32> {
  using signed_t   = i32;
  using unsigned_t = u32;
  static constexpr u32 NBinaryMask = 0xaaaaaaaa;
  static constexpr u32 Min = 0;
  static constexpr u32 Max = 0xffffffff;
};

template <>
struct traits<i64> {
  using signed_t   = i64;
  using unsigned_t = u64;
  using floating_t = f64;
  static constexpr u64 NBinaryMask = 0xaaaaaaaaaaaaaaaaULL;
  static constexpr i64 Min = 0x8000000000000000ll;
  static constexpr i64 Max = 0x7fffffffffffffffull;
};

template <>
struct traits<u64> {
  using signed_t   = i64;
  using unsigned_t = u64;
  static constexpr u64 NBinaryMask = 0xaaaaaaaaaaaaaaaaULL;
  static constexpr u64 Min = 0;
  static constexpr u64 Max = 0xffffffffffffffffull;
};

template <>
struct traits<f32> {
  using integral_t = i32;
  static constexpr int ExpBits = 8;
  static constexpr int ExpBias = (1 << (ExpBits - 1)) - 1;
  static constexpr f32 Min = -FLT_MAX;
  static constexpr f32 Max = FLT_MAX;
};

template <>
struct traits<f64> {
  using integral_t = i64;
  static constexpr int ExpBits = 11;
  static constexpr int ExpBias = (1 << (ExpBits - 1)) - 1;
  static constexpr f64 Min = -DBL_MAX;
  static constexpr f64 Max = DBL_MAX;
};

template <typename t> int
Exponent(t Val) {
  if (Val > 0) {
    int E;
    frexp(Val, &E);
    /* clamp exponent in case Val is denormal */
    return Max(E, 1 - traits<t>::ExpBias);
  }
  return -traits<t>::ExpBias;
}

}