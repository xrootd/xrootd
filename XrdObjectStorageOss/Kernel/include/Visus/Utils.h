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

#ifndef __VISUS_UTILS_H__
#define __VISUS_UTILS_H__

#include <Visus/Kernel.h>
#include <Visus/NumericLimits.h>
#include <Visus/HeapMemory.h>
#include <Visus/StringTree.h>
#include <Visus/StringUtils.h>

#include <cmath>
#include <functional>
#include <set>
#include <type_traits>

namespace Visus {

//////////////////////////////////////////////////////////////////////////
namespace Utils
{
  //find
  template <typename T>
  int find(const std::vector<T>& v,T value) {
    auto it = std::find(v.begin(), v.end(), value);
    return it == v.end() ? -1 : (int)(it - v.begin());
  }

  //contains
  template <typename T>
  bool contains(const std::vector<T>& v, T value) {
    auto it = std::find(v.begin(), v.end(), value);
    return it == v.end() ? false : true;
  }

  //remove
  template <typename Vector, typename Value>
  inline void remove(Vector& v, Value value) {
    auto it = std::find(v.begin(), v.end(), value);
    if (it != v.end()) v.erase(it);
  }

  //filter
  template <typename T>
  std::vector<T> filter(const std::vector< T >& v, std::function<bool(T)> predicate) {
    std::vector<T> ret;
    ret.reserve(v.size());
    std::copy_if(v.begin(), v.end(), std::back_inserter(ret), predicate);
    return ret;
  }


  //select
  template <typename T>
  inline std::vector<T> select(const std::vector<T>& v, std::function<bool(T)> predicate)
  {
    std::vector<T> ret;
    std::copy_if(v.begin(), v.end(), std::back_inserter(ret), predicate);
    return ret;
  }

  //range
  inline std::vector<int> range(int A, int B, int S)
  {
    std::vector<int> ret;
    for (int I = A; I < B; I += S)
      ret.push_back(I);
    return ret;
  }

  //range
  inline std::vector<int> range(int B) {
    return range(0, B, 1);
  }

  //pop_front
  template <typename T>
  inline void pop_front(std::vector<T>& v) {
    v.erase(v.begin());
  }

  //isByteAligned
  inline bool isByteAligned(Int64 bit) {
    return (bit & 0x07) ? false : true;
  }

  //alignToByte
  inline Int64 alignToByte(Int64 bit) {
    int mask = bit & 0x07; return mask ? (bit + (8 - mask)) : (bit);
  }

  //isValidNumber
  template<class T>
  typename std::enable_if< std::is_integral<T>::value , bool>::type
  isValidNumber(const T& value) {
    return true;
  }

  //isValidNumber
#if !SWIG
  template<class T>
  typename std::enable_if< std::is_floating_point<T>::value, bool>::type
  isValidNumber(const T& value) {
    return !std::isnan(value) && std::isfinite(value);
  }
#endif

  //getUnion
  template<class T>
  static std::set<T> getUnion(const std::set<T>& a, const std::set<T>& b)
  {
    std::set<T> ret = a;
    for (auto it : b)
      ret.insert(it);
    return ret;
  }

  //getIntersection
  template<class T>
  static std::set<T> getIntersection(const std::set<T>& a, const std::set<T>& b)
  {
    std::set<T> ret;
    for (auto it : a)
      if (b.count(it))
        ret.insert(it);
    return ret;
  }

  //overlap1d
  template<typename T>
  inline bool overlap1d(const T& a, const T& b, const T& p, const T& q) {
    return ((a) <= (q) && (b) >= (p));
  }

  //clamp
  template<typename T>
  inline T clamp(const T& v, const T& a, const T& b) {
    return (v < a) ? (a) : (v > b ? b : v);
  }

  //min
  template <typename T>
  inline T min(const std::vector<T>& vec) {
    VisusAssert(!vec.empty()); return *std::min_element(vec.begin(), vec.end());
  }

  //max
  template <typename T>
  inline T max(const std::vector<T>& vec) {
    VisusAssert(!vec.empty()); return *std::max_element(vec.begin(), vec.end());
  }

  //min
  template<typename T>
  inline T min(const T& a, const T& b) {
    return std::min(a, b);
  }

  //min
  template<typename T>
  inline T min(const T& a, const T& b, const T& c) {
    return min(a, min(b, c));
  }

  //min
  template<typename T>
  inline T min(const T& a, const T& b, const T& c, const T& d) {
    return min(a, min(b, c, d));
  }

  //min
  template<typename T>
  inline T min(const T& a, const T& b, const T& c, const T& d, const T& e) {
    return min(a, min(b, c, d, e));
  }

  //max
  template<typename T>
  inline T max(const T& a, const T& b) {
    return std::max(a, b);
  }

  //max
  template<typename T>
  inline T max(const T& a, const T& b, const T& c) {
    return max(a, max(b, c));
  }

  //max
  template<typename T>
  inline T max(const T& a, const T& b, const T& c, const T& d) {
    return max(a, max(b, c, d));
  }

  //max
  template<typename T>
  inline T max(const T& a, const T& b, const T& c, const T& d, const T& e) {
    return max(a, max(b, c, d, e));
  }

  //min_element_index
  template <typename T>
  inline int min_element_index(const std::vector<T>& vec) {
    return vec.empty() ? -1 : (int)std::distance(vec.begin(), std::min_element(vec.begin(), vec.end()));
  }

  //max_element_index
  template <typename T>
  inline int max_element_index(const std::vector<T>& vec) {
    return vec.empty() ? -1 : (int)std::distance(vec.begin(), std::max_element(vec.begin(), vec.end()));
  }

  //getRandInteger
  inline int getRandInteger(const int& a, const int& b) {
    return a + ((rand()) % (1 + b - a));
  }

  //getRandDouble in the range [a,b]
  VISUS_KERNEL_API double getRandDouble(double a = 0.0, double b = 1.0);

  //degreeToRadiant
  inline double degreeToRadiant(double value) {
    return (Math::Pi / 180.0) * value;
  }

  //radiantToDegree
  inline double radiantToDegree(double value) {
    return (180.0 / Math::Pi) * value;
  }

  //doubleAlmostEquals
  inline bool doubleAlmostEquals(double a, double b, double tolerance = NumericLimits<double>::epsilon()) {
    return fabs(b - a) <= tolerance;
  }

  //square
  template <typename T>
  inline T square(const T& value) {
    return value*value;
  }

  //find the power of 2 of an integer value (example 5->8)
  inline Int64 getPowerOf2(Int64 x)
  {
    Int64 n = 1;
    while (n < x) n <<= 1;
    return n;
  }

  //isPowerOfTwo
  inline bool isPowerOf2(const Int64& value) {
    return ((value & (value - 1)) == 0) ? true : false;
  }

  //return the number of bit of a number power of 2 (example GetLog2(1<<2)==2)
  inline int getLog2(Int64 value) {
    int ret = 0;
    while (value >>= 1) ret++;
    return ret;
  }

  //isLittleEndian (the Intel x86 processor represents a common little-endian architecture, and IBM z/Architecture mainframes are all big-endian processors)
  inline bool isLittleEndian() {
    union { Uint64 quad; Uint32 islittle; } test;
    test.quad = 1;
    return test.islittle ? true : false;
  }

  //isBigEndian
  inline bool isBigEndian() {
    return !isLittleEndian();
  }

  //see http://stackoverflow.com/questions/1001307/detecting-endianness-programmatically-in-a-c-program
  template <typename T>
  inline T fixEndian(T value)
  {
    T ret = 0;
    for (int I = 0; I < sizeof(ret); I++)
    {
      ret <<= 8;
      ret |= value & 0xff;
      value >>= 8;
    }
    return ret;
  };

  //Find the greatest common divisor of 2 numbers
  //See http://en.wikipedia.org/wiki/Greatest_common_divisor
  template <typename T>
  inline T greatestCommonDividor(T a, T b) {
    T c = 0; while (a != 0) { c = a; a = b%a; b = c; } return b;
  }

  //Find the least common multiple of 2 numbers
  //See http://en.wikipedia.org/wiki/Least_common_multiple
  template <typename T>
  inline T leastCommonMultiple(T a, T b) {
    return (b / greatestCommonDividor(a, b)) * a;
  }

  //isAligned
  template <typename T>
  inline bool isAligned(T value, T p1, T step) {
    return (step == 1) || ((value - p1) % step) == 0;
  }

  //alignToLeft
  template <typename T>
  inline T alignLeft(T value, T p1, T step)
  {
    if (step == 1) return value;
    T mod = (value - p1) % step;
    T ret = mod ? (value - mod) : (value);
    VisusAssert(isAligned<T>(ret, p1, step));
    return ret;
  }
  //alignRight
  template <typename T>
  inline T alignRight(T value, T p1, T step)
  {
    if (step == 1) return value;
    T mod = (value - p1) % step;
    T ret = mod ? (value + (step - mod)) : (value);
    VisusAssert(isAligned<T>(ret, p1, step));
    return ret;
  }

  //getBit
  inline bool getBit(const unsigned char* buffer, Int64 bit)
  {
    const Uint8& byte = buffer[bit >> 3];
    return (byte & (1 << (bit & 0x07))) ? true : false;
  }

  //setBit
  inline void setBit(unsigned char* buffer, Int64 bit, bool value)
  {
    //if you need atomic operation use setBitThreadSafe
    Uint8& Byte = buffer[bit >> 3];
    const Uint8 Mask = 1 << (bit & 0x07);
    Byte = value ? (Byte | Mask) : (Byte & (~Mask));
  }

  //setBitThreadSafe
  VISUS_KERNEL_API void setBitThreadSafe(unsigned char* buffer, Int64 bit, bool value);

  //notoverflow_add, result=a+b overflow happens when (a+b)>MAX ---> b>MAX-a
  template <typename coord_t>
  inline bool safe_add(coord_t& result, const coord_t& a, const coord_t& b)
  {
    if (!((a > 0 && b > 0) || (a < 0 && b < 0))) { result = a + b; return true; }
    if ((b > 0 ? +b : -b) > (NumericLimits<coord_t>::highest() - (a > 0 ? +a : -a))) return false;
    result = a + b;
    return true;
  }

  //notoverflow_sub (result=a-b)
  template <typename coord_t>
  inline bool safe_sub(coord_t& result, const coord_t& a, const coord_t& b) {
    return safe_add(result, a, -b);
  }

  //notoverflow_mul result=a*b overflow happens when (a*b)>MAX ----> b>MAX/a
  template <typename coord_t>
  inline bool safe_mul(coord_t& result, const coord_t& a, const coord_t& b)
  {
    if (!a || !b) { result = a*b; return true; }
    if ((b > 0 ? +b : -b) > (NumericLimits<coord_t>::highest() / (a > 0 ? +a : -a))) return false;
    result = a*b;
    return true;
  }

  //notoverflow_lshift, result=a<<shift overflow happens when (a<<shift)> MAX ----> a > MAX>>shift
  template <typename coord_t>
  inline bool safe_lshift(coord_t& result, const coord_t& a, const coord_t& shift)
  {
    VisusAssert(shift >= 0);
    if (!a) { result = 0; return true; }
    if ((a > 0 ? +a : -a) > (NumericLimits<coord_t>::highest() >> shift)) return false;
    result = a << shift;
    return true;
  }

  //breakInDebugger
  VISUS_KERNEL_API void breakInDebugger();

  //loadTextDocument
  VISUS_KERNEL_API String loadTextDocument(String url);

  //saveTextDocument
  VISUS_KERNEL_API void saveTextDocument(String url, String content);

  //loadTextDocument
  VISUS_KERNEL_API SharedPtr<HeapMemory> loadBinaryDocument(String url);

  //saveBinaryDocument
  VISUS_KERNEL_API void saveBinaryDocument(String url, SharedPtr<HeapMemory> src);

  //getPid
  VISUS_KERNEL_API int getPid();

  VISUS_KERNEL_API String getCurrentApplicationFile();

  //getenv
  VISUS_KERNEL_API String getEnv(String key);

  //setenv
  VISUS_KERNEL_API void setEnv(String key, String value);

}; //end class

//////////////////////////////////////////////////////
#if !SWIG
class VISUS_KERNEL_API DoAtExit
{
public:

  VISUS_CLASS(DoAtExit)

  std::function<void()> fn;

  //constructor
  DoAtExit(std::function<void()> fn_) : fn(fn_) {
  }

  //destructor
  ~DoAtExit() {
    if (fn)
      fn();
  }
};

#endif

} //namespace Visus

#endif //__VISUS_UTILS_H__


