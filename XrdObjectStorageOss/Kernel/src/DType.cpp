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

#include <Visus/DType.h>
#include <Visus/StringUtils.h>

#include <cctype>

namespace Visus {

//some common dtypes
DType DTypes::UINT1         =DType::fromString("uint1");

DType DTypes::INT8          =DType::fromString("int8");
DType DTypes::INT8_GA       =DType::fromString("int8[2]");
DType DTypes::INT8_RGB      =DType::fromString("int8[3]");
DType DTypes::INT8_RGBA     =DType::fromString("int8[4]");

DType DTypes::UINT8         =DType::fromString("uint8");
DType DTypes::UINT8_GA      =DType::fromString("uint8[2]");
DType DTypes::UINT8_RGB     =DType::fromString("uint8[3]");
DType DTypes::UINT8_RGBA    =DType::fromString("uint8[4]");

DType DTypes::INT16         =DType::fromString("int16");
DType DTypes::INT16_GA      =DType::fromString("int16[2]");
DType DTypes::INT16_RGB     =DType::fromString("int16[3]");
DType DTypes::INT16_RGBA    =DType::fromString("int16[4]");

DType DTypes::UINT16        =DType::fromString("uint16");
DType DTypes::UINT16_GA     =DType::fromString("uint16[2]");
DType DTypes::UINT16_RGB    =DType::fromString("uint16[3]");
DType DTypes::UINT16_RGBA   =DType::fromString("uint16[4]");

DType DTypes::INT32         =DType::fromString("int32");
DType DTypes::INT32_GA      =DType::fromString("int32[2]");
DType DTypes::INT32_RGB     =DType::fromString("int32[3]");
DType DTypes::INT32_RGBA    =DType::fromString("int32[4]");

DType DTypes::UINT32        =DType::fromString("uint32");
DType DTypes::UINT32_GA     =DType::fromString("uint32[2]");
DType DTypes::UINT32_RGB    =DType::fromString("uint32[3]");
DType DTypes::UINT32_RGBA   =DType::fromString("uint32[4]");

DType DTypes::INT64         =DType::fromString("int64");
DType DTypes::INT64_GA      =DType::fromString("int64[2]");
DType DTypes::INT64_RGB     =DType::fromString("int64[3]");
DType DTypes::INT64_RGBA    =DType::fromString("int64[4]");

DType DTypes::UINT64        =DType::fromString("uint64");
DType DTypes::UINT64_GA     =DType::fromString("uint64[2]");
DType DTypes::UINT64_RGB    =DType::fromString("uint64[3]");
DType DTypes::UINT64_RGBA   =DType::fromString("uint64[4]");

DType DTypes::FLOAT32       =DType::fromString("float32");
DType DTypes::FLOAT32_GA    =DType::fromString("float32[2]");
DType DTypes::FLOAT32_RGB   =DType::fromString("float32[3]");
DType DTypes::FLOAT32_RGBA  =DType::fromString("float32[4]");

DType DTypes::FLOAT64       =DType::fromString("float64");
DType DTypes::FLOAT64_GA    =DType::fromString("float64[2]");
DType DTypes::FLOAT64_RGB   =DType::fromString("float64[3]");
DType DTypes::FLOAT64_RGBA  =DType::fromString("float64[4]");


//////////////////////////////////////////////////////////////////////////////
DType DType::fromString(String s)
{
  if (s.empty())
    return DType();

  s=StringUtils::toLower(s);

  int  I=0;

  int  num     = 1;
  bool unsign  = false;
  bool decimal = false;
  int  bitsize = 0;

  auto failed=[&]() {
    PrintInfo("error parsing dtype",s);
    VisusAssert(false);
    return DType();
  };

  auto acceptSpaces=[&]() {
    while (I<s.length() && std::isspace(s[I]))
      I++;
  };

  auto acceptNumber=[&](int& value)
  {
    acceptSpaces();

    if (I==s.length() || !std::isdigit(s[I]))
      return false;

    value=0;
    while (I<s.length() && std::isdigit(s[I])) 
      value=value*10+(s[I++]-'0');
    return true;
  };

  auto acceptString=[&](const String& value) 
  {
    while (std::isspace(s[I]) && I<s.length())
      I++;

    if (s.substr(I,value.size())!=value)
      return false;

    I+=(int)value.length();
    return true;
  };

  auto acceptType=[&]() 
  {
    if (acceptString("uint"))  {unsign=true ; decimal=false; return true;}
    if (acceptString("int"))   {unsign=false; decimal=false; return true;}
    if (acceptString("float")) {unsign=false; decimal=true ; return true;}
    return false;
  };

  //example 3*uint8
  if (acceptNumber(num)) 
  {
    if (!(acceptString("*") && acceptType() && acceptNumber(bitsize))) 
      return failed();
  }
  //example uint8 | uint8[3]
  else
  {
    if (!(acceptType() && acceptNumber(bitsize)))
      return failed();

    if (acceptString("[") && !(acceptNumber(num) && acceptString("]")))
      return failed();
  }

  acceptSpaces();

  if (I!=s.length())
    return failed();

  return DType(num,DType(unsign,decimal,bitsize));
}

//////////////////////////////////////////////////////////////////////////
Range GetCppRange(DType dtype)
{
    dtype = dtype.get(0);

    if (dtype.isDecimal())
    {
        if (dtype == DTypes::FLOAT32)
            return Range((double)NumericLimits<Float32>::lowest(), (double)NumericLimits<Float32>::highest(), 0.0);

        if (dtype == DTypes::FLOAT64)
            return Range((double)NumericLimits<Float64>::lowest(), (double)NumericLimits<Float64>::highest(), 0.0);
    }
    else if (dtype.isUnsigned())
    {
        if (dtype == DTypes::UINT8) //0 255
            return Range((double)NumericLimits<Uint8>::lowest(), (double)NumericLimits<Uint8>::highest(), 1.0);

        if (dtype == DTypes::UINT16) //0 65535
            return Range((double)NumericLimits<Uint16>::lowest(), (double)NumericLimits<Uint16>::highest(), 1.0);

        if (dtype == DTypes::UINT32) //0 4294967295
            return Range((double)NumericLimits<Uint32>::lowest(), (double)NumericLimits<Uint32>::highest(), 1.0);

        if (dtype == DTypes::UINT64)
            return Range((double)NumericLimits<Uint64>::lowest(), (double)NumericLimits<Uint64>::highest(), 1.0);
    }
    else
    {
        if (dtype == DTypes::INT8) // -128 +127
            return Range((double)NumericLimits<Int8>::lowest(), (double)NumericLimits<Int8>::highest(), 1.0);

        if (dtype == DTypes::INT16) // 32768 32767
            return Range((double)NumericLimits<Int16>::lowest(), (double)NumericLimits<Int16>::highest(), 1.0);

        if (dtype == DTypes::INT32) //- 2147483648 2147483647
            return Range((double)NumericLimits<Int32>::lowest(), (double)NumericLimits<Int32>::highest(), 1.0);

        if (dtype == DTypes::INT64)
            return Range((double)NumericLimits<Int64>::lowest(), (double)NumericLimits<Int64>::highest(), 1.0);
    }

    ThrowException("internal error");
    return Range::invalid();
}

} //namespace Visus

