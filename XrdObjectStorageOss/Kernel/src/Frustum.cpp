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

#include <Visus/Frustum.h>

namespace Visus {


/////////////////////////////////////////////////////////////////////////
double Frustum::computeDistance(const Position& obj,Point2d screen_point,bool bUseFarPoint) const
{
  //Computes distance of a given object's bounding box along ray from camera through a given screen point.
  //Returns distance to nearest or farthest point of object, <0 for no intersection.

  const double failed=-1;

  if (!obj.valid())
    return failed;

  Frustum frustum=*this;
  frustum.multModelview(obj.getTransformation());

  auto ray=FrustumMap(frustum).getRay(screen_point);

  RayBoxIntersection intersection(ray,obj.getBoxNd());

  if (!intersection.valid)
    return failed;

  auto p1=frustum.getModelview()*ray.getPoint(bUseFarPoint?intersection.tmax:intersection.tmin);
  auto p2=frustum.getModelview()*ray.getPoint(0.0);

  double ret=(p2-p1).module();

  if (!Utils::isValidNumber(ret) || ret<0)
    return failed;

  return  ret;
}

/////////////////////////////////////////////////////////////////////////
double Frustum::computeZDistance(const Position& obj,bool bUseFarPoint) const
{
  //Returns distance to a plane perpendicular to camera direction containing nearest or farthest point of object.

  const double failed=-1;

  if (!obj.valid())
    return failed;

  std::vector<PointNd> points;
  for (auto point : obj.getPoints())
    points.push_back(getModelview() * point);

  //note: camera "looks" along negative Z axis, so we must negate the z values.
  bool objInFront=false;
  for (auto point : points)
  {
    objInFront = point[2] < 0.0;
    if (objInFront) break;
  }
  
  bool objBehind=false;
  for (auto point : points)
  {
    objBehind = point[2] >= 0.0;
    if (objBehind) break;
  }

  if (!objInFront) 
    return failed;

  if (bUseFarPoint)
  {
    double maxp=-points[0][2];
    for (auto point : points)
      maxp=std::max(maxp,-point[2]);
    return maxp;
  }

  if (objBehind) 
    return 1.0E-6;

  double minp=-points[0][2];
  for (auto point : points)
    minp=std::min(minp,-point[2]);
  return minp;
}

} //namespace Visus

