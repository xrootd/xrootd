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

#ifndef VISUS_CIRCLE_H
#define VISUS_CIRCLE_H

#include <Visus/Kernel.h>

namespace Visus {

/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API CirclePointDistance
{
public:

  VISUS_CLASS(CirclePointDistance)

  double distance;
  Point3d  closest_disk_point;
  inline CirclePointDistance() : distance(0),closest_disk_point(0,0,0) {}
};


/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Circle
{
public:

  VISUS_CLASS(Circle)

  //constructor
  inline Circle() :center(0,0,0),radius(0),normal(0,0,1)
  {}

  //constructor
  inline Circle(Point3d center_,double radius_,Point3d normal_) :center(center_),radius(radius_),normal(normal_.normalized())
    {}

  //getCenter
  inline Point3d getCenter() const
    {return center;}

  //getRadius
  inline double getRadius() const
    {return radius;}

  //getNormal
  inline Point3d getNormal() const
    {return normal;}

  //getDistance;
  CirclePointDistance getDistance(const Point3d& point) const
  {
    const Circle& circle=*this;

    CirclePointDistance ret;

    // see http://www.geometrictools.com/LibMathematics/Distance/Distance.html

    // Static distance queries.  Compute the distance from the point P to the
    // circle.  When P is on the normal line C+t*N where C is the circle
    // center and N is the normal to the plane containing the circle, then
    // all circle points are equidistant from P.  In this case the returned
    // point is (infinity,infinity,infinity).

    // Signed distance from point to plane of circle.
    Point3d diff0 = point - circle.getCenter();
    double dist = diff0.dot(circle.getNormal());

    // Projection of P-C onto plane is Q-C = P-C - (fDist)*N.
    Point3d diff1 = diff0 - dist*circle.getNormal();
    double sqrLen = diff1.module2();
    double sqrDistance;

    const double ZERO_TOLERANCE=1e-6;

    if (sqrLen >= ZERO_TOLERANCE)
    {
      ret.closest_disk_point = circle.getCenter() + (circle.getRadius()/sqrt(sqrLen))*diff1;
      Point3d diff2 = point - ret.closest_disk_point;
      sqrDistance = diff2.module2();
    }
    else
    {
      double Max=NumericLimits<double>::highest();
      ret.closest_disk_point = Point3d(Max,Max,Max);
      sqrDistance = circle.getRadius()*circle.getRadius() + dist*dist;
    }

    ret.distance=sqrt(sqrDistance);
    return ret;
  }

protected:


  Point3d  center;
  double radius;
  Point3d  normal;

}; //end class Circle

} //namespace Visus

#endif //VISUS_CIRCLE_H

