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

#ifndef _VISUS_HEAP_MEMORY_H__
#define _VISUS_HEAP_MEMORY_H__

#include <Visus/Kernel.h>

#include <cstring>

namespace Visus {


//////////////////////////////////////////////////////////////
class VISUS_KERNEL_API HeapMemory
{
public:

  VISUS_NON_COPYABLE_CLASS(HeapMemory)

  //constructor
  HeapMemory();

  //destructor
  virtual ~HeapMemory();

  //createUnmanaged
  static SharedPtr<HeapMemory> createUnmanaged(Uint8* p, Int64 n);

  //createManaged
  static SharedPtr<HeapMemory> createManaged(Uint8* p, Int64 n);

  //clone
  SharedPtr<HeapMemory> clone() const;

  //reserve (i.e. change the c_capacity() but not the c_size() neither dims neither dtype)
  bool reserve(Int64 new_m, const char* file, int line);

  //resize (i.e. change the c_size())
  bool resize(Int64 size, const char* file, int line);

  //shrink (so that c_capacity()==c_size())
  bool shrink();

  //hasConstantValue
  bool hasConstantValue(Uint8 value) const;

  //isAllZero
  bool isAllZero() const {
    return hasConstantValue(0);
  }

  //c_capacity
  inline Int64 c_capacity() const{
    return this->m;
  }

  //c_size
  inline Int64 c_size() const{
    return this->n;
  }

  //c_ptr
  inline Uint8* c_ptr(){
    return this->n ? this->p : nullptr;
  }

  //c_ptr
#if !SWIG
  inline const Uint8* c_ptr() const{
    return this->n ? this->p : nullptr;
  }
#endif

  //c_ptr
  template <typename Type>
  inline Type c_ptr(){
    return (Type)(this->n ? this->p : nullptr);
  }

  //fill
  inline void fill(int value){
    memset(c_ptr(), value, (size_t)c_size());
  }

  //toString
  String toString() const {
    return String((const char*)c_ptr(),(size_t)c_size());
  }

  //base64Encode
  String base64Encode() const;

  //base64Decode
  static SharedPtr<HeapMemory> base64Decode(const String& input);

  //copy
  static bool copy(const SharedPtr<HeapMemory>& dst,const SharedPtr<HeapMemory>& src)
  {
    if (!src || !dst)
      return false;

    auto nbytes=src->c_size();
    if (!dst->resize(nbytes,__FILE__,__LINE__))
      return false;
    memcpy(dst->c_ptr(),src->c_ptr(),nbytes);
    return true;
  }

  //equals
  static bool equals(SharedPtr<HeapMemory> a, SharedPtr<HeapMemory> b)
  {
    if (a && !a->c_size()) a.reset();
    if (b && !b->c_size()) b.reset();
    if (!a || !b) return !a && !b;
    return a->c_size() == b->c_size() && memcmp(a->c_ptr(), b->c_ptr(), a->c_size())==0;
  }

private:

  //if managed or not (i.e. if isUnmanaged I'm not the owner of the memory)
  bool  unmanaged;

  //current number of items
  Int64 n;

  //max number of items
  Int64 m;

  //pointer to data
  Uint8* p;

  //myRealloc
  bool myRealloc(Int64 new_m, const char* file, int line);
};



//////////////////////////////////////////////////////////////
#if !SWIG
class VISUS_KERNEL_API OutputBinaryStream
{
public:

  HeapMemory& out;

  //constructor
  OutputBinaryStream(HeapMemory& out_) : out(out_) {
  }

  //write
  OutputBinaryStream& write(const void* buffer, Int64 num)
  {
    if ((out.c_size() + num) >= out.c_capacity())
    {
      Int64 new_capacity = out.c_size() + num;

      if (new_capacity < out.c_capacity() * 2)
        new_capacity = out.c_capacity() * 2;
      
      new_capacity+= 64 * 1024;
      out.reserve(new_capacity, __FILE__, __LINE__);
    }

    auto offset = out.c_size();
    out.resize(out.c_size() + num, __FILE__, __LINE__);
    memcpy(out.c_ptr() + offset, buffer, num);
    return *this;
  }

  //operator<<
  OutputBinaryStream& operator<<(String value) {
    return write(value.c_str(), value.size());
  }

  //operator<<
  OutputBinaryStream& operator<<(HeapMemory& memory) {
    return write(memory.c_ptr(), memory.c_size());
  }

};
#endif


} //namespace Visus

#endif //_VISUS_HEAP_MEMORY_H__
