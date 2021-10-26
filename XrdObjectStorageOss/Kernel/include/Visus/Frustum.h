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

#ifndef VISUS_FRUSTUM_H
#define VISUS_FRUSTUM_H

#include <Visus/Kernel.h>
#include <Visus/Matrix.h>
#include <Visus/Rectangle.h>
#include <Visus/Box.h>
#include <Visus/ConvexHull.h>
#include <Visus/Position.h>
#include <Visus/Ray.h>


namespace Visus {

/////////////////////////////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Frustum 
{
public:

  VISUS_CLASS(Frustum)

  //! return the frustum points
  enum 
  {
    POINT_NEAR_BOTTOM_LEFT,
    POINT_NEAR_BOTTOM_RIGHT,
    POINT_NEAR_TOP_RIGHT,
    POINT_NEAR_TOP_LEFT,
    POINT_FAR_BOTTOM_LEFT,
    POINT_FAR_BOTTOM_RIGHT,
    POINT_FAR_TOP_RIGHT,
    POINT_FAR_TOP_LEFT
  };

  enum 
  {
    PLANE_LEFT,
    PLANE_RIGHT,
    PLANE_TOP,
    PLANE_BOTTOM,
    PLANE_NEAR,
    PLANE_FAR
  };

  //constructor
  Frustum(){
  }

  //constructor
  Frustum(const Viewport& viewport_,const Matrix& projection_,const Matrix& modelview_)
    : viewport(viewport_) ,projection(projection_),modelview(modelview_) {
  }


  //constructor
  Frustum(const Frustum& other, const Matrix& right) : Frustum(other){
    this->multModelview(right);
  }

  //operator==
  bool operator==(const Frustum& other) const {
    return modelview==other.modelview && projection==other.projection && viewport==other.viewport;
  }

  //operator==
  bool operator!=(const Frustum& other) const {
    return !(operator==(other));
  }

  //get
  const Matrix&   getModelview () const {return modelview ;}
  const Matrix&   getProjection() const {return projection;}
  const Viewport& getViewport  () const {return viewport  ;}

  //valid
  bool valid() const {
    return modelview.valid() && projection.valid() && viewport.valid();
  }

  //screen box
  BoxNd getScreenBox() const
  {
    Point3d p1(viewport.x                ,viewport.y                 ,0);
    Point3d p2(viewport.x +viewport.width,viewport.y +viewport.height,1);
    return BoxNd(p1,p2);
  }

  //load
  void loadModelview (const Matrix&   T) {modelview =T;}
  void loadProjection(const Matrix&   T) {projection=T;}
  void setViewport   (const Viewport& V) {viewport  =V;}

  //mult
  void multModelview (const Matrix& T) {if (T.isIdentity()) return;modelview *=T; }
  void multProjection(const Matrix& T) {if (T.isIdentity()) return;projection*=T; }

  //pickMatrix
  Matrix pickMatrix(double x, double y, double dx, double dy) const{
    return (dx <= 0 || dy <= 0)? Matrix::identity(4) : // If we don't have a valid region we return the identity
      Matrix::translate(Point3d((getViewport().width-2*(x-getViewport().x))/dx,(getViewport().height-2*(y-getViewport().y))/dy,0)) *
      Matrix::scale(Point3d(getViewport().width/dx,getViewport().height/dy, 1));
  }


  //getConvexHull
  ConvexHull getConvexHull()
  {
    std::vector<Point3d> points;
    {
      points.resize(8);
      Matrix T=(getProjection() * getModelview()).invert();
      points[POINT_NEAR_BOTTOM_LEFT ]=(T * Point3d(-1, -1, -1));
      points[POINT_NEAR_BOTTOM_RIGHT]=(T * Point3d(+1, -1, -1));
      points[POINT_NEAR_TOP_RIGHT   ]=(T * Point3d(+1, +1, -1));
      points[POINT_NEAR_TOP_LEFT    ]=(T * Point3d(-1, +1, -1));
      points[POINT_FAR_BOTTOM_LEFT  ]=(T * Point3d(-1, -1, +1));
      points[POINT_FAR_BOTTOM_RIGHT ]=(T * Point3d(+1, -1, +1));
      points[POINT_FAR_TOP_RIGHT    ]=(T * Point3d(+1, +1, +1));
      points[POINT_FAR_TOP_LEFT     ]=(T * Point3d(-1, +1, +1));
    }

    //(see http://www.cs.otago.ac.nz/postgrads/alexis/planeExtraction.pdf)
    std::vector<Plane> planes;
    {
      planes.resize(6);
      Matrix T=(getProjection() * getModelview()).transpose();
      planes[PLANE_LEFT  ]=(Plane(-(T[ 3] + T[ 0]),-(T[ 7] + T[ 4]),-(T[11] + T[ 8]),-(T[15] + T[12])));
      planes[PLANE_RIGHT ]=(Plane(-(T[ 3] - T[ 0]),-(T[ 7] - T[ 4]),-(T[11] - T[ 8]),-(T[15] - T[12])));
      planes[PLANE_TOP   ]=(Plane(-(T[ 3] - T[ 1]),-(T[ 7] - T[ 5]),-(T[11] - T[ 9]),-(T[15] - T[13])));
      planes[PLANE_BOTTOM]=(Plane(-(T[ 3] + T[ 1]),-(T[ 7] + T[ 5]),-(T[11] + T[ 9]),-(T[15] + T[13])));
      planes[PLANE_NEAR  ]=(Plane(-(T[ 3] + T[ 2]),-(T[ 7] + T[ 6]),-(T[11] + T[10]),-(T[15] + T[14])));
      planes[PLANE_FAR   ]=(Plane(-(T[ 3] - T[ 2]),-(T[ 7] - T[ 6]),-(T[11] - T[10]),-(T[15] - T[14])));

    }
    return ConvexHull(points,planes);
  }

  //computeDistance (return <0 if error)
  double computeDistance(const Position& obj,Point2d screen_point,bool bUseFarPoint=false) const;

  //computeZDistance (return <0 if error)
  double computeZDistance(const Position& obj,bool bUseFarPoint=false) const;

  //getViewportDirectTransformation (-1,+1)x(-1,+1) -> (x,x+width)x(y,y+height)
  static Matrix getViewportDirectTransformation(const Viewport& viewport)
  {
    double sx = viewport.width  / 2.0; double ox = viewport.x + viewport.width  / 2.0;
    double sy = viewport.height / 2.0; double oy = viewport.y + viewport.height / 2.0;
    double sz = 1 / 2.0; double oz = 1 / 2.0;

    return Matrix(
      sx, 0, 0, ox,
      0, sy, 0, oy,
      0, 0, sz, oz,
      0, 0, 0, 1);
  }

  //getViewportDirectTransformation
  Matrix getViewportDirectTransformation() const {
    return getViewportDirectTransformation(viewport);
  }

  //getViewportInverseTransformation
  static Matrix getViewportInverseTransformation(const Viewport& viewport)
  {
    double sx = viewport.width  / 2.0; double ox = viewport.x + viewport.width  / 2.0;
    double sy = viewport.height / 2.0; double oy = viewport.y + viewport.height / 2.0;
    double sz = 1 / 2.0; double oz = 1 / 2.0;

    return Matrix(
      1 / sx, 0, 0, -ox / sx,
      0, 1 / sy, 0, -oy / sy,
      0, 0, 1 / sz, -oz / sz,
      0, 0, 0, 1);
  }

  //getViewportInverseTransformation
  Matrix getViewportInverseTransformation() const {
    return getViewportInverseTransformation(viewport);
  }

protected:

  Matrix   modelview = Matrix::identity(4);
  Matrix   projection = Matrix::identity(4);
  Viewport viewport;

};//end class



/////////////////////////////////////////////////////////////
class VISUS_KERNEL_API FrustumMap : public LinearMap
{
public:

  VISUS_CLASS(FrustumMap)

  Frustum     frustum;

  MatrixMap   viewport;
  MatrixMap   projection;
  MatrixMap   modelview;

  //constructor
  FrustumMap(){
  }

  //constructor
  FrustumMap(const Frustum& frustum) 
  {
    this->frustum    = frustum;
    this->viewport   = MatrixMap(frustum.getViewportDirectTransformation(), frustum.getViewportInverseTransformation());
    this->projection = frustum.getProjection();
    this->modelview  = frustum.getModelview ();
    VisusAssert(getSpaceDim() == 4);
  }

  //getSpaceDim
  virtual int getSpaceDim() const override {
    auto ret = modelview.getSpaceDim();;
    VisusAssert(ret == 4);
    return ret;
  }

  //setSpaceDim
  virtual void setSpaceDim(int value) override {
    VisusReleaseAssert(value == 4);
    return;
  }


  //getFrustum
#if !SWIG
  const Frustum& getFrustum() const {
    return frustum;
  }
#endif

  //toNormalizedScreenCoordinates
  PointNd toNormalizedScreenCoordinates(Point3d p) const {
    return (projection.T * (modelview.T * PointNd(p, 1.0))).dropHomogeneousCoordinate();
  }

  //fromNormalizedScreenCoordinates
  Point3d fromNormalizedScreenCoordinates(PointNd p) {
    return (modelview.Ti * (projection.Ti * PointNd(p, 1.0))).dropHomogeneousCoordinate().toPoint3();
  }

  //applyDirectMap
  virtual PointNd applyDirectMap(PointNd p) const override 
  {
    if (p.getPointDim() < getSpaceDim()) 
    {
      p.setPointDim(getSpaceDim()); 
      p.back() = 1;
    }

    return (viewport.T * (projection.T * (modelview.T * p)));
  }

  //applyInverseMap
  virtual PointNd applyInverseMap(PointNd p) const override 
  {
    if (p.getPointDim() < getSpaceDim()) 
    {
      p.setPointDim(getSpaceDim()); 
      p.back() = 1;
    }

    p = modelview .Ti * (projection.Ti * (viewport.Ti * p));
    if (!p.back()) p.back()=1;
    return p;
  }

  //applyDirectMap
  virtual Plane applyDirectMap(Plane h) const override
  {
    VisusAssert(h.getSpaceDim() == getSpaceDim());
    return Plane(((PointNd(h) * modelview.Ti) * projection.Ti) * viewport.Ti);
  }

  //applyDirectMap
  virtual Plane applyInverseMap(Plane h) const override
  {
    VisusAssert(h.getSpaceDim() == getSpaceDim());
    return Plane(((PointNd(h) * viewport.T) * projection.T) * modelview.T);
  }

public:

  //applyDirectMapFromEye
  PointNd applyDirectMapFromEye(PointNd p) const
  {
    if (p.getPointDim() < getSpaceDim())
    {
      p.setPointDim(getSpaceDim());
      p.back() = 1;
    }
    return (viewport.T * (projection.T * p));
  }

  //applyInverseMapToEye
  PointNd applyInverseMapToEye(PointNd p) const
  {
    if (p.getPointDim() < getSpaceDim())
    {
      p.setPointDim(getSpaceDim());
      p.back() = 1;
    }
    p = projection.Ti * (viewport.Ti * p);
    if (!p.back()) p.back() = 1;
    return p;
  }

  //projectPoint
  Point2d projectPoint(Point3d p) const {
    return applyDirectMap(PointNd(p,1.0)).dropHomogeneousCoordinate().toPoint2();
  }

  //unprojectPoint
  Point3d unprojectPoint(Point2d p, double Z = 0.0) const{
    return applyInverseMap(PointNd(p[0], p[1], Z, 1.0)).dropHomogeneousCoordinate().toPoint3();
  }

  //unprojectPointToEye
  Point3d unprojectPointToEye(Point2d p, double Z = 0.0) const{
    return applyInverseMapToEye(PointNd(p[0], p[1], Z, 1.0)).dropHomogeneousCoordinate().toPoint3();
  }

  //return the ray
  Ray getRay(Point2d p) const  {
    return Ray::fromTwoPoints(
      unprojectPoint(p, 0.0), 
      unprojectPoint(p, 1.0));
  }

};



} //namespace Visus

#endif //VISUS_FRUSTUM_H

