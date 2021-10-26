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

#ifndef VISUS_ARRAY_H
#define VISUS_ARRAY_H

#include <Visus/Kernel.h>
#include <Visus/Field.h>
#include <Visus/Position.h>
#include <Visus/HeapMemory.h>
#include <Visus/Url.h>
#include <Visus/Color.h>

#include <set>

namespace Visus {

//////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Array 
{
public:

  VISUS_CLASS(Array)

  //dtype
  DType dtype;

  //dimension of the image
  PointNi dims;

  //Url
  String url;

  // layout
  String layout;

  //the bounds of the data
  Position bounds;

  //could be that the data needs some clipping
  Position clipping;

  //internal use only
  SharedPtr<HeapMemory> heap=std::make_shared<HeapMemory>();

  //for midx
  SharedPtr<Array> alpha;

  //texture
  SharedPtr<DynObject> texture;

  //run_time_attributes
  StringMap run_time_attributes;

  //constructor
  Array() {
  }

  //constructor
  Array(PointNi dims, DType dtype, SharedPtr<HeapMemory> heap_ = SharedPtr<HeapMemory>()) 
    : heap(heap_ ? heap_ : std::make_shared<HeapMemory>())
  {
    if (!this->resize(dims, dtype, __FILE__, __LINE__)) 
      ThrowException("resize of array failed, out of memory");
  }

  //constructor
  Array(Int64 x, DType dtype, SharedPtr<HeapMemory> heap = SharedPtr<HeapMemory>())
    : Array(PointNi(std::vector<Int64>({ x })), dtype, heap) {
  }

  //constructor
  Array(Int64 x, Int64 y, DType dtype, SharedPtr<HeapMemory> heap = SharedPtr<HeapMemory>())
    : Array(PointNi(std::vector<Int64>({ x, y })), dtype, heap) {
  }

  //constructor
  Array(Int64 x, Int64 y, Int64 z, DType dtype, SharedPtr<HeapMemory> heap = SharedPtr<HeapMemory>())
    : Array(PointNi(std::vector<Int64>({ x,y,z })), dtype, heap) {
  }

  //constructor
  Array(PointNi dims, DType dtype, String c_address, bool bSharedMem = false)
  {
    Uint8* c_ptr = (Uint8*)cuint64(c_address);

    if (bSharedMem)
      this->heap = HeapMemory::createUnmanaged(c_ptr, dtype.getByteSize(dims));

    if (!this->resize(dims, dtype, __FILE__, __LINE__))
      ThrowException("resize of array failed, out of memory");

    if (!bSharedMem)
      memcpy(this->c_ptr(), c_ptr,dtype.getByteSize(dims));
  }

  //destructor
  virtual ~Array() {
  }

  //clone
  Array clone() const
  {
    Array ret = *this;
    ret.heap = this->heap->clone();
    return ret;
  }
  
  //valid (NOTE an array is valid even if it has zero samples, like in 'preview' mode)
  bool valid() const {
    return dtype.valid();
  }

  //getPointDim
  int getPointDim() const {
    return dims.getPointDim();
  }

  //getWidth
  int getWidth() const {
    return valid() ? (dims.getPointDim() >= 1 ? (int)dims[0] : 1) : 0;
  }

  //getHeight
  int getHeight() const {
    return valid() ? (dims.getPointDim() >= 2 ? (int)dims[1] : 1) : 0;
  }

  //getDepth
  int getDepth() const {
    return valid() ? (dims.getPointDim() >= 3 ? (int)dims[2] : 1) : 0;
  }

  //getTotalNumberOfSamples
  Int64 getTotalNumberOfSamples() const {
    return dims.innerProduct();
  }

  //shareProperties
  void shareProperties(Array other)
  {
    this->layout              = other.layout;
    this->bounds              = other.bounds;
    this->clipping            = other.clipping;
    this->run_time_attributes = other.run_time_attributes;
  }

public:

  //createView
  static Array createView(Array src, PointNi dims, DType dtype, Int64 c_offset = 0)
  {
    Int64  ret_c_size = dtype.getByteSize(dims);
    Uint8* ret_c_ptr = (Uint8*)(src.c_ptr() + c_offset);
    VisusAssert(ret_c_ptr + ret_c_size <= src.c_ptr() + src.c_size());
    return Array(dims, dtype, SharedPtr<HeapMemory>(HeapMemory::createUnmanaged(ret_c_ptr, ret_c_size)));
  }

  //createView
  static Array createView(Array src, Int64 x, DType dtype, Int64 c_offset = 0){
    return createView(src, PointNi(std::vector<Int64>({ x })), dtype, c_offset);
  }

  //createView
  static Array createView(Array src, Int64 x, Int64 y, DType dtype, Int64 c_offset = 0){
    return createView(src, PointNi(std::vector<Int64>({ x,y })), dtype, c_offset);
  }

  //createView
  static Array createView(Array src, Int64 x, Int64 y, Int64 z, DType dtype, Int64 c_offset = 0){
    return createView(src, PointNi(std::vector<Int64>({ x,y,z })), dtype, c_offset);
  }

  //isAllZero
  inline bool hasConstantValue(Uint8 value) const {
    return heap->hasConstantValue(value);
  }

  //isAllZero
  inline bool isAllZero() const{
    return heap->isAllZero();
  }

  //c_capacity
  inline Int64 c_capacity() const{
    return heap->c_capacity();
  }

  //c_size
  inline Int64 c_size() const{
    return heap->c_size();
  }

  //c_ptr
  inline unsigned char* c_ptr(){
    return heap->c_ptr();
  }

  //c_address
  inline String c_address() const {
    return cstring((Uint64)c_ptr());
  }

  //c_ptr
  template <typename Type>
  inline Type c_ptr() {
    return heap->c_ptr<Type>();
  }

  //c_ptr
#if !SWIG
  inline const unsigned char* c_ptr() const{
    return heap->c_ptr();
  }
#endif

  //shrink
  inline bool shrink(){
    return heap->shrink();
  }

  //fillWithValue
  inline void fillWithValue(int value) {
    memset(c_ptr(), value, (size_t)c_size());
  }

  //resize
  bool resize(PointNi dims, DType dtype, const char* file, int line)
  {
    if (!heap->resize(dtype.getByteSize(dims), file, line)) return false;
    this->dims = dims;
    this->dtype = dtype;
    return true;
  }

  //resize
  inline bool resize(Int64 x, DType dtype, const char* file, int line){
    return resize(PointNi(std::vector<Int64>({ x })), dtype, file, line);
  }

  //resize
  inline bool resize(Int64 x, Int64 y, DType dtype, const char* file, int line){
    return resize(PointNi(std::vector<Int64>({ x,y })), dtype, file, line);
  }

  //resize
  inline bool resize(Int64 x, Int64 y, Int64 z, DType dtype, const char* file, int line){
    return resize(PointNi(std::vector<Int64>({ x,y,z })), dtype, file, line);
  }

  //getComponent
  Array getComponent(int C, Aborted aborted = Aborted()) const;

  //setComponent
  bool setComponent(int C, Array src, Aborted aborted = Aborted());

};


#if !SWIG

#pragma pack(push, 1)
template <int nbytes>
class Sample { public: Uint8 v[nbytes];};
#pragma pack(pop)

class BitAlignedSample {};
  
//////////////////////////////////////////////////////////////
template <typename Sample>
class GetSamples
{
public:

  //_______________________________________________________
  class Range
  {
  public:

    Sample* ptr;
    Int64   offset;
    Int64   num;

    //constructor
    inline Range(Sample* ptr_,Int64 offset_,Int64 num_) : ptr(ptr_),offset(offset_),num(num_) {
    }

    //operator=
    inline Range& operator=(const Range& other) {
      if (num!=other.num) ThrowException("range with different dimensions");
      memcpy(ptr+offset,other.ptr+other.offset,sizeof(Sample)*num);
      return *this;
    }

    //operator==
    inline bool operator==(const Range& other) {
      if (num!=other.num)  return false;
      return memcmp(ptr+offset,other.ptr+other.offset,sizeof(Sample)*num)==0;
    }
  };

  //constructor
  inline GetSamples() : ptr(nullptr),num(0) {
  }

  //constructor
  inline GetSamples(Array array) 
    : ptr((Sample*)array.c_ptr()),num(array.c_size()/sizeof(Sample))
  {
    VisusAssert(array.dtype.getByteSize()==sizeof(Sample));
    VisusAssert(Utils::isByteAligned(array.dtype.getBitSize()));
  }

  //operator[]
  inline const Sample& operator[](const Int64& index) const {
    VisusAssert(index>=0 && index<num);
    return ptr[index];
  }

  //operator[]
  inline Sample& operator[](const Int64& index) {
    VisusAssert(index>=0 && index<num);
    return ptr[index];
  }

  //range
  inline Range range(const Int64& offset,const Int64& num) {
    VisusAssert((offset+num)<=this->num);
    return Range(ptr,offset,num);
  }

private:
  
  Sample* ptr=nullptr;
  Int64 num=0;

};

//////////////////////////////////////////////////////////////
template <>
class GetSamples<BitAlignedSample>
{
public:

  //_______________________________________________________
  class Range
  {
  public:

    GetSamples& samples;
    Int64 offset;
    Int64 num;

    //constructor
    inline Range(GetSamples& samples_,Int64 offset_,Int64 num_) : samples(samples_),offset(offset_),num(num_) {
    }

    //operator=
    inline Range& operator=(const Range& other) 
    {
      if (num!=other.num || samples.bitsize!=other.samples.bitsize) 
        ThrowException("range not compatible");

      if (samples.is_byte_aligned)
      {
        memcpy(samples.ptr + (size_t)(this->offset*samples.bytesize), other.samples.ptr + (size_t)(other.offset*samples.bytesize), (size_t)(samples.bytesize*num));
      }
      else
      {
        Int64 totbits        = samples.bitsize*num;
        Int64 dst_bit_offset = this->offset*samples.bitsize;
        Int64 src_bit_offset = other.offset*samples.bitsize;
        Int64 b1, b2, done   = 0;

        for (b1 = 0; (!Utils::isByteAligned(dst_bit_offset + b1) || !Utils::isByteAligned(src_bit_offset + b1)) && b1 < totbits; b1++, done++) 
        {
          auto bit_r=Utils::getBit(other.samples.ptr, src_bit_offset + b1);
          Utils::setBit(samples.ptr, dst_bit_offset + b1,bit_r);
        }

        for (b2 = totbits - 1; (!Utils::isByteAligned(dst_bit_offset + b2 + 1) || !Utils::isByteAligned(src_bit_offset + b2 + 1)) && b2 >= b1; b2--, done++) 
        {
          auto bit_r=Utils::getBit(other.samples.ptr, src_bit_offset + b2);
          Utils::setBit(samples.ptr, dst_bit_offset + b2,bit_r);
        }

        if (done != totbits) 
          memcpy(samples.ptr + ((dst_bit_offset + b1) >> 3), other.samples.ptr + ((src_bit_offset + b1) >> 3), (size_t)((1 + (b2 - b1)) >> 3));
      }
      return *this;
    }

    //operator==
    inline bool operator==(const Range& other) {
    
      if (num!=other.num || samples.bitsize!=other.samples.bitsize) 
        return false;

      if (samples.is_byte_aligned)
      {
        return memcmp(samples.ptr + (size_t)(this->offset*samples.bytesize), other.samples.ptr + (size_t)(other.offset*samples.bytesize), (size_t)(samples.bytesize*num))==0;
      }
      else
      {
        Int64 totbits = samples.bitsize*num;
        Int64 dst_bit_offset = this->offset*samples.bitsize;
        Int64 src_bit_offset = other.offset*samples.bitsize;
        Int64 b1, b2, done = 0;

        for (b1 = 0; (!Utils::isByteAligned(dst_bit_offset + b1) || !Utils::isByteAligned(src_bit_offset + b1)) && b1 < totbits; b1++, done++)
        {
          auto bit_w = Utils::getBit(this->samples.ptr, dst_bit_offset + b1);
          auto bit_r = Utils::getBit(other.samples.ptr, src_bit_offset + b1);
          if (bit_w != bit_r) return false;
        }

        for (b2 = totbits - 1; (!Utils::isByteAligned(dst_bit_offset + b2 + 1) || !Utils::isByteAligned(src_bit_offset + b2 + 1)) && b2 >= b1; b2--, done++)
        {
          int bit_w = Utils::getBit(samples.ptr, dst_bit_offset + b2);
          int bit_r = Utils::getBit(other.samples.ptr, src_bit_offset + b2);
          if (bit_w != bit_r) return false;
        }

        if (done != totbits)
          return memcmp(samples.ptr + ((dst_bit_offset + b1) >> 3), other.samples.ptr + ((src_bit_offset + b1) >> 3), (size_t)((1 + (b2 - b1)) >> 3))==0;

        return true;
      }    
    }
  };

  //constructor
  inline GetSamples() {
  }

  //constructor
  inline GetSamples(Array array) 
  {
    this->bitsize         = array.dtype.getBitSize();
    this->is_byte_aligned = Utils::isByteAligned(this->bitsize);
    this->bytesize        = (is_byte_aligned ? this->bitsize : (int)Utils::alignToByte(bitsize)) >> 3;
    this->ptr             = array.c_ptr();
    this->num             = array.getTotalNumberOfSamples();
  }

  //operator[]
  inline Range operator[](const Int64& index) {
    return range(index,1);
  }

  //range
  inline Range range(const Int64& offset,const Int64& num) {
    VisusAssert((offset+num)<=this->num);
    return Range(*this,offset,num);
  }

private:
  
  Uint8*  ptr=nullptr;

  int     bitsize=0;
  bool    is_byte_aligned=false;
  int     bytesize=0;
  Int64   num=0;

};


//////////////////////////////////////////////////////////////
template <typename T>
class GetComponentSamples
{
public:

  T*            ptr = nullptr;
  PointNi       dims;
  Int64         tot=0;
  int           stride=0;
  int           C=0;

  //default constructor
  inline GetComponentSamples() {
  }

  //constructor
  inline GetComponentSamples(Array array,int C_) : C(C_)
  {
    VisusAssert(C>= 0 && C<array.dtype.ncomponents());
    VisusAssert(array.dtype.getByteSize()==sizeof(T)*array.dtype.ncomponents());

    this->ptr    = ((T*)array.c_ptr())+C;
    this->dims   = array.dims;
    this->tot    = array.getTotalNumberOfSamples();
    this->stride = array.dtype.ncomponents();
  }

  //operator[]
  inline T& operator[](const Int64& index) {
    VisusAssert(index>=0 && index<tot);
    return ptr[index*stride];
  }

  //operator[]
  inline const T& operator[](const Int64& index) const {
    VisusAssert(index>=0 && index<tot);
    return ptr[index*stride];
  }

};

//////////////////////////////////////////////////////////////
inline void CopySamples(Array& write,Int64 woffset,Array read,Int64 roffset,Int64 tot) {
  GetSamples<BitAlignedSample>(write).range(woffset,tot)=GetSamples<BitAlignedSample>(read).range(roffset,tot);
}

inline bool CompareSamples(Array& write,Int64 woffset,Array read,Int64 roffset,Int64 tot) {
  return GetSamples<BitAlignedSample>(write).range(woffset,tot)==GetSamples<BitAlignedSample>(read).range(roffset,tot);
}

//see http://eli.thegreenplace.net/2014/perfect-forwarding-and-universal-references-in-c/
template<class Operation,typename... Args>
inline bool NeedToCopySamples(Operation& op,DType dtype,Args&&... args)
{
  int bitsize=dtype.getBitSize();
  if (Utils::isByteAligned(bitsize)) 
  {
    switch (int bytesize = bitsize >> 3)
    {
      case    1: return op.template execute<  Sample<   1> >(std::forward<Args>(args)...);
      case    2: return op.template execute<  Sample<   2> >(std::forward<Args>(args)...);
      case    3: return op.template execute<  Sample<   3> >(std::forward<Args>(args)...);
      case    4: return op.template execute<  Sample<   4> >(std::forward<Args>(args)...);
      case    5: return op.template execute<  Sample<   5> >(std::forward<Args>(args)...);
      case    6: return op.template execute<  Sample<   6> >(std::forward<Args>(args)...);
      case    7: return op.template execute<  Sample<   7> >(std::forward<Args>(args)...);
      case    8: return op.template execute<  Sample<   8> >(std::forward<Args>(args)...);
      case    9: return op.template execute<  Sample<   9> >(std::forward<Args>(args)...);
      case   10: return op.template execute<  Sample<  10> >(std::forward<Args>(args)...);
      case   11: return op.template execute<  Sample<  11> >(std::forward<Args>(args)...);
      case   12: return op.template execute<  Sample<  12> >(std::forward<Args>(args)...);
      case   13: return op.template execute<  Sample<  13> >(std::forward<Args>(args)...);
      case   14: return op.template execute<  Sample<  14> >(std::forward<Args>(args)...);
      case   15: return op.template execute<  Sample<  15> >(std::forward<Args>(args)...);
      case   16: return op.template execute<  Sample<  16> >(std::forward<Args>(args)...);
      case   17: return op.template execute<  Sample<  17> >(std::forward<Args>(args)...);
      case   18: return op.template execute<  Sample<  18> >(std::forward<Args>(args)...);
      case   19: return op.template execute<  Sample<  19> >(std::forward<Args>(args)...);
      case   20: return op.template execute<  Sample<  20> >(std::forward<Args>(args)...);
      case   21: return op.template execute<  Sample<  21> >(std::forward<Args>(args)...);
      case   22: return op.template execute<  Sample<  22> >(std::forward<Args>(args)...);
      case   23: return op.template execute<  Sample<  23> >(std::forward<Args>(args)...);
      case   24: return op.template execute<  Sample<  24> >(std::forward<Args>(args)...);
      case   25: return op.template execute<  Sample<  25> >(std::forward<Args>(args)...);
      case   26: return op.template execute<  Sample<  26> >(std::forward<Args>(args)...);
      case   27: return op.template execute<  Sample<  27> >(std::forward<Args>(args)...);
      case   28: return op.template execute<  Sample<  28> >(std::forward<Args>(args)...);
      case   29: return op.template execute<  Sample<  29> >(std::forward<Args>(args)...);
      case   30: return op.template execute<  Sample<  30> >(std::forward<Args>(args)...);
      case   31: return op.template execute<  Sample<  31> >(std::forward<Args>(args)...);
      case   32: return op.template execute<  Sample<  32> >(std::forward<Args>(args)...);
      case   64: return op.template execute<  Sample<  64> >(std::forward<Args>(args)...);
      case  128: return op.template execute<  Sample< 128> >(std::forward<Args>(args)...);
      case  256: return op.template execute<  Sample< 256> >(std::forward<Args>(args)...);
      case  512: return op.template execute<  Sample< 512> >(std::forward<Args>(args)...);
      case 1024: return op.template execute<  Sample<1024> >(std::forward<Args>(args)...);
      default:
        ThrowException("please add a new 'case XX:'");
        break;
    }
  }

  return op.template execute< BitAlignedSample >(std::forward<Args>(args)...);
}

//see http://eli.thegreenplace.net/2014/perfect-forwarding-and-universal-references-in-c/
template<class Operation,typename... Args>
inline bool ExecuteOnCppSamples(Operation& op,DType dtype,Args&&... args)
{
  if (dtype.isVectorOf(DTypes::INT8   )) return op.template execute<Int8   >(std::forward<Args>(args)...);
  if (dtype.isVectorOf(DTypes::UINT8  )) return op.template execute<Uint8  >(std::forward<Args>(args)...);
  if (dtype.isVectorOf(DTypes::INT16  )) return op.template execute<Int16  >(std::forward<Args>(args)...);
  if (dtype.isVectorOf(DTypes::UINT16 )) return op.template execute<Uint16 >(std::forward<Args>(args)...);
  if (dtype.isVectorOf(DTypes::INT32  )) return op.template execute<Int32  >(std::forward<Args>(args)...);
  if (dtype.isVectorOf(DTypes::UINT32 )) return op.template execute<Uint32 >(std::forward<Args>(args)...);
  if (dtype.isVectorOf(DTypes::INT64  )) return op.template execute<Int64  >(std::forward<Args>(args)...);
  if (dtype.isVectorOf(DTypes::UINT64 )) return op.template execute<Uint64 >(std::forward<Args>(args)...);
  if (dtype.isVectorOf(DTypes::FLOAT32)) return op.template execute<Float32>(std::forward<Args>(args)...);
  if (dtype.isVectorOf(DTypes::FLOAT64)) return op.template execute<Float64>(std::forward<Args>(args)...);

  VisusAssert(false);
  return false;
}

#endif //if !SWIG



} //namespace Visus

#include <Visus/ArrayUtils.h>
#include <Visus/ArrayPlugin.h>

#endif //VISUS_ARRAY_H



