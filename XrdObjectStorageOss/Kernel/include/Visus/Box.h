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

#ifndef VISUS_BOX_H
#define VISUS_BOX_H

#include <Visus/Kernel.h>
#include <Visus/Plane.h>

#include <algorithm>

namespace Visus {

///////////////////////////////////////////////////////////////////
template <typename T>
class BoxN 
{
public:
  
  typedef PointN<T> Point;

  //points (see valid() function)
  Point p1, p2;

  //constructor
  BoxN() {
  }

  //copy constructor (needed by swig)
  BoxN(const BoxN& other) : p1(other.p1), p2(other.p2) {
}

  //constructor
  BoxN(int pdim) : p1(pdim), p2(pdim) {
  }

  //constructor
  BoxN(Point p1_, Point p2_) : p1(p1_), p2(p2_) {
    VisusAssert(p1.getPointDim() == p2.getPointDim());
  }

  //construtor
  BoxN(const std::vector<Point>& points) : BoxN()
  {
    for (auto point : points)
      addPoint(point);
  }

  //getPointDim
  int getPointDim() const {
    return p1.getPointDim();
  }

  //setPointDim
  void setPointDim(int pdim) {
    p1.setPointDim(pdim);
    p2.setPointDim(pdim);
  }

  //withPointDim
  BoxN withPointDim(int pdim) const {
    auto ret = *this;
    ret.setPointDim(pdim);
    return ret;
  }

  //withoutBack
  BoxN withoutBack() const {
    return BoxN(p1.withoutBack(), p2.withoutBack());
  }

  //return an invalid box (dimension 0, see addPoint)
  static BoxN invalid() {
    return BoxN();
  }

  //valid
  bool valid() const {
    return getPointDim()>0 && p1 <= p2;
  }

  //isFullDim
  bool isFullDim() const {
    return getPointDim()>0 && p1<p2;
  }

  //center
  Point center() const {
    return 0.5*(p1 + p2);
  }

  //size
  Point size() const {
    return p2 - p1;
  }

  //maxsize
  T maxsize() const {
    return *size().max_element();
  }

  //maxsize_index
  int maxsize_index() const {
    auto s = size(); return(int)std::distance(s.begin(), s.max_element());
  }

  //minsize
  T minsize() const {
    return *size().min_element();
  }

  //minsize_index
  int minsize_index() const {
    auto s = size(); return (int)std::distance(s.begin(), s.min_element());
  }

  //middle
  Point middle() const {
    return 0.5*(p1 + p2);
  }

  //addPoint
  void addPoint(Point p) {
    if (!this->valid())
    {
      this->p1 = p;
      this->p2 = p;
      return;
    }
    
    auto pdim = std::max(p.getPointDim(), this->getPointDim());
    p.setPointDim(pdim);
    this->setPointDim(pdim);
    this->p1 = Point::min(this->p1, p);
    this->p2 = Point::max(this->p2, p);
  }
  
  //toBox3
  BoxN toBox3() const {
    return this->withPointDim(3);
  }

  //test if a point is inside the box
  bool containsPoint(Point p) const {
    return this->p1 <= p && p <= this->p2;
  }

  //test if two box are equal
  bool operator==(const BoxN& b) const {
    return p1 == b.p1 && p2 == b.p2;
  }

  //test equality
  bool operator!=(const BoxN& b) const {
    return !(this->operator==(b));
  }

  //intersect
  bool intersect(const BoxN& other) const {
    return valid() && other.valid() ? (p1 <= other.p2 && p2 >= other.p1) : false;
  }

  //strictIntersect
  bool strictIntersect(const BoxN& other) const {
    return valid() && other.valid() ? (p1<other.p2 && p2>other.p1) : false;
  }

  //get intersection of two boxes
  BoxN getIntersection(BoxN b) const {
    auto a = *this;
    if (!a.valid()) return a;
    if (!b.valid()) return b;

    //must have the same dimension
#if 1
    VisusAssert(a.getPointDim() == b.getPointDim());
#else
    auto pdim = std::max(a.getPointDim(),b.getPointDim());
    a.setPointDim(pdim);
    b.setPointDim(pdim);
#endif

    return BoxN(
      Point::max(a.p1, b.p1), 
      Point::min(a.p2, b.p2));
  }

  //get union of two boxes
  BoxN getUnion(BoxN b) const {
    auto a = *this;
    if (!a.valid()) return b;
    if (!b.valid()) return a;

    //must have the same dimension
#if 1
    VisusAssert(a.getPointDim() == b.getPointDim());
#else
    auto pdim = std::max(a.getPointDim(),b.getPointDim());
    a.setPointDim(pdim);
    b.setPointDim(pdim);
#endif

    return BoxN(
      Point::min(a.p1, b.p1),
      Point::max(a.p2, b.p2));
  }

  //containsBox
  bool containsBox(const BoxN& other) const {
    return p1 <= other.p1 && other.p2 <= p2;
  }

  //scaleAroundCenter
  BoxN scaleAroundCenter(double scale)
  {
    Point center = this->center();
    Point size = scale*(p2 - p1);
    return BoxN(center - size*0.5, center + size*0.5);
  }

  //getSlab
  BoxN getSlab(int axis, T v1, T v2) const {
    auto p1 = this->p1; p1[axis] = v1;
    auto p2 = this->p2; p2[axis] = v2;
    return BoxN(p1,p2);
  }

  BoxN getXSlab(T x1, T x2) const { return getSlab(0, x1, x2); }
  BoxN getYSlab(T y1, T y2) const { return getSlab(1, y1, y2); }
  BoxN getZSlab(T z1, T z2) const { return getSlab(2, z1, z2); }

  //translate
  BoxN translate(const Point& vt) const {
    return BoxN(p1 + vt, p2 + vt);
  }

  //getPoints
  std::vector<Point> getPoints() const {
    auto pdim = getPointDim();
    if (pdim == 0)
      return std::vector<Point>();

    if (pdim == 1)
      return std::vector<Point>({ this->p1,this->p2 });

    //note: the order is important for 2d and 3d
    if (pdim == 2)
      return std::vector<Point>({ Point(p1[0], p1[1]), Point(p2[0], p1[1]), Point(p2[0], p2[1]), Point(p1[0], p2[1])});
      
    //recursive
    std::vector<Point> ret;

    auto prev_points = withoutBack().getPoints();
    for (auto point : prev_points)
      ret.push_back(Point(point, this->p1.back()));

    for (auto point : prev_points)
      ret.push_back(Point(point, this->p2.back()));

    return ret;
  }

#if !SWIG
  class Edge
  {
  public:
    int axis;
    int index0;
    int index1;
    Edge(int axis_ = 0, int index0_ = 0, int index1_ = 0) : axis(axis_), index0(index0_), index1(index1_) {}
  };

  typedef std::vector<Edge> Edges;
  static Edges getEdges(int pdim)
  {
    if (pdim == 2)
    {
      return Edges({
        Edge(0,0,1),
        Edge(1,1,2),
        Edge(0,2,3),
        Edge(1,3,0),
        });
    }

    if (pdim == 3)
    {
      return Edges({
          Edge(0,0,1),
          Edge(1,1,2),
          Edge(0,2,3),
          Edge(1,3,0),
          Edge(0,4,5),
          Edge(1,5,6),
          Edge(0,6,7),
          Edge(1,7,4),
          Edge(2,0,4),
          Edge(2,1,5),
          Edge(2,2,6),
          Edge(2,3,7)
        });
    }

    ThrowException("internal error");
    return Edges();
  };
#endif

  //getAlphaPoint
  Point getAlphaPoint(Point alpha) const {
    return this->p1 + alpha.innerMultiply(this->p2-this->p1);
  }

  //return the planes (pointing outside)
  std::vector<Plane> getPlanes() const
  {
    int pdim = getPointDim();
    std::vector<Plane> ret;
    for (int I = 0; I < pdim; I++)
    {
      std::vector<double> h1(pdim + 1, 0.0);  h1[I] = (double)-1; h1.back() = (double)+this->p1[I]; ret.push_back(Plane(h1));
      std::vector<double> h2(pdim + 1, 0.0);  h2[I] = (double)+1; h2.back() = (double)-this->p2[I]; ret.push_back(Plane(h2));
    }
    return ret;
  }

  //castTo
  template <typename Other>
  Other castTo() const {
    return Other(
                 this->p1.template castTo<typename Other::Point>(),
                 this->p2.template castTo<typename Other::Point>());
  }

public:

  //construct from string
  static BoxN fromString(String value,bool bInterleave=true)
  {
    std::istringstream parser(value);

    //x1 x2   y1 y2   z1 z2
    if (bInterleave)
    {
      std::vector<T> v1, v2; T value1, value2;
      while (parser >> value1 >> value2) {
        v1.push_back(value1); v2.push_back(value2);
      }
      return BoxN(Point(v1), Point(v2));
    }
    //x1 y1 z1   x2 y2 z2
    else
    {
      std::vector<T> v;  T parsed;
      while (parser >> parsed)
        v.push_back(parsed);
      auto N = v.size() / 2;
      VisusAssert(N * 2 == v.size());
      return BoxN(
        Point(std::vector<T>(v.begin(), v.begin() + N)),
        Point(std::vector<T>(v.begin() + N, v.end())));
    }
  }

  //construct to string
  String toString(bool bInterleave=true) const  
  {
    int pdim = getPointDim();
    if (!pdim) return "";
    
    //x1 x2   y1 y2   z1 z2
    if (bInterleave)
    {
      std::ostringstream out;
      for (int I = 0; I < pdim; I++)
        out << (I ? " " : "") << p1[I] << " " << p2[I];
      return out.str();
    }
    //x1 y1 z1   x2 y2 z2
    else
    {
      return cstring(p1,p2);
    }
  }

  //toOldFormatString 
  String toOldFormatString() const
  {
    auto tmp = (*this);
    tmp.p2 = tmp.p2 - Point::one(getPointDim());
    return tmp.toString(/*bInterleave*/true);
  }

  //parseFromOldFormatString
  static BoxN parseFromOldFormatString(int pdim,String src)
  {
    auto ret = BoxN::fromString(src).withPointDim(pdim);
    ret.p2 += Point::one(pdim);
    return ret;
  }

  //write`
  void write(Archive& ar) const
  {
    ar.write("p1", p1);
    ar.write("p2", p2);
  }

  //write
  void read(Archive& ar)
  {
    ar.read("p1", p1);
    ar.read("p2", p2);
  }


}; //end class BoxN

typedef BoxN<double> BoxNd;
typedef BoxN<Int64 > BoxNi;

///////////////////////////////////////////////////////////////////
//backward compatible (for VisusWeaving)
class VISUS_KERNEL_API Box3d
{
public:

  typedef Point3d Point;

  //points (see valid() function)
  Point p1, p2;

  //constructor
  Box3d() {
  }

  //constructor
  Box3d(Point p1_, Point p2_) : p1(p1_), p2(p2_) {
  }

  //return an invalid box
  static Box3d invalid()
  {
    double L = -std::numeric_limits<double>::max();
    double H = +std::numeric_limits<double>::max();
    return Box3d(Point(H, H, H), Point(L, L, L));
  }

  //valid (note: an axis can have zero dimension, trick to store slices too)
  bool valid() const {
    return p1.valid() && p2.valid() && p1.x <= p2.x && p1.y <= p2.y  && p1.z <= p2.z;
  }

  //center
  Point center() const {
    return 0.5*(p1 + p2);
  }

  //size
  Point size() const {
    return p2 - p1;
  }

  //middle
  Point middle() const {
    return 0.5*(p1 + p2);
  }

  //addPoint
  void addPoint(Point p) {
    this->p1 = Point::min(this->p1, p);
    this->p2 = Point::max(this->p2, p);
  }

  //getPoint
  Point getPoint(double alpha, double beta, double gamma) const {
    return p1 + Point(alpha*(p2.x - p1.x), beta*(p2.y - p1.y), gamma*(p2.z - p1.z));
  }

  //test if a point is inside the box
  bool containsPoint(Point p) const {
    return this->p1 <= p && p <= this->p2;
  }

  //test if two box are equal
  bool operator==(const Box3d& b) const {
    return p1 == b.p1 && p2 == b.p2;
  }

  //test equality
  bool operator!=(const Box3d& b) const {
    return !(this->operator==(b));
  }

  //intersect
  bool intersect(const Box3d& other) const {
    return valid() && other.valid() ? (p1 <= other.p2 && p2 >= other.p1) : false;
  }

  //get intersection of two boxes
  Box3d getIntersection(const Box3d& b) const {
    Box3d ret;
    ret.p1 = Point::max(this->p1, b.p1);
    ret.p2 = Point::min(this->p2, b.p2);
    return ret;
  }

  //get union of two boxes
  Box3d getUnion(const Box3d& b) const {
    Box3d ret;
    ret.p1 = Point::min(this->p1, b.p1);
    ret.p2 = Point::max(this->p2, b.p2);
    return ret;
  }

  //construct to string
  String toString() const {
    return p1.toString() + " " + p2.toString();
  }

  //toBoxNd
  BoxNd toBoxNd() const {
    return BoxNd(PointNd(p1.x,p1.y,p1.z),PointNd(p2.x, p2.y, p2.z));
  }

  //toBoxNd
  BoxNi toBoxNi() const {
    return BoxNi(PointNi((int)p1.x, (int)p1.y, (int)p1.z), PointNi((int)p2.x, (int)p2.y, (int)p2.z));
  }

  //fromBoxNi
  static Box3d fromBoxNi(const BoxNd src) {
    auto dim = src.getPointDim();
    return Box3d(
      Point3d(dim >= 1 ? src.p1[0] : 0, dim >= 2 ? src.p1[1] : 0, dim >= 3 ? src.p1[2] : 0),
      Point3d(dim >= 1 ? src.p2[0] : 0, dim >= 2 ? src.p2[1] : 0, dim >= 3 ? src.p2[2] : 0));
  }

  //fromBoxNd
  static Box3d fromBoxNd(const BoxNd src) {
    auto dim = src.getPointDim();
    return Box3d(
      Point3d(dim >= 1 ? src.p1[0] : 0, dim >= 2 ? src.p1[1] : 0, dim >= 3 ? src.p1[2] : 0),
      Point3d(dim >= 1 ? src.p2[0] : 0, dim >= 2 ? src.p2[1] : 0, dim >= 3 ? src.p2[2] : 0));
  }

};

} //namespace Visus

#endif //VISUS_BOX_H


