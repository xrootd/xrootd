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

#ifndef VISUS_RANGE_H
#define VISUS_RANGE_H

#include <Visus/Kernel.h>
#include <Visus/NumericLimits.h>
#include <Visus/Utils.h>
#include <Visus/Aborted.h>
#include <Visus/StringTree.h>

#include <algorithm>

namespace Visus {

////////////////////////////////////////////////////////
class VISUS_KERNEL_API Range 
{
public:

  VISUS_CLASS(Range)

  double from;
  double to;
  double step;

  //constructor
  Range() : from(0.0), to(0.0), step(0.0) {
  }

  //copy constructor
  Range(const Range &r) : from(r.from), to(r.to), step(r.step) {
  }

  //constructor
  Range(double from, double to, double step)
  {
    this->from = from;
    this->to = to;
    this->step = step;
  }

  //fromString
  static Range fromString(const String& value)
  {
    Range ret;
    std::istringstream in(value);
    in >> ret.from >> ret.to >> ret.step;
    return ret;
  }

  //numeric_limits
  template <typename CppType>
  static inline Range numeric_limits()
  {
    Range ret;
    ret.from = NumericLimits<CppType>::lowest();
    ret.to = NumericLimits<CppType>::highest();
    ret.step = 0;
    return ret;
  }

  //destructor
  virtual ~Range()
  {}

  //operator==
  inline bool operator==(const Range& other) const {
    return from == other.from && to == other.to && step == other.step;
  }

  inline bool operator!=(const Range& other) const {
    return !(operator==(other));
  }

  //contains
  inline bool contains(double value) const {
    return from <= value && value <= to;
  }

  //invalid
  static Range invalid() {
    return Range(NumericLimits<double>::highest(), NumericLimits<double>::lowest(),0);
  }


  //delta
  inline double delta() const {
    return to - from;
  }

  //clamp
  inline double clamp(double v) const {
    return Utils::clamp(v, from, to);
  }

  //toString
  String toString() const {
    return cstring(this->from,this->to,this->step);
  }

  //getUnion
  inline Range getUnion(const Range& other) const
  {
    Range ret;
    ret.from = std::min(this->from, other.from);
    ret.to = std::max(this->to, other.to);
    ret.step = 0;
    return ret;
  }

  //getIntersection
  inline Range getIntersection(const Range& other) const
  {
    Range ret;
    ret.from = std::max(this->from, other.from);
    ret.to = std::min(this->to, other.to);
    ret.step = 0;
    return ret;
  }


public:

  //write
  void write(Archive& ar) const
  {
    ar.write("from", from);
    ar.write("to", to);
    ar.write("step", step);
  }

  //read
  void read(Archive& ar)
  {
    ar.read("from", from);
    ar.read("to", to);
    ar.read("step", step);
  }

};


} //namespace Visus

#endif //VISUS_RANGE_H

