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

#include "HeapMemory.h"

#include <algorithm>

namespace Visus {

////////////////////////////////////////////////////////
HeapMemory::HeapMemory() : unmanaged(false),n(0),m(0),p(nullptr)
{}

////////////////////////////////////////////////////////
HeapMemory::~HeapMemory()
{
  if (!unmanaged && p) 
    myRealloc(0,__FILE__,__LINE__);
}

////////////////////////////////////////////////////////
SharedPtr<HeapMemory> HeapMemory::createUnmanaged(Uint8* p,Int64 n)
{
  auto ret=std::make_shared<HeapMemory>();
  ret->unmanaged=true;
  ret->n=n;
  ret->m=n;
  ret->p=(Uint8*)p;
  return ret;
}

////////////////////////////////////////////////////////
SharedPtr<HeapMemory> HeapMemory::createManaged(Uint8* p, Int64 n)
{
  auto dst = std::make_shared<HeapMemory>();
  auto src = HeapMemory::createUnmanaged(p, n);
  if (!copy(dst, src))
    return SharedPtr<HeapMemory>();
  return  dst; 
}


////////////////////////////////////////////////////////
SharedPtr<HeapMemory> HeapMemory::clone() const
{
  auto ret=std::make_shared<HeapMemory>();
  if (!ret->resize(this->n,__FILE__,__LINE__))
    ThrowException("clone error");
  memcpy(ret->c_ptr(),this->c_ptr(),(size_t)n);
  return ret;
}

////////////////////////////////////////////////////////
bool HeapMemory::reserve(Int64 new_m,const char* file,int line)
{
  Int64 old_m=this->m;
  if (old_m>=new_m)  return true;
  if (unmanaged) {VisusAssert(false);return false;}
  return myRealloc(new_m,file,line);
}

////////////////////////////////////////////////////////
bool HeapMemory::resize(Int64 n,const char* file,int line)
{
  if (n>m && !this->reserve(n,file,line)) return false;
  this->n = n;
  return true;
}

////////////////////////////////////////////////////////
bool HeapMemory::shrink()
{
  if (this->m==this->n) return true;
  if (unmanaged) {VisusAssert(false);return false;}
  myRealloc(this->n,__FILE__,__LINE__);
  return true;
}

////////////////////////////////////////////////////////
bool HeapMemory::hasConstantValue(Uint8 value) const
{
  const Uint8* buf  = this->c_ptr();
  Int64        size = this->c_size();
  if (!size)
    return true;
  else
    return buf[0]== value && memcmp(buf, buf + 1, (size_t)size - 1)==0;
}


////////////////////////////////////////////////////////////////////////////////////////
bool HeapMemory::myRealloc(Int64 new_m,const char* file,int line)
{
  const Int64&   old_m=this->m;
  Uint8* old_p=this->p;
  Uint8* new_p=0;

  //wrong call
  if (new_m<0)
  {
    VisusAssert(false);
    return false;
  }

  //too much!
  if ((size_t)new_m!=new_m)
  {
    VisusAssert(false);
    return false;
  }

  //useless call
  if (!old_m && !new_m)
  {
    VisusAssert(old_p==0);
    return true;
  }

  //free
  if (!new_m)
  {
    free(this->p);

    this->p=0;
    this->m=0;
    this->n=0;
    return true;
  }

  //malloc
  if (!old_p)
  {
    new_p=(Uint8*)malloc((size_t)new_m);
  }
  //realloc
  else
  {
    new_p=(Uint8*)realloc(old_p,(size_t)new_m);
  }

  //failed
  if (!new_p) 
  {
    VisusAssert(false);
    return false;
  }

  this->m=new_m;
  this->p=new_p;
  this->n=std::min(this->n,this->m);
  return true;
}


////////////////////////////////////////////////////////////////////////////
//see http://en.wikibooks.org/wiki/Algorithm_Implementation/Miscellaneous/Base64
////////////////////////////////////////////////////////////////////////////

String HeapMemory::base64Encode() const
{
  const char encodeLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const char padCharacter = '=';

  std::basic_string<char> ret;
  Int64 nbytes=((this->c_size()/3) + (this->c_size() % 3 > 0)) * 4;

  //too much
  if ((size_t)nbytes!=nbytes)
  {
    VisusAssert(false);
    return "";
  }

  ret.reserve((size_t)nbytes);
  long temp;

  const unsigned char* cursor=this->c_ptr();

  size_t total=(size_t)this->c_size()/3;
  for(size_t idx = 0; idx < total; idx++)
  {
    temp  = (*cursor++) << 16; //Convert to big endian
    temp += (*cursor++) << 8;
    temp += (*cursor++);
    ret.append(1,encodeLookup[(temp & 0x00FC0000) >> 18]);
    ret.append(1,encodeLookup[(temp & 0x0003F000) >> 12]);
    ret.append(1,encodeLookup[(temp & 0x00000FC0) >> 6 ]);
    ret.append(1,encodeLookup[(temp & 0x0000003F)      ]);
  }

  switch (this->c_size() % 3)
  {
    case 1:
      temp  = (*cursor++) << 16; //Convert to big endian
      ret.append(1,encodeLookup[(temp & 0x00FC0000) >> 18]);
      ret.append(1,encodeLookup[(temp & 0x0003F000) >> 12]);
      ret.append(2,padCharacter);
      break;
   case 2:
      temp  = (*cursor++) << 16; //Convert to big endian
      temp += (*cursor++) << 8;
      ret.append(1,encodeLookup[(temp & 0x00FC0000) >> 18]);
      ret.append(1,encodeLookup[(temp & 0x0003F000) >> 12]);
      ret.append(1,encodeLookup[(temp & 0x00000FC0) >> 6 ]);
      ret.append(1,padCharacter);
      break;
  }
  return ret;
}


///////////////////////////////////////////////////////////////////////////
SharedPtr<HeapMemory> HeapMemory::base64Decode(const String& input)
{
  auto ret=std::make_shared<HeapMemory>();

  const char padCharacter = '=';

  if (input.length() % 4) //Sanity check
    return nullptr;

  size_t padding = 0;
  if (input.length())
  {
    if (input[input.length()-1] == padCharacter)
      padding++;

    if (input[input.length()-2] == padCharacter)
      padding++;
  }

  //wrong base64 string
  VisusAssert(padding>0);

  //Setup a vector to hold the result
  if (!ret->resize(((input.length()/4)*3)-padding,__FILE__,__LINE__))
    return nullptr;

  unsigned char* decodedBytes=ret->c_ptr();
  long temp=0; //Holds decoded quanta
  auto cursor = input.begin();

  while (cursor < input.end())
  {
    for (size_t quantumPosition = 0; quantumPosition < 4; quantumPosition++)
    {
      temp <<= 6;
      if       (*cursor >= 0x41 && *cursor <= 0x5A) temp |= *cursor - 0x41;                      
      else if  (*cursor >= 0x61 && *cursor <= 0x7A) temp |= *cursor - 0x47;
      else if  (*cursor >= 0x30 && *cursor <= 0x39) temp |= *cursor + 0x04;
      else if  (*cursor == 0x2B)                    temp |= 0x3E; //change to 0x2D for Url alphabet
      else if  (*cursor == 0x2F)                    temp |= 0x3F; //change to 0x5F for Url alphabet
      else if  (*cursor == padCharacter) //pad
      {
        switch( input.end() - cursor )
        {
        case 1: //One pad character
          *decodedBytes++=((temp >> 16) & 0x000000FF);
          *decodedBytes++=((temp >> 8 ) & 0x000000FF);
          goto FINISHED;

        case 2: //Two pad characters
          *decodedBytes++=((temp >> 10) & 0x000000FF);
          goto FINISHED;

        default:
          ThrowException("Invalid Padding in Base 64!");
        }
      }  
      else
      {
        ThrowException("Non-Valid Character in Base 64!");
      }
      cursor++;
    }
    *decodedBytes++=((temp >> 16) & 0x000000FF);
    *decodedBytes++=((temp >> 8 ) & 0x000000FF);
    *decodedBytes++=((temp      ) & 0x000000FF);
  }

FINISHED:

  ret->resize(decodedBytes-ret->c_ptr(),__FILE__,__LINE__);
  ret->shrink();
  return ret;
}

} //namespace Visus