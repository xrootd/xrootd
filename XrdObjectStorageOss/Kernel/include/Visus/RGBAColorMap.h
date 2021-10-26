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

#ifndef VISUS_RGBA_COLORMAP_H
#define VISUS_RGBA_COLORMAP_H

#include <Visus/Kernel.h>
#include <Visus/Model.h>
#include <Visus/Color.h>
#include <Visus/Array.h>

namespace Visus {

////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API InterpolationMode
{
public:

  VISUS_CLASS(InterpolationMode)

    enum Type {
    Default,
    Flat,
    Inverted
  };

  //InterpolationMode
  InterpolationMode(Type type_ = Default) : type(type_) {
  }

  //InterpolationMode
  static InterpolationMode fromString(String s) {
    if (s == "Default")  return Default;
    else if (s == "Flat")     return Flat;
    else if (s == "Inverted") return Inverted;
    return Default;
  }

  //toString
  String toString() const
  {
    switch (type)
    {
    case Flat: return "Flat";
    case Inverted: return "Inverted";
    case Default:
    default: return "Default";
    }
  }

  //get
  Type get() {
    return type;
  }

  //getValues
  static std::vector<String> getValues() {
    return { "Default" ,"Flat","Inverted" };
  }

private:

  Type type = Default;

};


/////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API RGBAColorMap
{
public:

  VISUS_CLASS(RGBAColorMap)

  typedef std::pair<double, Color> Point;

  double             min_x = NumericLimits<double>::highest();
  double             max_x = NumericLimits<double>::lowest();
  std::vector<Point> colors;
  InterpolationMode::Type interpolation = InterpolationMode::Default;

  //constructor
  RGBAColorMap() {
  }

  //constructor
  RGBAColorMap(const double* values, size_t num);

  //destructor
  ~RGBAColorMap() {
  }

  //setColorAt
  void setColorAt(double x, Color color) {
    VisusAssert(colors.empty() || colors.back().first <= x);
    this->colors.push_back(std::make_pair(x,color));
    min_x = std::min(min_x, x);
    max_x = std::max(max_x, x);
  }

  //colorAt
  Color colorAt(double x) const;

  //toArray
  Array toArray(int nsamples) const;

};

} //namespace Visus

#endif //VISUS_RGBA_COLORMAP_H

