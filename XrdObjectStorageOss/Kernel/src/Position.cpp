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

#include <Visus/Position.h>
#include <Visus/Polygon.h>

namespace Visus {

//////////////////////////////////////////////////
Position::Position(BoxNd box)
{
  auto pdim = box.getPointDim();
  if (pdim == 0) return;
  this->T = Matrix::identity(box.getPointDim()+1);
  this->box = box;
}

  //////////////////////////////////////////////////
void Position::prependTransformation(const Matrix& Left)
{
  auto pdim = getPointDim(); 
  if (pdim == 0) return;
  this->T = Left * this->T;

  auto sdim = getSpaceDim();

  //only scale and translate
#if 0
  if (!T.isIdentity() && this->getPoints() == toAxisAlignedBox().getPoints())
  {
    this->box = toAxisAlignedBox();
    this->T = Matrix::identity(sdim);
  }
#endif
}


//////////////////////////////////////////////////
//https://n-e-r-v-o-u-s.com/blog/?p=4415
double VolumeTetrahedra(Point3d a, Point3d b, Point3d c, Point3d d)
{
  return fabs((a - d).dot((b - d).cross((c - d))))/6.0;
}

//////////////////////////////////////////////////
double Position::computeVolume(std::vector<Point3d> p)
{
  //https://www.researchgate.net/figure/Construction-of-a-uniform-tetrahedron-mesh_fig1_228349569
  const int indices[6][4] =
  {
    { 2, 4, 5, 6},
    { 2, 5, 6, 7},
    { 2, 3, 5, 7},
    { 0, 2, 4, 5},
    { 0, 1, 2, 5},
    { 1, 2, 3, 5}
  };

  auto ret = 0.0;
  for (int I = 0; I < 6; I++)
    ret += VolumeTetrahedra(p[indices[I][0]], p[indices[I][1]], p[indices[I][2]], p[indices[I][3]]);

  return ret;
}

//////////////////////////////////////////////////
double Position::computeVolume() const {

  if (!this->valid())
    return 0.0;

  auto pdim = box.getPointDim();

  if (pdim == 2)
  {
    std::vector<Point2d> points;
    for (auto point : this->box.getPoints())
      points.push_back((T * point).toPoint2());
    return Quad(points).area();
  }
  else if (pdim == 3)
  {
    std::vector<Point3d> points;
    for (auto point : this->box.getPoints())
      points.push_back((T * point).toPoint3());

    return computeVolume(points);
  }
  else
  {
    //todo...
    VisusAssert(false);
    return 0.0;
  }
}

//////////////////////////////////////////////////
std::vector<PointNd> Position::getPoints() const
{
  std::vector<PointNd> ret;

  if (!this->valid())
    return ret;

  auto points = this->box.getPoints();
  for (auto point : points)
    ret.push_back(T * point);

  return ret;
}


//////////////////////////////////////////////////
static bool IsPointInsideHull(const PointNd point, const std::vector<Plane>& planes)
{
  const double epsilon = 1e-4;
  bool bInside = true;
  for (int I = 0; bInside && I < (int)planes.size(); I++)
    bInside = planes[I].getDistance(point) < epsilon;
  return bInside;
};

//////////////////////////////////////////////////
Position Position::shrink(BoxNd dst_box,LinearMap& map,Position in_position)
{
  if (!in_position.valid())
    return in_position;

  const int unit_box_edges[12][2]=
  {
    {0,1}, {1,2}, {2,3}, {3,0},
    {4,5}, {5,6}, {6,7}, {7,4},
    {0,4}, {1,5}, {2,6}, {3,7}
  };

  //always working in 3d
  dst_box.setPointDim(3);
  map.setSpaceDim(4);

  auto position = in_position;
  position.setSpaceDim(4);
  position.setPointDim(3);

  auto dst_points = dst_box.getPoints();

  const LinearMap& T2(map);
  MatrixMap T1(position.T);
  auto src_box=position.box.toBox3();
  auto shrinked_box= BoxNd::invalid();

  int box_dim= src_box.minsize()>0? 3 : 2;
  if (box_dim ==2)
  {
    int slice_axis= src_box.minsize_index();
    VisusAssert(slice_axis >= 0 && slice_axis<3);
    VisusAssert(!src_box.size()[(slice_axis + 0) % 3]);
    VisusAssert( src_box.size()[(slice_axis + 1) % 3]);
    VisusAssert( src_box.size()[(slice_axis + 2) % 3]);

    double slice_pos=src_box.p1[slice_axis];

    auto slice_plane = PointNd(4);
    slice_plane.back() = -slice_pos;
    slice_plane[slice_axis] = 1;

    //how the plane is transformed by <T> (i.e. go to screen)
    Plane slice_plane_in_screen = T2.applyDirectMap(T1.applyDirectMap(Plane(slice_plane)));

    //point classification
    double distances[8];

    //points belonging to the plane
    for (int I = 0; I < 8; I++)
    {
      auto p = dst_points[I];
      distances[I] = slice_plane_in_screen.getDistance(p);
      if (!distances[I])
      {
        p = T1.applyInverseMap(T2.applyInverseMap(p)).dropHomogeneousCoordinate();
        p[slice_axis] = slice_pos; //I know it must implicitely be on the Z plane! 
        shrinked_box.addPoint(p);
      }
    }

    //split edges
    for (int E = 0; E < 12; E++)
    {
      int i1 = unit_box_edges[E][0]; double h1 = distances[i1];
      int i2 = unit_box_edges[E][1]; double h2 = distances[i2];
      if ((h1 > 0 && h2 < 0) || (h1 < 0 && h2>0))
      {
        auto p1 = dst_points[i1]; h1 = fabs(h1);
        auto p2 = dst_points[i2]; h2 = fabs(h2);
        double alpha = h2 / (h1 + h2);
        double beta = h1 / (h1 + h2);
        auto p = T1.applyInverseMap(T2.applyInverseMap(alpha * p1 + beta * p2)).dropHomogeneousCoordinate();
        p[slice_axis] = slice_pos; //I know it must implicitely be on the Z plane! 
        shrinked_box.addPoint(p);
      }
    }
  }
  else
  {
    //see http://www.gamedev.net/community/forums/topic.asp?topic_id=224689
    VisusAssert(box_dim == 3);

    //the first polyhedra (i.e. position)
    auto V1 = src_box.getPoints();
    auto H1 = src_box.getPlanes();

    //the second polyhedra (transformed to be in the first polyhedra system, i.e. position)
    PointNd V2[8];
    for (int I = 0; I < 8; I++)
      V2[I] = T1.applyInverseMap(T2.applyInverseMap(dst_points[I])).dropHomogeneousCoordinate();

    auto H2 = dst_box.getPlanes();
    for (int H = 0; H < 6; H++)
      H2[H] = T1.applyInverseMap(T2.applyInverseMap(H2[H]));

    //point of the first (second) polyhedra inside second (first) polyhedra
    for (int V = 0; V < 8; V++) {
      if (IsPointInsideHull(V1[V], H2))
        shrinked_box.addPoint(V1[V]);
    }

    for (int V = 0; V < 8; V++) {
      if (IsPointInsideHull(V2[V], H1))
        shrinked_box.addPoint(V2[V]);
    }

    //intersection of first polydra edges with second polyhedral planes
    for (int H = 0; H < 6; H++)
    {
      double distances[8];
      for (int V = 0; V < 8; V++)
        distances[V] = H2[H].getDistance(V1[V]);

      for (int E = 0; E < 12; E++)
      {
        int i1 = unit_box_edges[E][0]; double h1 = distances[i1];
        int i2 = unit_box_edges[E][1]; double h2 = distances[i2];
        if ((h1 > 0 && h2 < 0) || (h1 < 0 && h2>0))
        {
          auto p1 = V1[i1]; h1 = fabs(h1);
          auto p2 = V1[i2]; h2 = fabs(h2);
          double alpha = h2 / (h1 + h2);
          double beta = h1 / (h1 + h2);
          auto p = alpha * p1 + beta * p2;
          if (IsPointInsideHull(p, H2))
            shrinked_box.addPoint(p);
        }
      }
    }

    //intersection of second polyhedra edges with first polyhedral planes
    for (int H = 0; H < 6; H++)
    {
      double distances[8];
      for (int V = 0; V < 8; V++)
        distances[V] = H1[H].getDistance(V2[V]);

      for (int E = 0; E < 12; E++)
      {
        int i1 = unit_box_edges[E][0]; double h1 = distances[i1];
        int i2 = unit_box_edges[E][1]; double h2 = distances[i2];
        if ((h1 > 0 && h2 < 0) || (h1 < 0 && h2>0))
        {
          auto p1 = V2[i1]; h1 = fabs(h1);
          auto p2 = V2[i2]; h2 = fabs(h2);
          double alpha = h2 / (h1 + h2);
          double beta = h1 / (h1 + h2);
          auto p = alpha * p1 + beta * p2;
          if (IsPointInsideHull(p, H1))
            shrinked_box.addPoint(p);
        }
      }
    }
  }

  if (!shrinked_box.valid())
    return Position::invalid();

  shrinked_box = shrinked_box.getIntersection(src_box);
  shrinked_box.setPointDim(in_position.box.getPointDim());

  return Position(in_position.T, shrinked_box);

}

//////////////////////////////////////////////////
void Position::write(Archive& ar) const
{
  if (!valid()) return;
  ar.write("T", T);
  ar.write("box", box);
}

//////////////////////////////////////////////////
void Position::read(Archive& ar)
{
  ar.read("T", T, Matrix::identity(4));
  ar.read("box", box);
}

} //namespace Visus

