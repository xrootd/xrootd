#ifndef __SFS_AIO_H__
#define __SFS_AIO_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d S f s A i o . h h                           */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <signal.h>
#include <sys/types.h>
// _POSIX_ASYNCHRONOUS_IO, if it is defined, is in unistd.h.
#include <unistd.h>
#ifdef _POSIX_ASYNCHRONOUS_IO
#ifdef __APPLE__
#include <AvailabilityMacros.h>
#include <sys/aio.h>
#else
#include <aio.h>
#endif
#else
struct aiocb {           // Minimal structure to avoid compiler errors
       int    aio_fildes;
       void  *aio_buf;
       size_t aio_nbytes;
       off_t  aio_offset;
       int    aio_reqprio;
       struct sigevent aio_sigevent;
      };
#endif

// The XrdSfsAIO class is meant to be derived. This object provides the
// basic interface to handle AIO control block queues not processing.
//
class XrdSfsAio
{
public:

struct aiocb sfsAio;

ssize_t      Result; // If >= 0 valid result; else is -errno

const char  *TIdent; // Trace information (optional)

// Method to handle completed reads
//
virtual void doneRead() = 0;

// Method to hand completed writes
//
virtual void doneWrite() = 0;

// Method to recycle free object
//
virtual void Recycle() = 0;

             XrdSfsAio() {
#if defined(__APPLE__) && (!defined(MAC_OS_X_VERSION_10_4) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_4)
                         sfsAio.aio_sigevent.sigev_value.sigval_ptr = (void *)this;
#else
                         sfsAio.aio_sigevent.sigev_value.sival_ptr  = (void *)this;
#endif
                         sfsAio.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
                         sfsAio.aio_reqprio = 0;
                         TIdent = (char *)"";
                        }
virtual     ~XrdSfsAio() {}
};
#endif
