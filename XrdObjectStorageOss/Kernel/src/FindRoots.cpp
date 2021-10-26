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


#include <Visus/FindRoots.h>

#include <cmath>

namespace Visus {


///////////////////////////////////////////////////////////
std::vector<double> FindRoots::solve(double c0, double c1)
{ 
  std::vector<double> ret;

  const double Epsilon=1e-6;

  if (fabs(c1) >= Epsilon)
  {
    ret.push_back(-c0/c1);
    return ret;
  }

  return ret;
}

///////////////////////////////////////////////////////////
std::vector<double> FindRoots::solve(double c0, double c1, double c2)
{
  // Quadratic equations: c2*x^2+c1*x+c0 = 0
  const double Epsilon=1e-6;
  if (fabs(c2) <= Epsilon)
  {
    // Polynomial is linear.
    return solve(c0, c1,Epsilon);
  }

  std::vector<double> ret;

  double discr = c1*c1 - ((double)4)*c0*c2;
  if (fabs(discr) <= Epsilon)
  {
    discr = (double)0;
  }

  if (discr < (double)0)
    return ret;

  double tmp = ((double)0.5)/c2;

  if (discr > (double)0)
  {
    discr = sqrt(discr);
    ret.push_back(tmp*(-c1 - discr));
    ret.push_back(tmp*(-c1 + discr));
  }
  else
  {
    ret.push_back( -tmp*c1);
  }

  return ret;
}

///////////////////////////////////////////////////////////
std::vector<double> FindRoots::solve(double c0, double c1, double c2, double c3)
{
  // Cubic equations: c3*x^3+c2*x^2+c1*x+c0 = 0
  const double Epsilon=1e-6;

  // Polynomial is quadratic.
  if (fabs(c3) <= Epsilon)
    return solve(c0, c1, c2);

  std::vector<double> ret;

  // Make polynomial monic, x^3+c2*x^2+c1*x+c0.
  double invC3 = ((double)1)/c3;
  c0 *= invC3;
  c1 *= invC3;
  c2 *= invC3;

  // Convert to y^3+a*y+b = 0 by x = y-c2/3.
  const double third = (double)1/(double)3;
  const double twentySeventh = (double)1/(double)27;
  double offset = third*c2;
  double a = c1 - c2*offset;
  double b = c0+c2*(((double)2)*c2*c2-((double)9)*c1)*twentySeventh;
  double halfB = ((double)0.5)*b;

  double discr = halfB*halfB + a*a*a*twentySeventh;
  if (fabs(discr) <= Epsilon)
    discr = (double)0;

  if (discr > (double)0)  // 1 real, 2 complex roots
  {
    discr = sqrt(discr);
    double temp = -halfB + discr;

    if (temp >= (double)0)
      ret.push_back(+pow(temp, third));
    else
      ret.push_back(-pow(-temp, third));

    temp = -halfB - discr;

    if (temp >= (double)0)
      ret[0] += pow(temp, third);
    else
      ret[0] -= pow(-temp, third);

    ret[0] -= offset;
  }
  else if (discr < (double)0) 
  {
    const double sqrt3 = sqrt((double)3);
    double dist = sqrt(-third*a);
    double angle = third*atan2(sqrt(-discr),-halfB);
    double cs = cos(angle);
    double sn = sin(angle);
    ret.push_back(((double)2)*dist*cs - offset);
    ret.push_back(-dist*(cs + sqrt3*sn) - offset);
    ret.push_back(-dist*(cs - sqrt3*sn) - offset);
  }
  else 
  {
    double temp;
    if (halfB >= (double)0)
      temp = -pow(halfB, third);
    else
      temp = pow(-halfB, third);

    ret.push_back(((double)2)*temp - offset);
    ret.push_back(-temp - offset);
    ret.push_back(ret[1]);
  }

  return ret;
}

///////////////////////////////////////////////////////////
std::vector<double> FindRoots::solve(double c0, double c1, double c2, double c3,double c4)
{
  // Quartic equations: c4*x^4+c3*x^3+c2*x^2+c1*x+c0 = 0
  // see http://www.geometrictools.com/LibMathematics/NumericalAnalysis/NumericalAnalysis.html
  const double Epsilon=1e-6;

  // Polynomial is cubic.
  if (fabs(c4) <= Epsilon)
    return solve(c0, c1, c2, c3);

  std::vector<double> ret;

  // Make polynomial monic, x^4+c3*x^3+c2*x^2+c1*x+c0.
  double invC4 = ((double)1)/c4;
  c0 *= invC4;
  c1 *= invC4;
  c2 *= invC4;
  c3 *= invC4;

  // Reduction to resolvent cubic polynomial y^3+r2*y^2+r1*y+r0 = 0.
  double r0 = -c3*c3*c0 + ((double)4)*c2*c0 - c1*c1;
  double r1 = c3*c1 - ((double)4)*c0;
  double r2 = -c2;

  // always produces at least one root
  double y = solve(r0, r1, r2, (double)1)[0];
 
  double discr = ((double)0.25)*c3*c3 - c2 + y;
  if (fabs(discr) <= Epsilon)
    discr = (double)0;

  if (discr > (double)0) 
  {
    double r = sqrt(discr);
    double t1 = ((double)0.75)*c3*c3 - r*r - ((double)2)*c2;
    double t2 = (((double)4)*c3*c2 - ((double)8)*c1 - c3*c3*c3) /(((double)4.0)*r);

    double tPlus = t1 + t2;
    double tMinus = t1 - t2;
    if (fabs(tPlus) <= Epsilon) 
      tPlus = (double)0;

    if (fabs(tMinus) <= Epsilon) 
      tMinus = (double)0;

    if (tPlus >= (double)0)
    {
      double d = sqrt(tPlus);
      ret.push_back(-((double)0.25)*c3 + ((double)0.5)*(r + d));
      ret.push_back(-((double)0.25)*c3 + ((double)0.5)*(r - d));
    }

    if (tMinus >= (double)0)
    {
      double e = sqrt(tMinus);
      ret.push_back(-((double)0.25)*c3+((double)0.5)*(e - r));
      ret.push_back(-((double)0.25)*c3-((double)0.5)*(e + r));
    }
  }
  else if (discr < (double)0)
  {
    ;
  }
  else
  {
    double t2 = y*y - ((double)4)*c0;

    if (t2 >= -Epsilon) 
    {
      if (t2 < (double)0) // round to zero
        t2 = (double)0;

      t2 = ((double)2)*sqrt(t2);
      double t1 = ((double)0.75)*c3*c3 - ((double)2)*c2;
      double tPlus = t1 + t2;
      if (tPlus >= Epsilon)
      {
        double d = sqrt(tPlus);
        ret.push_back(-((double)0.25)*c3 + ((double)0.5)*d);
        ret.push_back(-((double)0.25)*c3 - ((double)0.5)*d);
      }

      double tMinus = t1 - t2;
      if (tMinus >= Epsilon) 
      {
        double e = sqrt(tMinus);
        ret.push_back(-((double)0.25)*c3+((double)0.5)*e);
        ret.push_back(-((double)0.25)*c3-((double)0.5)*e);
      }
    }
  }

  return ret;
}

} //namespace Visus


