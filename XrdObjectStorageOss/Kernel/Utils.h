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

#include "Kernel.h"
#include "HeapMemory.h"
#include "StringUtils.h"

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

  //select
  template <typename T>
  inline std::vector<T> select(const std::vector<T>& v, std::function<bool(T)> predicate)
  {
    std::vector<T> ret;
    std::copy_if(v.begin(), v.end(), std::back_inserter(ret), predicate);
    return ret;
  }


  //pop_front
  template <typename T>
  inline void pop_front(std::vector<T>& v) {
    v.erase(v.begin());
  }

  //isValidNumber
  template<class T>
  typename std::enable_if< std::is_integral<T>::value , bool>::type
  isValidNumber(const T& value) {
    return true;
  }

  //isValidNumber
  template<class T>
  typename std::enable_if< std::is_floating_point<T>::value, bool>::type
  isValidNumber(const T& value) {
    return !std::isnan(value) && std::isfinite(value);
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

  //breakInDebugger
   void breakInDebugger();

  //loadTextDocument
   String loadTextDocument(String url);

  //saveTextDocument
   void saveTextDocument(String url, String content);

  //loadTextDocument
   SharedPtr<HeapMemory> loadBinaryDocument(String url);

  //saveBinaryDocument
   void saveBinaryDocument(String url, SharedPtr<HeapMemory> src);

  //getPid
   int getPid();

  //getenv
   String getEnv(String key);

  //setenv
   void setEnv(String key, String value);

}; //end class

} //namespace Visus

#endif //__VISUS_UTILS_H__


