/******************************************************************************/
/*                                                                            */
/*                 X r d O s s C s i F i l e A i o . c c                      */
/*                                                                            */
/* (C) Copyright 2021 CERN.                                                   */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* In applying this licence, CERN does not waive the privileges and           */
/* immunities granted to it by virtue of its status as an Intergovernmental   */
/* Organization or submit itself to any jurisdiction.                         */
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
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include "XrdOssCsiTrace.hh"
#include "XrdOssCsi.hh"
#include "XrdOssCsiPages.hh"
#include "XrdOssCsiFileAio.hh"
#include "XrdOuc/XrdOucCRC.hh"

#include <string>
#include <algorithm>
#include <mutex>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

extern XrdOucTrace  OssCsiTrace;

XrdOssCsiFileAioStore::~XrdOssCsiFileAioStore()
{
   XrdOssCsiFileAio *p;
   while((p=list_))
   {
      list_ = list_->next_;
      delete p;
   }
}

int XrdOssCsiFile::Read(XrdSfsAio *aiop)
{
   if (!pmi_) return -EBADF;

   XrdOssCsiFileAio *nio = XrdOssCsiFileAio::Alloc(&aiostore_);
   nio->Init(aiop, this, false, 0, true);
   nio->SchedReadJob();
   return 0;
}

int XrdOssCsiFile::Write(XrdSfsAio *aiop)
{
   if (!pmi_) return -EBADF;
   if (rdonly_) return -EBADF;

   XrdOssCsiFileAio *nio = XrdOssCsiFileAio::Alloc(&aiostore_);
   nio->Init(aiop, this, false, 0, false);
   // pages will be locked when write is scheduled
   nio->SchedWriteJob();
   return 0;
}

int XrdOssCsiFile::pgRead(XrdSfsAio *aioparm, uint64_t opts)
{
   if (!pmi_) return -EBADF;

   XrdOssCsiFileAio *nio = XrdOssCsiFileAio::Alloc(&aiostore_);
   nio->Init(aioparm, this, true, opts, true);
   nio->SchedReadJob();
   return 0;
}

int XrdOssCsiFile::pgWrite(XrdSfsAio *aioparm, uint64_t opts)
{
   if (!pmi_) return -EBADF;
   if (rdonly_) return -EBADF;
   uint64_t pgopts = opts;

   const int prec = XrdOssCsiPages::pgWritePrelockCheck(
          (void *)aioparm->sfsAio.aio_buf,
          (off_t)aioparm->sfsAio.aio_offset,
          (size_t)aioparm->sfsAio.aio_nbytes,
          aioparm->cksVec,
          opts);
   if (prec < 0)
   {
      return prec;
   }
          
   XrdOssCsiFileAio *nio = XrdOssCsiFileAio::Alloc(&aiostore_);
   nio->Init(aioparm, this, true, pgopts, false);
   // pages will be locked when write is scheduled
   nio->SchedWriteJob();
   return 0;
}

int XrdOssCsiFile::Fsync(XrdSfsAio *aiop)
{
   aioWait();
   aiop->Result = this->Fsync();
   aiop->doneWrite();
   return 0;
}
