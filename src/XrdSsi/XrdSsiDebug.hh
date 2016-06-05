#ifndef _XRDSSI_DEBUG_H
#define _XRDSSI_DEBUG_H
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i D e b u g . h h                         */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <iostream>
#include <pthread.h>
#include <stdlib.h>

// This class is used by the client to perform tracing. The server uses the
// XrdSsiTrace.hh to implement tracing due to infrastructure differences.
//
class XrdSsiDebug
{
public:

bool  isON;

void  Beg(const char *epname=0)
         {pthread_mutex_lock(&debugMutex);
          if (epname) std::cerr <<epname <<": ";
         }

void  End() {std::cerr <<'\n' <<std::flush; pthread_mutex_unlock(&debugMutex);}

      XrdSsiDebug() {pthread_mutex_init(&debugMutex, NULL);
                     isON = (getenv("XRDSSIDEBUG") != 0);
                    }

     ~XrdSsiDebug() {pthread_mutex_destroy(&debugMutex);}

private:

pthread_mutex_t debugMutex;
};

#ifndef NODEBUG

namespace XrdSsi
{
extern    XrdSsiDebug DeBug;
}

#define DBG(txt) \
if (XrdSsi::DeBug.isON) \
   {XrdSsi::DeBug.Beg(epname); std::cerr<<'['<<std::hex<<this<<std::dec<<"] "<<txt; \
    XrdSsi::DeBug.End();}

#define DBGNT(txt) \
if (XrdSsi::DeBug.isON) \
   {XrdSsi::DeBug.Beg(epname); cerr <<txt; XrdSsi::DeBug.End();}

#define EPNAME(x) static const char *epname = x;

#else

#define DBG(y)
#define DBGTASK(y)
#define EPNAME(x)
#endif
#endif
