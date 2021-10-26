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

#ifndef VISUS_LZ4_ENCODER_H
#define VISUS_LZ4_ENCODER_H

#include <Visus/Kernel.h>
#include <Visus/Encoder.h>

//self contained lz4
namespace LZ4
{
#include <lz4/lz4.h>
}

namespace Visus {

//////////////////////////////////////////////////////////////
class VISUS_KERNEL_API LZ4Encoder : public Encoder
{
public:

  VISUS_CLASS(LZ4Encoder)

  //constructor
  LZ4Encoder(String specs) {
  }

  //destructor
  virtual ~LZ4Encoder() {
  }

  //isLossy
  virtual bool isLossy() const override {
    return false;
  }

  //encode
  virtual SharedPtr<HeapMemory> encode(PointNi dims, DType dtype, SharedPtr<HeapMemory> decoded) override
  {
    if (!decoded)
      return SharedPtr<HeapMemory>();

    using namespace LZ4;

    auto encoded_bound = LZ4_compressBound((int)decoded->c_size());

    auto encoded = std::make_shared<HeapMemory>();
    if (!encoded->resize(encoded_bound, __FILE__, __LINE__))
      return SharedPtr<HeapMemory>();

#if LZ4_VERSION_MAJOR<=1 && LZ4_VERSION_MINOR<=6
#define LZ4_compress_default(source,dest,sourceSize,destSize) LZ4_compress(source,dest,sourceSize)
#endif

    auto encoded_size = LZ4_compress_default((const char*)decoded->c_ptr(), (char*)encoded->c_ptr(), (int)decoded->c_size(), (int)encoded->c_size());
    if (encoded_size <= 0)
      return SharedPtr<HeapMemory>();

    if (!encoded->resize(encoded_size, __FILE__, __LINE__))
      return SharedPtr<HeapMemory>();

    return encoded;
  }

  //decode
  virtual SharedPtr<HeapMemory> decode(PointNi dims, DType dtype, SharedPtr<HeapMemory> encoded) override
  {
    if (!encoded)
      return SharedPtr<HeapMemory>();

    auto decoded = std::make_shared<HeapMemory>();
    if (!decoded->resize(dtype.getByteSize(dims), __FILE__, __LINE__))
      return SharedPtr<HeapMemory>();

    auto nbytes = LZ4::LZ4_decompress_safe((const char*)encoded->c_ptr(), (char*)decoded->c_ptr(), (int)encoded->c_size(), (int)decoded->c_size());
    if (nbytes <= 0)
      return SharedPtr<HeapMemory>();

    if (nbytes != decoded->c_size()) {
      VisusAssert(false);
      return SharedPtr<HeapMemory>();
    }

    return decoded;
  }

};

} //namespace Visus

#endif //VISUS_LZ4_ENCODER_H

