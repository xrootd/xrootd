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

#ifndef VISUS_RECTANGLE_H
#define VISUS_RECTANGLE_H

#include <Visus/Kernel.h>
#include <Visus/Point.h>

#include <algorithm>

namespace Visus {

/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Rectangle2i
{
public:

  VISUS_CLASS(Rectangle2i)

  Int64 x=0, y = 0, width = 0, height = 0;

  //constructor
  Rectangle2i() {
  }

  //constructor
  Rectangle2i(int x_, int y_, int width_, int height_) : x(x_), y(y_), width(width_), height(height_) {
  }

  //constructor
  Rectangle2i(Point2i p1, Point2i p2)
  {
    this->x = p1[0];
    this->y = p1[1];
    this->width = p2[0] - p1[0];
    this->height = p2[1] - p1[1];
  }

  //constructor from string
  explicit Rectangle2i(String value) {
    std::istringstream parser(value); parser >> x >> y >> width >> height;
  }

  //getUnion
  Rectangle2i getUnion(const Rectangle2i& other) const
  {
    if (!this->valid()) return other;
    if (!other.valid()) return *this;
    Point2i p1(std::min(this->p1()[0], other.p1()[0]), std::min(this->p1()[1], other.p1()[1]));
    Point2i p2(std::max(this->p2()[0], other.p2()[0]), std::max(this->p2()[1], other.p2()[1]));
    return Rectangle2i(p1, p2);
  }

  //getUnion
  Rectangle2i getIntersection(const Rectangle2i& other) const
  {
    if (!this->valid()) return *this;
    if (!other.valid()) return other;
    Point2i p1(std::max(this->p1()[0], other.p1()[0]), std::max(this->p1()[1], other.p1()[1]));
    Point2i p2(std::min(this->p2()[0], other.p2()[0]), std::min(this->p2()[1], other.p2()[1]));
    return Rectangle2i(p1, p2);
  }

  //scaleAroundCenter
  Rectangle2i scaleAroundCenter(double sx, double sy) const
  {
    Point2i half_size((int)(0.5*width*sx), (int)(0.5*height*sy));
    return Rectangle2i(center() - half_size, center() + half_size);
  }

  //valid
  bool valid() const {
    return width > 0 && height > 0;
  }

  //containsPoint
  bool containsPoint(Point2i p) const {
    return valid() && p[0] >= x && p[0] < (x + width) && p[1] >= y && p[1] < (y + height);
  }

  //p1
  Point2i p1() const {
    return Point2i(x, y);
  }

  //p2
  Point2i p2() const {
    return Point2i(x + width, y + height);
  }

  //center
  Point2i center() const {
    return Point2i(x + width / 2, y + height / 2);
  }

  //size
  Point2i size() const {
    return Point2i(width, height);
  }

  //operator==
  bool operator==(const Rectangle2i& other) const {
    return x == other.x && y == other.y && width == other.width && height == other.height;
  }

  //operator==
  bool operator!=(const Rectangle2i& other) const {
    return !(operator==(other));
  }

  //write
  void write(Archive& ar) const
  {
    ar.write("x", x);
    ar.write("y", y);
    ar.write("width", width);
    ar.write("height", height);
  }

  //read
  void read(Archive& ar)
  {
    ar.read("x", x);
    ar.read("y", y);
    ar.read("width", width);
    ar.read("height", height);
  }

  //convert to string
  String toString() const  {
    return cstring(x, y, width, height);
  }

}; //end class Reclangle2i

/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Rectangle2d
{
public:

  VISUS_CLASS(Rectangle2d)

  double x=0, y = 0, width = 0, height = 0;

  //constructor
  Rectangle2d() {
  }

  //constructor
  Rectangle2d(double x_, double y_, double width_, double height_) : x(x_), y(y_), width(width_), height(height_) {
  }

  //constructor
  Rectangle2d(Point2d p1, Point2d p2)
  {
    x = std::min(p1[0], p2[0]);
    y = std::min(p1[1], p2[1]);
    width = fabs(p2[0] - p1[0]);
    height = fabs(p2[1] - p1[1]);
  }

  //destructor
  ~Rectangle2d() {
  }

  //fromString
  static Rectangle2d fromString(String s) {
    Rectangle2d ret;
    std::istringstream parser(s);
    parser >> ret.x >> ret.y >> ret.width >> ret.height;
    return ret;
  }

  //valid
  bool valid() const {
    return width > 0 && height > 0;
  }

  //getAspectRatio
  double getAspectRatio() const {
    return width / double(height);
  }

  //containsPoint
  bool containsPoint(Point2d p) const {
    return valid() && p[0] >= x && p[0] < (x + width) && p[1] >= y && p[1] < (y + height);
  }

  //p1
  Point2d p1() const {
    return Point2d(x, y);
  }

  //p2
  Point2d p2() const {
    return Point2d(x + width, y + height);
  }

  //center
  Point2d center() const {
    return 0.5*(p1() + p2());
  }

  //size
  Point2d size() const {
    return Point2d(width, height);
  }

  //translate
  Rectangle2d translate(double tx, double ty) const {
    return Rectangle2d(x + tx, y + ty, width, height);
  }

  //translate
  Rectangle2d translate(Point2i vt) const {
    return translate((double)vt[0], (double)vt[1]);
  }

  //scale
  Rectangle2d scale(double sx, double sy) const {
    return Rectangle2d(x*sx, y*sy, width, height*sy);
  }

  //scale
  Rectangle2d scale(const Point2d vs) const {
    return scale(vs[0], vs[1]);
  }

  //operator==
  bool operator==(const Rectangle2d& other) const {
    return x == other.x && y == other.y && width == other.width && height == other.height;
  }

  //operator==
  bool operator!=(const Rectangle2d& other) const {
    return !(operator==(other));
  }

  //write
  void write(Archive& ar) const
  {
    ar.write("x", x);
    ar.write("y", y);
    ar.write("width", width);
    ar.write("height", height);
  }

  //read
  void read(Archive& ar)
  {
    ar.read("x", x);
    ar.read("y", y);
    ar.read("width", width);
    ar.read("height", height);
  }

  //convert to string
  String toString() const {
    return cstring(x, y, width, height);
  }

}; //end class Rectangle2d

typedef Rectangle2d Viewport;

} //namespace Visus

#endif //VISUS_RECTANGLE_H


