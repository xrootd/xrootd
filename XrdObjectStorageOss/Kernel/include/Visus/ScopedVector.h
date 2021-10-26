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

#ifndef _VISUS_SCOPED_VECTOR_H__
#define _VISUS_SCOPED_VECTOR_H__

#include <Visus/Kernel.h>

#include <vector>
#include <algorithm>

namespace Visus {

template <class T>
class ScopedVector
{
public:

  typedef typename std::vector<T*>::iterator       iterator;
  typedef typename std::vector<T*>::const_iterator const_iterator;

  //constructor
  ScopedVector(){
  }

  //destructor
  inline ~ScopedVector() {
    clear();
  }

  //begin
  inline iterator begin() {
    return ptrs.begin();
  }

  //begin
  inline const_iterator begin() const {
    return ptrs.begin();
  }

  //end
  inline iterator end() {
    return ptrs.end();
  }

  //begin
  inline const_iterator end() const {
    return ptrs.end();
  }

  //find
  inline iterator find(T* item) {
    return std::find(ptrs.begin(), ptrs.end(), item);
  }

  //clear
  inline void clear() {
    for (iterator it = begin(); it != end(); it++) 
      reset(it); 
    ptrs.clear();
  }

  //push_back
  inline void push_back(T* VISUS_DISOWN(item)) {
    ptrs.push_back(item);
  }

  //push_front
  inline void push_front(T* VISUS_DISOWN(item)) {
    ptrs.insert(ptrs.begin(), item);
  }

  //pop_back
  inline void pop_back() {
    if (ptrs.back()) 
      delete ptrs.back(); 
    ptrs.pop_back();
  }

  //empty
  inline bool empty() const {
    return ptrs.empty();
  }

  //size
  inline int size() const {
    return (int)ptrs.size();
  }

  //resize
  inline void resize(int n)
  {
    while (n < size()) pop_back();
    while (n > size()) push_back(nullptr);
  }

  //front
  inline T* front() {
    return ptrs.front();
  }

  //front
  inline const T* front() const {
    return ptrs.front();
  }

  //back
  inline T* back() {
    return ptrs.back();
  }

  //back
  inline const T* back() const {
    return ptrs.back();
  }

  //operator[]
  inline T* operator[](int index) {
    return ptrs[index];
  }

  //operator[]
  inline const T* operator[](int index) const {
    return ptrs[index];
  }

  //release
  inline VISUS_NEWOBJECT(T*) release(int index) {
    VisusAssert(index >= 0 && index < size()); T* ret = ptrs[index]; 
    ptrs[index] = nullptr; 
    return ret;
  }

  //swap
  inline void swap(ScopedVector& other) {
    std::swap(ptrs, other.ptrs);
  }

  //reverse
  inline void reverse() {
    std::reverse(ptrs.begin(), ptrs.end());
  }


#if !SWIG

  //release
  inline T* release(iterator it) {
    VisusAssert(it >= begin() && it < end()); T* ret = (*it); (*it) = nullptr; return ret;
  }

  //reset
  inline void reset(iterator it) {
    VisusAssert(it >= begin() && it < end()); 
    if (*it) 
      delete (*it); 
    (*it) = nullptr;
  }

  //reset
  inline void reset(iterator it, T* VISUS_DISOWN(item)){
    VisusAssert(it >= begin() && it < end()); 
    if (*it) 
      delete (*it); 
    (*it) = item;
  }

  //reset
  inline void reset(int index, T* VISUS_DISOWN(item)) {
    VisusAssert(index >= 0 && index < size()); 
    if (ptrs[index]) 
      delete ptrs[index]; 
    ptrs[index] = item;
  }

  //insert
  inline void insert(iterator it, T* VISUS_DISOWN(item)) {
    VisusAssert(it >= begin() && it <= end()); ptrs.insert(it, item);
  }

  //erase
  inline void erase(iterator it) {
    VisusAssert(it >= begin() && it < end()); 
    if (*it) 
      delete (*it); 
    ptrs.erase(it);
  }
#endif


private:
  std::vector<T*> ptrs;
  VISUS_NON_COPYABLE_CLASS(ScopedVector)
};


} //namespace Visus


#endif //_VISUS_SCOPED_VECTOR_H__
