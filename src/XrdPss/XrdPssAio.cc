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

#include <stdio.h>
#include <unistd.h>

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
