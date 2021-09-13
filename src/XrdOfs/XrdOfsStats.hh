#ifndef __XRDOFS_STATS_H__
#define __XRDOFS_STATS_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d O f s S t a t s . h h                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdlib>

#include "XrdSys/XrdSysPthread.hh"

class XrdOfsStats
{
public:

struct      StatsData
{
int         numOpenR;   // Read
int         numOpenW;   // Write
int         numOpenP;   // Posc
int         numUnpsist; // Posc
int         numHandles;
int         numRedirect;
int         numStarted;
int         numReplies;
int         numErrors;
int         numDelays;
int         numSeventOK;
int         numSeventER;
int         numTPCgrant;
int         numTPCdeny;
int         numTPCerrs;
int         numTPCexpr;
}           Data;

XrdSysMutex sdMutex;

inline void Add(int &Cntr) {sdMutex.Lock(); Cntr++; sdMutex.UnLock();}

inline void Dec(int &Cntr) {sdMutex.Lock(); Cntr--; sdMutex.UnLock();}

       int  Report(char *Buff, int Blen);

       void setRole(const char *theRole) {myRole = theRole;}

            XrdOfsStats() : myRole("?") {memset(&Data, 0, sizeof(Data));}
           ~XrdOfsStats() {}

private:

const char *myRole;
};
#endif
