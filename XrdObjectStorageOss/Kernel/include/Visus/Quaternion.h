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

#ifndef VISUS_QUATERNION_H
#define VISUS_QUATERNION_H

#include <Visus/Kernel.h>
#include <Visus/Point.h>

namespace Visus {

//////////////////////////////////////////////////////////////
class VISUS_KERNEL_API Quaternion
{
public:

  VISUS_CLASS(Quaternion)

  //identity
  double w = 1.0, x = 0.0, y = 0.0, z = 0.0;

  //default constructor
  Quaternion() {
  }

  //constructor
  explicit Quaternion(double w, double x, double y, double z) 
  {
    //isNull?
    if (w == 0.0 && x == 0.0 && y == 0.0 && z == 0.0)
    {
      this->w = this->x = this->y = this->z = 0.0;
      return;
    }

    double mod2 = w * w + x * x + y * y + z * z; VisusAssert(mod2 > 0.0);
    double vs = (mod2==1.0) ? 1.0 : (1.0 / sqrt(mod2));
    this->w = w * vs;
    this->x = x * vs;
    this->y = y * vs;
    this->z = z * vs;
  }

  //constructor
  explicit Quaternion(Point3d axis, double angle)
  {
    if (axis[0]==0.0 && axis[1]==0.0 && axis[2]==0.0)
    {
      this->w = this->x = this->y = this->z = 0.0;
      return;
    }

    axis = axis.normalized();

    double half_angle = 0.5*angle;
    double c = cos(half_angle);
    double s = sin(half_angle);
    
    this->w = c;
    this->x = s * axis[0];
    this->y = s * axis[1];
    this->z = s * axis[2];
  }

  //constructor from string
  static Quaternion fromString(String value) {
    Quaternion ret;
    std::istringstream parser(value);
    parser >> ret.w >> ret.x >> ret.y >> ret.z;
    return ret;
  }

  //operator[]
  Point4d toPoint4d() const {
    return Point4d(x,y,z,w);
  }

  //isNull
  bool isNull() const {
    return w == 0.0 && x == 0.0 && y == 0.0 && z == 0.0;
  }

  //isIdentity
  bool isIdentity() const {
    return w == 1.0 && x == 0.0 && y == 0.0 && z == 0.0;
  }

  //identity
  static Quaternion identity() {
    return Quaternion();
  }

  //null
  static Quaternion null() {
    return Quaternion(0.0, 0.0, 0.0, 0.0);
  }

  //convert to string
  String toString() const {
    return cstring(w, x, y, z);
  }

  //return the main axis of rotation
  Point3d getAxis() const {
    return x!=0.0 || y!=0.0 || z!=0.0 ? Point3d(x, y, z).normalized() : Point3d(0,0,1);
  }

  //return the angle of rotation
   double getAngle() const {
     return 2.0*acos(Utils::clamp(w, -1.0, +1.0));
  }

  //q1*q2
   Quaternion operator*(const Quaternion& q2) const
  {
    return Quaternion(
      w * q2.w - x * q2.x - y * q2.y - z * q2.z,
      w * q2.x + x * q2.w + y * q2.z - z * q2.y,
      w * q2.y + y * q2.w + z * q2.x - x * q2.z,
      w * q2.z + z * q2.w + x * q2.y - y * q2.x);
  }

  //operator==
  bool operator==(const Quaternion& other) const {
    return w == other.w && x == other.x && y == other.y && z == other.z;
  }

  //operator!=
  bool operator!=(const Quaternion& other) const {
    return !(operator==(other));
  }

  //q1*=q2
   Quaternion& operator*=(const Quaternion& q2) {
    (*this) = (*this)*q2; return *this;
  }

  //toEulerAngles (https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles)
  static Quaternion fromEulerAngles(double roll, double pitch, double yaw)
  {
	  double t0 = cos(yaw   * 0.5);
	  double t1 = sin(yaw   * 0.5);
	  double t2 = cos(roll  * 0.5);
	  double t3 = sin(roll  * 0.5);
	  double t4 = cos(pitch * 0.5);
	  double t5 = sin(pitch * 0.5);

    return Quaternion(
      t0 * t2 * t4 + t1 * t3 * t5,
      t0 * t3 * t4 - t1 * t2 * t5,
      t0 * t2 * t5 + t1 * t3 * t4,
      t1 * t2 * t4 - t0 * t3 * t5);
  }

  //toEulerAngles (https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles)
  Point3d toEulerAngles() const 
  {
    double ysqr = y * y;

	  // roll (x-axis rotation)
	  double t0 = +2.0 * (w * x + y * z);
	  double t1 = +1.0 - 2.0 * (x * x + ysqr);
	  double roll = atan2(t0, t1);

	  // pitch (y-axis rotation)
	  double t2 = +2.0 * (w * y - z * x);
	  t2 = t2 >  1.0 ? 1.0 : t2;
	  t2 = t2 < -1.0 ? -1.0 : t2;
	  double pitch = asin(t2);

	  // yaw (z-axis rotation)
	  double t3 = +2.0 * (w * z + x * y);
	  double t4 = +1.0 - 2.0 * (ysqr + z * z);  
	  double yaw = atan2(t3, t4);

    return Point3d(roll,pitch,yaw);
  }

  //conjugate
  Quaternion conjugate() const {
    return Quaternion(w, -x, -y, -z);
  }

  //operator*
  Point3d operator*(const Point3d& p) const 
  {
    auto t2 = +this->w * this->x;
    auto t3 = +this->w * this->y;
    auto t4 = +this->w * this->z;
    auto t5 = -this->x * this->x;
    auto t6 = +this->x * this->y;
    auto t7 = +this->x * this->z;
    auto t8 = -this->y * this->y;
    auto t9 = +this->y * this->z;
    auto t1 = -this->z * this->z;

    auto x = 2.0 * ((t8 + t1) * p[0] + (t6 - t4) * p[1] + (t3 + t7) * p[2]) + p[0];
    auto y = 2.0 * ((t4 + t6) * p[0] + (t5 + t1) * p[1] + (t9 - t2) * p[2]) + p[1];
    auto z = 2.0 * ((t7 - t3) * p[0] + (t2 + t9) * p[1] + (t5 + t8) * p[2]) + p[2];

    return Point3d(x, y, z);
  }

}; //end class Quaternion



} //namespace Visus

#endif //VISUS_QUATERNION_H

