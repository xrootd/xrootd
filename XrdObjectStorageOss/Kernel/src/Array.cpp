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

#include <Visus/Array.h>
#include <Visus/Color.h>
#include <Visus/Utils.h>


namespace Visus {


///////////////////////////////////////////////////////////////////////////////////////
Array Array::getComponent(int C, Aborted aborted) const
{
  //return the same component
  if (this->dtype.ncomponents() == 1 && C == 0)
    return *this;

  Int64 Soffset = this->dtype.getBitsOffset(C); DType Sdtype = this->dtype;
  Int64 Doffset = 0;                            DType Ddtype = this->dtype.get(C);

  Array dst;
  if (!dst.resize(this->dims, Ddtype, __FILE__, __LINE__))
    return Array();

  dst.shareProperties(*this);

  //TODO!
  if (!Utils::isByteAligned(Ddtype.getBitSize()) || !Utils::isByteAligned(Soffset) ||
    !Utils::isByteAligned(Sdtype.getBitSize()) || !Utils::isByteAligned(Doffset))
  {
    VisusAssert(aborted());
    return Array();
  }

  int bytesize = Ddtype.getBitSize() >> 3;

  Soffset >>= 3; int Sstep = (Sdtype.getBitSize()) >> 3;
  Doffset >>= 3; int Dstep = (Ddtype.getBitSize()) >> 3;

  Uint8*       dst_p = dst.c_ptr() + Doffset;
  const Uint8* src_p = this->c_ptr() + Soffset;

  Int64 tot = this->getTotalNumberOfSamples();
  for (Int64 sample = 0; sample < tot; sample++, dst_p += Dstep, src_p += Sstep)
  {
    if (aborted()) return Array();
    memcpy(dst_p, src_p, bytesize);
  }

  return dst;
}


///////////////////////////////////////////////////////////////////////////////
bool Array::setComponent(int C, Array src, Aborted aborted)
{
  if (!src.valid())
    return false;

  Int64  Doffset = this->dtype.getBitsOffset(C);
  DType  Ddtype = this->dtype.get(C);
  int    Dstep = this->dtype.getBitSize();

  //automatic casting
  if (src.dtype != Ddtype)
    return setComponent(C, ArrayUtils::cast(src, Ddtype, aborted), aborted);

  Int64 Soffset = 0;
  DType Sdtype = src.dtype;
  int   Sstep = Sdtype.getBitSize();

  if (Ddtype.getBitSize() != Sdtype.getBitSize() || src.dims != this->dims)
  {
    VisusAssert(aborted());
    if (!aborted())
      PrintWarning("cannot copy, dtype or dims not compatible!)");
    return false;
  }

  //todo
  if (!Utils::isByteAligned(Sdtype.getBitSize()) || !Utils::isByteAligned(Sstep) || !Utils::isByteAligned(Soffset) ||
    !Utils::isByteAligned(Ddtype.getBitSize()) || !Utils::isByteAligned(Dstep) || !Utils::isByteAligned(Doffset))
  {
    VisusAssert(aborted());
    return false;
  }

  int bytesize = Sdtype.getBitSize() >> 3;
  Doffset >>= 3; Dstep >>= 3;
  Soffset >>= 3; Sstep >>= 3;
  unsigned char* dst_p = this->c_ptr() + Doffset;
  unsigned char* src_p = src.c_ptr() + Soffset;
  Int64 tot = src.getTotalNumberOfSamples();
  for (Int64 I = 0; I < tot; I++, dst_p += Dstep, src_p += Sstep)
  {
    if (aborted()) return false;
    memcpy(dst_p, src_p, bytesize);
  }

  return true;
}


} //namespace Visus

