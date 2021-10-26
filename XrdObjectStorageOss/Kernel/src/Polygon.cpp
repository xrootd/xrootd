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

#include <Visus/Polygon.h>

#include <functional>

namespace Visus {


//////////////////////////////////////////////////////////////////
class ClipPolygon2d
{
public:

  //______________________________________________
  template <class Comp>
  class BoundaryHor
  {
  public:
    BoundaryHor(double y_) : m_Y(y_) {}
    bool isInside(const Point2d& pnt) const { return Comp()(pnt[1], m_Y); }	// return true if pnt[1] is at the inside of the boundary
    Point2d intersect(const Point2d& p0, const Point2d& p1) const			// return intersection point of line p0...p1 with boundary
    {																	// assumes p0...p1 is not strictly horizontal

      Point2d d = p1 - p0;
      double xslope = d[0] / d[1];

      Point2d r;
      r[1] = m_Y;
      r[0] = p0[0] + xslope * (m_Y - p0[1]);
      return r;
    }
  private:
    double m_Y;
  };

  //______________________________________________
  template <class Comp>
  class BoundaryVert
  {
  public:
    BoundaryVert(double x_) : m_X(x_) {}
    bool isInside(const Point2d& pnt) const { return Comp()(pnt[0], m_X); }
    Point2d intersect(const Point2d& p0, const Point2d& p1) const		// assumes p0...p1 is not strictly vertical
    {

      Point2d d = p1 - p0;
      double yslope = d[1] / d[0];

      Point2d r;
      r[0] = m_X;
      r[1] = p0[1] + yslope * (m_X - p0[0]);
      return r;
    }
  private:
    double m_X;
  };

  //______________________________________________
  template <class BoundaryClass, class Stage>
  class ClipStage : private BoundaryClass
  {
  public:
    ClipStage(Stage& nextStage, double position)
      : BoundaryClass(position)
      , m_NextStage(nextStage)
      , m_bFirst(true)
    {}

    //handleVertex
    void handleVertex(const Point2d& pntCurrent)	// Function to handle one vertex
    {
      bool bCurrentInside = BoundaryClass::isInside(pntCurrent);		// See if vertex is inside the boundary.

      if (m_bFirst)	// If this is the first vertex...
      {
        m_pntFirst = pntCurrent;	// ... just remember it,...

        m_bFirst = false;
      }
      else		// Common cases, not the first vertex.
      {
        if (bCurrentInside)	// If this vertex is inside...
        {
          if (!m_bPreviousInside)		// ... and the previous one was outside
            m_NextStage.handleVertex(BoundaryClass::intersect(m_pntPrevious, pntCurrent));
          // ... first output the intersection point.

          m_NextStage.handleVertex(pntCurrent);	// Output the current vertex.
        }
        else if (m_bPreviousInside) // If this vertex is outside, and the previous one was inside...
          m_NextStage.handleVertex(BoundaryClass::intersect(m_pntPrevious, pntCurrent));
        // ... output the intersection point.

        // If neither current vertex nor the previous one are inside, output nothing.
      }
      m_pntPrevious = pntCurrent;		// Be prepared for next vertex.
      m_bPreviousInside = bCurrentInside;
    }

    //finalize
    void finalize()
    {
      handleVertex(m_pntFirst);		// Close the polygon.
      m_NextStage.finalize();			// Delegate to the next stage.
    }


  private:
    Stage& m_NextStage;			// the next stage
    bool m_bFirst;				// true if no vertices have been handled
    Point2d m_pntFirst;			// the first vertex
    Point2d m_pntPrevious;		// the previous vertex
    bool m_bPreviousInside;		// true if the previous vertex was inside the Boundary
  };

  //______________________________________________
  class OutputStage
  {
  public:
    OutputStage() : m_pDest(0) {}
    void setDestination(std::vector<Point2d> * pDest) { m_pDest = pDest; }
    void handleVertex(const Point2d& pnt) { m_pDest->push_back(pnt); }	// Append the vertex to the output container.
    void finalize() {}		// Do nothing.
  private:

    std::vector<Point2d> * m_pDest;
  };

  const std::vector<Point2d>& polygon;

  typedef ClipStage< BoundaryHor<  std::less<double>          >, OutputStage>	ClipBottom;
  typedef ClipStage< BoundaryVert< std::greater_equal<double> >, ClipBottom>	ClipLeft;
  typedef ClipStage< BoundaryHor<  std::greater_equal<double> >, ClipLeft>		ClipTop;
  typedef ClipStage< BoundaryVert< std::less<double>          >, ClipTop>		  ClipRight;

  // Our data members.
  OutputStage m_stageOut;
  ClipBottom  m_stageBottom;
  ClipLeft    m_stageLeft;
  ClipTop     m_stageTop;
  ClipRight   m_stageRight;

  // SutherlandHodgman algorithm
  ClipPolygon2d(const Rectangle2d& rect,const std::vector<Point2d>& polygon_)
    : m_stageBottom(m_stageOut   , rect.p2()[1])	
    , m_stageLeft  (m_stageBottom, rect.p1()[0])		
    , m_stageTop   (m_stageLeft  , rect.p1()[1])			
    , m_stageRight (m_stageTop   , rect.p2()[0])
    , polygon(polygon_)
  {
  }

  //clipPolygon
  std::vector<Point2d> doClip()
  {
    std::vector<Point2d> ret;
    m_stageOut.setDestination(&ret);
    for (auto point : polygon) 
      m_stageRight.handleVertex(point);
    m_stageRight.finalize();
    return ret;
  }
};

//////////////////////////////////////////////////////////////////
Polygon2d Polygon2d::clip(const Rectangle2d& r) const
{
  return Polygon2d(ClipPolygon2d(r,this->points).doClip()); 
}


//////////////////////////////////////////////////////////////////
double Polygon2d::area() const {

  if (!valid())
    return 0;

  int N=(int)points.size();

  if (N<3)
    return 0.0;

  //http://paulbourke.net/geometry/polygonmesh/source1.c
  double area = 0;
  for (int i = 0; i < N; i++) 
  {
    int j = (i + 1) % N;
    area += points[i][0] * points[j][1];
    area -= points[i][1] * points[j][0];
  }

  area /= 2.0;
  return (area < 0 ? -area : area);
}



///////////////////////////////////////////////////////////////////////////
Matrix Quad::findQuadHomography(const Quad& dst_quad, const Quad& src_quad)
{
  auto dst = dst_quad.points;
  auto src = src_quad.points;

  double P[8][9] = {
    { -src[0][0], -src[0][1], -1,         0,         0,  0, src[0][0]*dst[0][0], src[0][1]*dst[0][0], -dst[0][0] },
    { 0,         0,  0, -src[0][0], -src[0][1], -1, src[0][0]*dst[0][1], src[0][1]*dst[0][1], -dst[0][1] },
    { -src[1][0], -src[1][1], -1,         0,         0,  0, src[1][0]*dst[1][0], src[1][1]*dst[1][0], -dst[1][0] },
    { 0,         0,  0, -src[1][0], -src[1][1], -1, src[1][0]*dst[1][1], src[1][1]*dst[1][1], -dst[1][1] },
    { -src[2][0], -src[2][1], -1,         0,         0,  0, src[2][0]*dst[2][0], src[2][1]*dst[2][0], -dst[2][0] },
    { 0,         0,  0, -src[2][0], -src[2][1], -1, src[2][0]*dst[2][1], src[2][1]*dst[2][1], -dst[2][1] },
    { -src[3][0], -src[3][1], -1,         0,         0,  0, src[3][0]*dst[3][0], src[3][1]*dst[3][0], -dst[3][0] },
    { 0,         0,  0, -src[3][0], -src[3][1], -1, src[3][0]*dst[3][1], src[3][1]*dst[3][1], -dst[3][1] },
  };

  double* A = &P[0][0];
  const int n = 9;
  int i = 0, j = 0, m = n - 1;
  while (i < m && j < n) {
    int maxi = i;
    for (int k = i + 1; k<m; k++) {
      if (fabs(A[k*n + j]) > fabs(A[maxi*n + j])) {
        maxi = k;
      }
    }
    if (A[maxi*n + j] != 0) {
      if (i != maxi)
        for (int k = 0; k<n; k++) {
          double aux = A[i*n + k];
          A[i*n + k] = A[maxi*n + k];
          A[maxi*n + k] = aux;
        }
      double A_ij = A[i*n + j];
      for (int k = 0; k<n; k++) {
        A[i*n + k] /= A_ij;
      }
      for (int u = i + 1; u< m; u++) {
        double A_uj = A[u*n + j];
        for (int k = 0; k<n; k++) {
          A[u*n + k] -= A_uj*A[i*n + k];
        }
      }
      i++;
    }
    j++;
  }
  for (int i = m - 2; i >= 0; i--) {
    for (int j = i + 1; j<n - 1; j++) {
      A[i*n + m] -= A[i*n + j] * A[j*n + m];
    }
  }

  return Matrix(
    P[0][8], P[3][8], P[6][8],
    P[1][8], P[4][8], P[7][8],
    P[2][8], P[5][8], 1.0).transpose();
}


///////////////////////////////////////////////////////////////////////////
Polygon2d Quad::intersection(const Quad& A, const Quad& B)
{
  auto b1 = A.getBoundingBox();
  auto b2 = B.getBoundingBox();
  if (!b1.intersect(b2))
    return Polygon2d();

  auto T = findQuadHomography(Quad(1, 1), A);
  auto clipped = Quad(T, B).clip(Rectangle2d(0, 0, 1, 1));
  return Polygon2d(T.invert(), clipped);
}

///////////////////////////////////////////////////////////////////////////
bool Quad::isConvex() const {

  auto computeSign = [](Point2d p1, Point2d p2, Point2d p3) {
    auto dx1 = p2[0] - p1[0];
    auto dy1 = p2[1] - p1[1];
    auto dx2 = p3[0] - p2[0];
    auto dy2 = p3[1] - p2[1];
    return ((dx1 * dy2 - dy1 * dx2) >= 0) ? +1 : -1;
  };

  auto s0 = computeSign(points[0], points[1], points[2]);
  auto s1 = computeSign(points[1], points[2], points[3]);
  auto s2 = computeSign(points[2], points[3], points[0]);
  auto s3 = computeSign(points[3], points[0], points[1]);

  return s0 == s1 && s1 == s2 && s2 == s3;
};


} //namespace Visus