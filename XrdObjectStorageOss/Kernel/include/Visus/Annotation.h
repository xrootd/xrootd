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

#ifndef VISUS_ANNOTATION_H__
#define VISUS_ANNOTATION_H__

#include <Visus/Kernel.h>
#include <Visus/Color.h>
#include <Visus/Matrix.h>
#include <Visus/Point.h>

namespace Visus {


////////////////////////////////////////////////////////
class VISUS_KERNEL_API Annotation
{
public:

  Color   stroke;
  int     stroke_width = 1;
  Color   fill;

  //constructor
  virtual ~Annotation() {
  }

  //cloneAnnotation
  virtual SharedPtr<Annotation> cloneAnnotation() const = 0;

  //prependModelview
  virtual void prependModelview(Matrix T) = 0;

};

//////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Annotations
{
public:

  typedef std::vector< SharedPtr<Annotation> >::iterator       iterator;
  typedef std::vector< SharedPtr<Annotation> >::const_iterator const_iterator;

  bool enabled = true;

  //constructor
  Annotations() {
  }

  //destructor
  ~Annotations() {
  }

  //empty
  bool empty() const {
    return v.empty();
  }

  //size
  size_t size() const {
    return v.size();
  }

  //push_back
  void push_back(SharedPtr<Annotation> value) {
    v.push_back(value);
  }

  //begin
  iterator begin() {
    return v.begin();
  }

  //end
  iterator end() {
    return v.end();
  }

  //begin
  const_iterator begin() const {
    return v.begin();
  }

  //end
  const_iterator end() const {
    return v.end();
  }

  //read
  void read(Archive& ar);

private:

  std::vector< SharedPtr<Annotation> > v;
};

///////////////////////////////////////////////////////
class VISUS_KERNEL_API PointOfInterest : public Annotation
{
public:

  Point3d point;
  int     magnet_size = 0;
  String  text;

  //destructor
  virtual ~PointOfInterest() {
  }

  //cloneAnnotation
  virtual SharedPtr<Annotation> cloneAnnotation() const override {
    return std::make_shared<PointOfInterest>(*this);
  }

  //prependModelview
  virtual void prependModelview(Matrix T) override {
    T.setSpaceDim(4);
    this->point = (T * PointNd(this->point)).toPoint3();
  }

};



///////////////////////////////////////////////////////
class VISUS_KERNEL_API PolygonAnnotation : public Annotation
{
public:

  std::vector<Point3d> points;

  //destructor
  virtual ~PolygonAnnotation() {
  }

  //clone
  virtual SharedPtr<Annotation> cloneAnnotation() const override {
    return std::make_shared<PolygonAnnotation>(*this);
  }

  //prependModelview
  virtual void prependModelview(Matrix T) override {
    T.setSpaceDim(4);
    for (auto& point : points)
      point = (T * PointNd(point)).toPoint3();
  }

};





} //namespace Visus

#endif //VISUS_ANNOTATION_H__
