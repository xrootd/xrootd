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

#ifndef VISUS_KERNEL_H__
#define VISUS_KERNEL_H__

#include <memory>
#include <string>
#include <sstream>
#include <atomic>
#include <exception>
#include <vector>

//__________________________________________________________
#if defined(_WIN32)

    //otherwise min() max() macro are declared
  #ifndef NOMINMAX
  #define NOMINMAX
  #endif

#endif 



//__________________________________________________________
#define VISUS_DEPRECATED(className) className

#define VISUS_STRINGIFY_MACRO_HELPER(a) #a

#define VISUS_JOIN_MACRO_HELPER(a, b) a ## b
#define VISUS_JOIN_MACRO(item1, item2)   VISUS_JOIN_MACRO_HELPER (item1, item2)


#ifndef VISUS_DISOWN
  #define VISUS_DISOWN(...) __VA_ARGS__
#endif

#ifndef VISUS_NEWOBJECT
  #define VISUS_NEWOBJECT(...) __VA_ARGS__
#endif


namespace Visus {

typedef signed char        Int8;
typedef unsigned char      Uint8;
typedef signed short       Int16;
typedef unsigned short     Uint16;
typedef signed int         Int32;
typedef unsigned int       Uint32;
typedef float              Float32;
typedef double             Float64;
typedef long long          Int64;
typedef unsigned long long Uint64;
typedef Int64              BigInt;

typedef std::string String;

namespace Math {
  const double Pi = 3.14159265358979323846;
}
  
//https://en.cppreference.com/w/cpp/language/type_alias
template<class T>
using SharedPtr = std::shared_ptr<T>;
  
template<class T>
using UniquePtr = std::unique_ptr<T>;
  

 inline String     cstring()                     { return ""; }
 inline String     cstring(bool    v)            { return v ? "True" : "False"; }
 inline String     cstring(Int32   v)            { return std::to_string(v); }
 inline String     cstring(Uint32  v)            { return std::to_string(v); }
 inline String     cstring(Float32 v)            { return std::to_string(v); }
 inline String     cstring(Float64 v)            { return std::to_string(v); }
 inline String     cstring(Int64   v)            { return std::to_string(v); }
 inline String     cstring(Uint64  v)            { return std::to_string(v); }
 inline String     cstring(const String& value)  { return value; }
 inline String     cstring(const char* value)    { return String(value); }
 inline String     cstring(char* value)          { return String(value); }
 inline String     cstring(off_t value)          { return std::to_string(value); }
  
#if !defined(_WIN32)
   inline String   cstring(size_t v)             { return std::to_string(v); }
#endif

template <typename Value>
inline String cstring(const Value& value) { 
  return value.toString(); 
}

template <typename Left, typename... Args>
inline String cstring(Left left, Args&&... args) {
  auto A = cstring(left);
  auto B = cstring(std::forward<Args>(args)...);
  return A + (A.empty() || B.empty() ? "" : " ") + B;
}


inline String concatenate() {
  return String();
}

inline String concatenate(String a) {
  return a;
}

template <typename Value>
inline String concatenate(const Value& value) {
  return cstring(value);
}

inline String concatenate(String a,String b) {
  return a+b;
}

template <typename Left, typename... Args>
inline String concatenate(Left left, Args&&... args) {
  return concatenate(left) + concatenate(std::forward<Args>(args)...);
}

template <typename Value>
inline String cnamed(String name, const Value& value) {
  return concatenate(name,"(",value,")");
}

        bool       cbool  (String s);
 inline int        cint   (const String& s) { return s.empty() ? 0 : std::stoi(s); }
 inline float      cfloat (const String& s) { return s.empty() ? 0 : std::stof(s); }
 inline double     cdouble(const String& s) { return s.empty() ? 0 : std::stod(s); }
 inline Int64      cint64 (const String& s) { return s.empty() ? 0 : std::stoll(s); }
 inline Uint64     cuint64(const String& s) { return s.empty() ? 0 : std::stoull(s); }

//String->BigInt
 inline BigInt cbigint(const String& s) {
  return cint64(s);
}

//BigInt->Int64
 inline Int64 cint64(const BigInt& value) {
  return value;
}

template <typename Value>
inline Value from_string(const std::string& s) {
  std::istringstream parser(s);
  Value ret;  
  parser >> ret; 
  return ret;
}

 void PrintLine(String file, int line, int severity, String msg);

#define PrintDebug(...)   PrintLine(__FILE__,__LINE__, 0, cstring(__VA_ARGS__))
#define PrintInfo(...)    PrintLine(__FILE__,__LINE__, 1, cstring(__VA_ARGS__))
#define PrintWarning(...) PrintLine(__FILE__,__LINE__, 2, cstring(__VA_ARGS__))
#define PrintError(...)   PrintLine(__FILE__,__LINE__, 3, cstring(__VA_ARGS__))

 void RedirectLogTo(void(*)(String msg, void*), void* user_data = nullptr);

//VisusAssertFailed
 void VisusAssertFailed(const char* file, int line, const char* expr);

//PrintMessageToTerminal
 void PrintMessageToTerminal(const String& value);

//ThrowExceptionEx
 void ThrowExceptionEx(String file,int line,String what);

template <typename... Args>
inline void ThrowExceptionEx(String file, int line, Args&&... args) {
  ThrowExceptionEx(file, line, cstring(std::forward<Args>(args)...));
}

#define ThrowException(...) (ThrowExceptionEx(__FILE__,__LINE__,__VA_ARGS__))

#  define VisusReleaseAssert(_Expression) { \
    if (!(_Expression)) \
    Visus::VisusAssertFailed(__FILE__,__LINE__,#_Expression); \
}\
/*--*/

#  if defined(_DEBUG)
#    define VisusAssert(_Expression) VisusReleaseAssert(_Expression)
#  else
#    define VisusAssert(_Expression) ((void)0) 
#  endif


#define VISUS_CLASS(className)

#ifndef VISUS_NON_COPYABLE_CLASS
#define VISUS_NON_COPYABLE_CLASS(className) \
  VISUS_CLASS(className) \
  className (const className&)=delete;\
  className& operator= (const className&)=delete;\
  /*--*/
#endif

#ifndef VISUS_PIMPL_CLASS
#define VISUS_PIMPL_CLASS(className) \
  VISUS_NON_COPYABLE_CLASS(className) \
  class        Pimpl;  \
  friend class Pimpl;  \
  Pimpl*       pimpl=nullptr;  \
  /*--*/
#endif

class  KernelModule 
{
public:

  static int num_attached;

  //attach
  static void attach();

  //detach
  static void detach();
};


} //namespace Visus


#endif //VISUS_KERNEL_H__

