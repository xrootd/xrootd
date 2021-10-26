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

#include <Visus/Ray.h>
#include <Visus/FindRoots.h>


namespace Visus {

static inline bool overlap1d(double __a,double __b,double __p,double __q)
  {return ((__a)<=(__q) && (__b)>=(__p));}


///////////////////////////////////////////////////////////////////////////
RayPlaneIntersection::RayPlaneIntersection(const Ray& ray,const Plane& plane4)  : valid(false),t(0)
{
  double t= -plane4.getDistance(ray.getOrigin())/(plane4.getNormal().dot(ray.getDirection()));
  if (!Utils::isValidNumber(t)) return;

  this->valid=true;
  this->t      =t;
  this->point  =ray.getPoint(t);
}

///////////////////////////////////////////////////////////////////////////
RayBoxIntersection::RayBoxIntersection(const Ray& ray,const BoxNd& box) : valid(false),tmin(0),tmax(0)
{
  VisusAssert(ray.getPointDim() == 3);
  VisusAssert(box.getPointDim() == 3);

  double& tmin=this->tmin;
  double& tmax=this->tmax;

  double txmin,txmax;
  PointNd inv_direction=ray.getDirection().inv();

  txmin  = ((inv_direction[0]>=0? box.p1[0] : box.p2[0]) - ray.getOrigin()[0]) * inv_direction[0];
  txmax  = ((inv_direction[0]>=0? box.p2[0] : box.p1[0]) - ray.getOrigin()[0]) * inv_direction[0];

  tmin=txmin;
  tmax=txmax;

  double tymin = ((inv_direction[1]>=0? box.p1[1] : box.p2[1]) - ray.getOrigin()[1]) * inv_direction[1];
  double tymax = ((inv_direction[1]>=0? box.p2[1] : box.p1[1]) - ray.getOrigin()[1]) * inv_direction[1];

  if (!overlap1d(tmin,tmax,tymin,tymax)) return ;
  if (tymin > tmin) tmin = tymin;
  if (tymax < tmax) tmax = tymax;

  double tzmin = ((inv_direction[2]>=0? box.p1[2] : box.p2[2]) - ray.getOrigin()[2]) * inv_direction[2];
  double tzmax = ((inv_direction[2]>=0? box.p2[2] : box.p1[2]) - ray.getOrigin()[2]) * inv_direction[2];

  if (!overlap1d(tmin,tmax,tzmin,tzmax)) return ;
  if (tzmin > tmin) tmin = tzmin;
  if (tzmax < tmax) tmax = tzmax;

  this->valid=true;
}

///////////////////////////////////////////////////////////////////
RaySphereIntersection::RaySphereIntersection(const Ray& ray,const Sphere& sp) : valid(false),tmin(0),tmax(0)
{
  // find intersection of ray p=(o + t*v) and sphere (|p-c|^2=r^2).
  //
  // Sphere-line intersection: |o + t*v - c|^2 - r^2 = 0
  // ==> (t*v + (o-c))^2 - r^2 = 0
  // ==> t^2v^2 + 2tv(o-c) + (o-c)^2 - r^2 = 0
  //        ---     ------   -------------
  //         A       B/2          C
  // ==> quadratic: t=(-b +- sqrt(b^2 - 4ac)) / 2a
  // evaluate determinant d to determine # of intersections:
  //  if d < 0, no intersections
  //  if d = 0, 1 tangential intersection
  //   if d > 0, 2 intersections

  //Giorgio scorzelli: are you sure this code is correct?! when you are sure remove the assert
  //also please use FindRoots::solve()
  VisusAssert(false);

  auto oc=ray.getOrigin() - sp.center;

  double B = ray.getDirection().dot(oc);  // B/2
  double C = oc.dot(oc) - sp.radius * sp.radius;
  double d = 4.0f * (B * B - C);

  if (d<0)
    return ; //not valid

  double part2 = -0.5f * sqrt(d);

  this->valid=true;
  this->tmin=std::min(part2 - B, -B - part2);
  this->tmax=std::max(part2 - B, -B - part2);
}

/////////////////////////////////////////////////////////////////////////////////////////
RaySegmentDistance::RaySegmentDistance(const Ray& ray,const Segment& segment) : distance(0),closest_ray_point(0,0,0),closest_segment_point(0,0,0)
{
  //see http://www.geometrictools.com/LibMathematics/Distance/DistanceBody.html

  auto   diff   =  ray.getOrigin()- segment.getCenter();
  double a01    = -ray.getDirection().dot(segment.getDirection());
  double b0     =  diff.dot(ray.getDirection());
  double b1     = -diff.dot(segment.getDirection());
  double c      =  diff.module2();
  double det    =  fabs((double)1 - a01*a01);
  double extent =  0.5*segment.length();

  double s0, s1, squared_distance, extDet;

  const double ZERO_TOLERANCE = 1e-06f;
  if (det >= ZERO_TOLERANCE)
  {
    // The ray and segment are not parallel.
    s0 = a01*b1 - b0;
    s1 = a01*b0 - b1;
    extDet = extent * det;

    if (s0 >= (double)0)
    {
      if (s1 >= -extDet)
      {
        if (s1 <= extDet)  // region 0
        {
          // Minimum at interior points of ray and segment.
          double invDet = ((double)1)/det;
          s0 *= invDet;
          s1 *= invDet;
          squared_distance = s0*(s0 + a01*s1 + ((double)2)*b0) +  s1*(a01*s0 + s1 + ((double)2)*b1) + c;
        }
        else  // region 1
        {
          s1 = extent;
          s0 = -(a01*s1 + b0);
          if (s0 > (double)0)
          {
            squared_distance = -s0*s0 + s1*(s1 + ((double)2)*b1) + c;
          }
          else
          {
            s0 = (double)0;
            squared_distance = s1*(s1 + ((double)2)*b1) + c;
          }
        }
      }
      else  // region 5
      {
        s1 = -extent;
        s0 = -(a01*s1 + b0);
        if (s0 > (double)0)
        {
          squared_distance = -s0*s0 + s1*(s1 + ((double)2)*b1) + c;
        }
        else
        {
          s0 = (double)0;
          squared_distance = s1*(s1 + ((double)2)*b1) + c;
        }
      }
    }
    else
    {
      if (s1 <= -extDet)  // region 4
      {
        s0 = -(-a01*extent + b0);
        if (s0 > (double)0)
        {
          s1 = -extent;
          squared_distance = -s0*s0 + s1*(s1 + ((double)2)*b1) + c;
        }
        else
        {
          s0 = (double)0;
          s1 = -b1;
          if (s1 < -extent)
          {
            s1 = -extent;
          }
          else if (s1 > extent)
          {
            s1 = extent;
          }
          squared_distance = s1*(s1 + ((double)2)*b1) + c;
        }
      }
      else if (s1 <= extDet)  // region 3
      {
        s0 = (double)0;
        s1 = -b1;
        if (s1 < -extent)
        {
          s1 = -extent;
        }
        else if (s1 > extent)
        {
          s1 = extent;
        }
        squared_distance = s1*(s1 + ((double)2)*b1) + c;
      }
      else  // region 2
      {
        s0 = -(a01*extent + b0);
        if (s0 > (double)0)
        {
          s1 = extent;
          squared_distance = -s0*s0 + s1*(s1 + ((double)2)*b1) + c;
        }
        else
        {
          s0 = (double)0;
          s1 = -b1;
          if (s1 < -extent)
          {
            s1 = -extent;
          }
          else if (s1 > extent)
          {
            s1 = extent;
          }
          squared_distance = s1*(s1 + ((double)2)*b1) + c;
        }
      }
    }
  }
  else
  {
    // Ray and segment are parallel.
    if (a01 > (double)0)
    {
      // Opposite direction vectors.
      s1 = -extent;
    }
    else
    {
      // Same direction vectors.
      s1 = extent;
    }

    s0 = -(a01*s1 + b0);
    if (s0 > (double)0)
    {
      squared_distance = -s0*s0 + s1*(s1 + ((double)2)*b1) + c;
    }
    else
    {
      s0 = (double)0;
      squared_distance = s1*(s1 + ((double)2)*b1) + c;
    }
  }

  // Account for numerical round-off errors.
  if (squared_distance < (double)0)
    squared_distance = (double)0;


  this->distance=sqrt(squared_distance);
  this->closest_ray_point     = ray    .getOrigin() + s0 * ray    .getDirection();
  this->closest_segment_point = segment.getCenter() + s1 * segment.getDirection();
}


////////////////////////////////////////////////////////////////////////////
RayPointDistance::RayPointDistance(const Ray& ray,const PointNd& point) : distance(0),closest_ray_point(0,0,0)
{
  // see http://www.geometrictools.com/LibMathematics/Distance/Distance.html
  
  auto diff = point - ray.getOrigin();
  
  double mRayParameter = ray.getDirection().dot(diff);
  
  if (mRayParameter > (double)0)
  {
    this->closest_ray_point = ray.getOrigin() + mRayParameter*ray.getDirection();
  }
  else
  {
    this->closest_ray_point = ray.getOrigin();
    mRayParameter = (double)0;
  }

  this->distance=(this->closest_ray_point - point).module();
}



/////////////////////////////////////////////////////////////////////////
RayCircleDistance::RayCircleDistance(const Ray& ray,const Circle& circle) : distance(0),closest_ray_point(0,0,0),closest_circle_point(0,0,0)
{
  VisusAssert(ray.getPointDim() == 3);

  //see http://www.geometrictools.com/LibMathematics/Distance/Distance.html

  // Static distance queries.  Compute the distance from the point P to the
  // circle.  When P is on the normal line C+t*N where C is the circle
  // center and N is the normal to the plane containing the circle, then
  // all circle points are equidistant from P.  In this case the returned
  // point is (infinity,infinity,infinity).

  auto   diff = ray.getOrigin() - circle.getCenter();
  double diffSqrLen = diff.module2();
  double MdM = ray.getDirection().module2();
  double DdM = diff.dot(ray.getDirection());
  double NdM = ray.getDirection().dot(circle.getNormal());
  double DdN = diff.dot(circle.getNormal());

  double a0 = DdM;
  double a1 = MdM;
  double b0 = DdM - NdM*DdN;
  double b1 = MdM - NdM*NdM;
  double c0 = diffSqrLen - DdN*DdN;
  double c1 = b0;
  double c2 = b1;
  double rsqr = circle.getRadius()*circle.getRadius();

  double a0sqr = a0*a0;
  double a1sqr = a1*a1;
  double twoA0A1 = ((double)2)*a0*a1;
  double b0sqr = b0*b0;
  double b1Sqr = b1*b1;
  double twoB0B1 = ((double)2)*b0*b1;
  double twoC1 = ((double)2)*c1;

  // The minimum point B+t*M occurs when t is a root of the quartic
  // equation whose coefficients are defined below.

  std::vector<double> polyroots=FindRoots::solve
  (
    a0sqr*c0 - b0sqr*rsqr,
    twoA0A1*c0 + a0sqr*twoC1 - twoB0B1*rsqr,
    a1sqr*c0 + twoA0A1*twoC1 + a0sqr*c2 - b1Sqr*rsqr,
    a1sqr*twoC1 + twoA0A1*c2,
    a1sqr*c2
  );

  double min_dist = NumericLimits<double>::highest();
  for (int i = 0; i < (int)polyroots.size(); ++i)
  {
    // Compute distance from P(t) to circle.
    auto P = ray.getOrigin() + polyroots[i]*ray.getDirection();

    CirclePointDistance query=circle.getDistance(P.toPoint3());

    double dist = query.distance;
    if (dist < min_dist)
    {
      min_dist = dist;
      this->closest_ray_point    = P;
      this->closest_circle_point = query.closest_disk_point;
    }
  }

  this->distance=min_dist;
}



///////////////////////////////////////////////////////////////////
RayLineDistance::RayLineDistance(const Ray& ray,const Line& line) : distance(0),closest_ray_point(0,0,0)
{
  //http://www.geometrictools.com/LibMathematics/Distance/Wm5DistLine3Ray3.cpp

  auto kDiff = line.getOrigin() - ray.getOrigin();
  double a01 = -ray.getDirection().dot(line.getDirection());
  double b0 = kDiff.dot(line.getDirection());
  double c = kDiff.module2();
  double det = fabs((double)1 - a01*a01);
  double b1, s0, s1, sqrDist;

  const double ZERO_TOLERANCE=1e-6;

  if (det >= ZERO_TOLERANCE)
  {
    b1 = -kDiff.dot(ray.getDirection());
    s1 = a01*b0 - b1;

    if (s1 >= (double)0)
    {
      // Two interior points are closest, one on line and one on ray.
      double invDet = ((double)1)/det;
      s0 = (a01*b1 - b0)*invDet;
      s1 *= invDet;
      sqrDist = s0*(s0 + a01*s1 + ((double)2)*b0) +
        s1*(a01*s0 + s1 + ((double)2)*b1) + c;
    }
    else
    {
      // Origin of ray and interior point of line are closest.
      s0 = -b0;
      s1 = (double)0;
      sqrDist = b0*s0 + c;
    }
  }
  else
  {
    // Lines are parallel, closest pair with one point at ray origin.
    s0 = -b0;
    s1 = (double)0;
    sqrDist = b0*s0 + c;
  }

  this->closest_ray_point  = ray .getOrigin() + s1*ray .getDirection();
  this->closest_line_point = line.getOrigin() + s0*line.getDirection();

  // Account for numerical round-off errors.
  if (sqrDist < (double)0)
    sqrDist = (double)0;

  this->distance=sqrt(sqrDist);
}

} //namespace Visus


