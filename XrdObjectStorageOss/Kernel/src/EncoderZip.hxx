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

#ifndef VISUS_ZIP_ENCODER_H
#define VISUS_ZIP_ENCODER_H

#include <Visus/Kernel.h>
#include <Visus/Encoder.h>

namespace ZLib
{
#include "zlib/zlib.h"
}


namespace Visus {

//////////////////////////////////////////////////////////////
class VISUS_KERNEL_API ZipEncoder : public Encoder
{
public:

  VISUS_CLASS(ZipEncoder)

  int compression_level=0;

  //constructor
  ZipEncoder(String specs)
  {
    auto options = StringUtils::split(specs, "-");

    for (auto it : options)
    {
      if (it == "Z_NO_COMPRESSION")
        compression_level |= Z_NO_COMPRESSION;

      else if (it == "Z_BEST_SPEED")
        compression_level |= Z_BEST_SPEED;

      else if (it == "Z_BEST_COMPRESSION")
        compression_level |= Z_BEST_COMPRESSION;

      else if (it == "Z_DEFAULT_COMPRESSION")
        compression_level |= Z_DEFAULT_COMPRESSION;
    }

    if (!compression_level)
      compression_level = Z_DEFAULT_COMPRESSION;
  }

  //destructor
  virtual ~ZipEncoder() {
  }

  //isLossy
  virtual bool isLossy() const override
  {
    return false;
  }

  //encode
  virtual SharedPtr<HeapMemory> encode(PointNi dims, DType dtype, SharedPtr<HeapMemory> decoded) override
  {
    if (!decoded)
      return SharedPtr<HeapMemory>();

    ZLib::uLong zbound = ZLib::compressBound(ZLib::uLong(decoded->c_size()));

    auto encoded = std::make_shared<HeapMemory>();
    if (!encoded->resize(zbound, __FILE__, __LINE__))
      return SharedPtr<HeapMemory>();

    if (ZLib::compress2(encoded->c_ptr(), &zbound, decoded->c_ptr(), (ZLib::uLong)decoded->c_size(), compression_level) != Z_OK)
      return SharedPtr<HeapMemory>();

    if (!encoded->resize(zbound, __FILE__, __LINE__))
      return SharedPtr<HeapMemory>();

    return encoded;
  }

  //decode
  virtual SharedPtr<HeapMemory> decode(PointNi dims, DType dtype, SharedPtr<HeapMemory> encoded) override
  {
    if (!encoded)
      return SharedPtr<HeapMemory>();

    ZLib::uLong decoded_len = (ZLib::uLong)(dtype.getByteSize(dims));

    auto decoded = std::make_shared<HeapMemory>();
    if (!decoded->resize(dtype.getByteSize(dims), __FILE__, __LINE__))
      return SharedPtr<HeapMemory>();

    if (ZLib::uncompress(decoded->c_ptr(), &decoded_len, encoded->c_ptr(), (ZLib::uLong)encoded->c_size()) != Z_OK)
      return SharedPtr<HeapMemory>();

    VisusAssert(decoded_len == decoded->c_size());
    return decoded;
  }

};

} //namespace Visus

#endif //VISUS_ZIP_ENCODER_H

