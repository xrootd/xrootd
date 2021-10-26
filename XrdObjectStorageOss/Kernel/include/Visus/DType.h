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

#ifndef VISUS_DTYPE_H
#define VISUS_DTYPE_H

#include <Visus/Kernel.h>
#include <Visus/Point.h>
#include <Visus/Range.h>

namespace Visus {

  //////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API DType
{
public:

  VISUS_CLASS(DType)

  //default constructor
  DType() {
  }

  //constructor for AtomicType
  DType(bool unsign_, bool decimal_, int bitsize_)
    : num(1),unsign(decimal_ || bitsize_ == 1? false : unsign_),decimal(decimal_),bitsize(bitsize_),ranges(1)
  {
    VisusAssert(bitsize > 0);
    this->description= concatenate(unsign ? "u" : "", decimal ? "float" : "int", bitsize);
  }

  //constructor for VectorType
  DType(int num_,DType other) 
    : num(num_),unsign(other.unsign),decimal(other.decimal),bitsize(other.bitsize),ranges(num)
  {
    VisusAssert(num && other.ncomponents()==1);
    this->description= concatenate(other.description, num > 1 ? "[" + cstring(num) + "]" : "");
  }

  //use this if you have the value representation of dtype from string representation 
  //the generic format is [int_value*] [u](int|float)<num_bits>
  //example: uint8, 2*float32, 3*int7
  static DType fromString(String s);

  //toString
  virtual String toString() const {
    return this->description;
  }

  //isDecimal
  bool isDecimal() const {
    return decimal ? true : false;
  }

  //operator==
  bool operator==(DType other) const {
    return this->num==other.num && this->unsign==other.unsign && this->decimal==other.decimal && this->bitsize==other.bitsize;
  }

  bool operator!=(DType other) const {
    return (*this == other) ? false : true;
  }

  //valid
  bool valid() const  {
    return num > 0;
  }

  //isVectorOf (dtype[0] || n*dtype)
  bool isVectorOf(DType other) const {
    return num>0 && bitsize==other.bitsize && unsign==other.unsign && decimal==other.decimal;
  }

  //ncomponents
  int ncomponents() const {
    return num;
  }

  //withNumberOfComponents
  DType withNumberOfComponents(int N) const {
    return DType(valid()? N : 0, get(0));
  }

  //getBitSize
  int getBitSize() const {
    return num * bitsize;
  }

  // get the total number of bits of a number of samples
  Int64 getBitSize(Int64 tot) const {
    return tot <= 0 ? (0) : (tot * getBitSize());
  }

  //getBitSize
  Int64 getBitSize(PointNi dims) const {
    return getBitSize(dims.innerProduct());
  }

  // return the total number (upper bound) of bytes of a certain number of samples 
  int getByteSize() const {
    return (int)Utils::alignToByte(getBitSize()) >> 3;
  }

  // return the total number (upper bound) of bytes of a certain number of samples 
  Int64 getByteSize(Int64 tot)  const {
    return (tot <= 0) ? (0) : (Utils::alignToByte(getBitSize(tot)) >> 3);
  }

  //getByteSize
  Int64 getByteSize(PointNi dims) const {
    return getByteSize(dims.innerProduct());
  }

  //isUnsigned
  bool isUnsigned() const {
    return unsign? true: false;
  }

  //getBitsOffset
  int getBitsOffset(int C) const {
    VisusAssert(C>=0 && C<ncomponents());
    return C * bitsize;
  }

  //get
  DType get(int C) const 
  {
    VisusAssert(C>=0 && C<ncomponents());
    return num==1? (*this) : DType(unsign,decimal,bitsize).withDTypeRange(getDTypeRange(C));
  }

  //getDTypeRange
  Range getDTypeRange(int component=0) const {
    VisusAssert(component>=0 && component<=ncomponents());
    return ranges[component];
  }

  //withDTypeRange
  DType withDTypeRange(Range value,int component=0) const 
  {
    VisusAssert(component>=0 && component<=ncomponents());
    DType ret=*this;
    ret.ranges[component]=value;
    return ret;
  }

private:

  String description;

  //included in description
  int  num      = 0;
  bool unsign   = false;
  bool decimal  = false;
  int  bitsize  = 0;
  
  //not included in description
  std::vector<Range> ranges;


};

VISUS_KERNEL_API Range GetCppRange(DType dtype);



  //////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API DTypes
{
public:

  //some common dtypes
  static DType UINT1;
  static DType INT8;
  static DType INT8_GA;
  static DType INT8_RGB;
  static DType INT8_RGBA;
  static DType UINT8;
  static DType UINT8_GA;
  static DType UINT8_RGB;
  static DType UINT8_RGBA;
  static DType INT16;
  static DType INT16_GA;
  static DType INT16_RGB;
  static DType INT16_RGBA;
  static DType UINT16;
  static DType UINT16_GA;
  static DType UINT16_RGB;
  static DType UINT16_RGBA;
  static DType INT32;
  static DType INT32_GA;
  static DType INT32_RGB;
  static DType INT32_RGBA;
  static DType UINT32;
  static DType UINT32_GA;
  static DType UINT32_RGB;
  static DType UINT32_RGBA;
  static DType INT64;
  static DType INT64_GA;
  static DType INT64_RGB;
  static DType INT64_RGBA;
  static DType UINT64;
  static DType UINT64_GA;
  static DType UINT64_RGB;
  static DType UINT64_RGBA;
  static DType FLOAT32;
  static DType FLOAT32_GA;
  static DType FLOAT32_RGB;
  static DType FLOAT32_RGBA;
  static DType FLOAT64;
  static DType FLOAT64_GA;
  static DType FLOAT64_RGB;
  static DType FLOAT64_RGBA;

private:

  DTypes()=delete;

};

namespace CppDTypes
{
  template <typename T> inline DType get() {VisusAssert(false);return DType();};

  template<>  inline DType get<Int8   >() {return DTypes::INT8   ;}
  template<>  inline DType get<Int16  >() {return DTypes::INT16  ;}
  template<>  inline DType get<Int32  >() {return DTypes::INT32  ;}
  template<>  inline DType get<Int64  >() {return DTypes::INT64  ;}
  template<>  inline DType get<Uint8  >() {return DTypes::UINT8  ;}
  template<>  inline DType get<Uint16 >() {return DTypes::UINT16 ;}
  template<>  inline DType get<Uint32 >() {return DTypes::UINT32 ;}
  template<>  inline DType get<Uint64 >() {return DTypes::UINT64 ;}
  template<>  inline DType get<Float32>() {return DTypes::FLOAT32;}
  template<>  inline DType get<Float64>() {return DTypes::FLOAT64;}

}


} //namespace Visus


#endif //VISUS_DTYPE_H

