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

#ifndef VISUS_RAY_H
#define VISUS_RAY_H

#include <Visus/Kernel.h>
#include <Visus/Box.h>
#include <Visus/Matrix.h>
#include <Visus/Sphere.h>
#include <Visus/Line.h>
#include <Visus/Segment.h>
#include <Visus/Circle.h>

namespace Visus {


/////////////////////////////////////////////////////////////////////
class Ray
{
public:

  VISUS_CLASS(Ray)

  PointNd origin;
  PointNd direction;


  //constructor
  Ray() {
  }

  //constructor
  Ray(PointNd origin_, PointNd direction_)
    :origin(origin_),direction(direction_.normalized()) {
  }

  //fromTwoPoints
  static Ray fromTwoPoints(const PointNd& A, const PointNd& B) {
    return Ray(A,(B-A));
  }

  //getPointDim
  int getPointDim() const {
    return origin.getPointDim();
  }

  //valid
  bool valid() const {
    return this->origin.valid() && this->direction.module2();
  }

  //getOrigin
  const PointNd& getOrigin() const {
    return this->origin;
  }

  //getDirection
  const PointNd& getDirection() const {
    return this->direction;
  }

  //get a point to a certain distance
  PointNd getPoint(double alpha) const {
    return origin+ alpha *direction;
  }

  //transformByMatrix 
  Ray transformByMatrix(const Matrix&  M) const {
    return Ray::fromTwoPoints(M * getPoint(0), M * getPoint(1));
  }

  //findIntersectionOnZeroPlane
  PointNd findIntersectionOnZeroPlane() const {
    VisusAssert(origin.getPointDim() == 3);
    VisusAssert(direction[2] != 0);
    double alpha = -origin[2] / direction[2];
    return getPoint(alpha);
  }

}; //end class Ray



/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API RayBoxIntersection
{
public:

  VISUS_CLASS(RayBoxIntersection)

  bool   valid;
  double tmin;
  double tmax;

  RayBoxIntersection(const Ray& ray,const BoxNd& box);
};

/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API RayPlaneIntersection
{
public:

  VISUS_CLASS(RayPlaneIntersection)

  bool     valid;
  double   t;
  PointNd    point;

  RayPlaneIntersection(const Ray& ray,const Plane& plane);
};


/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API RaySphereIntersection
{
public:

  VISUS_CLASS(RaySphereIntersection)

  bool   valid;
  double tmin;
  double tmax;

  RaySphereIntersection(const Ray& ray,const Sphere& sp);
};


/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API RayPointDistance
{
public:

  VISUS_CLASS(RayPointDistance)

  double   distance;
  PointNd    closest_ray_point ;

  RayPointDistance(const Ray& ray,const PointNd& point);
};


/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API RayLineDistance
{
public:

  VISUS_CLASS(RayLineDistance)

  double distance;
  PointNd  closest_ray_point ;
  PointNd  closest_line_point ;

  RayLineDistance(const Ray& ray,const Line& line);
};

/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API RaySegmentDistance
{
public:

  VISUS_CLASS(RaySegmentDistance)

  double distance;
  PointNd  closest_ray_point    ;
  PointNd  closest_segment_point;

  RaySegmentDistance(const Ray& ray,const Segment& segment);
};

/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API RayCircleDistance
{
public:

  VISUS_CLASS(RayCircleDistance)

  double distance;
  PointNd  closest_ray_point;
  PointNd  closest_circle_point;

  RayCircleDistance(const Ray& ray,const Circle& circle);

};


} //namespace Visus

#endif //VISUS_RAY_H

