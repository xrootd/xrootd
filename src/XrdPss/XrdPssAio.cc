/******************************************************************************/
/*                                                                            */
/*                          X r d P s s A i o . c c                           */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
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
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <cstdio>
#include <unistd.h>

#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdPosix/XrdPosixExtra.hh"
#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdPss/XrdPss.hh"
#include "XrdPss/XrdPssAioCB.hh"
#include "XrdSfs/XrdSfsAio.hh"

// All AIO interfaces are defined here.
 
/******************************************************************************/
/*                                 F s y n c                                  */
/******************************************************************************/
  
/*
  Function: Async fsync() a file

  Input:    aiop      - A aio request object
*/

int XrdPssFile::Fsync(XrdSfsAio *aiop)
{

// Execute this request in an asynchronous fashion
//
   XrdPosixXrootd::Fsync(fd, XrdPssAioCB::Alloc(aiop, true));
   return 0;
}

/******************************************************************************/
/*                                p g R e a d                                 */
/******************************************************************************/

/*
  Function: Async read bytes from the associated file

  Input:    aiop      - An aio request object

   Output:  <0 -> Operation failed, value is negative errno value.
            =0 -> Operation queued
            >0 -> Operation not queued, system resources unavailable or
                                        asynchronous I/O is not supported.
*/
  
int XrdPssFile::pgRead(XrdSfsAio* aiop, uint64_t opts)
{
   XrdPssAioCB *aioCB = XrdPssAioCB::Alloc(aiop, false, true);
   uint64_t psxOpts = (aiop->cksVec ? XrdPosixExtra::forceCS : 0);

// Execute this request in an asynchronous fashion
//
   XrdPosixExtra::pgRead(fd, (void *)aiop->sfsAio.aio_buf,
                             (off_t)aiop->sfsAio.aio_offset,
                             (size_t)aiop->sfsAio.aio_nbytes,
                             aioCB->csVec, psxOpts, aioCB);
   return 0;
}

/******************************************************************************/
/*                               p g W r i t e                                */
/******************************************************************************/
  
/*
  Function: Async write bytes from into the associated file

  Input:    aiop      - An aio request object.

   Output:  <0 -> Operation failed, value is negative errno value.
            =0 -> Operation queued
            >0 -> Operation not queued, system resources unavailable or
                                        asynchronous I/O is not supported.
*/
  
int XrdPssFile::pgWrite(XrdSfsAio *aiop, uint64_t opts)
{

// Check if caller wants to verify the checksums before writing
//
   if (aiop->cksVec && (opts & XrdOssDF::Verify))
      {XrdOucPgrwUtils::dataInfo dInfo((const char *)(aiop->sfsAio.aio_buf),
                                       aiop->cksVec, aiop->sfsAio.aio_offset,
                                       aiop->sfsAio.aio_nbytes);
       off_t bado;
       int   badc;
       if (!XrdOucPgrwUtils::csVer(dInfo, bado, badc)) return -EDOM;
      }

// Get a callback object as no errors can error here
//
   XrdPssAioCB *aioCB = XrdPssAioCB::Alloc(aiop, true, true);

// Check if caller want checksum generated and possibly returned
//
   if ((opts & XrdOssDF::doCalc) || aiop->cksVec == 0)
      {XrdOucPgrwUtils::csCalc((const char *)(aiop->sfsAio.aio_buf),
                               (off_t)(aiop->sfsAio.aio_offset),
                               (size_t)(aiop->sfsAio.aio_nbytes),
                               aioCB->csVec);
       if (aiop->cksVec) memcpy(aiop->cksVec, aioCB->csVec.data(),
                                aioCB->csVec.size()*sizeof(uint32_t));
      } else {
       int n = XrdOucPgrwUtils::csNum(aiop->sfsAio.aio_offset,
                                      aiop->sfsAio.aio_nbytes);
       aioCB->csVec.resize(n);
       aioCB->csVec.assign(n, 0);
       memcpy(aioCB->csVec.data(), aiop->cksVec, n*sizeof(uint32_t));
      }

// Issue the pgWrite
//
   XrdPosixExtra::pgWrite(fd, (void *)aiop->sfsAio.aio_buf,
                              (off_t)aiop->sfsAio.aio_offset,
                              (size_t)aiop->sfsAio.aio_nbytes,
                              aioCB->csVec, 0, aioCB);
   return 0;
}
  
/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/

/*
  Function: Async read `blen' bytes from the associated file, placing in 'buff'

  Input:    aiop      - An aio request object

   Output:  <0 -> Operation failed, value is negative errno value.
            =0 -> Operation queued
            >0 -> Operation not queued, system resources unavailable or
                                        asynchronous I/O is not supported.
*/
  
int XrdPssFile::Read(XrdSfsAio *aiop)
{

// Execute this request in an asynchronous fashion
//
   XrdPosixXrootd::Pread(fd, (void *)aiop->sfsAio.aio_buf,
                             (size_t)aiop->sfsAio.aio_nbytes,
                             (off_t)aiop->sfsAio.aio_offset,
                             XrdPssAioCB::Alloc(aiop, false));
   return 0;
}

/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/
  
/*
  Function: Async write `blen' bytes from 'buff' into the associated file

  Input:    aiop      - An aio request object.

   Output:  <0 -> Operation failed, value is negative errno value.
            =0 -> Operation queued
            >0 -> Operation not queued, system resources unavailable or
                                        asynchronous I/O is not supported.
*/
  
int XrdPssFile::Write(XrdSfsAio *aiop)
{

// Execute this request in an asynchronous fashion
//
   XrdPosixXrootd::Pwrite(fd, (const void *)aiop->sfsAio.aio_buf,
                              (size_t)aiop->sfsAio.aio_nbytes,
                              (off_t)aiop->sfsAio.aio_offset,
                              XrdPssAioCB::Alloc(aiop, true));
   return 0;
}
