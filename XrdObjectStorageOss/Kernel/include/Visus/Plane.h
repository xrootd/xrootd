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

#ifndef VISUS_PLANE_H
#define VISUS_PLANE_H

#include <Visus/Kernel.h>
#include <Visus/Point.h>

namespace Visus {

/////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Plane : public PointNd
{
public:

  VISUS_CLASS(Plane)

  //default constructor
  Plane() {
  }

  //constructor
  explicit Plane(std::vector<double> v) : PointNd(v) {
    (*this) *= 1.0 / getNormal().module();
  }

  //constructor
  explicit Plane(const PointNd& v) : Plane(v.toVector()) {
  }

  //constructor
  explicit Plane(double X,double Y,double Z,double W) 
    : Plane(PointNd(X,Y,Z,W)) {
  }

  //constructor from normal and distance
  explicit Plane(PointNd n,double d) 
  {
    this->PointNd::operator=(PointNd(n.normalized()));
    push_back(-d);
  }

  //constructor from normal and point
  explicit Plane(PointNd n,PointNd p)
  {
    n=n.normalized();
    this->PointNd::operator=(PointNd(n.toVector()));
    push_back(-(n.dot(p)));
  }

  //from 3 points
  explicit Plane(Point3d p0,Point3d p1,Point3d p2)
  {
    auto n=(p1-p0).cross(p2-p0).normalized();
    this->PointNd::operator=(PointNd(n.toVector()));
    push_back(-(n.dot(p0)));
  }

  //getSpaceDim
  int getSpaceDim() const {
    return PointNd::getPointDim();
  }

  //getNormal
  PointNd getNormal() const {
    return this->withoutBack();
  }

  //getDistance
  inline double getDistance(const PointNd& v) const  {
    return this->dot(PointNd(v, 1.0));
  }

  //! projectPoint (see http://www.9math.com/book/projection-Point3d-plane)
  inline PointNd projectPoint(PointNd P) const
  {
    auto N=getNormal();
    return P-(N * getDistance(P));
  }

  //! projectVector (see http://www.gamedev.net/community/forums/topic.asp?topic_id=345149&whichpage=1&#2255698)
  inline PointNd projectVector(PointNd V) const
  {
    auto N=getNormal();
    return V-(N *(V.dot(N)));
  }

private:

  //getPointDim
  int getPointDim() const {
    return PointNd::getPointDim();
  }



};//end class Plane


} //namespace Visus

#endif //VISUS_PLANE_H

