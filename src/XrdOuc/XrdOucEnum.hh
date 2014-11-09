//------------------------------------------------------------------------------
// Copyright (c) 2013 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#ifndef __XRD_OUC_ENUM_HH__
#define __XRD_OUC_ENUM_HH__

#define XRDOUC_ENUM_OPERATORS( T ) \
  inline T  operator |  (const T  a, const T b) { return T(int(a) | int(b)); }  \
  inline T &operator |= (      T &a, const T b) { return a = a | b; }           \
  inline T  operator &  (const T  a, const T b) { return T(int(a) & int(b)); }  \
  inline T &operator &= (      T &a, const T b) { return a = a & b; }           \
  inline T  operator ^  (const T  a, const T b) { return T(int(a) ^ int(b)); }  \
  inline T &operator ^= (      T &a, const T b) { return a = a ^ b; }           \
  inline T  operator ~  (const T a)             { return T(~int(a)); }


#endif // __XRD_OUC_ENUM_HH__
