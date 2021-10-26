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

#include <Visus/LocalCoordinateSystem.h>

namespace Visus {

////////////////////////////////////////////////////////////////////
LocalCoordinateSystem::LocalCoordinateSystem(Point3d x_,Point3d y_,Point3d z_,Point3d c_) 
  : x(x_),y(y_),z(z_),c(c_)
{}

////////////////////////////////////////////////////////////////////
LocalCoordinateSystem::LocalCoordinateSystem(Point3d center_,int axis)
{
  switch (axis)
  {
    case 0:(*this)=LocalCoordinateSystem(Point3d(0,1,0),Point3d(0,0,1),Point3d(1,0,0),c);break;
    case 1:(*this)=LocalCoordinateSystem(Point3d(1,0,0),Point3d(0,0,1),Point3d(0,1,0),c);break;
    case 2:(*this)=LocalCoordinateSystem(Point3d(1,0,0),Point3d(0,1,0),Point3d(0,0,1),c);break;
  }
}

////////////////////////////////////////////////////////////////////
LocalCoordinateSystem::LocalCoordinateSystem(Matrix T)
{
  T.setSpaceDim(4);
  this->x=Point3d(T[ 0],T[ 4],T[ 8]); //c*this) * (1,0,0,0)
  this->y=Point3d(T[ 1],T[ 5],T[ 9]); //(*this) * (0,1,0,0)
  this->z=Point3d(T[ 2],T[ 6],T[10]); //(*this) * (0,0,1,0)
  this->c=Point3d(T[ 3],T[ 7],T[11]); //(*this) * (0,0,0,1)
}

////////////////////////////////////////////////////////////////////
LocalCoordinateSystem::LocalCoordinateSystem(const Position& pos)
{
  auto T=pos.getTransformation();
  auto box=pos.getBoxNd();
  auto points = box.getPoints();
  Point3d Pc=T * box.center().toPoint3();
  Point3d P0= (T * points[0]).toPoint3();
  Point3d Px= (T * points[1]).toPoint3();
  Point3d Py= (T * points[3]).toPoint3();
  Point3d Pz= (T * points[4]).toPoint3();
  this->x=0.5*(Px-P0);
  this->y=0.5*(Py-P0);
  this->z=0.5*(Pz-P0);
  this->c=Pc;
}

////////////////////////////////////////////////////////////////////
LocalCoordinateSystem LocalCoordinateSystem::toUniformSize() const 
{
  double M=Utils::max(
    this->getAxis(0).module(),
    this->getAxis(1).module(),
    this->getAxis(2).module());

  auto C=this->getCenter();
  auto X=this->getAxis(0).normalized();
  auto Y=this->getAxis(1).normalized();
  auto Z=this->getAxis(2).normalized();

  if      (!X.module2()) X=Y.cross(Z).normalized();
  else if (!Y.module2()) Y=Z.cross(X).normalized();
  else if (!Z.module2()) Z=X.cross(Y).normalized();
  
  return LocalCoordinateSystem(
    M * X,
    M * Y,
    M * Z,
    C);
}

////////////////////////////////////////////////////////////////////
void LocalCoordinateSystem::write(Archive& ar) const
{
  ar.write("x", x);
  ar.write("y", y);
  ar.write("z", z);
  ar.write("c", c);
}

////////////////////////////////////////////////////////////////////
void LocalCoordinateSystem::read(Archive& ar) 
{
  ar.read("x", x);
  ar.read("y", y);
  ar.read("z", z);
  ar.read("c", c);
}



} //namespace Visus