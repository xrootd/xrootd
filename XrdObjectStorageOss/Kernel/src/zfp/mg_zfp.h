/* Adapted from the zfp compression library */

#pragma once

#include "mg_common.h"

namespace mg {

/* Forward/inverse zfp lifting in 1D */
template <typename t> void FLift(t* P, int S);
template <typename t> void ILift(t* P, int S);
template <typename t> void FLiftRev(t* P, int S);
template <typename t> void ILiftRev(t* P, int S);

/* zfp transform in 3D. The input is assumed to be in row-major order. */
template <typename t> void ForwardZfp(t* P);
template <typename t> void InverseZfp(t* P);
template <typename t, int S = 4> void ForwardZfp2D(t* P);
template <typename t, int S = 4> void InverseZfp2D(t* P);
template <typename t, int S = 4> void ForwardZfpRev2D(t* P);
template <typename t, int S = 4> void InverseZfpRev2D(t* P);

/* Reorder coefficients within a zfp block, and convert them from/to negabinary */
template <typename t, typename u> void ForwardShuffle(t* IBlock, u* UBlock);
template <typename t, typename u> void InverseShuffle(u* UBlock, t* IBlock);
template <typename t, typename u, int S = 4> void ForwardShuffle2D(t* IBlock, u* UBlock);
template <typename t, typename u, int S = 4> void InverseShuffle2D(u* IBlock, t* UBlock);

/* Pad partial block of width N < 4 and stride S */
template <typename t> void PadBlock(t* P, int N, int S);

struct bitstream;
/* Encode/decode a single bit plane B of a zfp block */
// TODO: turn this into a template? TODO: pointer aliasing?
template <typename t> void Encode(int D, t* Block, int B, i64 S, i8& N, bitstream* Bs);
template <typename t> void Decode(int D, t* Block, int B, i64 S, i8& N, bitstream* Bs);

} // namespace mg

#include "mg_zfp.inl"
