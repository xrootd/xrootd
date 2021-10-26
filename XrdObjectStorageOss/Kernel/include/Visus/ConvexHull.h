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

#ifndef VISUS_CONVEX_HULL_H
#define VISUS_CONVEX_HULL_H

#include <Visus/Kernel.h>

namespace Visus {


/////////////////////////////////////////////////////////////
class VISUS_KERNEL_API ConvexHull
{
  std::vector<Point3d> points;
  std::vector<Plane>   planes;
  BoxNd                aabb=BoxNd::invalid();

public:

  VISUS_CLASS(ConvexHull)

  //default constructor
  ConvexHull() 
  {}

  //constructor
  ConvexHull(const std::vector<Point3d>& points_,const std::vector<Plane>& planes_) : points(points_),planes(planes_)
  {
    for (int I=0;I<(int)points.size();I++)
      aabb.addPoint(points[I]);
  }

  //getPoints
  const std::vector<Point3d>& getPoints() const
  {return points;}

  //getPlanes 
  const std::vector<Plane>& getPlanes() const
  {return planes;}

  //getAxisAlignedBoundingBox
  const BoxNd& getAxisAlignedBoundingBox() const
  {return aabb;}

  //containsPoint
  inline bool containsPoint(const Point3d& p)
  {
    if (planes.empty()) 
      return false;

    for (int P=0;P<(int)planes.size();P++)
      {if (planes[P].getDistance(p)>0) return false;} //outside

    return true;
  }

  //intersectBox
  inline bool intersectBox(BoxNd box)
  {
    if (!this->aabb.valid() || !box.valid())
      return false;

    box.setPointDim(3);

    //trivial and fast check if they overlap
    if (!this->aabb.intersect(box))
      return false;

    /*
                 _____
                |     |
                |     |
    ___________ | box |
    \         / |     |
     \ this  /  |_____|
      \     /
       \___/
    */

    for (auto plane : planes)
    {
      VisusAssert(plane.getSpaceDim() == 4);
      double min_distance= plane.getDistance(Point3d(
        plane[0]>=0?box.p1[0]:box.p2[0] ,
        plane[1]>=0?box.p1[1]:box.p2[1] ,
        plane[2]>=0?box.p1[2]:box.p2[2]));
      if (min_distance>=0) return false;
    }

    return true; //can I have false positive?
  }

  //intersectConvexHull 
  inline bool intersectConvexHull(const ConvexHull& B)
  {
    const ConvexHull& A=*this;

    if (!A.aabb.valid() || !B.aabb.valid())
      return false;

    //trivial and fast check if they overlap
    if (!A.aabb.intersect(B.aabb))
      return false;


    /*
                 ___________
                 \         /
      ___________ \   B   / 
      \         /  \     /  
       \   A   /    \___/   
        \     /
         \___/
    */

    for (int I=0;I<(int)A.planes.size();I++)
    {
      double min_distance=NumericLimits<double>::highest();
      for (int J=0;J<(int)B.points.size();J++)
        min_distance=std::min(min_distance,A.planes[I].getDistance(B.points[J]));
      if (min_distance>=0) return false;
    }

    for (int I=0;I<(int)B.planes.size();I++)
    {
      double min_distance=NumericLimits<double>::highest();
      for (int J=0;J<(int)A.points.size();J++)
        min_distance=std::min(min_distance,B.planes[I].getDistance(A.points[J]));
      if (min_distance>=0) return false;
    }

    return true; //can have false positive?
  }

};

} //namespace Visus

#endif //VISUS_CONVEX_HULL_H

