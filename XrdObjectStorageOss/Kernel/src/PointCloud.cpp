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

#include <Visus/PointCloud.h>
#include <Visus/StringUtils.h>

#include <set>

namespace Visus {


//////////////////////////////////////////////////////////////////
void PointCloud::translate(Point3d vt)
{
  for (auto& point:points)
    point+=vt;
}


//////////////////////////////////////////////////////////////////
void PointCloud::scale(Point3d vs)
{
  for (auto& point:points)
  {
    point[0]*=vs[0];
    point[1]*=vs[1];
    point[2]*=vs[2];
  }
}



//////////////////////////////////////////////////////////////////
bool PointCloud::open(String filename)
{
  this->points.clear();
  this->colors.clear();

  String content=Utils::loadTextDocument(filename);
  if (content.empty())
    return false;

  bool bFirstLine=true;
  bool bHasColors=false;
  for (auto line : StringUtils::getNonEmptyLines(content))
  {
    auto tokens=StringUtils::split(line," ",true);

    if (bFirstLine)
    {
      bHasColors = tokens.size() >= 6;
      bFirstLine = false;
    }

    this->points.push_back(Point3d(atof(tokens[0].c_str()),atof(tokens[1].c_str()),atof(tokens[2].c_str())));

    if (bHasColors)
      this->colors.push_back(Point3i(atoi(tokens[3].c_str()),atoi(tokens[4].c_str()),atoi(tokens[5].c_str())));
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////////
class GetLocalCoordinateSystem
{
public:

  LocalCoordinateSystem& dst;
  std::vector<Point3d>   points;
  double                 ransac_threshold;
  int                    ransac_num_iterations;

  GetLocalCoordinateSystem(LocalCoordinateSystem& dst_,const std::vector<Point3d>& points_,double ransac_threshold_,int ransac_num_iterations_) 
    : dst(dst_),points(points_),ransac_threshold(ransac_threshold_),ransac_num_iterations(ransac_num_iterations_)
  {
  }

  //run
  void run()
  {
	  if (points.size()<3)
    {
		  dst=LocalCoordinateSystem(Point3d(1,0,0),Point3d(0,1,0),Point3d(0,0,1));
      return;
    }

    auto inliers=points;

    //remove outliers
    if (ransac_threshold && ransac_num_iterations)
    {
      Plane PLANE;
      int   NUM_INLIERS=-1;

	    for (int I=0;I<ransac_num_iterations;I++)
	    {
		    std::vector<int>  indices;
		    std::set<size_t>  used;
		    while (indices.size()<3)
		    {
			    int sample=Utils::getRandInteger(0,(int)points.size()-1);
			    if (used.find(sample)!=used.end()) continue;
			    used.insert(sample);
			    indices.push_back(sample);
		    }

        Plane plane(points[indices[0]],points[indices[1]],points[indices[2]]);

		    int num_inliers=0;
		    for (const auto& point : points)
		    {
			    if (fabs(plane.getDistance(point))<ransac_threshold)
				    num_inliers++;
		    }

		    if (num_inliers>NUM_INLIERS)
		    {
			    PLANE      =plane;
			    NUM_INLIERS=num_inliers;
		    }
	    }

      inliers.clear();
      inliers.reserve(points.size());
      for (const auto& point : points)
      {
        if (fabs(PLANE.getDistance(point))<ransac_threshold)
          inliers.push_back(point);
      }

	    if (inliers.size()<3)
		    ThrowException("Not enough inliers");

      //PrintInfo("Found #",inliers.size(),"inliers from #",points.size(),"points");
    }

    //this is the Singular value decomposition (SVD)
    Point3d X,Y,Z,C=getCentroid(inliers);
    {
      double scatter_matrix[3][3]; 
      int    order[3];
      double diag_matrix[3];
      double off_diag_matrix[3];
      FindScatterMatrix(inliers,C, scatter_matrix, order);
      Tred2(scatter_matrix, diag_matrix, off_diag_matrix);
      Tqli(diag_matrix, off_diag_matrix, scatter_matrix);

      int min_index=0,max_index=0;
      for (int i = 1; i < 3; i++)
      {
        min_index=(diag_matrix[i]<diag_matrix[min_index])? i : min_index;
        max_index=(diag_matrix[i]>diag_matrix[max_index])? i : max_index;
      }

      for (int i = 0; i < 3; i++)
      {
        X[order[i]] = scatter_matrix[i][max_index];
        Z[order[i]] = scatter_matrix[i][min_index];
      }

      X=X.normalized();
      Z=Z.normalized();
      Y=Z.cross(X).normalized();
    }

    //project points
    Point3d module;
    for (const Point3d& point : inliers)
    {
      Point3d v=point-C;
      module[0]=std::max(module[0],fabs(v.dot(X)));
      module[1]=std::max(module[1],fabs(v.dot(Y)));
      module[2]=std::max(module[2],fabs(v.dot(Z)));
    }

    X=module[0] * X;
    Y=module[1] * Y;
    Z=module[2] * Z;
    dst=LocalCoordinateSystem(X,Y,Z,C);
  }

private:

  //getCentroid
  static Point3d getCentroid(const std::vector<Point3d>& points) 
  {
    Point3d ret;
    double coff=1.0/(double)points.size();
    for (const auto& point : points)
      ret+=coff*point;
    return ret;
  }

  //FindScatterMatrix
  static void FindScatterMatrix(const std::vector<Point3d> &points,const Point3d &centroid,double scatter_matrix[3][3],int order[3]) 
  {
    for (int i = 0; i < 3; i++) 
      scatter_matrix[i][0] = scatter_matrix[i][1] = scatter_matrix[i][2] = 0;

    for (int i = 0; i < (int)points.size(); i++)
    {
      const Point3d &P = points[i];
      Point3d d = Point3d(P[0], P[1], P[2]) - centroid;
      scatter_matrix[0][0] += d[0]*d[0];
      scatter_matrix[0][1] += d[0]*d[1];
      scatter_matrix[0][2] += d[0]*d[2];
      scatter_matrix[1][1] += d[1]*d[1];
      scatter_matrix[1][2] += d[1]*d[2];
      scatter_matrix[2][2] += d[2]*d[2];
    }
    scatter_matrix[1][0] = scatter_matrix[0][1];
    scatter_matrix[2][0] = scatter_matrix[0][2];
    scatter_matrix[2][1] = scatter_matrix[1][2];

    order[0] = 0; order[1] = 1; order[2] = 2;
    if (scatter_matrix[0][0] > scatter_matrix[1][1]) 
    {
      double TempD = scatter_matrix[0][0];
      scatter_matrix[0][0] = scatter_matrix[1][1];
      scatter_matrix[1][1] = TempD;

      TempD = scatter_matrix[0][2];
      scatter_matrix[0][2] = scatter_matrix[2][0] = scatter_matrix[1][2];
      scatter_matrix[1][2] = scatter_matrix[2][1] = TempD;
      int TempI = order[0];
      order[0] = order[1];
      order[1] = TempI;
    }

    if (scatter_matrix[1][1] > scatter_matrix[2][2]) 
    {
      double TempD = scatter_matrix[1][1];
      scatter_matrix[1][1] = scatter_matrix[2][2];
      scatter_matrix[2][2] = TempD;
      TempD = scatter_matrix[0][1];
      scatter_matrix[0][1] = scatter_matrix[1][0] = scatter_matrix[0][2];
      scatter_matrix[0][2] = scatter_matrix[2][0] = TempD;
      int TempI = order[1];
      order[1] = order[2];
      order[2] = TempI;
    }

    if (scatter_matrix[0][0] > scatter_matrix[1][1]) 
    {
      double TempD = scatter_matrix[0][0];
      scatter_matrix[0][0] = scatter_matrix[1][1];
      scatter_matrix[1][1] = TempD;
      TempD = scatter_matrix[0][2];
      scatter_matrix[0][2] = scatter_matrix[2][0] = scatter_matrix[1][2];
      scatter_matrix[1][2] = scatter_matrix[2][1] = TempD;
      int TempI = order[0];
      order[0] = order[1];
      order[1] = TempI;
    }
  }

  //Tred2
  static void Tred2(double a[3][3], double d[3], double e[3]) 
  {
    int l, k, i, j;
    double scale, hh, h, g, f;
    for (i = 3; i >= 2; i--)
    {
      l = i - 1;
      h = scale = 0.0;
      if (l > 1)
      {
        for (k = 1; k <= l; k++)
          scale += fabs(a[i - 1][k - 1]);

        if (scale == 0.0)        
          e[i - 1] = a[i - 1][l - 1];
        else
        {
          for (k = 1; k <= l; k++)
          {
            a[i - 1][k - 1] /= scale;   
            h += a[i - 1][k - 1] * a[i - 1][k - 1];    
          }
          f = a[i - 1][l - 1];
          g = f > 0 ? -sqrt(h) : sqrt(h);
          e[i - 1] = scale*g;
          h -= f*g;   
          a[i - 1][l - 1] = f - g;   
          f = 0.0;
          for (j = 1; j <= l; j++)
          {
            a[j - 1][i - 1] = a[i - 1][j - 1] / h; 
            g = 0.0; 
            for (k = 1    ; k <= j; k++) g += a[j - 1][k - 1] * a[i - 1][k - 1];
            for (k = j + 1; k <= l; k++) g += a[k - 1][j - 1] * a[i - 1][k - 1];
            e[j - 1] = g / h; 
            f += e[j - 1] * a[i - 1][j - 1];
          }
          hh = f / (h + h);   
          for (j = 1; j <= l; j++) 
          {
            f = a[i - 1][j - 1]; 
            e[j - 1] = g = e[j - 1] - hh*f;
            for (k = 1; k <= j; k++) 
              a[j - 1][k - 1] -= (f*e[k - 1] + g*a[i - 1][k - 1]);
          }
        }
      }
      else
        e[i - 1] = a[i - 1][l - 1];
      d[i - 1] = h;
    }

    d[0] = 0.0;
    e[0] = 0.0;

    for (i = 1; i <= 3; i++)
    {
      l = i - 1;
      if (d[i - 1])
      {
        for (j = 1; j <= l; j++)
        {
          g = 0.0;
          for (k = 1; k <= l; k++) g += a[i - 1][k - 1] * a[k - 1][j - 1];
          for (k = 1; k <= l; k++) a[k - 1][j - 1] -= g*a[k - 1][i - 1];
        }
      }
      d[i - 1] = a[i - 1][i - 1];
      a[i - 1][i - 1] = 1.0;
      for (j = 1; j <= l; j++)
        a[j - 1][i - 1] = a[i - 1][j - 1] = 0.0;
    }
  }

  //Tqli
  static void Tqli(double d[3], double e[3], double z[3][3]) 
  {
    int     m, l, iter, i, k;
    double  s, r, p, g, f, dd, c, b;

    for (i = 2; i <= 3; i++)
      e[i - 2] = e[i - 1];
    e[2] = 0.0;
    for (l = 1; l <= 3; l++)
    {
      iter = 0;
      do
      {
        for (m = l; m <= 2; m++)
        {
          dd = fabs(d[m - 1]) + fabs(d[m]);
          if (fabs(e[m - 1]) + dd == dd)
            break;
        }
        if (m != l)
        {
          if (iter++== 30)
            ThrowException("Too many iterations in TQLI");

          g = (d[l] - d[l - 1]) / (2.0f*e[l - 1]); /* form shift */
          r = sqrt((g*g) + 1.0f);
          #define    SIGN(a,b)    ((b)<0? -fabs(a):fabs(a))
          g = d[m - 1] - d[l - 1] + e[l - 1] / (g + SIGN(r, g)); /* this is dm-ks */
          s = c = 1.0;
          p = 0.0;
          for (i = m - 1; i >= l; i--)
          {
            f = s*e[i - 1];
            b = c*e[i - 1];
            if (fabs(f) >= fabs(g))
            {
              c = g / f;
              r = sqrt((c*c) + 1.0f);
              e[i] = f*r;
              c *= (s = 1.0f / r);
            }
            else
            {
              s = f / g;
              r = sqrt((s*s) + 1.0f);
              e[i] = g*r;
              s *= (c = 1.0f / r);
            }
            g = d[i] - p;
            r = (d[i - 1] - g)*s + 2.0f*c*b;
            p = s*r;
            d[i] = g + p;
            g = c*r - b;
            for (k = 1; k <= 3; k++)
            {
              f = z[k - 1][i];
              z[k - 1][i] = s*z[k - 1][i - 1] + c*f;
              z[k - 1][i - 1] = c*z[k - 1][i - 1] - s*f;
            }
          }
          d[l - 1] = d[l - 1] - p;
          e[l - 1] = g;
          e[m - 1] = 0.0f;
        }
      } while (m != l);
    }
  }


};

LocalCoordinateSystem PointCloud::getLocalCoordinateSystem(double ransac_threshold,int ransac_num_iterations)
{
  LocalCoordinateSystem ret;
  GetLocalCoordinateSystem(ret,this->points,ransac_threshold,ransac_num_iterations).run();
  return ret;
}

} //namespace Visus