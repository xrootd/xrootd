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
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THEg
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

#ifndef VISUS_POINT_H
#define VISUS_POINT_H

#include "Kernel.h"
#include "Utils.h"

#include <array>
#include <algorithm>
#include <type_traits>

namespace Visus {


//////////////////////////////////////////////////////////////
class PointNi
{

  int pdim = 0;

  std::array<Int64, 5> coords = std::array<Int64, 5>({ 0,0,0,0,0 });

public:

  typedef Int64 T;


  typedef typename std::array<T, 5>::iterator       iterator;
  typedef typename std::array<T, 5>::const_iterator const_iterator;

  //default constructor
  PointNi() {
  }

  //copy constructor
  PointNi(const PointNi& other) : pdim(other.pdim), coords(other.coords) {
  }

  //constructor
  explicit PointNi(const std::vector<T>& v) {
    for (auto it : v)
      push_back(it);
  }

  //constructor
  explicit PointNi(int pdim_) : pdim(pdim_) {
  }

  //constructor
  explicit PointNi(const PointNi& left, T right) : PointNi(left) {
    push_back(right);
  }

  //constructor
  explicit PointNi(T a, T b) {
    push_back(a);
    push_back(b);
  }

  //constructor
  explicit PointNi(T a, T b, T c) : PointNi(a,b) {
    push_back(c);
  }

  //constructor
  explicit PointNi(T a, T b, T c,T d) : PointNi(a,b,c) {
    push_back(d);
  }

  //constructor
  explicit PointNi(T a, T b, T c, T d,T e) : PointNi(a,b,c,d) {
    push_back(e);
  }

  //getPointDim
  int getPointDim() const {
    return pdim;
  }

  //push_back
  void push_back(T value) {
    VisusAssert(pdim < 5);
    coords[pdim++] = value;
  }

  //pop_back
  void pop_back() {
    VisusAssert(pdim > 0);
    coords[--pdim] = 0;
  }

  //withoutBack
  PointNi withoutBack() const {
    auto ret = *this;
    ret.pop_back();
    return ret;
  }

  //back
  T back() const {
    VisusAssert(pdim > 0);
    return coords[pdim - 1];
  }

  //back
  T& back() {
    VisusAssert(pdim > 0);
    return coords[pdim - 1];
  }

  //begin
  const_iterator begin() const {
    return coords.begin();
  }

  //begin
  const_iterator end() const {
    return coords.begin() + pdim;
  }

  //setPointDim
  void setPointDim(int new_pdim, T default_value = 0.0) {
    auto old_pdim = this->pdim;
    this->pdim = new_pdim;
    for (int I = old_pdim; I < new_pdim; I++)
      get(I) = default_value;
  }

  //get
  T& get(int i) {
    VisusAssert(i >= 0 && i < pdim);
    return coords[i];
  }

  //const
  const T& get(int i) const {
    VisusAssert(i >= 0 && i < pdim);
    return coords[i];
  }

  //a!=b
  bool operator!=(const PointNi& other) const {
    return !(operator==(other));
  }

  //dropHomogeneousCoordinate
  PointNi dropHomogeneousCoordinate() const {
    return applyOperation(*this, MulByCoeff<double>(1.0 / back())).withoutBack();
  }

  //castTo
  template <typename Other>
  Other castTo() const {
    auto pdim = getPointDim();
    auto ret = Other(pdim);
    for (int I = 0; I < pdim; I++)
      ret[I] = (typename Other::T)get(I);
    return ret;
  }

  //zero
  static PointNi zero(int pdim) {
    return PointNi(pdim);
  }

  //one
  static PointNi one(int pdim) {
    return PointNi(std::vector<T>(pdim, T(1)));
  }

  //toVector
  std::vector<T> toVector() const {
    return std::vector<T>(begin(),end());
  }

  //test if numers are ok
  bool valid() const {
    return checkAll<ConditionValidNumber>(*this);
  }

  //operator[]
  const T& operator[](int i) const {
    return get(i);
  }

  //operator[]
  T& operator[](int i) {
    return get(i);
  }

  //set
  PointNi& set(int i,T value){
    get(i)=value; return *this;
  }

  //operator-
  PointNi operator-()  const {
    return applyOperation<NegOp>(*this);
  }

  //a+b
  PointNi operator+(const PointNi& other)  const {
    return applyOperation<AddOp>(*this, other);
  }

  //a-b
  PointNi operator-(const PointNi& other)  const {
    return applyOperation<SubOp>(*this, other);
  }

  //a*s
  template <typename Coeff>
  PointNi operator*(Coeff coeff) const {
    return applyOperation(*this, MulByCoeff<Coeff>(coeff));
  }

  //a+=b
  PointNi& operator+=(const PointNi& other) {
    return ((*this) = (*this) + other);
  }

  //a-=b
  PointNi& operator-=(const PointNi& other) {
    return ((*this) = (*this) - other);
  }

  //a*=s
  PointNi& operator*=(T s) {
    return ((*this) = (*this) * s);
  }

  //min
  static PointNi min(const PointNi& a, const PointNi& b) {
    return applyOperation<MinOp>(a, b);
  }

  //max
  static PointNi max(const PointNi& a, const PointNi& b) {
    return applyOperation<MaxOp>(a, b);
  }

  static PointNi clamp(const PointNi& v, const PointNi& a,const PointNi& b) {
    return applyOperation<ClampOp>(v,a,b);
  }

  //module2
  T module2() const {
    return this->dot(*this);
  }

  //module
  double module() const {
    return std::sqrt((double)module2());
  }

  //distance between two points
  double distance(const PointNi& p) const {
    return (p - *this).module();
  }


  //abs
  PointNi abs() const {
    return applyOperation<AbsOp>(*this);
  }

  //inv
  PointNi inv() const {
    return applyOperation<InvOp>(*this);  
  }


  //min_element
  const_iterator min_element() const {
    return std::min_element(begin(), end());
  }

  //max_element
  const_iterator max_element() const {
    return std::max_element(begin(), end());
  }

  //min_element_index
  int min_element_index() const {
    return getPointDim()? (int)std::distance(begin(), min_element()) : -1;
  }

  //max_element_index
  int max_element_index() const {
    return getPointDim() ? (int)std::distance(begin(), max_element()) : -1;
  }

  bool checkAllEqual       (const PointNi& a, const PointNi& b) const { return checkAll< ConditionE  >(a, b); }
  bool checkAllLess        (const PointNi& a, const PointNi& b) const { return checkAll< ConditionL  >(a, b); }
  bool checkAllLessEqual   (const PointNi& a, const PointNi& b) const { return checkAll< ConditionLE >(a, b); }
  bool checkAllGreater     (const PointNi& a, const PointNi& b) const { return checkAll< ConditionG  >(a, b); }
  bool checkAllGreaterEqual(const PointNi& a, const PointNi& b) const { return checkAll< ConditionGE >(a, b); }

  //operator (<,<=,>,>=) (NOTE: it's different from lexigraphical order)

  bool operator==(const PointNi& b) const { return (this->pdim == b.pdim) && checkAllEqual(*this, b);}

  bool operator< (const PointNi& b) const { return checkAllLess        (*this, b); }
  bool operator<=(const PointNi& b) const { return checkAllLessEqual   (*this, b); }
  bool operator> (const PointNi& b) const { return checkAllGreater     (*this, b); }
  bool operator>=(const PointNi& b) const { return checkAllGreaterEqual(*this, b); }

public:

  //dot product
  T dot(const PointNi& other) const {
    VisusAssert(pdim == other.pdim);
    return
      (pdim >= 1 ? (coords[0] * other.coords[0]) : 0) +
      (pdim >= 2 ? (coords[1] * other.coords[1]) : 0) +
      (pdim >= 3 ? (coords[2] * other.coords[2]) : 0) +
      (pdim >= 4 ? (coords[3] * other.coords[3]) : 0) +
      (pdim >= 5 ? (coords[4] * other.coords[4]) : 0);
  }

  //dotProduct
  T dotProduct(const PointNi& other) const {
    return dot(other);
  }

  //stride 
  PointNi stride() const
  {
    auto pdim = getPointDim();
    auto ret = PointNi(pdim);
    ret[0] = 1;
    for (int I = 0; I < pdim - 1; I++)
      ret[I + 1] = ret[I] * get(I);
    return ret;
  }

  //innerMultiply
  PointNi innerMultiply(const PointNi& other) const {
    return applyOperation<MulOp>(*this, other);
  }

  //innerDiv
  PointNi innerDiv(const PointNi& other) const {
    return applyOperation<DivOp>(*this, other);
  }

  //innerProduct 
  T innerProduct() const
  {
    auto pdim = getPointDim();
    if (pdim == 0) 
      return 0;

    return
      (pdim >= 1 ? coords[0] : 1) *
      (pdim >= 2 ? coords[1] : 1) *
      (pdim >= 3 ? coords[2] : 1) *
      (pdim >= 4 ? coords[3] : 1) *
      (pdim >= 5 ? coords[4] : 1);
  }

  //innerMod
  //leftShift
  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointNi leftShift(const T & value) const {
    return applyOperation(*this, LShiftByValue(value));
  }

  //rightShift
  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointNi rightShift(const T & value) const {
    return applyOperation(*this, RShiftByValue(value));
  }

  //leftShift
  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointNi leftShift(const PointNi & value) const {
    return applyOperation<LShiftOp>(*this, value);
  }

  //rightShift
  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointNi rightShift(const PointNi & value) const {
    return applyOperation<RShiftOp>(*this, value);
  }

  template <typename = std::enable_if<std::is_integral<T>::value > >
  PointNi innerMod(const PointNi & other) const {
    return applyOperation<ModOp>(*this, other);
  }

public:


  //fromString
  static PointNi fromString(String src)
  {
    std::vector<T> ret;
    std::istringstream parser(src);
    T parsed; while (parser >> parsed)
      ret.push_back(parsed);
    return PointNi(ret);
  }

  //convert to string
  String toString(String sep = " ") const {
    auto pdim = getPointDim();
    std::ostringstream out;
    for (int I = 0; I < pdim; I++)
      out << (I ? sep : "") << get(I);
    return out.str();
  }

  //operator<<
  friend std::ostream& operator<<(std::ostream& out, const PointNi& p) {
    out << "<" << p.toString(",") << ">";
    return out;
  }

private:

  struct NegOp  { static T compute(T a) { return -a; } };
  struct AbsOp  { static T compute(T a) { return a >= 0 ? +a : -a; } };
  struct InvOp  { static T compute(T a) { return (T)(1.0 / a); } };

  struct AddOp { static T compute(T a, T b) { return a + b; } };
  struct SubOp { static T compute(T a, T b) { return a - b; } };
  struct MulOp { static T compute(T a, T b) { return a * b; } };
  struct DivOp { static T compute(T a, T b) { return a / (b?b:1); } };
  struct ModOp { static T compute(T a, T b) { return a % b; } };

  struct MinOp   { static T compute(T a, T b) { return std::min(a, b); } };
  struct MaxOp   { static T compute(T a, T b) { return std::max(a, b); } };

  struct ClampOp { static T compute(T v, T a, T b) { return Utils::clamp(v, a, b); } };

  struct LShiftOp { static T compute(T a, T b) { return a << b; } };
  struct RShiftOp { static T compute(T a, T b) { return a >> b; } };

  struct ConditionE  { static bool isTrue(T a, T b) { return a == b; } };
  struct ConditionL  { static bool isTrue(T a, T b) { return a <  b; } };
  struct ConditionLE { static bool isTrue(T a, T b) { return a <= b; } };
  struct ConditionG  { static bool isTrue(T a, T b) { return a >  b; } };
  struct ConditionGE { static bool isTrue(T a, T b) { return a >= b; } };

  struct ConditionValidNumber { static bool isTrue(T a) { return Utils::isValidNumber(a); } };

  template <typename Coeff>
  class MulByCoeff {
  public:
    Coeff value;
    MulByCoeff(Coeff value_ = Coeff(0)) : value(value_) {}
    T compute(T a) { return (T)(a * value); }
  };

  class LShiftByValue  {
  public:
    T value;
    LShiftByValue(T value_ = T(0)) : value(value_) {}
    T compute(T a) { return a << value; }
  };

  class RShiftByValue {
  public:
    T value;
    RShiftByValue(T value_ = T(0)) : value(value_) {}
    T compute(T a) { return a >> value; }
  };

  //applyOperation
  template <typename Operation>
  static PointNi applyOperation(const PointNi& a)
  {
    PointNi ret(a.getPointDim());
    ret.coords[0] = Operation::compute(a.coords[0]);
    ret.coords[1] = Operation::compute(a.coords[1]);
    ret.coords[2] = Operation::compute(a.coords[2]);
    ret.coords[3] = Operation::compute(a.coords[3]);
    ret.coords[4] = Operation::compute(a.coords[4]);
    return ret;
  }

  //applyOperation
  template <typename Operation>
  static PointNi applyOperation(const PointNi& a, Operation op)
  {
    PointNi ret(a.getPointDim());
    ret.coords[0] = op.compute(a.coords[0]);
    ret.coords[1] = op.compute(a.coords[1]);
    ret.coords[2] = op.compute(a.coords[2]);
    ret.coords[3] = op.compute(a.coords[3]);
    ret.coords[4] = op.compute(a.coords[4]);
    return ret;
  }

  //applyOperation
  template <typename Operation>
  static PointNi applyOperation(const PointNi& a, const PointNi& b)
  {
    auto pdim = a.getPointDim();
    VisusAssert(pdim == b.getPointDim());
    PointNi ret(pdim);
    ret.coords[0] = Operation::compute(a.coords[0], b.coords[0]);
    ret.coords[1] = Operation::compute(a.coords[1], b.coords[1]);
    ret.coords[2] = Operation::compute(a.coords[2], b.coords[2]);
    ret.coords[3] = Operation::compute(a.coords[3], b.coords[3]);
    ret.coords[4] = Operation::compute(a.coords[4], b.coords[4]);
    return ret;
  }

  //applyOperation
  template <typename Operation>
  static PointNi applyOperation(const PointNi& v, const PointNi& a, const PointNi& b)
  {
    auto pdim = a.getPointDim();
    VisusAssert(pdim == b.getPointDim());
    PointNi ret(pdim);
    ret.coords[0] = Operation::compute(v.coords[0], a.coords[0], b.coords[0]);
    ret.coords[1] = Operation::compute(v.coords[1], a.coords[1], b.coords[0]);
    ret.coords[2] = Operation::compute(v.coords[2], a.coords[2], b.coords[0]);
    ret.coords[3] = Operation::compute(v.coords[3], a.coords[3], b.coords[0]);
    ret.coords[4] = Operation::compute(v.coords[4], a.coords[4], b.coords[0]);
    return ret;
  }

  //checkAll
  template <typename Condition>
  static bool checkAll(const PointNi& a) {
    auto pdim = a.getPointDim();
    return
      (pdim < 1 || (Condition::isTrue(a.coords[0]) &&
      (pdim < 2 || (Condition::isTrue(a.coords[1]) &&
      (pdim < 3 || (Condition::isTrue(a.coords[2]) &&
      (pdim < 4 || (Condition::isTrue(a.coords[3]) &&
      (pdim < 5 || (Condition::isTrue(a.coords[4])))))))))));
  }

  //checkAll
  template <typename Condition>
  static bool checkAll(const PointNi& a, const PointNi& b) {
    auto pdim = a.getPointDim();
    VisusAssert(pdim == b.getPointDim());
    return
      (pdim < 1 || (Condition::isTrue(a.coords[0], b.coords[0]) &&
      (pdim < 2 || (Condition::isTrue(a.coords[1], b.coords[1]) &&
      (pdim < 3 || (Condition::isTrue(a.coords[2], b.coords[2]) &&
      (pdim < 4 || (Condition::isTrue(a.coords[3], b.coords[3]) &&
      (pdim < 5 || (Condition::isTrue(a.coords[4], b.coords[4])))))))))));
  }

};//end class PointNi

template <typename Value>
inline PointNi operator*(Value s, const PointNi& p) {
  return p * s;
}

} //namespace Visus


#endif //VISUS_POINT__H
