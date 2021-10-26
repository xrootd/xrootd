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

#include <Visus/Matrix.h>

namespace Visus {

//////////////////////////////////////////////////////////////////////
Matrix Matrix::perspective(double fovy, double aspect, double zNear, double zFar)
{
  Matrix m = Matrix::identity(4);
  double sine, cotangent, deltaZ;
  double radians = fovy / 2 * Math::Pi / 180;

  deltaZ = zFar - zNear;
  sine = sin(radians);
  if ((deltaZ == 0) || (sine == 0) || (aspect == 0)) 
    return Matrix();

  cotangent = cos(radians) / sine;
  m(0, 0) = cotangent / aspect;
  m(1, 1) = cotangent;
  m(2, 2) = -(zFar + zNear) / deltaZ;
  m(2, 3) = -1;
  m(3, 2) = -2 * zNear * zFar / deltaZ;
  m(3, 3) = 0;

  return m.transpose();
}

//////////////////////////////////////////////////////////////////////
Matrix Matrix::lookAt(Point3d Eye,Point3d Center,Point3d Up)
{
  Point3d forward, side, up;
  Matrix m = Matrix::identity(4);

  forward[0] = Center[0] - Eye[0];
  forward[1] = Center[1] - Eye[1];
  forward[2] = Center[2] - Eye[2];

  up[0] = Up[0];
  up[1] = Up[1];
  up[2] = Up[2];

  forward=forward.normalized();

  /* Side = forward x up */
  side = forward.cross(up);
  side=side.normalized();

  /* Recompute up as: up = side x forward */
  up=side.cross(forward);

  m(0, 0) = side[0];
  m(1, 0) = side[1];
  m(2, 0) = side[2];

  m(0, 1) = up[0];
  m(1, 1) = up[1];
  m(2, 1) = up[2];

  m(0, 2) = -forward[0];
  m(1, 2) = -forward[1];
  m(2, 2) = -forward[2];

  return m.transpose() * translate(PointNd(-1*Eye));
}

//////////////////////////////////////////////////////////////////////
void Matrix::getLookAt(Point3d& eye, Point3d& dir, Point3d& up) const
{
  Matrix vmat = this->invert();
  eye = Point3d(vmat[3], vmat[7], vmat[11]);
  up = Point3d(vmat[1], vmat[5], vmat[9]);
  dir = Point3d(-vmat[2], -vmat[6], -vmat[10]).normalized();
}

//////////////////////////////////////////////////////////////////////
Matrix Matrix::rotateAroundAxis(Point3d axis,double angle)
{
  if (!angle)
    return Matrix();

  axis = axis.normalized();
  double x = axis[0];
  double y = axis[1];
  double z = axis[2];

  double c = cos(angle);
  double s = sin(angle);

  return Matrix(
    x * x * (1 - c) + c, x * y * (1 - c) - z * s, x * z * (1 - c) + y * s, 0,
    y * x * (1 - c) + z * s, y * y * (1 - c) + c, y * z * (1 - c) - x * s, 0,
    x * z * (1 - c) - y * s, y * z * (1 - c) + x * s, z * z * (1 - c) + c, 0,
    0, 0, 0, 1);
}


//////////////////////////////////////////////////////////////////////
Matrix Matrix::frustum(double left, double right,double bottom, double top,double nearZ, double farZ)
{
  Matrix m = Matrix::identity(4);
  #define M(row,col)  m[col*4+row]
    M(0,0) = (2*nearZ) / (right-left); M(0,1) =                        0;  M(0,2) =   (right+left) / (right-left);  M(0,3) = 0;
    M(1,0) =                        0; M(1,1) = (2*nearZ) / (top-bottom);  M(1,2) =   (top+bottom) / (top-bottom);  M(1,3) = 0;
    M(2,0) =                        0; M(2,1) =                        0;  M(2,2) = -(farZ+nearZ) / ( farZ-nearZ);  M(2,3) =  -(2*farZ*nearZ) / (farZ-nearZ);
    M(3,0) =                        0; M(3,1) =                        0;  M(3,2) =                            -1;  M(3,3) = 0;
  #undef M
  return m.transpose();
}

//////////////////////////////////////////////////////////////////////
Matrix Matrix::ortho(double left, double right,double bottom, double top,double nearZ, double farZ)
{
  Matrix m = Matrix::identity(4);
  #define M(row,col)  m[col*4+row]
    M(0,0) = 2 / (right-left);M(0,1) =                0;M(0,2) =                 0;M(0,3) = -(right+left) / (right-left);
    M(1,0) =                0;M(1,1) = 2 / (top-bottom);M(1,2) =                 0;M(1,3) = -(top+bottom) / (top-bottom);
    M(2,0) =                0;M(2,1) =                0;M(2,2) = -2 / (farZ-nearZ);M(2,3) = -(farZ+nearZ) / (farZ-nearZ);
    M(3,0) =                0;M(3,1) =                0;M(3,2) =                 0;M(3,3) = 1;
  #undef M
  return m.transpose();
}


//////////////////////////////////////////////////////////////////////
Matrix Matrix::viewport(int x,int y,int width,int height)
{
  return Matrix(
    width/2.0,            0,  0    ,   x+width /2.0,
            0,   height/2.0,  0    ,   y+height/2.0,
            0,            0,  1/2.0,   0+     1/2.0,
            0,            0,      0,              1
    );
}

//////////////////////////////////////////////////////////////////////
Matrix Matrix::scaleAroundAxis(Point3d axis, double k)
{
  return
    Matrix(
      1 + (k - 1) * axis[0] * axis[0], (k - 1) * axis[0] * axis[1], (k - 1) * axis[0] * axis[2], 0,
      (k - 1) * axis[0] * axis[1], 1 + (k - 1) * axis[1] * axis[1], (k - 1) * axis[1] * axis[2], 0,
      (k - 1) * axis[0] * axis[2], (k - 1) * axis[1] * axis[2], 1 + (k - 1) * axis[2] * axis[2], 0,
      0, 0, 0, 1);
}

//////////////////////////////////////////////////////////////////////
Matrix Matrix::embed(int axis, double offset)
{
  VisusAssert(axis >= 0 && axis < 3);
  Matrix ret = Matrix::identity(4);
  const Point3d X(1, 0, 0);
  const Point3d Y(0, 1, 0);
  const Point3d Z(0, 0, 1);
  const Point3d W(0, 0, 0);
  if (axis == 0)
    return Matrix::translate(axis, offset) * Matrix(Y, Z, X, W);
  else if (axis == 1)
    return Matrix::translate(axis, offset) * Matrix(X, Z, Y, W);
  else
    return Matrix::translate(axis, offset) * Matrix(X, Y, Z, W);
}

  


//////////////////////////////////////////////////////////
Quaternion Matrix::toQuaternion() const
{
  VisusAssert(getSpaceDim() == 3 || getSpaceDim() == 4);

  const auto& m = *this;

  double kRot[3][3] =
  {
    {m(0,0) , m(0,1) , m(0,2)},
    {m(1,0) , m(1,1) , m(1,2)},
    {m(2,0) , m(2,1) , m(2,2)}
  };

  // Algorithm in Ken Shoemake's article in 1987 SIGGRAPH course notes
  // article "Quaternion Calculus and Fast Animation".

  double fTrace = kRot[0][0] + kRot[1][1] + kRot[2][2];
  double fRoot;

  double qw, qx, qy, qz;

  if (fTrace > 0.0)
  {
    // |w| > 1/2, may as well choose w > 1/2
    fRoot = (double)sqrt(fTrace + 1.0);  // 2w
    qw = 0.5f * fRoot;
    fRoot = 0.5f / fRoot;  // 1/(4w)
    qx = (kRot[2][1] - kRot[1][2]) * fRoot;
    qy = (kRot[0][2] - kRot[2][0]) * fRoot;
    qz = (kRot[1][0] - kRot[0][1]) * fRoot;
  }
  else
  {
    // |w| <= 1/2
    int s_iNext[3] = { 1, 2, 0 };
    int i = 0;
    if (kRot[1][1] > kRot[0][0])
      i = 1;
    if (kRot[2][2] > kRot[i][i])
      i = 2;
    int j = s_iNext[i];
    int k = s_iNext[j];

    fRoot = (double)sqrt(kRot[i][i] - kRot[j][j] - kRot[k][k] + 1.0);
    double* apkQuat[3] = { &qx, &qy, &qz };
    *apkQuat[i] = 0.5f * fRoot;
    fRoot = (double)0.5f / fRoot;
    qw = (kRot[k][j] - kRot[j][k]) * fRoot;
    *apkQuat[j] = (kRot[j][i] + kRot[i][j]) * fRoot;
    *apkQuat[k] = (kRot[k][i] + kRot[i][k]) * fRoot;
  }

  return Quaternion(qw, qx, qy, qz);
}



//////////////////////////////////////////////////////////
Matrix Matrix::rotate(const Quaternion& q_,Point3d vt)
{
  auto q = q_.toPoint4d();
  double fTx = 2.0f * q[0]; double fTy = 2.0f * q[1]; double fTz = 2.0f * q[2];
  double fTwx = fTx * q[3]; double fTwy = fTy * q[3]; double fTwz = fTz * q[3];
  double fTxx = fTx * q[0]; double fTxy = fTy * q[0]; double fTxz = fTz * q[0];
  double fTyy = fTy * q[1]; double fTyz = fTz * q[1]; double fTzz = fTz * q[2];

  return Matrix(
    1.0f - (fTyy + fTzz), (fTxy - fTwz), (fTxz + fTwy), vt[0],
    (fTxy + fTwz), 1.0f - (fTxx + fTzz), (fTyz - fTwx), vt[1],
    (fTxz - fTwy), (fTyz + fTwx), 1.0f - (fTxx + fTyy), vt[2],
    0,0,0,1);
}


//////////////////////////////////////////////////////////////////////
QDUMatrixDecomposition::QDUMatrixDecomposition(const Matrix& T) 
{
  double m[3][3]=
  {
    {T(0,0) , T(0,1) , T(0,2)},
    {T(1,0) , T(1,1) , T(1,2)},
    {T(2,0) , T(2,1) , T(2,2)}
  };

  double kQ[3][3];

  double fInvLength = m[0][0]*m[0][0] + m[1][0]*m[1][0] + m[2][0]*m[2][0];
  if (!Utils::doubleAlmostEquals(fInvLength,0)) fInvLength = 1.0/sqrt(fInvLength);

  kQ[0][0] = m[0][0]*fInvLength;
  kQ[1][0] = m[1][0]*fInvLength;
  kQ[2][0] = m[2][0]*fInvLength;

  double fDot = kQ[0][0]*m[0][1] + kQ[1][0]*m[1][1] + kQ[2][0]*m[2][1];
  kQ[0][1] = m[0][1]-fDot*kQ[0][0];
  kQ[1][1] = m[1][1]-fDot*kQ[1][0];
  kQ[2][1] = m[2][1]-fDot*kQ[2][0];
  fInvLength = kQ[0][1]*kQ[0][1] + kQ[1][1]*kQ[1][1] + kQ[2][1]*kQ[2][1];
  if (!Utils::doubleAlmostEquals(fInvLength,0)) fInvLength = 1.0/sqrt(fInvLength);
        
  kQ[0][1] *= fInvLength;
  kQ[1][1] *= fInvLength;
  kQ[2][1] *= fInvLength;

  fDot = kQ[0][0]*m[0][2] + kQ[1][0]*m[1][2] + kQ[2][0]*m[2][2];
  kQ[0][2] = m[0][2]-fDot*kQ[0][0];
  kQ[1][2] = m[1][2]-fDot*kQ[1][0];
  kQ[2][2] = m[2][2]-fDot*kQ[2][0];

  fDot = kQ[0][1]*m[0][2] + kQ[1][1]*m[1][2] + kQ[2][1]*m[2][2];
  kQ[0][2] -= fDot*kQ[0][1];
  kQ[1][2] -= fDot*kQ[1][1];
  kQ[2][2] -= fDot*kQ[2][1];

  fInvLength = kQ[0][2]*kQ[0][2] + kQ[1][2]*kQ[1][2] + kQ[2][2]*kQ[2][2];
  if (!Utils::doubleAlmostEquals(fInvLength,0)) fInvLength = 1.0/sqrt(fInvLength);

  kQ[0][2] *= fInvLength;
  kQ[1][2] *= fInvLength;
  kQ[2][2] *= fInvLength;

  double fDet = 
      kQ[0][0]*kQ[1][1]*kQ[2][2] + kQ[0][1]*kQ[1][2]*kQ[2][0] +
      kQ[0][2]*kQ[1][0]*kQ[2][1] - kQ[0][2]*kQ[1][1]*kQ[2][0] -
      kQ[0][1]*kQ[1][0]*kQ[2][2] - kQ[0][0]*kQ[1][2]*kQ[2][1];

  if ( fDet < 0.0 )
  {
    for (int iRow = 0; iRow < 3; iRow++)
    for (int iCol = 0; iCol < 3; iCol++)
        kQ[iRow][iCol] = -kQ[iRow][iCol];
  }

  double kR[3][3];
  kR[0][0] = kQ[0][0]*m[0][0] + kQ[1][0]*m[1][0] +kQ[2][0]*m[2][0];
  kR[0][1] = kQ[0][0]*m[0][1] + kQ[1][0]*m[1][1] +kQ[2][0]*m[2][1];
  kR[1][1] = kQ[0][1]*m[0][1] + kQ[1][1]*m[1][1] +kQ[2][1]*m[2][1];
  kR[0][2] = kQ[0][0]*m[0][2] + kQ[1][0]*m[1][2] +kQ[2][0]*m[2][2];
  kR[1][2] = kQ[0][1]*m[0][2] + kQ[1][1]*m[1][2] +kQ[2][1]*m[2][2];
  kR[2][2] = kQ[0][2]*m[0][2] + kQ[1][2]*m[1][2] +kQ[2][2]*m[2][2];

  this->D[0] = kR[0][0];
  this->D[1] = kR[1][1];
  this->D[2] = kR[2][2];

  double fInvD0 = 1.0f/this->D[0];
  this->U[0] = kR[0][1]*fInvD0;
  this->U[1] = kR[0][2]*fInvD0;
  this->U[2] = kR[1][2]/this->D[1];

  this->Q= Matrix::identity(4);
  this->Q(0,0)=kQ[0][0];this->Q(0,1)=kQ[0][1];this->Q(0,2)=kQ[0][2];
  this->Q(1,0)=kQ[1][0];this->Q(1,1)=kQ[1][1];this->Q(1,2)=kQ[1][2];
  this->Q(2,0)=kQ[2][0];this->Q(2,1)=kQ[2][1];this->Q(2,2)=kQ[2][2];
}

//////////////////////////////////////////////////////////////////////
TRSMatrixDecomposition::TRSMatrixDecomposition(const Matrix& T)  
{
  QDUMatrixDecomposition qdu(T);
  this->translate   = Point3d(T(0,3),T(1,3),T(2,3));
  this->rotate      = qdu.Q.toQuaternion();
  this->scale       = qdu.D;
}

//////////////////////////////////////////////////////////////////////
Matrix Matrix::submatrix(int row, int column) const
{
  Matrix result(this->dim - 1);
  int subi = 0;
  for (int i = 0; i < this->dim; i++)
  {
    int subj = 0;
    
    if (i == row) 
      continue;
    
    for (int j = 0; j < this->dim; j++) 
    {
      if (j == column) 
        continue;

      result(subi, subj) = get(i, j);
      subj++;
    }

    subi++;
  }
  return result;
}

//////////////////////////////////////////////////////////////////////
double Matrix::getMinor(int row, int column) const {
  if (this->dim == 2)
    return Matrix(get(1, 1), get(1, 0), get(0, 1), get(0, 0)).determinant();
  else
    return submatrix(row, column).determinant();
}

//////////////////////////////////////////////////////////////////////
double Matrix::cofactor(int row, int column) const {
  double minor;

  // special case for when our matrix is 2x2
  if (this->dim == 2) {
    if (row == 0 && column == 0) minor = get(1, 1);
    else if (row == 1 && column == 1) minor = get(0, 0);
    else if (row == 0 && column == 1) minor = get(1, 0);
    else if (row == 1 && column == 0) minor = get(0, 1);
  }
  else
  {
    minor = getMinor(row, column);
  }

  return (row + column) % 2 == 0 ? minor : -minor;
}

//////////////////////////////////////////////////////////////////////
Matrix Matrix::cofactorMatrix() const {
  Matrix ret(this->dim);
  for (int i = 0; i < this->dim; i++) {
    for (int j = 0; j < this->dim; j++) {
      ret(i, j) = cofactor(i, j);
    }
  }
  return ret;
}

//////////////////////////////////////////////////////////////////////
Matrix Matrix::adjugate() const {
  return cofactorMatrix().transpose();
}

//////////////////////////////////////////////////////////////////////
double Matrix::determinant() const
{
  if (this->dim == 2)
    return get(0, 0) * get(1, 1) - get(1, 0) * get(0, 1);

  double ret = 0;
  for (int c = 0; c < this->dim; c++) 
    ret += pow(-1, c) * operator()(0, c) * submatrix(0, c).determinant();
  return ret;
}

//////////////////////////////////////////////////////////////////////
Matrix Matrix::invert() const
{
  if (isIdentity())
    return *this;

  if (isZero())
    return *this;

  double det = determinant();

  //not invertible
  if (det == 0)
    return *this;

  return adjugate() * (1.0 / det);
}

} //namespace Visus

