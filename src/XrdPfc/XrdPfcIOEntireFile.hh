#ifndef __XRDPFC_IO_ENTIRE_FILE_HH__
#define __XRDPFC_IO_ENTIRE_FILE_HH__
//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman
//----------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------

#include <string>

#include "XrdSys/XrdSysPthread.hh"
#include "XrdPfcIO.hh"
#include "XrdPfc.hh"
#include "XrdPfcStats.hh"
#include "XrdPfcFile.hh"

class XrdSysError;
class XrdOssDF;
class XrdOucIOVec;

namespace XrdPfc
{
//----------------------------------------------------------------------------
//! \brief Downloads original file into a single file on local disk.
//! Handles read requests as they come along.
//----------------------------------------------------------------------------
class IOEntireFile : public IO
{
public:
   IOEntireFile(XrdOucCacheIO *io, Cache &cache);

   ~IOEntireFile();

   //------------------------------------------------------------------------
   //! Check if File was opened successfully.
   //------------------------------------------------------------------------
   bool HasFile() const { return m_file != 0; }

   //---------------------------------------------------------------------
   //! Pass Read request to the corresponding File object.
   //!
   //! @param Buffer
   //! @param Offset
   //! @param Length
   //!
   //! @return number of bytes read
   //---------------------------------------------------------------------
   using XrdOucCacheIO::Read;

   virtual int Read(char *Buffer, long long Offset, int Length);

   //---------------------------------------------------------------------
   //! Pass ReadV request to the corresponding File object.
   //!
   //! @param readV
   //! @param n number of XrdOucIOVecs
   //!
   //! @return total bytes read
   //---------------------------------------------------------------------
   using XrdOucCacheIO::ReadV;

   virtual int ReadV(const XrdOucIOVec *readV, int n);

   virtual void Update(XrdOucCacheIO &iocp);

   //! \brief Abstract virtual method of XrdPfcIO
   //! Called to check if destruction needs to be done in a separate task.
   bool ioActive() /* override */;

   //! \brief Abstract virtual method of XrdPfcIO
   //! Called to destruct the IO object after it is no longer used.
   void DetachFinalize() /* override */;
   
   virtual int  Fstat(struct stat &sbuff);

   virtual long long FSize();

private:
   File        *m_file;
   struct stat *m_localStat;
   int initCachedStat(const char* path);
};

}
#endif

