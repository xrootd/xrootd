/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#ifndef VISUS_ZFP_ENCODER_H
#define VISUS_ZFP_ENCODER_H

#include <Visus/Kernel.h>
#include <Visus/Encoder.h>

//self contained lz4
#include <zfp/mg_zfp.h>
#include <zfp/mg_bitstream.h>
#include <cstdint>

namespace Visus {

  //////////////////////////////////////////////////////////////
  class VISUS_KERNEL_API ZfpEncoder : public Encoder
  {
    int num_bit_planes=0;
  public:

    VISUS_CLASS(ZfpEncoder)

      //constructor
    ZfpEncoder(String specs) {
      auto options = StringUtils::split(specs, "-");
      for (auto it : options)
      {
        Int64 temp;
        bool success = StringUtils::tryParse(it, temp);
        if (success) {
          num_bit_planes = (int)temp;
        }
      }

      VisusReleaseAssert(num_bit_planes);
    }

    //destructor
    virtual ~ZfpEncoder() {
    }

    //isLossy
    virtual bool isLossy() const override {
      return true;
    }

    //encode
    virtual SharedPtr<HeapMemory> encode(PointNi dims, DType dtype, SharedPtr<HeapMemory> decoded) override
    {
      if (!decoded)
        return SharedPtr<HeapMemory>();
      auto encoded_bound = ((int)decoded->c_size()) * 3 / 2; // TODO: may not be conservative enough
      auto encoded = std::make_shared<HeapMemory>();
      if (!encoded->resize(encoded_bound, __FILE__, __LINE__))
        return SharedPtr<HeapMemory>();
      mg::bitstream bs; InitWrite(&bs, mg::buffer(encoded->c_ptr(), encoded->c_size()));

      uint8_t* src = decoded->c_ptr();
      int d = dims.getPointDim(); 
      int nblocksz = d > 2  ? int((dims[2] + 3) / 4) : 1;
      int nblocksy = int((dims[1] + 3) / 4), nblocksx = int((dims[0] + 3) / 4);
      int nc = dtype.ncomponents();
      bool special = (nc == 3 || nc == 4) && (dtype.isVectorOf(DTypes::INT8) || dtype.isVectorOf(DTypes::UINT8));
      for (int pz = 0; pz < nblocksz; ++pz) {
      for (int py = 0; py < nblocksy; ++py) {
      for (int px = 0; px < nblocksx; ++px) { /* for each block */
        int b = pz * nblocksy * nblocksx + py * nblocksx + px;
        int s = b * (mg::Pow4[d]);
        int dx = px * 4, dy = py * 4, dz = pz * 4;
        int mx = mg::Min(4, int(dims[0]) - dx), my = mg::Min(4, int(dims[1]) - dy), mz = 1;
        if (d == 3)
          mz = mg::Min(4, int(dims[2]) - dz);
        double    f64buf[4 * 4 * 4];
        int64_t   i64buf[4 * 4 * 4];
        uint64_t  u64buf[4 * 4 * 4];
        double*   f64block = f64buf;
        float*    f32block = (float*)f64buf;
        int64_t*  i64block = i64buf;
        uint64_t* u64block = u64buf;
        int32_t*  i32block = (int32_t*)i64buf;
        uint32_t* u32block = (uint32_t*)u64buf;
        for (int c = 0; c < nc; ++c) { // for each component
          /* copy samples to local buffers */
          if (d == 3) {
            if (dtype.isVectorOf(DTypes::FLOAT64)) {
              for (int bz = 0; bz < mz; ++bz) { for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                int zz = dz + bz, yy = dy + by, xx = dx + bx;
                f64block[bz * 16 + by * 4 + bx] = ((const double*)src)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c]; }}
              }
              mg::PadBlock(f64block, mx, my, mz);
            } else if (dtype.isVectorOf(DTypes::FLOAT32)) {
              for (int bz = 0; bz < 4; ++bz) { for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int zz = dz + bz, yy = dy + by, xx = dx + bx;
                f32block[bz * 16 + by * 4 + bx] = ((const float*)src)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c]; }}
              }
              mg::PadBlock(f32block, mx, my, mz);
            } else if (dtype.isVectorOf(DTypes::INT64) || dtype.isVectorOf(DTypes::UINT64)) {
              for (int bz = 0; bz < 4; ++bz) { for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int zz = dz + bz, yy = dy + by, xx = dx + bx;
                i64block[bz * 16 + by * 4 + bx] = ((const int64_t*)src)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c]; }}
              }
              mg::PadBlock(i64block, mx, my, mz);
            } else if (dtype.isVectorOf(DTypes::INT32) || dtype.isVectorOf(DTypes::UINT32)) {
              for (int bz = 0; bz < 4; ++bz) { for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int zz = dz + bz, yy = dy + by, xx = dx + bx;
                i32block[bz * 16 + by * 4 + bx] = ((const int32_t*)src)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c]; }}
              }
              mg::PadBlock(i32block, mx, my, mz);
            } else if (dtype.isVectorOf(DTypes::INT16) || dtype.isVectorOf(DTypes::UINT16)) {
              for (int bz = 0; bz < 4; ++bz) { for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int zz = dz + bz, yy = dy + by, xx = dx + bx;
                i32block[bz * 16 + by * 4 + bx] = ((const int16_t*)src)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c]; }}
              }
              mg::PadBlock(i32block, mx, my, mz);
            } else if (dtype.isVectorOf(DTypes::INT8) || dtype.isVectorOf(DTypes::UINT8)) {
              for (int bz = 0; bz < 4; ++bz) { for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int zz = dz + bz, yy = dy + by, xx = dx + bx;
                i32block[bz * 16 + by * 4 + bx] = ((const int8_t*)src)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c]; }}
              }
              mg::PadBlock(i32block, mx, my, mz);
            }
          } else if (d == 2) {
            if (dtype.isVectorOf(DTypes::FLOAT64)) {
              for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int yy = dy + by, xx = dx + bx;
                f64block[by * 4 + bx] = ((const double*)src)[nc * (yy * dims[0] + xx) + c]; }
              }
              mg::PadBlock2D(f64block, mx, my);
            } else if (dtype.isVectorOf(DTypes::FLOAT32)) {
              for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int yy = dy + by, xx = dx + bx;
                f32block[by * 4 + bx] = ((const float*)src)[nc * (yy * dims[0] + xx) + c]; }
              }
              mg::PadBlock2D(f32block, mx, my);
            } else if (dtype.isVectorOf(DTypes::INT64) || dtype.isVectorOf(DTypes::UINT64)) {
              for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int yy = dy + by, xx = dx + bx;
                i64block[by * 4 + bx] = ((const int64_t*)src)[nc * (yy * dims[0] + xx) + c]; }
              }
              mg::PadBlock2D(i64block, mx, my);
            } else if (dtype.isVectorOf(DTypes::INT32) || dtype.isVectorOf(DTypes::UINT32)) {
              for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int yy = dy + by, xx = dx + bx;
                i32block[by * 4 + bx] = ((const int32_t*)src)[nc * (yy * dims[0] + xx) + c]; }
              }
              mg::PadBlock2D(i32block, mx, my);
            } else if (dtype.isVectorOf(DTypes::INT16) || dtype.isVectorOf(DTypes::UINT16)) {
              for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int yy = dy + by, xx = dx + bx;
                i32block[by * 4 + bx] = ((const int16_t*)src)[nc * (yy * dims[0] + xx) + c]; }
              }
              mg::PadBlock2D(i32block, mx, my);
            } else if (dtype.isVectorOf(DTypes::INT8) || dtype.isVectorOf(DTypes::UINT8)) {
              for (int by = 0; by < 4; ++by) { for (int bx = 0; bx < 4; ++bx) {
                int yy = dy + by, xx = dx + bx;
                i32block[by * 4 + bx] = ((const int8_t*)src)[nc * (yy * dims[0] + xx) + c]; }
              }
              mg::PadBlock2D(i32block, mx, my);
            }
          }
          /* quantize */
          int emax = 0;
          if (dtype.isVectorOf(DTypes::FLOAT64)) {
            double maxabs = 0;
            for (int i = 0; i < mg::Pow4[d]; ++i)
              maxabs = std::max(maxabs, std::abs(f64block[i]));
            emax = mg::Exponent(maxabs);
            int bits  = 64 - d - 1;
            double scale = ldexp(1, bits - 1 - emax);
            for (int i = 0; i < mg::Pow4[d]; ++i)
              i64block[i] = int64_t(scale * f64block[i]);
          } else if (dtype.isVectorOf(DTypes::FLOAT32)) {
            float maxabs = 0;
            for (int i = 0; i < mg::Pow4[d]; ++i)
              maxabs = std::max(maxabs, std::abs(f32block[i]));
            emax = mg::Exponent(maxabs);
            int bits  = 32 - d - 1;
            double scale = ldexp(1, bits - 1 - emax);
            for (int i = 0; i < mg::Pow4[d]; ++i)
              i32block[i] = int32_t(scale * f32block[i]);
          }
          /* zfp transform */
          if (d == 3) {
            if (dtype.getBitSize() / nc > 32) { // > 32 bits
              mg::ForwardZfp(i64block);
              mg::ForwardShuffle(i64block, u64block);
            } else { // <= 32 bits
              mg::ForwardZfp(i32block);
              mg::ForwardShuffle(i32block, u32block);
            }
          } else if (d == 2) {
            if (dtype.getBitSize() / nc > 32) {
              mg::ForwardZfp2D(i64block);
              mg::ForwardShuffle2D(i64block, u64block);
            } else {
              mg::ForwardZfp2D(i32block);
              mg::ForwardShuffle2D(i32block, u32block);
            }
          }
          /* encode */
          // NOTE: for integers, we support:
          // 60-bit (61 in 2D) signed integers (stored as int64_t) and 59-bit (60 in 2D) unsigned integers (stored as uint64_t)
          // 28-bit (29 in 2D) signed integers (stored as int32_t) and 27-bit (28 in 2D) unsigned integers (stored as uint32_t)
          // 16-bit signed integers (stored as int16_t) and 16-bit unsigned integers (stored as uint16_t)
          //  8-bit signed integers (stored as  int8_t) and  8-bit unsigned integers (stored as  uint8_t)          
          int nbitplanes = 64;
          if (dtype.isVectorOf(DTypes::INT32) || dtype.isVectorOf(DTypes::UINT32) || dtype.isVectorOf(DTypes::FLOAT32))
            nbitplanes = 32;
          else if (dtype.isVectorOf(DTypes::INT16) || dtype.isVectorOf(DTypes::UINT16))
            nbitplanes = 16 + d + 1;
          else if (dtype.isVectorOf(DTypes::INT8) || dtype.isVectorOf(DTypes::UINT8))
            nbitplanes = 8 + d + 1;
          int8_t n = 0;
          if (dtype.isVectorOf(DTypes::FLOAT64))
            mg::Write(&bs, emax + mg::traits<double>::ExpBias, mg::traits<double>::ExpBits);
          else if (dtype.isVectorOf(DTypes::FLOAT32))
            mg::Write(&bs, emax + mg::traits<float>::ExpBias, mg::traits<float>::ExpBits);
          if (dtype.getBitSize() / nc > 32) {
            for (int bp = nbitplanes - 1, b = 0; bp >= 0 && b < num_bit_planes; --bp, ++b)
              mg::Encode(d, u64block, bp, encoded_bound * 8, n, &bs);
          } else {
            for (int bp = nbitplanes - 1, b = 0; bp >= 0 && b < num_bit_planes; --bp, ++b)
              mg::Encode(d, u32block, bp, encoded_bound * 8, n, &bs);
          }
        } // end component loop
      }}} // end block loop
      mg::Flush(&bs);
      if (!encoded->resize(mg::Size(bs), __FILE__, __LINE__))
        return SharedPtr<HeapMemory>();

      return encoded;
    }

    //decode
    virtual SharedPtr<HeapMemory> decode(PointNi dims, DType dtype, SharedPtr<HeapMemory> encoded) override
    {
      static int counter = 0;
      if (!encoded)
        return SharedPtr<HeapMemory>();
      auto decoded = std::make_shared<HeapMemory>();
      if (!decoded->resize(dtype.getByteSize(dims), __FILE__, __LINE__))
        return SharedPtr<HeapMemory>();
      mg::bitstream bs; InitRead(&bs, mg::buffer(encoded->c_ptr(), encoded->c_size()));

      uint8_t* dst = decoded->c_ptr();
      memset(dst, 0, decoded->c_size());
      int d = dims.getPointDim(); // dimension (2 or 3) TODO: 1D?
      int nblocksz = d > 2 ? int((dims[2] + 3) / 4) : 1;
      int nblocksy = int((dims[1] + 3) / 4), nblocksx = int((dims[0] + 3) / 4);
      int nc = dtype.ncomponents();
      bool special = (nc == 3 || nc == 4) && (dtype.isVectorOf(DTypes::INT8) || dtype.isVectorOf(DTypes::UINT8));
      for (int pz = 0; pz < nblocksz; ++pz) {
        for (int py = 0; py < nblocksy; ++py) {
          for (int px = 0; px < nblocksx; ++px) { /* for each block */
            int b = pz * nblocksy * nblocksx + py * nblocksx + px;
            int s = b * (mg::Pow4[d]);
            int dx = px * 4, dy = py * 4, dz = pz * 4;
            int mx = mg::Min(4, int(dims[0]) - dx), my = mg::Min(4, int(dims[1]) - dy), mz = 1;
            if (d == 3)
              mz = mg::Min(4, int(dims[2]) - dz);
            double    f64buf[4 * 4 * 4];
            int64_t   i64buf[4 * 4 * 4];
            uint64_t  u64buf[4 * 4 * 4];
            double*   f64block = f64buf;
            float*    f32block = (float*)f64buf;
            int64_t*  i64block = i64buf;
            uint64_t* u64block = u64buf;
            int32_t*  i32block = (int32_t*)i64buf;
            uint32_t* u32block = (uint32_t*)u64buf;
            for (int c = 0; c < nc; ++c) { // for each component
              memset(f64buf, 0, sizeof(f64buf));
              memset(i64buf, 0, sizeof(i64buf));
              memset(u64buf, 0, sizeof(u64buf));
              /* decode */
              int emax = 0;
              if (dtype.isVectorOf(DTypes::FLOAT64))
                emax = (int)mg::Read(&bs, mg::traits<double>::ExpBits) - mg::traits<double>::ExpBias;
              else if (dtype.isVectorOf(DTypes::FLOAT32))
                emax = (int)mg::Read(&bs, mg::traits<float>::ExpBits) - mg::traits<float>::ExpBias;
              int nbitplanes = 64;
              if (dtype.isVectorOf(DTypes::INT32) || dtype.isVectorOf(DTypes::UINT32) || dtype.isVectorOf(DTypes::FLOAT32))
                nbitplanes = 32;
              else if (dtype.isVectorOf(DTypes::INT16) || dtype.isVectorOf(DTypes::UINT16))
                nbitplanes = 16 + d + 1;
              else if (dtype.isVectorOf(DTypes::INT8) || dtype.isVectorOf(DTypes::UINT8))
                nbitplanes = 8 + d + 1;
              int8_t n = 0;
              if (dtype.getBitSize() / nc > 32) {
                for (int bp = nbitplanes - 1; bp >= 0 && BitSize(bs) < encoded->c_size() * 8; --bp)
                  mg::Decode(d, u64block, bp, encoded->c_size() * 8, n, &bs);
              }
              else {
                for (int bp = nbitplanes - 1; bp >= 0 && BitSize(bs) < encoded->c_size() * 8; --bp)
                  mg::Decode(d, u32block, bp, encoded->c_size() * 8, n, &bs);
              }
              /* zfp inverse transform */
              if (d == 3) {
                if (dtype.getBitSize() / nc > 32) {
                  mg::InverseShuffle(u64block, i64block);
                  mg::InverseZfp(i64block);
                } else { // <= 32 bits
                  mg::InverseShuffle(u32block, i32block);
                  mg::InverseZfp(i32block);
                }
              } else if (d == 2) {
                if (dtype.getBitSize() / nc > 32) {
                  mg::InverseShuffle2D(u64block, i64block);
                  mg::InverseZfp2D(i64block);
                } else { // <= 32 bits
                  mg::InverseShuffle2D(u32block, i32block);
                  mg::InverseZfp2D(i32block);
                }
              }
              /* dequantize */
              if (dtype.isVectorOf(DTypes::FLOAT64)) {
                int bits = 64 - d - 1;
                double scale = 1.0 / ldexp(1, bits - 1 - emax);
                for (int i = 0; i < mg::Pow4[d]; ++i)
                  f64block[i] = scale * i64block[i];
              }
              else if (dtype.isVectorOf(DTypes::FLOAT32)) {
                int bits = 32 - d - 1;
                double scale = 1.0 / ldexp(1, bits - 1 - emax);
                for (int i = 0; i < mg::Pow4[d]; ++i)
                  f32block[i] = float(scale * i32block[i]);
              }
              /* copy the samples out */
              if (d == 3) {
                if (dtype.isVectorOf(DTypes::FLOAT64)) {
                  for (int bz = 0; bz < mz; ++bz) { for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int zz = dz + bz, yy = dy + by, xx = dx + bx;
                    ((double*)dst)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c] = f64block[bz * 16 + by * 4 + bx]; }}
                  }
                } else if (dtype.isVectorOf(DTypes::FLOAT32)) {
                  for (int bz = 0; bz < mz; ++bz) { for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int zz = dz + bz, yy = dy + by, xx = dx + bx;
                    ((float*)dst)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c] = f32block[bz * 16 + by * 4 + bx]; }}
                  }
                } else if (dtype.isVectorOf(DTypes::INT64) || dtype.isVectorOf(DTypes::UINT64)) {
                  for (int bz = 0; bz < mz; ++bz) { for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int zz = dz + bz, yy = dy + by, xx = dx + bx;
                    ((int64_t*)dst)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c] = i64block[bz * 16 + by * 4 + bx]; }}
                  }
                } else if (dtype.isVectorOf(DTypes::INT32) || dtype.isVectorOf(DTypes::UINT32)) {
                  for (int bz = 0; bz < mz; ++bz) { for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int zz = dz + bz, yy = dy + by, xx = dx + bx;
                    ((int32_t*)dst)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c] = i32block[bz * 16 + by * 4 + bx]; }}
                  }
                } else if (dtype.isVectorOf(DTypes::INT16) || dtype.isVectorOf(DTypes::UINT16)) {
                  for (int bz = 0; bz < mz; ++bz) { for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int zz = dz + bz, yy = dy + by, xx = dx + bx;
                    ((int16_t*)dst)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c] = (int16_t)i32block[bz * 16 + by * 4 + bx]; }}
                  }
                } else if (dtype.isVectorOf(DTypes::INT8) || dtype.isVectorOf(DTypes::UINT8)) {
                  for (int bz = 0; bz < mz; ++bz) { for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int zz = dz + bz, yy = dy + by, xx = dx + bx;
                    ((int8_t*)dst)[nc * (zz * dims[0] * dims[1] + yy * dims[0] + xx) + c] = (int8_t)i32block[bz * 16 + by * 4 + bx]; }}
                  }
                }
              }
              else if (d == 2) {
                if (dtype.isVectorOf(DTypes::FLOAT64)) {
                  for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int yy = dy + by, xx = dx + bx;
                    ((double*)dst)[nc * (yy * (dims[0]) + xx) + c] = f64block[by * 4 + bx]; }
                  }
                } else if (dtype.isVectorOf(DTypes::FLOAT32)) {
                  for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int yy = dy + by, xx = dx + bx;
                    ((float*)dst)[nc * (yy * (dims[0]) + xx) + c] = f32block[by * 4 + bx]; }
                  }
                } else if (dtype.isVectorOf(DTypes::INT64) || dtype.isVectorOf(DTypes::UINT64)) {
                  for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int yy = dy + by, xx = dx + bx;
                    ((int64_t*)dst)[nc * (yy * (dims[0]) + xx) + c] = i64block[by * 4 + bx]; }
                  }
                } else if (dtype.isVectorOf(DTypes::INT32) || dtype.isVectorOf(DTypes::UINT32)) {
                  for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int yy = dy + by, xx = dx + bx;
                    ((int32_t*)dst)[nc * (yy * dims[0] + xx) + c] = i32block[by * 4 + bx]; }
                  }
                } else if (dtype.isVectorOf(DTypes::INT16) || dtype.isVectorOf(DTypes::UINT16)) {
                  for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int yy = dy + by, xx = dx + bx;
                    ((int16_t*)dst)[nc * (yy * dims[0] + xx) + c] = (int16_t)i32block[by * 4 + bx]; }
                  }
                } else if (dtype.isVectorOf(DTypes::INT8) || dtype.isVectorOf(DTypes::UINT8)) {
                  for (int by = 0; by < my; ++by) { for (int bx = 0; bx < mx; ++bx) {
                    int yy = dy + by, xx = dx + bx;
                    ((int8_t*)dst)[nc * (yy * dims[0] + xx) + c] = (int8_t)i32block[by * 4 + bx]; }
                  }
                }
              }
            }
          }
        }
      } // end block loops
      return decoded;
    }
  };

} //namespace Visus

#endif //VISUS_ZFP_ENCODER_H

