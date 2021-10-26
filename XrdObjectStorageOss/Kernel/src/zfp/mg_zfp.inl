#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include "mg_bitstream.h"

namespace mg {

/*
zfp lifting transform for 4 samples in 1D.
 non-orthogonal transform
        ( 4  4  4  4) (X)
 1/16 * ( 5  1 -1 -5) (Y)
        (-4  4  4 -4) (Z)
        (-2  6 -6  2) (W) */
// TODO: look into range expansion for this transform
template <typename t> void 
FLift(t* P, int S) {
  assert(P);
  assert(S > 0);
  t X = P[0 * S], Y = P[1 * S], Z = P[2 * S], W = P[3 * S];
  X += W; X >>= 1; W -= X;
  Z += Y; Z >>= 1; Y -= Z;
  X += Z; X >>= 1; Z -= X;
  W += Y; W >>= 1; Y -= W;
  W += Y >> 1; Y -= W >> 1;
  P[0 * S] = X; P[1 * S] = Y; P[2 * S] = Z; P[3 * S] = W;
}

/*
zfp inverse lifting transform for 4 samples in 1D.
NOTE: this lifting is not perfectly reversible
 non-orthogonal transform
       ( 4  6 -4 -1) (x)
 1/4 * ( 4  2  4  5) (y)
       ( 4 -2  4 -5) (z)
       ( 4 -6 -4  1) (w) */
template <typename t> void
ILift(t* P, int S) {
  assert(P);
  assert(S > 0);
  t X = P[0 * S], Y = P[1 * S], Z = P[2 * S], W = P[3 * S];
  Y += W >> 1; W -= Y >> 1;
  Y += W; W <<= 1; W -= Y;
  Z += X; X <<= 1; X -= Z;
  Y += Z; Z <<= 1; Z -= Y;
  W += X; X <<= 1; X -= W;
  P[0 * S] = X; P[1 * S] = Y; P[2 * S] = Z; P[3 * S] = W;
}

// revertible transform
/*
** high-order Lorenzo transform
** ( 1  0  0  0) (x)
** (-1  1  0  0) (y)
** ( 1 -2  1  0) (z)
** (-1  3 -3  1) (w)
*/
template <typename t> void
FLiftRev(t* P, int S) {
  t X = P[0 * S], Y = P[1 * S], Z = P[2 * S], W = P[3 * S];
  W -= Z; Z -= Y; Y -= X;
  W -= Z; Z -= Y;
  W -= Z;
  P[0 * S] = X; P[1 * S] = Y; P[2 * S] = Z; P[3 * S] = W;
}

/*
** high-order Lorenzo transform (P4 Pascal matrix)
** ( 1  0  0  0) (x)
** ( 1  1  0  0) (y)
** ( 1  2  1  0) (z)
** ( 1  3  3  1) (w)
*/
template <typename t> void
ILiftRev(t* P, int S) {
  t X = P[0 * S], Y = P[1 * S], Z = P[2 * S], W = P[3 * S];
  W += Z;
  Z += Y; W += Z;
  Y += X; Z += Y; W += Z;
  P[0 * S] = X; P[1 * S] = Y; P[2 * S] = Z; P[3 * S] = W;
}

template <typename t> void
ForwardZfp(t* P) {
  assert(P);
  /* transform along X */
  for (int Z = 0; Z < 4; ++Z)
    for (int Y = 0; Y < 4; ++Y)
      FLift(P + 4 * Y + 16 * Z, 1);
  /* transform along Y */
  for (int X = 0; X < 4; ++X)
    for (int Z = 0; Z < 4; ++Z)
      FLift(P + 16 * Z + 1 * X, 4);
  /* transform along Z */
  for (int Y = 0; Y < 4; ++Y)
    for (int X = 0; X < 4; ++X)
      FLift(P + 1 * X + 4 * Y, 16);
}

template <typename t> void
ForwardZfpRev(t* P) {
  assert(P);
  /* transform along X */
  for (int Z = 0; Z < 4; ++Z)
    for (int Y = 0; Y < 4; ++Y)
      FLiftRev(P + 4 * Y + 16 * Z, 1);
  /* transform along Y */
  for (int X = 0; X < 4; ++X)
    for (int Z = 0; Z < 4; ++Z)
      FLiftRev(P + 16 * Z + 1 * X, 4);
  /* transform along Z */
  for (int Y = 0; Y < 4; ++Y)
    for (int X = 0; X < 4; ++X)
      FLiftRev(P + 1 * X + 4 * Y, 16);
}

template <typename t, int S> void
ForwardZfp2D(t* P) {
  assert(P);
  /* transform along X */
  for (int Y = 0; Y < S; ++Y)
    FLift(P + S * Y, 1);
  /* transform along Y */
  for (int X = 0; X < S; ++X)
    FLift(P + 1 * X, S);
}

template <typename t, int S> void
ForwardZfpRev2D(t* P) {
  assert(P);
  /* transform along X */
  for (int Y = 0; Y < S; ++Y)
    FLiftRev(P + S * Y, 1);
  /* transform along Y */
  for (int X = 0; X < S; ++X)
    FLiftRev(P + 1 * X, S);
}

template <typename t> void
InverseZfp(t* P) {
  assert(P);
  /* transform along Z */
  for (int Y = 0; Y < 4; ++Y)
    for (int X = 0; X < 4; ++X)
      ILift(P + 1 * X + 4 * Y, 16);
  /* transform along y */
  for (int X = 0; X < 4; ++X)
    for (int Z = 0; Z < 4; ++Z)
      ILift(P + 16 * Z + 1 * X, 4);
  /* transform along X */
  for (int Z = 0; Z < 4; ++Z)
    for (int Y = 0; Y < 4; ++Y)
      ILift(P + 4 * Y + 16 * Z, 1);
}

template <typename t> void
InverseZfpRev(t* P) {
  assert(P);
  /* transform along Z */
  for (int Y = 0; Y < 4; ++Y)
    for (int X = 0; X < 4; ++X)
      ILiftRev(P + 1 * X + 4 * Y, 16);
  /* transform along y */
  for (int X = 0; X < 4; ++X)
    for (int Z = 0; Z < 4; ++Z)
      ILiftRev(P + 16 * Z + 1 * X, 4);
  /* transform along X */
  for (int Z = 0; Z < 4; ++Z)
    for (int Y = 0; Y < 4; ++Y)
      ILiftRev(P + 4 * Y + 16 * Z, 1);
}

template <typename t, int S> void
InverseZfp2D(t* P) {
  assert(P);
  /* transform along y */
  for (int X = 0; X < S; ++X)
    ILift(P + 1 * X, S);
  /* transform along X */
  for (int Y = 0; Y < S; ++Y)
    ILift(P + S * Y, 1);
}

template <typename t, int S> void
InverseZfpRev2D(t* P) {
  assert(P);
  /* transform along y */
  for (int X = 0; X < S; ++X)
    ILiftRev(P + 1 * X, S);
  /* transform along X */
  for (int Y = 0; Y < S; ++Y)
    ILiftRev(P + S * Y, 1);
}

/*
Use the following array to reorder transformed coefficients in a zfp block.
The ordering is first by i + j + k, then by i^2 + j^2 + k^2. */
#define mg_Index(i, j, k) ((i) + 4 * (j) + 16 * (k))
constexpr i8 
Perm3[64] = {
  mg_Index(0, 0, 0), /*  0 : 0 */

  mg_Index(1, 0, 0), /*  1 : 1 */
  mg_Index(0, 1, 0), /*  2 : 1 */
  mg_Index(0, 0, 1), /*  3 : 1 */

  mg_Index(0, 1, 1), /*  4 : 2 */
  mg_Index(1, 0, 1), /*  5 : 2 */
  mg_Index(1, 1, 0), /*  6 : 2 */

  mg_Index(2, 0, 0), /*  7 : 2 */
  mg_Index(0, 2, 0), /*  8 : 2 */
  mg_Index(0, 0, 2), /*  9 : 2 */

  mg_Index(1, 1, 1), /* 10 : 3 */
  mg_Index(2, 1, 0), /* 11 : 3 */
  mg_Index(2, 0, 1), /* 12 : 3 */
  mg_Index(0, 2, 1), /* 13 : 3 */
  mg_Index(1, 2, 0), /* 14 : 3 */
  mg_Index(1, 0, 2), /* 15 : 3 */
  mg_Index(0, 1, 2), /* 16 : 3 */

  mg_Index(3, 0, 0), /* 17 : 3 */
  mg_Index(0, 3, 0), /* 18 : 3 */
  mg_Index(0, 0, 3), /* 19 : 3 */

  mg_Index(2, 1, 1), /* 20 : 4 */
  mg_Index(1, 2, 1), /* 21 : 4 */
  mg_Index(1, 1, 2), /* 22 : 4 */

  mg_Index(0, 2, 2), /* 23 : 4 */
  mg_Index(2, 0, 2), /* 24 : 4 */
  mg_Index(2, 2, 0), /* 25 : 4 */

  mg_Index(3, 1, 0), /* 26 : 4 */
  mg_Index(3, 0, 1), /* 27 : 4 */
  mg_Index(0, 3, 1), /* 28 : 4 */
  mg_Index(1, 3, 0), /* 29 : 4 */
  mg_Index(1, 0, 3), /* 30 : 4 */
  mg_Index(0, 1, 3), /* 31 : 4 */

  mg_Index(1, 2, 2), /* 32 : 5 */
  mg_Index(2, 1, 2), /* 33 : 5 */
  mg_Index(2, 2, 1), /* 34 : 5 */

  mg_Index(3, 1, 1), /* 35 : 5 */
  mg_Index(1, 3, 1), /* 36 : 5 */
  mg_Index(1, 1, 3), /* 37 : 5 */

  mg_Index(3, 2, 0), /* 38 : 5 */
  mg_Index(3, 0, 2), /* 39 : 5 */
  mg_Index(0, 3, 2), /* 40 : 5 */
  mg_Index(2, 3, 0), /* 41 : 5 */
  mg_Index(2, 0, 3), /* 42 : 5 */
  mg_Index(0, 2, 3), /* 43 : 5 */

  mg_Index(2, 2, 2), /* 44 : 6 */

  mg_Index(3, 2, 1), /* 45 : 6 */
  mg_Index(3, 1, 2), /* 46 : 6 */
  mg_Index(1, 3, 2), /* 47 : 6 */
  mg_Index(2, 3, 1), /* 48 : 6 */
  mg_Index(2, 1, 3), /* 49 : 6 */
  mg_Index(1, 2, 3), /* 50 : 6 */

  mg_Index(0, 3, 3), /* 51 : 6 */
  mg_Index(3, 0, 3), /* 52 : 6 */
  mg_Index(3, 3, 0), /* 53 : 6 */

  mg_Index(3, 2, 2), /* 54 : 7 */
  mg_Index(2, 3, 2), /* 55 : 7 */
  mg_Index(2, 2, 3), /* 56 : 7 */

  mg_Index(1, 3, 3), /* 57 : 7 */
  mg_Index(3, 1, 3), /* 58 : 7 */
  mg_Index(3, 3, 1), /* 59 : 7 */

  mg_Index(2, 3, 3), /* 60 : 8 */
  mg_Index(3, 2, 3), /* 61 : 8 */
  mg_Index(3, 3, 2), /* 62 : 8 */

  mg_Index(3, 3, 3), /* 63 : 9 */
};
#undef mg_Index

#define mg_Index(i, j) ((i) + 4 * (j))
constexpr i8
Perm2[16] = {
  mg_Index(0, 0),
  mg_Index(1, 0),
  mg_Index(0, 1),
  mg_Index(1, 1),
  mg_Index(2, 0),
  mg_Index(0, 2),
  mg_Index(2, 1),
  mg_Index(1, 2),
  mg_Index(3, 0),
  mg_Index(0, 3),
  mg_Index(2, 2),
  mg_Index(3, 1),
  mg_Index(1, 3),
  mg_Index(3, 2),
  mg_Index(2, 3),
  mg_Index(3, 3),
};
#undef mg_Index

template <typename t, typename u> void
ForwardShuffle(t* IBlock, u* UBlock) {
  auto Mask = traits<u>::NBinaryMask;
  for (int I = 0; I < 64; ++I)
    UBlock[I] = (u)((IBlock[Perm3[I]] + Mask) ^ Mask);
}

template <typename t, typename u, int S> void
ForwardShuffle2D(t* IBlock, u* UBlock) {
  auto Mask = traits<u>::NBinaryMask;
  for (int I = 0; I < S * S; ++I)
    UBlock[I] = (u)((IBlock[Perm2[I]] + Mask) ^ Mask);
}

template <typename t, typename u> void
InverseShuffle(u* UBlock, t* IBlock) {
  auto Mask = traits<u>::NBinaryMask;
  for (int I = 0; I < 64; ++I)
    IBlock[Perm3[I]] = (t)((UBlock[I] ^ Mask) - Mask);
}

template <typename t, typename u, int S> void
InverseShuffle2D(u* UBlock, t* IBlock) {
  auto Mask = traits<u>::NBinaryMask;
  for (int I = 0; I < S * S; ++I)
    IBlock[Perm2[I]] = (t)((UBlock[I] ^ Mask) - Mask);
}

template <typename t> void
PadBlock1D(t* P, int N, int S) {
  assert(P);
  assert(0 <= N && N <= 4);
  assert(S > 0);
  switch (N) {
  case 0:
    P[0 * S] = 0; /* fall through */
  case 1:
    P[1 * S] = P[0 * S]; /* fall through */
  case 2:
    P[2 * S] = P[1 * S]; /* fall through */
  case 3:
    P[3 * S] = P[0 * S]; /* fall through */
  default:
    break;
  }
}

template <typename t> void
PadBlock(t* P, int Nx, int Ny, int Nz) {
  for (int Z = 0; Z < 4; ++Z) 
    for (int Y = 0; Y < 4; ++Y) 
      PadBlock1D(P + Z * 16 + Y * 4, Nx, 1);

  for (int Z = 0; Z < 4; ++Z)
    for (int X = 0; X < 4; ++X)
      PadBlock1D(P + Z * 16 + X * 1, Ny, 4);

  for (int Y = 0; Y < 4; ++Y)
    for (int X = 0; X < 4; ++X)
      PadBlock1D(P + Y * 4 + X * 1, Nz, 16);
}

template <typename t> void
PadBlock2D(t* P, int Nx, int Ny) {
  for (int Y = 0; Y < 4; ++Y) 
    PadBlock1D(P + Y * 4, Nx, 1);

  for (int X = 0; X < 4; ++X)
    PadBlock1D(P + X * 1, Ny, 4);
}

// D is the dimension, K is the size of the block
template <typename t> void
Encode(int D, t* Block, int B, i64 S, i8& N, bitstream* Bs) {
  int NVals = Pow4[D];
  assert(NVals <= 64); // e.g. 4x4x4, 4x4, 8x8
  u64 X = 0;
  for (int I = 0; I < NVals; ++I)
    X += u64((Block[I] >> B) & 1u) << I;
  i8 P = (i8)Min((i64)N, S - BitSize(*Bs));
  if (P > 0) {
    WriteLong(Bs, X, P);
    X >>= P; // P == 64 is fine since in that case we don't need X any more
  }
  // TODO: we may be able to speed this up by getting rid of the shift of X
  // or the call bit BitSize()
  for (; BitSize(*Bs) < S && N < NVals;) {
    if (Write(Bs, !!X)) { // group is significant
      for (; BitSize(*Bs) < S && N + 1 < NVals;) {
        if (Write(Bs, X & 1u)) { // found a significant coeff, break and retest
          break;
        }
        else { // have not found a significant coeff, continue until we find one
          X >>= 1;
          ++N;
        }
      }
      if (BitSize(*Bs) >= S)
        break;
      X >>= 1;
      ++N;
    }
    else {
      break;
    }
  }
}

template <typename t> void
Decode(int D, t* Block, int B, i64 S, i8& N, bitstream* Bs) {
  //static_assert(is_unsigned<t>::Value);
  int NVals = Pow4[D];
  assert(NVals <= 64); // e.g. 4x4x4, 4x4, 8x8
  i8 P = (i8)Min((i64)N, S - BitSize(*Bs));
  u64 X = P > 0 ? ReadLong(Bs, P) : 0;
  for (; BitSize(*Bs) < S && N < NVals;) {
    if (Read(Bs)) {
      for (; BitSize(*Bs) < S && N + 1 < NVals;) {
        if (Read(Bs)) {
          break;
        }
        else {
          ++N;
        }
      }
      if (BitSize(*Bs) >= S)
        break;
      X += 1ull << (N++);
    }
    else {
      break;
    }
  }
  /* deposit bit plane from x */
  for (int I = 0; X; ++I, X >>= 1)
    Block[I] += (t)(X & 1u) << B;
}

} // namespace mg

