/******************************************************************************/
/*                                                                            */
/*                    P y X R o o t D T a p e R e s t . h h                   */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/******************************************************************************/

#ifndef PYXROOTDTAPEREST_HH_
#define PYXROOTDTAPEREST_HH_

#include "PyXRootD.hh"

namespace PyXRootD
{
  PyObject* TapeRestDiscover_cpp( PyObject *self, PyObject *args );
  PyObject* TapeRestArchiveInfo_cpp( PyObject *self, PyObject *args );
}

#endif // PYXROOTDTAPEREST_HH_
