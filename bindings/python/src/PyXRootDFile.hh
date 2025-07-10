//------------------------------------------------------------------------------
// Copyright (c) 2012-2025 by European Organization for Nuclear Research (CERN)
// Author: Justin Salmon <jsalmon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef PYXROOTDFILE_HH_
#define PYXROOTDFILE_HH_

#include "PyXRootD.hh"
#include "Utils.hh"

#include "XrdCl/XrdClFile.hh"

#include <deque>

namespace PyXRootD
{
  //----------------------------------------------------------------------------
  //! XrdCl::File binding class
  //----------------------------------------------------------------------------
  class File
  {
    public:
      static PyObject* Open( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Close( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Stat( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Read( File *self, PyObject *args, PyObject *kwds );
      static PyObject* ReadLine( File *self, PyObject *args, PyObject *kwds );
      static PyObject* ReadLines( File *self, PyObject *args, PyObject *kwds );
      static XrdCl::Buffer* ReadChunk( File *self, uint64_t offset, uint32_t size );
      static PyObject* ReadChunks( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Write( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Sync( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Truncate( File *self, PyObject *args, PyObject *kwds );
      static PyObject* VectorRead( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Fcntl( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Visa( File *self, PyObject *args, PyObject *kwds );
      static PyObject* IsOpen( File *self, PyObject *args, PyObject *kwds );
      static PyObject* GetProperty( File *self, PyObject *args, PyObject *kwds );
      static PyObject* SetProperty( File *self, PyObject *args, PyObject *kwds );
      static PyObject* SetXAttr( File *self, PyObject *args, PyObject *kwds );
      static PyObject* GetXAttr( File *self, PyObject *args, PyObject *kwds );
      static PyObject* DelXAttr( File *self, PyObject *args, PyObject *kwds );
      static PyObject* ListXAttr( File *self, PyObject *args, PyObject *kwds );
      static PyObject* OpenUsingTemplate( File *self, PyObject *args, PyObject *kwds );
      static PyObject* Clone( File *self, PyObject *args, PyObject *kwds );
    public:
      PyObject_HEAD
      XrdCl::File                *file;
      uint64_t                    currentOffset;
  };

  //----------------------------------------------------------------------------
  //! File binding type-object declaration
  //----------------------------------------------------------------------------
  extern PyTypeObject FileType;
}

#endif /* PYXROOTDFILE_HH_ */
