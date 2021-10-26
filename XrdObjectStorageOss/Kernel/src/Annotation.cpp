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

#include <Visus/Annotation.h>


namespace Visus {

/////////////////////////////////////////////////////////////////////////////
class SVGParser
{
public:

  Annotations& dst;

  //constructor
  SVGParser(Annotations& dst_) : dst(dst_){
  }

  //read
  void read(Archive& ar)
  {
    for (auto child : ar.getChilds())
      readGeneric(*child, StringMap());
  }

private:

  //readPoint
  static Point3d readPoint(String s)
  {
    auto v = StringUtils::split(s, ",");
    v.resize(2, "0.0");
    return Point3d(cdouble(v[0]), cdouble(v[1]), 0.0);
  }

  //readPoints
  static std::vector<Point3d> readPoints(String s)
  {
    std::vector<Point3d> ret;
    for (auto it : StringUtils::split(s, " "))
      ret.push_back(readPoint(it));
    return ret;
  }

  //readColor
  static Color readColor(const StringMap& attributes, String name)
  {
    auto color = Color::fromString(attributes.getValue(name));
    auto A = cdouble(attributes.getValue(name + "-opacity", "1.0"));
    return color.withAlpha(float(A));
  }

  //readGeneric
  void readGeneric(Archive& ar, StringMap attributes)
  {
    if (ar.name == "#comment")
      return;

    // accumulate attributes
    for (auto it : ar.attributes)
    {
      auto key = it.first;
      auto value = it.second;
      attributes.setValue(key, value);
    }

    if (ar.name == "g")
      return readGroup(ar, attributes);

    if (ar.name == "poi")
      return readPoi(ar, attributes);

    if (ar.name == "polygon")
      return readPolygon(ar, attributes);

    ThrowException("not supported");
  }

  //readGroup
  void readGroup(Archive& ar, const StringMap& attributes)
  {
    for (auto child : ar.getChilds())
      readGeneric(*child, attributes);
  }

  //readPoi
  void readPoi(Archive& ar, const StringMap& attributes)
  {
    auto poi = std::make_shared<PointOfInterest>();
    poi->point = readPoint(attributes.getValue("point"));
    poi->text = attributes.getValue("text");
    poi->magnet_size = cint(attributes.getValue("magnet-size", "20"));
    poi->stroke = readColor(attributes, "stroke");
    poi->stroke_width = cint(attributes.getValue("stroke-width"));
    poi->fill = readColor(attributes, "fill");
    dst.push_back(poi);
  }

  //readPolygon
  void readPolygon(Archive& ar, const StringMap& attributes)
  {
    auto polygon = std::make_shared<PolygonAnnotation>();
    polygon->points = readPoints(attributes.getValue("points"));
    polygon->stroke = readColor(attributes, "stroke");
    polygon->stroke_width = cint(attributes.getValue("stroke-width"));
    polygon->fill = readColor(attributes, "fill");
    dst.push_back(polygon);
  }

};


void Annotations::read(Archive& ar)
{
  SVGParser(*this).read(ar);
}

} //namespace Visus