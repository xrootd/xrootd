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

#ifndef VISUS_SEGMENT_H
#define VISUS_SEGMENT_H

#include <Visus/Kernel.h>

namespace Visus {

/////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Segment
{
public:

  VISUS_CLASS(Segment)

  Point3d p1,p2;

  //constructor
  inline Segment()
    :p1(0,0,0),p2(0,0,-1)
  {}

  //constructor
  inline Segment(Point3d p1_,Point3d p2_) :p1(p1_),p2(p2_) 
  {}

  //length
  inline double length() const
  {return (p2-p1).module();}

  //get a point to a certain distance
  inline Point3d getPoint(double t) const
  {
    VisusAssert(t>=0 && t<=1);
    return ((1-t)*p1)+(t*p2);
  }

  //getCenter
  inline Point3d getCenter() const
  {return 0.5*(p1+p2);}

  //getDirection
  inline Point3d getDirection() const
  {return (p2-p1).normalized();}


  //P' = A + tAB (t = (AB • AP) / || AB ||²)
  //http://answers.yahoo.com/question/index?qid=20080218071458AAYz1s1
  inline double getPointProjection(const Point3d& P) const
  {double t=(p2-p1).dot(P-p1)/(p2-p1).module2();return t;}


}; //end class Segment

} //namespace Visus

#endif //VISUS_SEGMENT_H

