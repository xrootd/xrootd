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
#include <Visus/Encoder.h>
#include <Visus/Color.h>
#include <Visus/Path.h>
#include <Visus/File.h>
#include <Visus/TransferFunction.h>

namespace Visus {

///////////////////////////////////////////////////////////////////////////////////////////////
Array ArrayUtils::loadImage(String url,std::vector<String> args)
{
  for (auto plugin : ArrayPlugins::getSingleton()->values)
  {
    auto ret = plugin->handleLoadImage(url, args);
    if (ret.valid())
    {
      ret.url=url;

      #ifdef _DEBUG
      PrintInfo(url,"loaded","dtype",ret.dtype,"dims",ret.dims);
      #endif
      
      return ret;
    }
  }

  PrintInfo("Cannot loadImage(",url);
  return Array();
}


///////////////////////////////////////////////////////////////////////////////////////////////
Array ArrayUtils::loadImageFromMemory(String url,SharedPtr<HeapMemory> heap,std::vector<String> args)
{
  for (auto plugin : ArrayPlugins::getSingleton()->values)
  {
    auto ret = plugin->handleLoadImageFromMemory(heap, args);
    if (ret.valid())
    {
      ret.url=url;
      return ret;
    }
  }

  PrintInfo("Cannot loadImageFromMemory",url);
  return Array();
}


///////////////////////////////////////////////////////////////////////////////////////////////
StringTree ArrayUtils::statImage(String url)
{
  for (auto plugin : ArrayPlugins::getSingleton()->values) 
  {
    StringTree ret=plugin->handleStatImage(url);
    if (ret.valid())
      return ret;
  }
  return StringTree();
}


///////////////////////////////////////////////////////////////////////////////////////////////
bool ArrayUtils::saveImage(String url,Array src,std::vector<String> args)
{
  if (!src.valid())
    return false;

  FileUtils::createDirectory(Path(url).getParent());

  for (auto plugin : ArrayPlugins::getSingleton()->values)
  {
    if (plugin->handleSaveImage(url,src,args)) 
    {
      src.url=url;
      return true;
    }
  }

  PrintInfo("Cannot saveImage",url);
  return false;
}

//////////////////////////////////////////////////////////////
SharedPtr<HeapMemory> ArrayUtils::encodeArray(String compression,Array array)
{
  if (!array.valid()) {
    VisusAssert(false);
    return SharedPtr<HeapMemory>();
  }

  SharedPtr<HeapMemory> encoded;
  if (compression.empty())
  {
    encoded = array.heap;
  }
  else
  {
    auto encoder = Encoders::getSingleton()->createEncoder(compression);
    if (!encoder)
    {
      VisusAssert(false);
      return SharedPtr<HeapMemory>();
    }
    //if encoder fails, just copy the array
    encoded = encoder->encode(array.dims, array.dtype, array.heap);
  }

  if (!encoded) {
    VisusAssert(false);
    return SharedPtr<HeapMemory>();
  }

  return encoded;
}


//////////////////////////////////////////////////////////////
Array ArrayUtils::decodeArray(String compression,PointNi dims,DType dtype,SharedPtr<HeapMemory> encoded)
{
  if (!encoded) {
    VisusAssert(false);
    return Array();
  }

  if (dims.innerProduct()<=0 || !dtype.valid())
    return Array();

  SharedPtr<HeapMemory> decoded;
  if (compression.empty())
  {
    decoded = encoded;
  }
  else
  {
    auto decoder = Encoders::getSingleton()->createEncoder(compression);
    if (!decoder) {
      VisusAssert(false);
      return Array();
    }
    decoded = decoder->decode(dims, dtype, encoded);
  }

  if (!decoded)
    return Array();

  VisusAssert(decoded->c_size()==dtype.getByteSize(dims));
  return Array(dims,dtype,decoded);
}


//////////////////////////////////////////////////////////////////////////////////////////
Array ArrayUtils::decodeArray(StringMap metadata, SharedPtr<HeapMemory> encoded)
{
  if (!encoded || !encoded->c_size())
    return Array();

  auto compression = metadata.getValue("visus-compression");
  auto nsamples = PointNi::fromString(metadata.getValue("visus-nsamples"));
  auto dtype = DType::fromString(metadata.getValue("visus-dtype"));
  auto layout = metadata.getValue("visus-layout");

  //backward compatible
  if (metadata.hasValue("visus-format"))
    layout = cint(metadata.getValue("visus-format")) ? "" : "hzorder";

  if (nsamples.innerProduct() <= 0 || !dtype.valid())
    return Array();

  //override compression if needed (think about legacy dataset)
  if (!metadata.hasValue("visus-compression") && !metadata.getValue("Content-Type").empty())
  {
    auto content_type = metadata.getValue("Content-Type");
    if      (content_type == "application/x-lz4")   compression = "lz4";
    else if (content_type == "application/zip")     compression = "zip";
    else if (content_type == "image/png")           compression = "png";
    else if (content_type == "image/jpeg")          compression = "jpg";
    else if (content_type == "image/tiff")          compression = "tif";
  }

  auto decoded = decodeArray(compression, nsamples, dtype, encoded);
  if (!decoded.valid())
    return Array();

  decoded.layout = layout;
  return decoded;

}

  
//////////////////////////////////////////////////////////////////////////////////////////
class InsertArraySamples 
{
public:

  template <class Sample>
  bool execute(
    Array& dst, PointNi wfrom, PointNi wto, PointNi wstep,
    Array  src, PointNi rfrom, PointNi rto, PointNi rstep,
    Aborted& aborted)
  {
    if (dst.dtype != src.dtype)
      return false;

    auto write = GetSamples<Sample>(dst);
    auto read  = GetSamples<Sample>(src);

    PointNi wdims = dst.dims; PointNi wstride = wdims.stride();
    PointNi rdims = src.dims; PointNi rstride = rdims.stride();

    int sample_bitsize = dst.dtype.getBitSize();

    int pdim = src.getPointDim();

    /*
    equivalent to:
      ...
      for (w[2] = wfrom[2], r[2] = rfrom[2]; w[2] < wto[2] && r[2] < rto[2]; w[2] += wstep[2], r[2] += rstep[2]) {
      for (w[1] = wfrom[1], r[1] = rfrom[1]; w[1] < wto[1] && r[1] < rto[1]; w[1] += wstep[1], r[1] += rstep[1]) {
      for (w[0] = wfrom[0], r[0] = rfrom[0]; w[0] < wto[0] && r[0] < rto[0]; w[0] += wstep[0], r[0] += rstep[0])
      {
        if (aborted())
          return false;

        write[wstride.dotProduct(w)]=read[rstride.dotProduct(r)];
      ...
      }
    */

    wstep = PointNi::max(wstep, PointNi::one(pdim));
    rstep = PointNi::max(rstep, PointNi::one(pdim));

    //check arguments
    VisusAssert(wfrom>=PointNi(pdim) && wfrom<=wto && wto<=wdims);

    PointNi tot = PointNi::one(pdim);
    PointNi wbegin(pdim); PointNi wdelta = PointNi::one(pdim);
    PointNi rbegin(pdim); PointNi rdelta = PointNi::one(pdim);
    for (int D = 0; D < pdim; D++)
    {
      //for offsets
      wbegin[D] = wstride[D] * wfrom[D]; 
      rbegin[D] = rstride[D] * rfrom[D]; 

      wdelta[D] = wstride[D] * wstep[D];
      rdelta[D] = rstride[D] * rstep[D];

      //align
      wto[D] = Utils::alignRight(wto[D], wfrom[D], wstep[D]);
      rto[D] = Utils::alignRight(rto[D], rfrom[D], rstep[D]);

      //compute totals
      tot[D] = std::min(
        (wto[D] - wfrom[D]) / wstep[D],
        (rto[D] - rfrom[D]) / rstep[D]);
    }

    //if you have problems set this to false
    PointNi ncontiguos(pdim);
    if (bool bEnableContiguous = true)
    {
      for (int D = 0; D < pdim; D++)
      {
        if (wstep[D] == 1 && rstep[D] == 1)
        {
          if (!D) 
          {
            ncontiguos[D] = tot[0];
          }
          else if (ncontiguos[D - 1] > 0 
            && wdims[D - 1] == rdims[D - 1]
            && !wfrom[D - 1] && wto[D - 1] == wdims[D - 1]
            && !rfrom[D - 1] && rto[D - 1] == rdims[D - 1]) 
          {
            ncontiguos[D] = ncontiguos[D - 1] * tot[D];
          }
        }
      }
    }

    PointNi p(pdim);
    PointNi woffset(pdim);
    PointNi roffset(pdim);

    #define ForExpr(D) \
      woffset[D] = (D==(pdim-1)? 0 : woffset[D+1]) + wbegin[D]; \
      roffset[D] = (D==(pdim-1)? 0 : roffset[D+1]) + rbegin[D]; \
      if (ncontiguos[D]) \
        write.range(woffset[D], ncontiguos[D]) = read.range(roffset[D], ncontiguos[D]); \
      else \
        for (p[D] = 0; p[D] < tot[D]; ++p[D], woffset[D] += wdelta[D], roffset[D] += rdelta[D]) { \
      /*--*/

    #define ForBody() \
      write[woffset[0]]=read[roffset[0]]; \
      /*--*/

    switch (pdim) 
    { 
    case 1:                                  if (aborted()) return false;            ForExpr(0) ForBody() }     break;
    case 2:                                  if (aborted()) return false; ForExpr(1) ForExpr(0) ForBody() }}    break;
    case 3:                       ForExpr(2) if (aborted()) return false; ForExpr(1) ForExpr(0) ForBody() }}}   break;
    case 4:            ForExpr(3) ForExpr(2) if (aborted()) return false; ForExpr(1) ForExpr(0) ForBody() }}}}  break;
    case 5: ForExpr(4) ForExpr(3) ForExpr(2) if (aborted()) return false; ForExpr(1) ForExpr(0) ForBody() }}}}} break;
    default: VisusAssert(false); return false;
    }

    #undef ForExpr
    #undef ForBody

    return true;
  }

};

bool ArrayUtils::insert(
  Array& dst, PointNi wfrom, PointNi wto, PointNi wstep,
  Array  src, PointNi rfrom, PointNi rto, PointNi rstep, Aborted aborted)
{
  InsertArraySamples op;
  return NeedToCopySamples(op,src.dtype,dst, wfrom, wto, wstep, src, rfrom, rto, rstep, aborted);
}


///////////////////////////////////////////////////////////////////////////////
Array ArrayUtils::splitAndGetFirst(Array src, int bit, Aborted aborted)
{
  PointNi dims = src.dims;
  VisusAssert((dims[bit] % 2) == 0); //todo!
  dims[bit] >>= 1;
  Array dst;
  if (!dst.resize(dims, src.dtype, __FILE__, __LINE__)) return Array();
  int pdim = src.getPointDim();
  PointNi wfrom(pdim), wto = dst.dims, wstep = PointNi::one(pdim);
  PointNi rfrom(pdim), rto = dst.dims, rstep = PointNi::one(pdim);
  return insert(dst, wfrom, wto, wstep, src, rfrom, rto, rstep, aborted)? dst : Array();
}

///////////////////////////////////////////////////////////////////////////////
Array ArrayUtils::splitAndGetSecond(Array src, int bit, Aborted aborted)
{
  PointNi dims = src.dims;
  VisusAssert((dims[bit] % 2) == 0); //todo!
  dims[bit] >>= 1;
  Array dst;
  if (!dst.resize(dims, src.dtype, __FILE__, __LINE__)) return Array();
  int pdim = src.getPointDim();
  PointNi wfrom(pdim), wto = dst.dims, wstep = PointNi::one(pdim);
  PointNi rfrom(pdim), rto = src.dims, rstep = PointNi::one(pdim); rfrom[bit] = dst.dims[bit];
  return insert(dst, wfrom, wto, wstep, src, rfrom, rto, rstep, aborted)? dst : Array();
}

///////////////////////////////////////////////////////////////////////////////
Array ArrayUtils::upSample(Array src, int bit, Aborted aborted)
{
  PointNi dims = src.dims;
  dims[bit] <<= 1;
  Array dst;
  if (!dst.resize(dims, src.dtype, __FILE__, __LINE__)) return Array();
  int pdim = src.getPointDim();
  PointNi wfrom(pdim), wto = dst.dims, wstep = PointNi::one(pdim); wstep[bit] <<= 1;
  PointNi rfrom(pdim), rto = src.dims, rstep = PointNi::one(pdim);
  if (!insert(dst, wfrom, wto, wstep, src, rfrom, rto, rstep, aborted)) return Array();
  wfrom[bit] = 1;
  if (!insert(dst, wfrom, wto, wstep, src, rfrom, rto, rstep, aborted)) return Array();
  return dst;
}

///////////////////////////////////////////////////////////////////////////////
Array ArrayUtils::downSample(Array src, int bit, Aborted aborted)
{
  PointNi dims = src.dims;
  VisusAssert((dims[bit] % 2) == 0); //todo!
  dims[bit] >>= 1;
  Array dst;
  if (!dst.resize(dims, src.dtype, __FILE__, __LINE__)) return Array();
  int pdim = src.getPointDim();
  PointNi wfrom(pdim), wto = dst.dims, wstep = PointNi::one(pdim);
  PointNi rfrom(pdim), rto = src.dims, rstep = PointNi::one(pdim); rstep[bit] <<= 1;
  return insert(dst, wfrom, wto, wstep, src, rfrom, rto, rstep, aborted)? dst : Array();
}

//////////////////////////////////////////////////////
Array ArrayUtils::crop(Array src, BoxNi box, Aborted aborted)
{
  int pdim = src.getPointDim();
  bool bArgOk = box.isFullDim() && BoxNi(PointNi(pdim),src.dims).containsBox(box);

  if (!bArgOk)
  {
    VisusAssert(aborted());
    return Array();
  }

  Array dst;
  if (!dst.resize(box.size(), src.dtype, __FILE__, __LINE__))
    return Array();

  PointNi wfrom = PointNi(pdim), wto = dst.dims, wstep = PointNi::one(pdim);
  PointNi rfrom = box.p1       , rto = box.p2  , rstep = PointNi::one(pdim);
  return insert(dst, wfrom, wto, wstep, src, rfrom, rto, rstep, aborted)? dst : Array();
}

//////////////////////////////////////////////////////
bool ArrayUtils::paste(Array& dst, BoxNi Dbox, Array src, BoxNi Sbox, Aborted aborted)
{
  bool bArgOk = Sbox.isFullDim()
    && Dbox.isFullDim()
    && Sbox.size() == Dbox.size()
    && BoxNi(PointNi(src.getPointDim()), src.dims).containsBox(Sbox)
    && BoxNi(PointNi(dst.getPointDim()), dst.dims).containsBox(Dbox);
  if (!bArgOk) { 
    VisusAssert(aborted()); 
    return false; 
  }

  int pdim = src.getPointDim();
  PointNi wfrom = Dbox.p1, wto = Dbox.p2, wstep = PointNi::one(pdim);
  PointNi rfrom = Sbox.p1, rto = Sbox.p2, rstep = PointNi::one(pdim);
  return insert(dst, wfrom, wto, wstep, src, rfrom, rto, rstep, aborted);
}

///////////////////////////////////////////////////////////////////////////////
class MirrorArraySamples 
{
public:

  template <class Sample>
  bool execute(Array& dst, Array& src, int axis, Aborted& aborted)
  {
    if (!dst.resize(src.dims, src.dtype, __FILE__, __LINE__)) return false;

    int pdim = src.getPointDim();

    PointNi stride = src.dims.stride();
    auto write=GetSamples<Sample>(dst);
    auto read =GetSamples<Sample>(src);
    for(auto loc =ForEachPoint(src.dims);!loc.end();loc.next())
    {
      if (aborted()) return false;
      PointNi rloc = loc.pos;
      PointNi wloc = loc.pos; wloc[axis] = src.dims[axis] - 1 - wloc[axis];
      Int64 rfrom = stride.dotProduct(rloc);
      Int64 wfrom = stride.dotProduct(wloc);
      write[wfrom]=read[rfrom];
    }
    return true;
  }

};

Array ArrayUtils::mirror(Array src, int axis, Aborted aborted)
{
  Array dst;
  MirrorArraySamples op;
  return NeedToCopySamples(op,src.dtype,dst, src, axis, aborted)? dst : Array();
}

///////////////////////////////////////////////////////////////////////////////////////
Array ArrayUtils::interleave(std::vector<Array> v, Aborted aborted)
{
  if (v.empty())
    return Array();

  if (v.size() == 1)
    return v[0];

  Array& first = v[0];
  for (int I = 0; I < (int)v.size(); I++)
  {
    if (first.dims != v[I].dims || first.dtype != v[I].dtype)
      return Array();
  }

  PointNi dims = first.dims;
  DType dtype((int)v.size(), first.dtype);

  Array dst;
  if (!dst.resize(dims, dtype, __FILE__, __LINE__))
    return Array();

  for (int I = 0; !aborted() && I < (int)v.size(); I++)
    dst.setComponent(I, v[I], aborted);

  if (aborted())
    return Array();

  dst.shareProperties(v[0]);
  return dst;
}

///////////////////////////////////////////////////////////////////////////////
Array ArrayUtils::interleave(Array data, Aborted aborted)
{
  //need to interleave (the input for example is RRRRR...GGGGG...BBBBB and I really need RGBRGBRGB)
  int ncomponents = data.dtype.ncomponents();
  if (ncomponents <= 1)
    return data;

  std::vector<Array> v;
  for (int I = 0; I < ncomponents; I++)
  {
    if (aborted())
      return Array();
    DType component_dtype = data.dtype.get(I);
    Int64 offset = I * component_dtype.getByteSize(data.dims);
    v.push_back(Array::createView(data, data.dims, component_dtype, offset));
  }

  return ArrayUtils::interleave(v, aborted);
}

/////////////////////////////////////////////////////////////////////
std::vector<Array> ArrayUtils::split(Array src, Aborted aborted)
{
  std::vector<Array> ret;
  for (int C : Utils::range(src.dtype.ncomponents()))
  {
    auto component = src.getComponent(C);
    component.shareProperties(src);
    ret.push_back(component);
  }
  return ret;
}


///////////////////////////////////////////////////////////////////////////////
struct ConvertToSameDTypeOp
{
  template <typename Type>
  bool execute(Array& dst, Array src, Aborted aborted)
  {
    int n = src.dtype.ncomponents();
    int m = dst.dtype.ncomponents();
    int ncomponents = std::min(m, n);
    Int64 totsamples = src.getTotalNumberOfSamples();

    //for each component...
    for (int C = 0; C < ncomponents; C++)
    {
      Type* src_p = ((Type*)src.c_ptr()) + C;
      Type* dst_p = ((Type*)dst.c_ptr()) + C;
      for (Int64 I = 0; I < totsamples; I++, src_p += n, dst_p += m)
      {
        if (aborted()) return false;
        *dst_p = *src_p;
      }
    }
    return true;
  }
};

Array ArrayUtils::smartCast(Array src, DType dtype_, Aborted aborted)
{
  //simple case, same dtype
  if (src.dtype == dtype_)
    return src;

  Int64 totsamples = src.getTotalNumberOfSamples();

  //overflow
  if (totsamples < 0) 
    return Array(); 

  Array dst;
  if (!dst.resize(src.dims, dtype_, __FILE__, __LINE__))
    return Array();

  dst.shareProperties(src);

  //important if the components to copy from src are less than dst
  dst.fillWithValue(0);

  //simple cases, just modifying the number of elements
  if (src.dtype.get(0) == dst.dtype.get(0))
  {
    ConvertToSameDTypeOp op;
    return ExecuteOnCppSamples(op, src.dtype, dst, src, aborted) ? dst : Array();
  }

  int n = src.dtype.ncomponents();
  int m = dst.dtype.ncomponents();
  int ncomponents = std::min(m, n);

  if (src.dtype.isVectorOf(DTypes::UINT16) && dst.dtype.isVectorOf(DTypes::UINT8))
  {
    for (int C = 0; C < ncomponents; C++)
    {
      //compute the range
      Uint16 min, max;
      {
        Uint16* src_p = ((Uint16*)src.c_ptr()) + C;
        min = *src_p;
        max = *src_p;
        for (Int64 I = 0; I < totsamples; I++, src_p += n)
        {
          if (aborted()) return Array();
          min = std::min(min, *src_p);
          max = std::max(max, *src_p);
        }
        //PrintInfo("Range for component C",C,"min",min,"max",max);
      }

      {
        Uint16* src_p = ((Uint16*)src.c_ptr()) + C;
        Uint8*  dst_p = ((Uint8*)dst.c_ptr()) + C;
        for (Int64 I = 0; I < totsamples; I++, src_p += n, dst_p += m)
        {
          if (aborted()) return Array();
          *dst_p = (Uint8)(255.0*(*src_p - min) / (double)(max - min));
        }
      }
    }
    return dst;
  }

  //uint8 -> float32 `
  if (src.dtype.isVectorOf(DTypes::UINT8) && dst.dtype.isVectorOf(DTypes::FLOAT32))
  {
    for (int C = 0; C < ncomponents; C++)
    {
      Uint8*   src_p = ((Uint8*)src.c_ptr()) + C;
      Float32* dst_p = ((Float32*)dst.c_ptr()) + C;
      for (Int64 I = 0; I < totsamples; I++, src_p += n, dst_p += m)
      {
        if (aborted()) return Array();
        *dst_p = (*src_p) / 255.0f;
      }
    }
    return dst;
  }

  //uint8 -> float64
  if (src.dtype.isVectorOf(DTypes::UINT8) && dst.dtype.isVectorOf(DTypes::FLOAT64))
  {
    for (int C = 0; C < ncomponents; C++)
    {
      Uint8*   src_p = ((Uint8*)src.c_ptr()) + C;
      Float64* dst_p = ((Float64*)dst.c_ptr()) + C;
      for (Int64 I = 0; I < totsamples; I++, src_p += n, dst_p += m)
      {
        if (aborted()) return Array();
        *dst_p = (*src_p) / 255.0;
      }
    }
    return dst;
  }

  //float32 -> float64
  if (src.dtype.isVectorOf(DTypes::FLOAT32) && dst.dtype.isVectorOf(DTypes::FLOAT64))
  {
    for (int C = 0; C < ncomponents; C++)
    {
      Float32* src_p = ((Float32*)src.c_ptr()) + C;
      Float64* dst_p = ((Float64*)dst.c_ptr()) + C;
      for (Int64 I = 0; I < totsamples; I++, src_p += n, dst_p += m)
      {
        if (aborted()) return Array();
        *dst_p = *src_p;
      }
    }
    return dst;
  }

  //float32 -> uint8
  if (src.dtype.isVectorOf(DTypes::FLOAT32) && dst.dtype.isVectorOf(DTypes::UINT8))
  {
    for (int C = 0; C < ncomponents; C++)
    {
      Range range = src.dtype.getDTypeRange(C);
      if (!range.delta()) range = computeRange(src,C);
      Float32*  src_p = ((Float32*)src.c_ptr()) + C;
      Uint8*    dst_p = ((Uint8*)dst.c_ptr()) + C;
      for (Int64 I = 0; I < totsamples; I++, src_p += n, dst_p += m)
      {
        if (aborted()) return Array();
        *dst_p = (Uint8)(255 * Utils::clamp((Float32)((*src_p) - range.from) / (Float32)(range.to - range.from), 0.0f, 1.0f));
      }
    }
    return dst;
  }

  //float64 -> uint8
  if (src.dtype.isVectorOf(DTypes::FLOAT64) && dst.dtype.isVectorOf(DTypes::UINT8))
  {
    for (int C = 0; C < ncomponents; C++)
    {
      Range range = src.dtype.getDTypeRange(C);
      if (!range.delta()) range = computeRange(src,C);
      Float64*  src_p = ((Float64*)src.c_ptr()) + C;
      Uint8*    dst_p = ((Uint8*)dst.c_ptr()) + C;
      for (Int64 I = 0; I < totsamples; I++, src_p += n, dst_p += m)
      {
        if (aborted()) return Array();
        *dst_p = (Uint8)(255 * Utils::clamp((Float64)((*src_p) - range.from) / (Float64)(range.to - range.from), 0.0, 1.0));
      }
    }
    return dst;
  }

  PrintWarning("cannot smartCast from ",src.dtype,"to",dst.dtype);
  VisusAssert(false);
  return Array();
}

/////////////////////////////////////////////////////////////////////
template <typename Dtype, typename Stype>
static Array CastArray(Array src, DType dtype, Aborted& aborted)
{
  //simple cases, just modifying the number of elements
  if (src.dtype.get(0) == dtype.get(0))
  {
    Array dst;

    if (!dst.resize(src.dims, dtype, __FILE__, __LINE__))
      return Array();

    dst.shareProperties(src);

    //important if the components to copy from src are less than dst
    dst.fillWithValue(0);
  
    ConvertToSameDTypeOp op;
    return ExecuteOnCppSamples(op,src.dtype,dst, src, aborted)? dst : Array();
  }

  if (dtype.ncomponents() != src.dtype.ncomponents())
  {
    VisusAssert(aborted());
    return Array();
  }

  int ncomponents = src.dtype.ncomponents();

  //useless call
  if (dtype == src.dtype)
    return src;

  Array dst;
  if (!dst.resize(src.dims, dtype, __FILE__, __LINE__))
    return Array();

  dst.shareProperties(src);

  //what to do with the SingleComponentRange?

  Dtype* dst_p = (Dtype*)dst.c_ptr();
  Stype* src_p = (Stype*)src.c_ptr();
  Int64 tot = src.getTotalNumberOfSamples()*ncomponents;
  for (Int64 I = 0; I < tot; I++)
  {
    if (aborted()) return Array();
    dst_p[I] = (Dtype)(src_p[I]);
  }
  return dst;
}

template <typename Dtype>
static Array CastArray(Array src, DType dtype, Aborted& aborted)
{
  if (src.dtype.isVectorOf(DTypes::INT8)) { return CastArray<Dtype, Int8   >(src, dtype, aborted); }
  if (src.dtype.isVectorOf(DTypes::UINT8)) { return CastArray<Dtype, Uint8  >(src, dtype, aborted); }
  if (src.dtype.isVectorOf(DTypes::INT16)) { return CastArray<Dtype, Int16  >(src, dtype, aborted); }
  if (src.dtype.isVectorOf(DTypes::UINT16)) { return CastArray<Dtype, Uint16 >(src, dtype, aborted); }
  if (src.dtype.isVectorOf(DTypes::INT32)) { return CastArray<Dtype, Int32  >(src, dtype, aborted); }
  if (src.dtype.isVectorOf(DTypes::UINT32)) { return CastArray<Dtype, Uint32 >(src, dtype, aborted); }
  if (src.dtype.isVectorOf(DTypes::INT64)) { return CastArray<Dtype, Int64  >(src, dtype, aborted); }
  if (src.dtype.isVectorOf(DTypes::UINT64)) { return CastArray<Dtype, Uint64 >(src, dtype, aborted); }
  if (src.dtype.isVectorOf(DTypes::FLOAT32)) { return CastArray<Dtype, Float32>(src, dtype, aborted); }
  if (src.dtype.isVectorOf(DTypes::FLOAT64)) { return CastArray<Dtype, Float64>(src, dtype, aborted); }
  return Array();
}

static Array CastArray(Array src, DType dtype, Aborted& aborted)
{
  if (dtype.isVectorOf(DTypes::INT8)) { return CastArray<Int8   >(src, dtype, aborted); }
  if (dtype.isVectorOf(DTypes::UINT8)) { return CastArray<Uint8  >(src, dtype, aborted); }
  if (dtype.isVectorOf(DTypes::INT16)) { return CastArray<Int16  >(src, dtype, aborted); }
  if (dtype.isVectorOf(DTypes::UINT16)) { return CastArray<Uint16 >(src, dtype, aborted); }
  if (dtype.isVectorOf(DTypes::INT32)) { return CastArray<Int32  >(src, dtype, aborted); }
  if (dtype.isVectorOf(DTypes::UINT32)) { return CastArray<Uint32 >(src, dtype, aborted); }
  if (dtype.isVectorOf(DTypes::INT64)) { return CastArray<Int64  >(src, dtype, aborted); }
  if (dtype.isVectorOf(DTypes::UINT64)) { return CastArray<Uint64 >(src, dtype, aborted); }
  if (dtype.isVectorOf(DTypes::FLOAT32)) { return CastArray<Float32>(src, dtype, aborted); }
  if (dtype.isVectorOf(DTypes::FLOAT64)) { return CastArray<Float64>(src, dtype, aborted); }
  return Array();
}

Array ArrayUtils::cast(Array src, DType dtype, Aborted aborted)
{
  if (src.dtype==dtype)
    return src;

  return CastArray(src, dtype, aborted);
}



/////////////////////////////////////////////////////////////////////
template <typename CppType>
Array SqrtArray(Array src, Aborted& aborted)
{
  int ncomponents = src.dtype.ncomponents();

  Array dst;
  if (!dst.resize(src.dims, src.dtype, __FILE__, __LINE__))
    return Array();
  dst.shareProperties(src);

  CppType* dst_p = (CppType*)dst.c_ptr();
  CppType* src_p = (CppType*)src.c_ptr();
  Int64 tot = src.getTotalNumberOfSamples()*ncomponents;
  for (Int64 I = 0; I < tot; I++)
  {
    if (aborted()) return Array();
    dst_p[I] = (CppType)sqrt(src_p[I]);
  }
  return dst;
}

Array ArrayUtils::sqrt(Array src, Aborted aborted)
{
  if (!src.valid())
    return Array();

  if (src.dtype.isVectorOf(DTypes::FLOAT32))
    return SqrtArray<Float32>(src, aborted);

  if (src.dtype.isVectorOf(DTypes::FLOAT64))
    return SqrtArray<Float64>(src, aborted);

  return sqrt(cast(src, DType(src.dtype.ncomponents(), DTypes::FLOAT32), aborted), aborted);
}

/////////////////////////////////////////////////////////////////////
template <typename CppType>
Array AddArray(Array src, CppType value, Aborted& aborted)
{
  int ncomponents = src.dtype.ncomponents();

  Array dst;
  if (!dst.resize(src.dims, src.dtype, __FILE__, __LINE__))
    return Array();

  dst.shareProperties(src);

  CppType* dst_p = (CppType*)dst.c_ptr();
  CppType* src_p = (CppType*)src.c_ptr();
  Int64 tot = src.getTotalNumberOfSamples()*ncomponents;
  for (Int64 I = 0; I < tot; I++)
  {
    if (aborted()) return Array();
    dst_p[I] = (CppType)(src_p[I] + value);
  }
  return dst;
}

Array ArrayUtils::add(Array src, double coeff, Aborted aborted)
{
  if (!src.valid())
    return Array();

  if (src.dtype.isVectorOf(DTypes::FLOAT32))
    return AddArray<Float32>(src, (Float32)coeff, aborted);

  if (src.dtype.isVectorOf(DTypes::FLOAT64))
    return AddArray<Float64>(src, (Float64)coeff, aborted);

  return add(cast(src, DType(src.dtype.ncomponents(), DTypes::FLOAT32), aborted), coeff, aborted);
}

/////////////////////////////////////////////////////////////////////
template <typename CppType>
Array SubArrayAndValue(Array a, CppType b, Aborted& aborted)
{
  int ncomponents = a.dtype.ncomponents();

  Array dst;
  if (!dst.resize(a.dims, a.dtype, __FILE__, __LINE__))
    return Array();
  dst.shareProperties(a);

  CppType* dst_p = (CppType*)dst.c_ptr();
  CppType* src_p = (CppType*)a.c_ptr();
  Int64 tot = a.getTotalNumberOfSamples()*ncomponents;
  for (Int64 I = 0; I < tot; I++)
  {
    if (aborted()) return Array();
    dst_p[I] = (CppType)(src_p[I] - b);
  }
  return dst;
}

Array ArrayUtils::sub(Array src, double coeff, Aborted aborted)
{
  if (!src.valid())
    return Array();

  if (src.dtype.isVectorOf(DTypes::FLOAT32))
    return SubArrayAndValue<Float32>(src, (Float32)coeff, aborted);

  if (src.dtype.isVectorOf(DTypes::FLOAT64))
    return SubArrayAndValue<Float64>(src, (Float64)coeff, aborted);

  return sub(cast(src, DType(src.dtype.ncomponents(), DTypes::FLOAT32), aborted), coeff, aborted);
}


/////////////////////////////////////////////////////////////////////
template <typename CppType>
Array SubNumberAndArray(CppType num, Array src, Aborted& aborted)
{
  int ncomponents = src.dtype.ncomponents();

  Array dst;
  if (!dst.resize(src.dims, src.dtype, __FILE__, __LINE__))
    return Array();
  dst.shareProperties(src);

  CppType* DST = (CppType*)dst.c_ptr();
  CppType* SRC = (CppType*)src.c_ptr();
  for (Int64 I = 0, tot = src.getTotalNumberOfSamples()*ncomponents; I < tot; I++)
  {
    if (aborted()) 
      return Array();

    DST[I] = (CppType)(num - SRC[I]);
  }
  return dst;
}


Array ArrayUtils::sub(double a, Array b, Aborted aborted)
{
  if (!b.valid())
    return Array();

  if (b.dtype.isVectorOf(DTypes::FLOAT32))
    return SubNumberAndArray<Float32>((Float32)a, b, aborted);

  if (b.dtype.isVectorOf(DTypes::FLOAT64))
    return SubNumberAndArray<Float64>((Float64)a, b, aborted);

  return sub(a,cast(b, DType(b.dtype.ncomponents(), DTypes::FLOAT32), aborted), aborted);
}


/////////////////////////////////////////////////////////////////////
template <typename CppType>
Array MulArray(Array src, CppType coeff, Aborted& aborted)
{
  int ncomponents = src.dtype.ncomponents();

  Array dst;
  if (!dst.resize(src.dims, src.dtype, __FILE__, __LINE__))
    return Array();

  dst.shareProperties(src);

  CppType* dst_p = (CppType*)dst.c_ptr();
  CppType* src_p = (CppType*)src.c_ptr();
  Int64 tot = src.getTotalNumberOfSamples()*ncomponents;
  for (Int64 I = 0; I < tot; I++)
  {
    if (aborted()) return Array();
    dst_p[I] = (CppType)(coeff*src_p[I]);
  }
  return dst;
}

Array ArrayUtils::mul(Array src, double coeff, Aborted aborted)
{
  if (!src.valid())
    return Array();

  if (src.dtype.isVectorOf(DTypes::FLOAT32))
    return MulArray<Float32> (src, (Float32)coeff, aborted);

  if (src.dtype.isVectorOf(DTypes::FLOAT64))
    return MulArray<Float64>(src, (Float64)coeff, aborted);

  return mul(cast(src, DType(src.dtype.ncomponents(), DTypes::FLOAT32), aborted), coeff, aborted);
}



/////////////////////////////////////////////////////////////////////
template <typename CppType>
Array DivArray(CppType coeff, Array src, Aborted& aborted)
{
  int ncomponents = src.dtype.ncomponents();

  Array dst;
  if (!dst.resize(src.dims, src.dtype, __FILE__, __LINE__))
    return Array();
  dst.shareProperties(src);

  CppType* dst_p = (CppType*)dst.c_ptr();
  CppType* src_p = (CppType*)src.c_ptr();
  Int64 tot = src.getTotalNumberOfSamples()*ncomponents;
  for (Int64 I = 0; I < tot; I++)
  {
    if (aborted()) return Array();
    dst_p[I] = (CppType)(coeff / src_p[I]);
  }
  return dst;
}

Array ArrayUtils::div(double coeff, Array src, Aborted aborted)
{
  if (src.dtype.isVectorOf(DTypes::FLOAT32))
    return DivArray<Float32>((Float32)coeff, src, aborted);

  if (src.dtype.isVectorOf(DTypes::FLOAT64))
    return DivArray<Float64>((Float64)coeff, src, aborted);

  return div(coeff, cast(src, DType(src.dtype.ncomponents(), DTypes::FLOAT32), aborted), aborted);
}


////////////////////////////////////////////////////////
class ResampleArraySamples 
{
public:

  template <class Sample>
  bool execute(Array& wbuffer, PointNi wdims, Array rbuffer, Aborted& aborted)
  {
    PointNi rdims = rbuffer.dims;
    if (wdims == rdims)
    {
      wbuffer = rbuffer.clone();
      return true;
    }

    if (!rdims.innerProduct() || !wdims.innerProduct())
      return false;

    if (!wbuffer.resize(wdims, rbuffer.dtype, __FILE__, __LINE__))
      return false;

    auto write=GetSamples<Sample>(wbuffer);
    auto read =GetSamples<Sample>(rbuffer);

    int pdim = wdims.getPointDim();
    
    PointNd vs(pdim);
    for (int D=0; D<pdim; D++)
      vs[D] = rdims[D] / (double)wdims[D];

    PointNi w(pdim); Int64 woffset = 0;
    PointNi r(pdim); PointNi          roffset(pdim);
    PointNi rstride = rdims.stride();

    #define ForExpr(D) \
      for (w[D] = 0; w[D] < wdims[D]; w[D] += 1) { \
        r[D] = Utils::clamp(Int64(w[D]* vs[D]),(Int64)0, rdims[D] - 1);  \
        roffset[D] = (D==pdim-1? 0 : roffset[D+1]) + rstride[D]*r[D]; \
    /*--*/

    #define ForBody() \
      write[woffset++]=read[roffset[0]];\
      /*--*/

    switch (pdim) 
    { 
    case 1:                                  if (aborted()) return false;            ForExpr(0) ForBody() }     break;
    case 2:                                  if (aborted()) return false; ForExpr(1) ForExpr(0) ForBody() }}    break;
    case 3:                       ForExpr(2) if (aborted()) return false; ForExpr(1) ForExpr(0) ForBody() }}}   break;
    case 4:            ForExpr(3) ForExpr(2) if (aborted()) return false; ForExpr(1) ForExpr(0) ForBody() }}}}  break;
    case 5: ForExpr(4) ForExpr(3) ForExpr(2) if (aborted()) return false; ForExpr(1) ForExpr(0) ForBody() }}}}} break;
    default: VisusAssert(false); return false;
    }
    #undef ForExpr
    #undef ForBody

    return true;
  }

};

Array ArrayUtils::resample(PointNi target_dims,Array rbuffer, Aborted aborted)
{
  Array wbuffer;
  ResampleArraySamples op;
  return NeedToCopySamples(op,rbuffer.dtype, wbuffer, target_dims, rbuffer, aborted)? wbuffer : Array();
}

///////////////////////////////////////////////////////////////////////////////
struct ComputeSingleComponentRangeOp
{
  template<class CppType>
  static bool execute(Range& range,Array src, int ncomponent, Aborted aborted)
  {
    Int64 tot=src.getTotalNumberOfSamples();
    if (!tot)  
      return false;
    
    range.from = NumericLimits<double>::highest();
    range.to   = NumericLimits<double>::lowest();

    auto samples=GetComponentSamples<CppType>(src,ncomponent);

    for (int I = 0; I < tot; I++)
    {
      if (aborted()) 
        return false;
      
      double value = (double)samples[I];
      if (value < range.from) range.from = value;
      if (value > range.to  ) range.to   = value;
    }

    return true;
  }
};


Range ArrayUtils::computeRange(Array src, int C, Aborted aborted)
{
  Range ret;
  ComputeSingleComponentRangeOp op;
  ExecuteOnCppSamples(op, src.dtype, ret, src, C, aborted);
  return ret;
}

/////////////////////////////////////////////////////////
class ComputeArrayPointOperation
{
public:

  
  Array   src;
  Aborted  aborted;

  //constructor
  ComputeArrayPointOperation(Array src_, Aborted aborted_) :
    src(src_), aborted(aborted_){
  }

  //exec
  template <typename Sample, class FilterClass>
  Array exec(const FilterClass& filter, double m, double M)
  {
    if (aborted())
      return Array();

    Array dst;
    if (!dst.resize(src.dims,src.dtype,__FILE__,__LINE__))
      return Array();

    //for each component...
    for (int C = 0; C < (int)src.dtype.ncomponents(); C++)
    {
      auto write=GetComponentSamples<Sample>(dst,C);
      auto read =GetComponentSamples<Sample>(src,C);

      int offset=0;
      for (auto P=ForEachPoint(src.dims);!P.end();P.next(), offset++)
      {
        if (aborted()) 
          return Array();

        double value = read[offset];
        value = (value- m) / (M - m);
        value = filter.transform(value);
        value = Utils::clamp(value, 0.0, 1.0);
        value = (m + (M - m)*value);
        write[offset] = (Sample) value;
      }
    }

    return dst;
  }

  //exec
  template <class FilterClass>
  Array exec(const FilterClass& filter)
  {
    if (src.dtype.isVectorOf(DTypes::UINT8  )) { return exec<Uint8  , FilterClass>(filter, NumericLimits<Uint8 >::lowest(), NumericLimits<Uint8 >::highest()); }
    if (src.dtype.isVectorOf(DTypes::UINT16 )) { return exec<Uint16 , FilterClass>(filter, NumericLimits<Uint16>::lowest(), NumericLimits<Uint16>::highest()); }
    if (src.dtype.isVectorOf(DTypes::FLOAT32)) { return exec<Float32, FilterClass>(filter, 0, 1); }
    if (src.dtype.isVectorOf(DTypes::FLOAT64)) { return exec<Float64, FilterClass>(filter, 0, 1); }

    //TODO...
    /*
      if (src.dtype.isVectorOf(DTypes::INT8   )) {return exec<Int8   ,FilterClass>(filter);}
      if (src.dtype.isVectorOf(DTypes::INT16  )) {return exec<Int16  ,FilterClass>(filter);}
      if (src.dtype.isVectorOf(DTypes::INT32  )) {return exec<Int32  ,FilterClass>(filter);}
      if (src.dtype.isVectorOf(DTypes::UINT32 )) {return exec<Uint32 ,FilterClass>(filter);}
      if (src.dtype.isVectorOf(DTypes::INT64  )) {return exec<Int64  ,FilterClass>(filter);}
      if (src.dtype.isVectorOf(DTypes::UINT64 )) {return exec<Uint64 ,FilterClass>(filter);}
      */

    VisusAssert(false);
    return Array();
  }

};


////////////////////////////////////////////////////////////////////////////
class BrightnessContrast
{
public:
  double brightness, contrast;
  BrightnessContrast(double brightness_, double contrast_) : brightness(brightness_), contrast(contrast_) {}
  inline double transform(double value) const { return (value - 0.5)*contrast + 0.5 + brightness; }
};

Array ArrayUtils::brightnessContrast(Array src,double brightness, double contrast, Aborted aborted)
{
  BrightnessContrast filter(brightness, contrast);
  return ComputeArrayPointOperation(src, aborted).exec(filter);
}

////////////////////////////////////////////////////////////////////////////////
class Threshold
{
public:
  double level;
  Threshold(double level_) : level(level_) {}
  inline double transform(double value) const { return value > level ? 1 : 0; }
};

Array ArrayUtils::threshold(Array src, double level, Aborted aborted) {
  return ComputeArrayPointOperation(src, aborted).exec(Threshold(level));
}


////////////////////////////////////////////////////////////////////////////////
class Invert
{
public:
  Invert() {}
  inline double transform(double value) const { return 1 - value; }
};

Array ArrayUtils::invert(Array src, Aborted aborted) {
  return ComputeArrayPointOperation(src, aborted).exec(Invert());
}

////////////////////////////////////////////////////////////////////////////////
class Levels
{
public:
  double gamma, in_min, in_max, out_min, out_max;

  //constructor
  Levels(double gamma_, double in_min_, double in_max_, double out_min_, double out_max_)
    : gamma(gamma_), in_min(in_min_), in_max(in_max_), out_min(out_min_), out_max(out_max_) {}

  //transform
  inline double transform(double value) const
  {
    value = (value - in_min) / (in_max - in_min);
    value = pow(value, gamma);
    return value * (out_max - out_min) + out_min;
  }
};

Array ArrayUtils::levels(Array src, double gamma, double in_min, double in_max, double out_min, double out_max, Aborted aborted){
  return ComputeArrayPointOperation(src, aborted).exec(Levels(gamma, in_min, in_max, out_min, out_max));
}

////////////////////////////////////////////////////////////////////////////////
class HueSaturationBrightness
{
public:

  Array src;
  double hue, saturation, brightness;
  Aborted aborted;

  //constructor
  HueSaturationBrightness(Array src_, double hue_, double saturation_, double brightness_, Aborted aborted_)
    : src(src_),hue(hue_), saturation(saturation_), brightness(brightness_) {
  }

  //exec
  template <typename T>
  Array exec()
  {
    Array dst;
    if (!dst.resize(src.dims, src.dtype, __FILE__, __LINE__))
      return Array();

    int N = src.dtype.ncomponents(); VisusAssert(N >= 3);
    T* DST = (T*)dst.c_ptr();
    T* SRC = (T*)src.c_ptr();

    for (auto P = ForEachPoint(src.dims); !P.end(); P.next())
    {
      if (aborted())
        return Array();

      Color rgb(SRC[0], SRC[1], SRC[2]);
      Color hsb = rgb.toHSB();

      int h = (int)(hsb.getHue()*255.0 + hue*255.0);
      h = h % 256;
      hsb.setHue(h / 255.0f);

      double s = hsb.getSaturation();
      s += saturation;
      hsb.setSaturation((float)s);

      double b = hsb.getBrightness();
      b += brightness;
      hsb.setBrightness((float)b);

      rgb = hsb.toRGB();
      DST[0] = (T)(255 * rgb.getRed());
      DST[1] = (T)(255 * rgb.getGreen());
      DST[2] = (T)(255 * rgb.getBlue());

      if (N == 4)
        DST[3] = SRC[3];

      DST += N;
      SRC += N;
    }

    return dst;
  }


  //exec
  Array exec()
  {
    if (aborted())
      return Array();

    //see http://sourceforge.net/p/freeimage/discussion/36111/thread/7830283c/
    if (src.dtype == DTypes::UINT8_RGB || src.dtype == DTypes::UINT8_RGBA)
      return exec<Uint8>();

    if (src.dtype == DTypes::UINT16_RGB || src.dtype == DTypes::UINT16_RGBA)
      return exec<Uint16>();

    VisusAssert(false);
    return Array();
  }

};

Array ArrayUtils::hueSaturationBrightness(Array src, double hue, double saturation, double brightness, Aborted aborted)
{
  return HueSaturationBrightness(src, hue, saturation, brightness, aborted).exec();
}


////////////////////////////////////////////////////////
class WarpPerspective
{
public:

  template <class Sample>
  bool execute(Array& dst,Matrix T,Array src,Aborted& aborted)
  {
    //not compatible
    if (dst.dtype != src.dtype) {
      VisusAssert(false);
      return false;
    }

    if (T.isIdentity() && src.dims==dst.dims)
    {
      dst = src;
      return true;
    }

    auto Ti=T.invert();

    auto wdims = dst.dims; 
    auto rdims = src.dims; 

    VisusReleaseAssert(dst.alpha && dst.alpha->dtype == DTypes::UINT8);
    VisusReleaseAssert(src.alpha && src.alpha->dtype == DTypes::UINT8);

    auto write = GetSamples<Sample>(dst); auto write_alpha = GetSamples<Uint8>(*dst.alpha);
    auto read  = GetSamples<Sample>(src); auto read_alpha  = GetSamples<Uint8>(*src.alpha);

    VisusAssert(dst.getPointDim() == src.getPointDim());
    auto pdim = dst.getPointDim();

    auto wstride = wdims.stride();
    auto rstride = rdims.stride();

    int wfrom = 0;

    if (pdim == 2)
    {
      Int64 rfrom;
      double py[3], px[3];
      Int64 X, Y;

      for (Y = 0; Y < wdims[1]; Y++)
      {
        if (aborted())
          return false;

        py[0] = Ti[1] * Y + Ti[2];
        py[1] = Ti[4] * Y + Ti[5];
        py[2] = Ti[7] * Y + Ti[8];

        for (X = 0; X < wdims[0]; X++, wfrom++)
        {
          px[0] = Ti[0] * X + py[0];
          px[1] = Ti[3] * X + py[1];
          px[2] = Ti[6] * X + py[2];

          px[0] /= px[2];
          px[1] /= px[2];

          if (px[0] >= 0 && px[0] < rdims[0] && px[1] >= 0 && px[1] < rdims[1])
          {
            rfrom = Int64(px[0]) * rstride[0] + Int64(px[1]) * rstride[1];
            write      [wfrom] = read      [rfrom];
            write_alpha[wfrom] = read_alpha[rfrom];
          }
        }
      }
    }
    else if (pdim == 3)
    {
      double px[4], py[4], pz[4];
      Int64 X,Y,Z,rfrom;
      for (Z = 0; Z < wdims[2]; Z++)
      {
        pz[0] = Ti[ 2] * Z + Ti[ 3];
        pz[1] = Ti[ 6] * Z + Ti[ 7];
        pz[2] = Ti[10] * Z + Ti[11];
        pz[3] = Ti[14] * Z + Ti[15];

        for (Y = 0; Y < wdims[1]; Y++)
        {
          if (aborted())
            return false;

          py[0] = Ti[ 1] * Y + pz[0];
          py[1] = Ti[ 5] * Y + pz[1];
          py[2] = Ti[ 9] * Y + pz[2];
          py[3] = Ti[13] * Y + pz[3];

          for (X = 0; X < wdims[0]; X++, wfrom++)
          {
            px[0] = Ti[ 0] * X + py[0];
            px[1] = Ti[ 4] * X + py[1];
            px[2] = Ti[ 8] * X + py[2];
            px[3] = Ti[12] * X + py[3];

            px[0] /= px[3];
            px[1] /= px[3];
            px[2] /= px[3];

            if (
              px[0] >= 0 && px[0] < rdims[0] &&
              px[1] >= 0 && px[1] < rdims[1] &&
              px[2] >= 0 && px[2] < rdims[2])
            {
              rfrom = Int64(px[0]) * rstride[0] + Int64(px[1]) * rstride[1] + Int64(px[2]) * rstride[2];
              write      [wfrom] = read      [rfrom];
              write_alpha[wfrom] = read_alpha[rfrom];
            }
          }
        }
      }
    }
    else
    {
      for (auto P = ForEachPoint(wdims); !P.end(); P.next(), ++wfrom)
      {
        if (aborted()) 
          return false;

        auto S = Ti* P.pos;

        VisusAssert(false);//todo...

        S[0] = (int)S[0];
        S[1] = (int)S[1];
        S[2] = (int)S[2];

        if (
          S[0] >= 0 && S[0] < rdims[0] &&
          S[1] >= 0 && S[1] < rdims[1] &&
          S[2] >= 0 && S[2] < rdims[2])
        {
          auto rfrom = int(S[0])*rstride[0] + int(S[1])*rstride[1] + int(S[2])*rstride[2];
          write      [wfrom] = read      [rfrom];
          write_alpha[wfrom] = read_alpha[rfrom];
        }
      }
    }

    dst.layout   =            src.layout; //row major
    dst.bounds   = Position(T,src.bounds); 
    dst.clipping = Position(T,src.clipping);

    return true;
  }

};

bool ArrayUtils::warpPerspective(Array& dst, Matrix T,Array src,Aborted aborted)
{
  WarpPerspective op;
  return NeedToCopySamples(op,src.dtype,dst, T,src, aborted);
}


////////////////////////////////////////////////////////////////////////
class BlendBuffers::Pimpl
{
public:
  Type          type;
  Aborted       aborted;

  //for average
  Array         num, den; 

  //for voronoi
  Array         best_distance;

  //execute
  template <class CppType>
  bool execute(Type type, Array& dst, Array src, Aborted aborted)
  {
    if (!src.valid()) {
      VisusAssert(false);
      return false;
    }

    //TODO: this is just to simplify the code
    if (!src.alpha)
    {
      src.alpha = std::make_shared<Array>(src.dims, DTypes::UINT8);
      src.alpha->fillWithValue(255);
    }
    VisusReleaseAssert(src.alpha->dtype == DTypes::UINT8);

    //first argument
    if (!dst.valid())
    {
      auto dims = src.dims;

      if (!dst.resize(dims, src.dtype, __FILE__, __LINE__))
        return false;

      dst.fillWithValue(0);
      dst.shareProperties(src);

      dst.alpha = std::make_shared<Array>(dims, DTypes::UINT8);
      dst.alpha->fillWithValue(0);
    }

    //just a preview
    if (!dst.getTotalNumberOfSamples())
      return true;

    auto dims   = dst.dims;
    auto pdim   = dims.getPointDim(); 
    VisusReleaseAssert(pdim <= 3); //todo other cases
    dims.setPointDim(3,1);

    auto width  = dims[0];
    auto height = dims[1];
    auto depth  = dims[2];
    auto ncomponents = dst.dtype.ncomponents();

    #define isEmptyLine() (!SRC_ALPHA[SampleId]  && (width == 1 || memcmp(&SRC_ALPHA[SampleId], &SRC_ALPHA[SampleId + 1], width - 1)==0))

    if (type == GenericBlend)
    {
      for (int C = 0; C < ncomponents; C++)
      {
        Int64 SampleId = 0;
        GetComponentSamples<CppType> DST(dst, C); GetSamples<Uint8> DST_ALPHA(*dst.alpha);
        GetComponentSamples<CppType> SRC(src, C); GetSamples<Uint8> SRC_ALPHA(*src.alpha);

        for (int Z = 0; Z < depth; Z++)
        {
          for (int Y = 0; Y < height; Y++)
          {
            if (aborted())
              return false;

            if (isEmptyLine())
            {
              SampleId += width;
              continue;
            }

            for (int X = 0; X < width; X++, ++SampleId)
            {
              if (SRC_ALPHA[SampleId])
              {
                auto alpha = SRC_ALPHA[SampleId] / 255.0;
                DST[SampleId] += (CppType)(alpha*SRC[SampleId]);
                DST_ALPHA[SampleId] = 255;
              }
            }
          }
        }
      }
      return true;
    }

    if (type == NoBlend)
    {
      for (int C = 0; C < ncomponents; C++)
      {
        Int64 SampleId = 0;
        GetComponentSamples<CppType> DST(dst, C); GetSamples<Uint8> DST_ALPHA(*dst.alpha);
        GetComponentSamples<CppType> SRC(src, C); GetSamples<Uint8> SRC_ALPHA(*src.alpha);

        for (int Z = 0; Z < depth; Z++)
        {
          for (int Y = 0; Y < height; Y++)
          {
            if (aborted())
              return false;

            if (isEmptyLine())
            {
              SampleId += width;
              continue;
            }

            for (int X = 0; X < width; X++, ++SampleId)
            {
              if (SRC_ALPHA[SampleId])
              {
                DST[SampleId] = SRC[SampleId];
                DST_ALPHA[SampleId] = 255;
              }
            }
          }
        }
      }
      return true;
    }

    if (type == AverageBlend)
    {
      if (!num.valid())
      {
        if (!num.resize(dims, DType(ncomponents , DTypes::FLOAT64), __FILE__, __LINE__))
          return false;

        if (!den.resize(dims, DType(ncomponents , DTypes::FLOAT64), __FILE__, __LINE__))
          return false;

        num.fillWithValue(0);
        den.fillWithValue(0);
      }

      for (int C = 0; C < ncomponents; C++)
      {
        Int64 SampleId = 0;
        GetComponentSamples<CppType> DST(dst, C); GetSamples<Uint8> DST_ALPHA(*dst.alpha);
        GetComponentSamples<CppType> SRC(src, C); GetSamples<Uint8> SRC_ALPHA(*src.alpha);
        GetComponentSamples<Float64> NUM(num, C);
        GetComponentSamples<Float64> DEN(den, C);

        for (int Z = 0; Z < depth; Z++)
        {
          for (int Y = 0; Y < height; Y++)
          {
            if (aborted())
              return false;

            if (isEmptyLine())
            {
              SampleId += width;
              continue;
            }

            for (int X = 0; X < width; X++, ++SampleId)
            {
              if (SRC_ALPHA[SampleId])
              {
                double alpha = SRC_ALPHA[SampleId] / 255.0;
                NUM[SampleId] += alpha * SRC[SampleId];
                DEN[SampleId] += alpha;
                DST[SampleId] = (CppType)(NUM[SampleId] / DEN[SampleId]);
                DST_ALPHA[SampleId] = 255;
              }
            }
          }
        }
      }

      return true;
    }

    if (type == VororoiBlend)
    { 
      if (!best_distance.valid())
      {
        if (!best_distance.resize(src.dims, DType(ncomponents, DTypes::FLOAT64), __FILE__, __LINE__))
          return false;

        for (int C = 0; C < ncomponents; C++)
        {
          GetComponentSamples<Float64> DST(best_distance, C);
          for (Int64 I = 0, Tot = dims.innerProduct(); I < Tot; I++)
            DST[I] = NumericLimits<double>::highest();
        }
      }

      VisusReleaseAssert(src.run_time_attributes.hasValue("PIXEL_TO_LOGIC"));
      VisusReleaseAssert(src.run_time_attributes.hasValue("LOGIC_CENTROID"));

      auto pixel_to_logic = Matrix::fromString(src.run_time_attributes.getValue("PIXEL_TO_LOGIC"));
      auto logic_centroid = PointNd::fromString(src.run_time_attributes.getValue("LOGIC_CENTROID"));

      VisusReleaseAssert(logic_centroid.getPointDim() == (pdim  ));
      VisusReleaseAssert(pixel_to_logic.getSpaceDim() == (pdim+1));

      auto T = pixel_to_logic;

      if (pdim == 2)
      {
        for (int C = 0; C < ncomponents; C++)
        {
          int SampleId = 0;
          GetComponentSamples<CppType> DST(dst, C); 
          GetComponentSamples<CppType> SRC(src, C); 
          GetSamples<Uint8> DST_ALPHA(*dst.alpha);
          GetSamples<Uint8> SRC_ALPHA(*src.alpha);
          GetComponentSamples<Float64> BEST_DISTANCE(best_distance, C);

          Int64 X, Y;
          double py[3], px[3], distance;

          for (Y = 0; Y < height; Y++)
          {
            if (aborted())
              return false;

            py[0] = T[1] * Y + T[2];
            py[1] = T[4] * Y + T[5];
            py[2] = T[7] * Y + T[8];

            for (X = 0; X < width; X++, ++SampleId)
            {
              if (SRC_ALPHA[SampleId])
              {
                //(T * Point3d(X, Y) - logic_centroid).module2();
                px[0] = T[0] * X + py[0];
                px[1] = T[3] * X + py[1];
                px[2] = T[6] * X + py[2];

                px[0] /= px[2];
                px[1] /= px[2];

                px[0] -= logic_centroid[0];
                px[1] -= logic_centroid[1];

                distance = px[0] * px[0] + px[1] * px[1];
                if (distance < BEST_DISTANCE[SampleId])
                {
                  BEST_DISTANCE[SampleId] = distance;
                  DST[SampleId] = SRC[SampleId];
                  DST_ALPHA[SampleId] = 255;
                }
              }
            }
          }
        }
      }
      else if (pdim==3)
      {
        for (int C = 0; C < ncomponents; C++)
        {
          Int64 SampleId = 0;
          GetComponentSamples<CppType> DST(dst, C); GetSamples<Uint8> DST_ALPHA(*dst.alpha);
          GetComponentSamples<CppType> SRC(src, C); GetSamples<Uint8> SRC_ALPHA(*src.alpha);
          GetComponentSamples<Float64> BEST_DISTANCE(best_distance, C);

          Int64 X, Y, Z;
          double pz[4], py[4], px[4], distance;

          for (Z = 0; Z < depth; Z++)
          {
            pz[0] = T[ 2] * Z + T[ 3];
            pz[1] = T[ 6] * Z + T[ 7];
            pz[2] = T[10] * Z + T[11];
            pz[3] = T[14] * Z + T[15];

            for (Y = 0; Y < height; Y++)
            {
              if (aborted())
                return false;

              if (isEmptyLine())
              {
                SampleId += width;
                continue;
              }

              py[0] = T[ 1] * Y + pz[0];
              py[1] = T[ 5] * Y + pz[1];
              py[2] = T[ 9] * Y + pz[2];
              py[3] = T[13] * Y + pz[3];

              for (X = 0; X < width; X++, ++SampleId)
              {
                if (SRC_ALPHA[SampleId])
                {
                  //(T * Point3d(X, Y, Z) - logic_centroid).module2();
                  px[0] = T[ 0] * X + py[0];
                  px[1] = T[ 4] * X + py[1];
                  px[2] = T[ 8] * X + py[2];
                  px[3] = T[12] * X + py[3];

                  px[0] /= px[3]; 
                  px[1] /= px[3]; 
                  px[2] /= px[3]; 

                  px[0] -= logic_centroid[0];
                  px[1] -= logic_centroid[1];
                  px[2] -= logic_centroid[2];

                  distance = px[0] * px[0] + px[1] * px[1] + px[2] * px[2];
                  if (distance < BEST_DISTANCE[SampleId])
                  {
                    BEST_DISTANCE[SampleId] = distance;
                    DST[SampleId] = SRC[SampleId];
                    DST_ALPHA[SampleId] = 255;
                  }
                }
              }
            }
          }
        }
      }
      else
      {
       ThrowException("internal error");
      }

      return true;
    }

    VisusAssert(false);
    return false;
  }
};


/////////////////////////////////////////////////////
BlendBuffers::BlendBuffers(Type type_, Aborted aborted_) : type(type_),aborted(aborted_) {
  pimpl = new Pimpl();
}

BlendBuffers::~BlendBuffers() {
  delete pimpl;
}

void BlendBuffers::addBlendArg(Array src) {
  ++nargs;
  ExecuteOnCppSamples(*pimpl, src.dtype, type, result,src, aborted);
}



/////////////////////////////////////////////////////////////////////////////////////////////
void ArrayUtils::setBufferColor(Array& buffer, Color color)
{
  VisusAssert(buffer.dtype.isVectorOf(DTypes::UINT8));

  int   nbytes = buffer.dtype.getByteSize();
  auto  rgb = buffer.c_ptr();

  if (!buffer.alpha)
  {
    Int64 offset = 0;
    for (auto P = ForEachPoint(buffer.dims); !P.end(); P.next(), offset++, rgb += nbytes)
    {
      if (nbytes >= 1) rgb[0] = int(255 * color.getRed());
      if (nbytes >= 2) rgb[1] = int(255 * color.getGreen());
      if (nbytes >= 3) rgb[2] = int(255 * color.getBlue());
    }
  }
  else
  {
    auto read_alpha = GetSamples<Uint8>(*buffer.alpha);

    Int64 offset = 0;
    for (auto P = ForEachPoint(buffer.dims); !P.end(); P.next(), offset++, rgb += nbytes)
    {
      if (read_alpha[offset])
      {
        if (nbytes >= 1) rgb[0] = int(255 * color.getRed());
        if (nbytes >= 2) rgb[1] = int(255 * color.getGreen());
        if (nbytes >= 3) rgb[2] = int(255 * color.getBlue());
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////
class ExecuteOperation
{
public:


  //________________________________________________________________
  template <typename Type>
  class ArrayIterator
  {
  public:

    //constructor
    ArrayIterator(Array array,int C=0)
    {
      Int64 bits_offset=array.dtype.getBitsOffset(C);
      this->dtype=array.dtype.get(C); 
      this->ptr=array.c_ptr()+(bits_offset>>3);
      this->stride=array.dtype.getByteSize();
    }

    //operator*
    inline const Type& operator*() const
    {return *((const Type*)ptr);}

    //operator*
    inline Type& operator*() 
    {return *((Type*)ptr);}

    //operator++
    inline void operator++()
    {ptr+=stride;}

    //operator++
    inline void operator++(int)
    {ptr+=stride;}

  private:

    DType     dtype;
    Uint8*    ptr;
    int       stride;

  };

  //________________________________________________________________
  template <typename Type>
  class ArrayMultiIterator
  {
  public:

    //constructor
    ArrayMultiIterator() : niterators(0)
    {}

    //push_back
    inline void push_back(ArrayIterator<Type> it)
    {
      iterators.push_back(it);
      ++niterators;
    }

    //size
    inline int size() const
    {return (int)iterators.size();}

    //operator[]
    inline ArrayIterator<Type>& operator[](const int& index)
    {return iterators[index];}

    //operator++
    inline void operator++(int)
    {for (int I=0;I<niterators;I++) iterators[I]++;}

    //operator++
    inline void operator++()
    {for (int I=0;I<niterators;I++) ++iterators[I];}

  private:

    int                                niterators;
    std::vector< ArrayIterator<Type> > iterators;

  };

  //________________________________________________________________
  template <typename Type>
  class AddOperation 
  {
  public:

    int nargs;

    //constructor
    AddOperation(int nargs_) : nargs(nargs_)
    {}

    //compute
    inline void compute(ArrayIterator<Type>& dst,ArrayMultiIterator<Type>& args)
    {
      double add=(double)0.0;
      for (int I=0;I<nargs;I++) add+=(double)(*args[I]);
      (*dst)=(Type)add;
    }
  };

  //________________________________________________________________
  template <typename Type>
  class SubOperation 
  {
  public:

    int nargs;

    //constructor
    SubOperation(int nargs_) : nargs(nargs_)
    {}

    //compute
    inline void compute(ArrayIterator<Type>& dst,ArrayMultiIterator<Type>& args)
    {
      double sub=(double)(*args[0]);
      for (int I=1;I<nargs;I++) sub-=(double)(*args[I]);
      (*dst)=((Type)sub);
    }
  };

  //________________________________________________________________
  template <typename Type>
  class MulOperation 
  {
  public:

    int nargs;

    //constructor
    MulOperation(int nargs_) : nargs(nargs_)
    {}

    //compute
    inline void compute(ArrayIterator<Type>& dst,ArrayMultiIterator<Type>& args)
    {
      double mul=(double)1.0;
      for (int I=0;I<nargs;I++) mul*=(double)(*args[I]);
      (*dst)=(Type)mul;
    }
  };

  //________________________________________________________________
  template <typename Type>
  class DivOperation 
  {
  public:

    int nargs;

    //constructor
    DivOperation(int nargs_) : nargs(nargs_)
    {}

    //compute
    inline void compute(ArrayIterator<Type>& dst,ArrayMultiIterator<Type>& args)
    {
      double num=(double)*args[0];
      double den=(double)1.0;
      for (int I=1;I<nargs;I++) den*=(double)(*args[I]);
      (*dst)=(Type)(num/den);
    }
  };

  //________________________________________________________________
  template <typename Type>
  class MinOperation 
  {
  public:

    int nargs;

    //constructor
    MinOperation(int nargs_) : nargs(nargs_)
    {}

    //compute
    inline void compute(ArrayIterator<Type>& dst,ArrayMultiIterator<Type>& args)
    {
      Type min=*args[0];
      for (int I=1;I<nargs;I++) min=std::min(min,*args[I]);
      (*dst)=(Type)min;
    }
  };

  //________________________________________________________________
  template <typename Type>
  class MaxOperaration 
  {
  public:

    int nargs;

    //constructor
    MaxOperaration(int nargs_) : nargs(nargs_)
    {}

    //compute
    inline void compute(ArrayIterator<Type>& dst,ArrayMultiIterator<Type>& args)
    {
      Type max=*args[0];
      for (int I=1;I<nargs;I++) max=std::max(max,*args[I]);
      (*dst)=(Type)max;
    }
  };

  //________________________________________________________________
  template <typename Type>
  class AverageOperation 
  {
  public:

    int nargs;

    //constructor
    AverageOperation(int nargs_) : nargs(nargs_)
    {}

    //compute
    inline void compute(ArrayIterator<Type>& dst,ArrayMultiIterator<Type>& args)
    {
      double avg=0.0;
      for (int I=0;I<nargs;I++) avg+=(double)(*args[I]);
      avg/=nargs;
      (*dst)=(Type)avg;
    }
  };

  //________________________________________________________________
  template <typename Type>
  class StandardDeviationOperation 
  {
  public:

    int nargs;

    //constructor
    StandardDeviationOperation(int nargs_) : nargs(nargs_)
    {}

    //compute
    inline void compute(ArrayIterator<Type>& dst,ArrayMultiIterator<Type>& args)
    {
      double avg=0.0;
      for (int I=0;I<nargs;I++) avg+=(double)(*args[I]);
      avg/=nargs;

      double sdv=0.0;
      for (int I=0;I<nargs;I++) 
        sdv+=(((double)(*args[I]))-avg) * (((double)(*args[I]))-avg);
      sdv=sqrt(sdv/nargs);

      (*dst)=(Type)sdv;
    }
  };

  //________________________________________________________________
  template <typename Type>
  class MedianOperation 
  {
  public:

    int               nargs;
    std::vector<Type> ordered;
    int               middle;

    //constructor
    MedianOperation(int nargs_) : nargs(nargs_)
    {
      ordered.resize(nargs);
      middle=nargs/2;
    }

    //compute
    inline void compute(ArrayIterator<Type>& dst,ArrayMultiIterator<Type>& args)
    {
      for (int I=0;I<nargs;I++) ordered[I]=*args[I];
      std::sort(ordered.begin(),ordered.end());

      (*dst) = ((nargs & 1)==0)?
        (Type)((double)ordered[middle-1]+(double)ordered[middle]/2.0) : //even
        (Type)ordered[middle]; //odd
    }
  };


  ArrayUtils::Operation    op;
  Array&                   dst;
  std::vector<Array>       args;
  Aborted                  aborted;

  //constructor
  ExecuteOperation(ArrayUtils::Operation op_,Array& dst_,std::vector<Array> args_,Aborted aborted_) 
    : op(op_),dst(dst_),args(args_),aborted(aborted_)
  {
    //remove null arguments
    int N=0;
    for (int I=0;I<(int)args.size();I++)
    {
      if (args[I].valid())
        args[N++]=args[I];
    }
    args.resize(N);
  }

  //guessDType
  DType guessDType()
  {
    int N=(int)args.size();

    //check that args dtype are "compatible)
    bool all_unsigned=true;
    int ncomponents=args[0].dtype.ncomponents();
    for (int I=0;I<N;I++)
    {
      DType dtype=args[I].dtype;

      //supported only atomic or vector dtypes
      if (!dtype.valid() || dtype.get(0).ncomponents()!=1 || args[I].dtype.ncomponents()!=ncomponents)
      {
        VisusAssert(aborted());
        return DType(); //invalid
      }
      all_unsigned=all_unsigned && dtype.isUnsigned();
    }

    for (int I=0;I<N;I++) {if (args[I].dtype.isVectorOf(DTypes::FLOAT64)) return args[I].dtype;}
    for (int I=0;I<N;I++) {if (args[I].dtype.isVectorOf(DTypes::FLOAT32)) return args[I].dtype;}
    for (int I=0;I<N;I++) {if (args[I].dtype.isVectorOf(DTypes::INT64)  || args[I].dtype.isVectorOf(DTypes::UINT64)) return DType(ncomponents,all_unsigned? DTypes::UINT64 : DTypes::INT64);}
    for (int I=0;I<N;I++) {if (args[I].dtype.isVectorOf(DTypes::INT32)  || args[I].dtype.isVectorOf(DTypes::UINT32)) return DType(ncomponents,all_unsigned? DTypes::UINT32 : DTypes::INT32);}
    for (int I=0;I<N;I++) {if (args[I].dtype.isVectorOf(DTypes::INT16)  || args[I].dtype.isVectorOf(DTypes::UINT16)) return DType(ncomponents,all_unsigned? DTypes::UINT16 : DTypes::INT16);}
    for (int I=0;I<N;I++) {if (args[I].dtype.isVectorOf(DTypes::INT8 )  || args[I].dtype.isVectorOf(DTypes::UINT8 )) return DType(ncomponents,all_unsigned? DTypes::UINT8  : DTypes::INT8 );}
    VisusAssert(aborted());
    return DType(); //invalid
  }


  //computeOperation
  template <class OperationClass, typename Type>
  bool computeOperation(ArrayIterator<Type> dst,ArrayMultiIterator<Type> args)
  {
    Int64 tot=this->dst.getTotalNumberOfSamples();
    OperationClass op((int)args.size());
    for (Int64 I=0;I<tot;I++,++dst,++args)
    {
      if (aborted()) return false;
      op.compute(dst,args);
    }
    return true;
  }

  //assignOperation
  template <typename Type>
  bool assignOperation(ArrayIterator<Type> dst,ArrayMultiIterator<Type> args)
  {
    switch (op)
    {
      case ArrayUtils::AddOperation               : return computeOperation< AddOperation                <Type> , Type >(dst,args);
      case ArrayUtils::SubOperation               : return computeOperation< SubOperation                <Type> , Type >(dst,args);
      case ArrayUtils::MulOperation               : return computeOperation< MulOperation                <Type> , Type >(dst,args);
      case ArrayUtils::DivOperation               : return computeOperation< DivOperation                <Type> , Type >(dst,args);
      case ArrayUtils::MinOperation               : return computeOperation< MinOperation                <Type> , Type >(dst,args);
      case ArrayUtils::MaxOperation               : return computeOperation< MaxOperaration              <Type> , Type >(dst,args);
      case ArrayUtils::AverageOperation           : return computeOperation< AverageOperation            <Type> , Type >(dst,args);
      case ArrayUtils::StandardDeviationOperation : return computeOperation< StandardDeviationOperation  <Type> , Type >(dst,args);
      case ArrayUtils::MedianOperation            : return computeOperation< MedianOperation             <Type> , Type >(dst,args);
      default: break;
    }
    VisusAssert(aborted());
    return false;
  }

  //assignIterators
  template <typename Type>
  bool assignIterators(int C)
  {
    int N=(int)args.size();
    ArrayIterator<Type> dst(this->dst,C);
    ArrayMultiIterator<Type> args;
    for (int I=0;I<N;I++)
    {
      VisusAssert(C>=0 && C<this->args[I].dtype.ncomponents());
      args.push_back(ArrayIterator<Type>(this->args[I],C));
    }
    return assignOperation(dst,args);
  }

  //forEachComponent
  template <typename Type>
  bool forEachComponent()
  {
    int ncomponents=args[0].dtype.ncomponents();
    for (int C=0;C<ncomponents;C++)
    {
      if (aborted())
        return false;

      if (!assignIterators<Type>(C))
        return false;
    }
    return true;
  }

  //resample
  template <typename Type>
  bool resample()
  {
    int N=(int)args.size();

    //find the biggest arg
    auto biggest=args[0];
    for (int I=0;I<(int)args.size();I++)
    {
      if (args[I].getTotalNumberOfSamples() > biggest.getTotalNumberOfSamples())
        biggest=args[I];
    }

    if (!dst.resize(biggest.dims,dst.dtype,__FILE__,__LINE__))
      return false;

    dst.shareProperties(biggest);

    for (int I=0;I<(int)args.size();I++)
    {
      if (aborted()) return false;
      if (args[I].dims!=biggest.dims)
      {
        args[I]=ArrayUtils::resample(biggest.dims,args[I],aborted);
        if (!args[I].valid())
        {
          VisusAssert(aborted());
          return false;
        }
      }
    }
    return forEachComponent<Type>();
  }


  //cast
  bool cast(DType dtype)
  {
    int pdim = args.front().getPointDim();

    if (!dst.resize(PointNi(pdim),dtype,__FILE__,__LINE__))
      return false;

    for (int I=0;I<(int)args.size();I++)
    {
      if (aborted())
        return false;

      if (args[I].valid() && args[I].dtype!=dtype)
      {
        args[I]=ArrayUtils::cast(args[I],dtype,aborted);
        if (!args[I].valid()) {
          VisusAssert(aborted());
          return false;
        }
      }
    }

    if (dtype.isVectorOf(DTypes::INT8   )) return resample<Int8   >();
    if (dtype.isVectorOf(DTypes::UINT8  )) return resample<Uint8  >();
    if (dtype.isVectorOf(DTypes::INT16  )) return resample<Int16  >();
    if (dtype.isVectorOf(DTypes::UINT16 )) return resample<Uint16 >();
    if (dtype.isVectorOf(DTypes::INT32  )) return resample<Int32  >();
    if (dtype.isVectorOf(DTypes::UINT32 )) return resample<Uint32 >();
    if (dtype.isVectorOf(DTypes::INT64  )) return resample<Int64  >();
    if (dtype.isVectorOf(DTypes::UINT64 )) return resample<Uint64 >();
    if (dtype.isVectorOf(DTypes::FLOAT32)) return resample<Float32>();
    if (dtype.isVectorOf(DTypes::FLOAT64)) return resample<Float64>();
    VisusAssert(aborted());
    return false;
  }

  //execute
  bool execute()
  {
    int N=(int)args.size();
    if (!N) 
      return false;

    DType dtype=guessDType();
    if (!dtype.valid())
      return false;

    return cast(dtype);
  }

};

Array ArrayUtils::executeOperation(Operation op,std::vector<Array> args,Aborted aborted)
{
  Array dst;
  return ExecuteOperation(op,dst,args,aborted).execute()? dst : Array();
}

///////////////////////////////////////////////////////////////////////////////
struct ConvolveOp
{
  template<typename SrcType>
  bool execute(Array& dst,Array src,Array& kernel,Aborted aborted)
  {
    //necessary conditions
    if (!src.dtype.valid() ||
        !kernel.getTotalNumberOfSamples() ||
        !kernel.dtype.valid() ||
        kernel.dtype.ncomponents()!=1)
    {
      VisusAssert(aborted());
      return false;
    }

    if (!dst.resize(src.dims,DType(src.dtype.ncomponents(),DTypes::FLOAT64),__FILE__,__LINE__))
      return false;

    dst.shareProperties(src);

    //I'm just interested in a "preview"
    if (!src.getTotalNumberOfSamples())
      return true;

    int pdim = src.getPointDim();

    //dimensions (ignore where dims==1 i.e. where memory layout does not change (for example src has dims (1,200,300,1,1)->(200,300))
    PointNi Sdims=PointNi::one(pdim); int Sspace=0;
    PointNi Kdims=PointNi::one(pdim); int Kspace=0;
    for (int I=0;I<pdim;I++)
    {
      VisusAssert(src   .dims[I]>=1);
      VisusAssert(kernel.dims[I]>=1);

      if (src.dims[I]==1 && kernel.dims[I]==1)
        continue;

      Sdims[Sspace++]=src   .dims[I];
      Kdims[Kspace++]=kernel.dims[I];
    }

    const PointNi Kcenter=Kdims.rightShift(1);

    if (!Kspace || !Sspace || Kspace>Sspace || ((Kcenter.leftShift(1))+PointNi::one(pdim))!=Kdims)
    {
      VisusAssert(aborted());
      return false;
    }

    //what to do with (set|get)SingleComponentRange?

    //for each component...
    int ncomponents=src.dtype.ncomponents();
    for (int C=0;C<ncomponents;C++)
    {
      PointNi        stride=Sdims.stride()*ncomponents;
      const SrcType* src_p=((SrcType*)src.c_ptr())+C;
      Float64*       dst_p=((Float64*)dst.c_ptr())+C;
      const Float64* kernel_begin=(Float64*)kernel.c_ptr();

      PointNi to=Sdims;

      // this is a trick to let the inner most loop executing only once
      to[0]=1; 

      const Int64 num_step_x=(Sdims[0])/1;

      for (auto P = ForEachPoint(to); !P.end(); P.next())
      {
        // we take out the most inner loop for parallelization
        for (Int64 i=0;i<Sdims[0];i++) 
        {
          // copy P so that different threads don't modify the same variable P
          PointNi Q=P.pos; 
          Q[0]=i;
          Float64 sum=0.0;
          const Float64* kernel_p=kernel_begin;
          PointNi Sp(pdim), Kp(pdim), Tp(pdim);
          //#define ForKernel(n) for (Kp[n]=0, Sp[n]=Q[n]-Kcenter[n]; Kp[n]<Kdims[n]; ++Kp[n], ++Sp[n]) { if (Sp[n]<0 || Sp[n]>=Sdims[n]) { int skip=1; int n1=n; while(n1-- != 0) skip *= Kdims[n1]; kernel_p += skip; continue; }
          #define ForKernel(n) for (Kp[n]=0, Sp[n]=Q[n]-Kcenter[n]; Kp[n]<Kdims[n]; ++Kp[n], ++Sp[n]) { Tp[n] = Sp[n] < 0 ? 0 : Sp[n]>=Sdims[n] ? Sdims[n]-1 : Sp[n];
          switch (Kspace)
          {
          case 1:                           ForKernel(0) sum+=(Float64)(src_p[Tp[0]*stride[0]                                ]) * (Float64)(*kernel_p++);}    break;
          case 2:              ForKernel(1) ForKernel(0) sum+=(Float64)(src_p[Tp[0]*stride[0]+Tp[1]*stride[1]                ]) * (Float64)(*kernel_p++);}}   break;
          case 3: ForKernel(2) ForKernel(1) ForKernel(0) sum+=(Float64)(src_p[Tp[0]*stride[0]+Tp[1]*stride[1]+Tp[2]*stride[2]]) * (Float64)(*kernel_p++);}}}  break;
          default:VisusAssert(false);
          }
          #undef ForKernel
          dst_p[stride[0]*Q[0]]=sum;
        }
        dst_p+=stride[0]*num_step_x;
        if (aborted()) return false;
      }
    }//for each component...
    return true;
  }
};

Array ArrayUtils::convolve(Array src,Array kernel,Aborted aborted) {
  Array dst;
  ConvolveOp op;
  return ExecuteOnCppSamples(op,src.dtype,dst,src,kernel,aborted)? dst : Array();
}

///////////////////////////////////////////////////////////////////////////////
// Run a median filter on src and produce dst
struct MedianHybridOp
{
  template<typename SrcType>
  bool execute(Array& dst,Array src,Array krn_size,Aborted aborted)
  {
    if (!src.dtype.valid() || krn_size.getTotalNumberOfSamples()==0)
      return false;

    if (src.getTotalNumberOfSamples()==0)
      return true;

    if (!dst.resize(src.dims,src.dtype,__FILE__,__LINE__))
      return false;
    dst.shareProperties(src);

    int pdim = src.getPointDim();

    PointNi src_dims=PointNi::one(pdim); int src_space = 0;
    PointNi krn_dims=PointNi::one(pdim); int krn_space = 0;
    
    for (int i=0;i<pdim;i++)
    {
      if (src.dims[i]<1)
        return false;

      if (src.dims[i]<=1 && krn_size.dims[i]<=1)
        continue;
      
      src_dims[src_space++]=src.dims[i];
      krn_dims[krn_space++]=krn_size.dims[i];
    }

    if (krn_space==0 || src_space==0 || krn_space>src_space)
      return false;

    // For each component...
    int ncomponents=src.dtype.ncomponents();
    for (int c=0;c<ncomponents;++c)
    {
      const PointNi stride=src_dims.stride()*ncomponents;
      const SrcType* src_ptr=reinterpret_cast<const SrcType*>(src.c_ptr())+c;
      SrcType* dst_ptr=reinterpret_cast<SrcType*>(dst.c_ptr())+c;
      PointNi to=src_dims;


      // this is a trick to let the inner most loop execute only once
      to[0]=1; 

      const Int64 num_step_x=(src_dims[0])/1;

      Int64 krn_max_dim= *krn_dims.max_element();

      // NOTE: in case of parallel processing, move this vector into the loop
      std::vector<SrcType> neighborhood_vals((krn_max_dim*2+1)*(krn_max_dim*2+1)*(krn_max_dim*2+1));
      for(auto p =ForEachPoint(to);!p.end();p.next())
      {
        // we take out the most inner loop for parallelization
        for (Int64 i=0;i<src_dims[0];i++)
        {
          // copy P so that different threads don't modify the same variable P
          PointNi src_center=p.pos; 
          src_center[0]=i;
          PointNi src_point=src_center;
          PointNi krn_point(pdim);
          Int64 j=0; // j will be the number of neighbors at the end
          Int64 face_begin=-1,face_end=face_begin;
          Int64 edge_begin=-1,edge_end=edge_begin;
          Int64 vertex_begin=-1,vertex_end=vertex_begin;

          #define ForKernel(n) \
            for (krn_point[n]=-krn_dims[n],src_point[n]=src_center[n]-krn_dims[n]; krn_point[n]<=krn_dims[n]; ++krn_point[n],++src_point[n]) { \
              if (src_point[n]<0 || src_point[n]>=src_dims[n]) \
                continue;\
          /*-==*/

          switch (krn_space)
          {
          case 1:
              ForKernel(0)
                neighborhood_vals[j++]=src_ptr[src_point[0]*stride[0]];
              }
            break;

          case 2:
            edge_begin = 0, edge_end = 0;
            vertex_begin = edge_end + 4 * krn_max_dim, vertex_end = vertex_begin;
            ForKernel(1)
            ForKernel(0)
              //neighborhood_vals[j++]=src_ptr[src_point[0]*stride[0]+src_point[1]*stride[1]];
              Int64 src_ptr_idx = src_point[0] * stride[0] + src_point[1] * stride[1];
              PointNi d = src_point - src_center;
              d[0] = d[0] >= 0 ? d[0] : -d[0];
              d[1] = d[1] >= 0 ? d[1] : -d[1];

              if (d[0] == 0 && d[1] == 0) // the center, skip
                continue;

              else if (d[0] == 0 || d[1] == 0) // share edge
                neighborhood_vals[edge_end++] = src_ptr[src_ptr_idx];

              else if (d[0] == d[1]) // share vertex
                neighborhood_vals[vertex_end++] = src_ptr[src_ptr_idx];

              ++j;
            }}
            break;
          case 3:

            face_begin = 0, face_end = 0;
            edge_begin = krn_max_dim * 6, edge_end = edge_begin;
            vertex_begin = edge_end + krn_max_dim * 12, vertex_end = vertex_begin;
            ForKernel(2)
            ForKernel(1)
            ForKernel(0)
              Int64 src_ptr_idx = src_point[0] * stride[0] + src_point[1] * stride[1] + src_point[2] * stride[2];
              PointNi d = src_point - src_center;
              d[0] = d[0] >= 0 ? d[0] : -d[0];
              d[1] = d[1] >= 0 ? d[1] : -d[1];
              d[2] = d[2] >= 0 ? d[2] : -d[2];

              if (d[0] == 0 && d[1] == 0 && d[2] == 0)
                continue;
              
              else if ((d[0] == 0 && d[1] == 0) || (d[1] == 0 && d[2] == 0) || (d[0] == 0 && d[2] == 0)) // share face
                neighborhood_vals[face_end++] = src_ptr[src_ptr_idx];

              else if ((d[0] == 0 && d[1] == d[2]) || (d[0] == d[1] && d[2] == 0) || (d[1] == 0 && d[0] == d[2])) // share edge
                neighborhood_vals[edge_end++] = src_ptr[src_ptr_idx];
              
              else if (d[0] == d[1] && d[1] == d[2]) // share vertex
                neighborhood_vals[vertex_end++] = src_ptr[src_ptr_idx];
              
              ++j;
            }}}
            break;

          default:
            Int64 src_ptr_idx=src_point[0]*stride[0]+src_point[1]*stride[1]+src_point[2]*stride[2];
            VisusAssert(false);
          };

          #undef ForKernel
          
          // Compute the median value in the neighborhood
          auto begin=neighborhood_vals.begin();
          if (krn_space==2)
          {
            SrcType center_val=src_ptr[src_center[0]*stride[0]+src_center[1]*stride[1]];
            SrcType medians[3];
            medians[0]=center_val;
            std::nth_element(begin+edge_begin,begin+(edge_end-edge_begin)/2,begin+edge_end);
            medians[1]=neighborhood_vals[(edge_end-edge_begin)/2];
            std::nth_element(begin+vertex_begin,begin+(vertex_end-vertex_begin)/2,begin+vertex_end);
            medians[2]=neighborhood_vals[(vertex_end-vertex_begin)/2];
            std::sort(medians,medians+3);
            dst_ptr[stride[0]*src_center[0]]=medians[1];
          }
          else if (krn_space==3)
          {
            SrcType center_val=src_ptr[src_center[0]*stride[0]+src_center[1]*stride[1]+src_center[2]*stride[2]];
            SrcType medians[4];
            medians[0]=center_val;
            std::nth_element(begin+face_begin,begin+(face_end-face_begin)/2,begin+face_end);
            medians[1]=neighborhood_vals[(face_end-face_begin)/2];
            std::nth_element(begin+edge_begin,begin+(edge_end-edge_begin)/2,begin+edge_end);
            medians[2]=neighborhood_vals[(edge_end-edge_begin)/2];
            std::nth_element(begin+vertex_begin,begin+(vertex_end-vertex_begin)/2,begin+vertex_end);
            medians[3]=neighborhood_vals[(vertex_end-vertex_begin)/2];
            std::sort(medians,medians+4);
            dst_ptr[stride[0]*src_center[0]]=(medians[1]+medians[2])/2;
          }
        }

        dst_ptr+=stride[0]*num_step_x;

        if (aborted())
          return false;
      }
    }//for each component...
    return true;
  }
};

Array ArrayUtils::medianHybrid(Array src,Array krn_size,Aborted aborted) {
  Array dst;
  MedianHybridOp op;
  return ExecuteOnCppSamples(op,src.dtype,dst,src,krn_size,aborted)? dst : Array();
}

///////////////////////////////////////////////////////////////////////////////
// Run a median filter on src and produce dst
struct MedianOp
{
  template<typename SrcType>
  bool execute(Array& dst,Array src,Array krn_size,int percent,Aborted aborted)
  {
    if (percent<0  ) percent=0;
    if (percent>100) percent=100;

    if (!src.dtype.valid() || krn_size.getTotalNumberOfSamples()==0)
      return false;

    if (src.getTotalNumberOfSamples()==0)
      return true;

    if (!dst.resize(src.dims,src.dtype,__FILE__,__LINE__))
      return false;
    dst.shareProperties(src);

    int pdim = src.getPointDim();

    PointNi src_dims=PointNi::one(pdim);
    PointNi krn_dims=PointNi::one(pdim);
    int src_space=0;
    int krn_space=0;
    for (int i=0;i<pdim;i++)
    {
      if (src.dims[i]<1)
        return false;

      if (src.dims[i]<=1 && krn_size.dims[i]<=1)
        continue;

      src_dims[src_space++]=src.dims[i];
      krn_dims[krn_space++]=krn_size.dims[i];
    }

    if (krn_space==0 || src_space==0 || krn_space>src_space)
      return false;

    // For each component...
    int ncomponents=src.dtype.ncomponents();
    for (int c=0;c<ncomponents;++c)
    {
      const PointNi stride=src_dims.stride()*ncomponents;
      const SrcType* src_ptr=reinterpret_cast<const SrcType*>(src.c_ptr())+c;
      SrcType* dst_ptr=reinterpret_cast<SrcType*>(dst.c_ptr())+c;
      PointNi to=src_dims;

      // this is a trick to let the inner most loop execute only once
      to[0]=1; 

      const Int64 num_step_x=(src_dims[0]);
      Int64 size=(*krn_size.dims.max_element())*2+1;
      Int64 neighborhood_size=krn_space==1?size:(krn_space==2?size*size:size*size*size);
      std::vector<SrcType> neighborhood_vals(neighborhood_size);

      for (auto p = ForEachPoint(to); !p.end(); p.next())
      {
        // we take out the most inner loop for parallelization
        for (Int64 i=0;i<src_dims[0];i++)
        {
          // copy P so that different threads don't modify the same variable P
          PointNi src_center=p.pos; 
          src_center[0]=i;
          PointNi src_point=src_center;
          PointNi krn_point(pdim);

          // j will be the number of neighbors at the end
          Int64 j=0; 

          #define ForKernel(n) \
            for (krn_point[n]=-krn_dims[n],src_point[n]=src_center[n]-krn_dims[n]; krn_point[n]<=krn_dims[n]; ++krn_point[n],++src_point[n]) { \
              if (src_point[n]<0 || src_point[n]>=src_dims[n]) \
                continue;\
          /*--*/

          switch (krn_space)
          {
          case 1:
            ForKernel(0)
              neighborhood_vals[j++]=src_ptr[src_point[0]*stride[0]];
            }
            break;
          case 2:
            ForKernel(1) 
            ForKernel(0)
              neighborhood_vals[j++]=src_ptr[src_point[0]*stride[0]+src_point[1]*stride[1]];
            }}
            break;
          case 3:
            ForKernel(2) 
            ForKernel(1) 
            ForKernel(0)
              neighborhood_vals[j++]=src_ptr[src_point[0]*stride[0]+src_point[1]*stride[1]+src_point[2]*stride[2]];
            }}}
            break;
          default:
            VisusAssert(false);
          };

          #undef ForKernel

          // Compute the median value in the neighborhood
          std::nth_element(neighborhood_vals.begin(),neighborhood_vals.begin()+(j*percent)/100,neighborhood_vals.begin()+j);
          dst_ptr[stride[0]*src_center[0]]=neighborhood_vals[(j*percent)/100];
        }
        dst_ptr+=stride[0]*num_step_x;

        if (aborted())
          return false;
      }
    }//for each component...
    return true;
  }
};

Array ArrayUtils::median(Array src,Array krn_size,int percent,Aborted aborted) {
  Array dst;
  MedianOp op;
  return ExecuteOnCppSamples(op,src.dtype,dst,src,krn_size,percent,aborted)? dst : Array();
}


} //namespace Visus

