#ifndef __XRDPOSIXSTATS_HH__
#define __XRDPOSIXSTATS_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d P o s i x S t a t s . h h                       */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>

#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdPosixStats
{
public:

struct PosixStats
{
long long Opens;
long long OpenErrs;
long long Closes;
long long CloseErrs;
}         X;

inline void Get(XrdPosixStats &D)
               {sMutex.Lock();
                memcpy(&D.X, &X, sizeof(PosixStats));
                sMutex.UnLock();
               }

inline void  Add(long long &Dest, long long Val)
                {sMutex.Lock(); Dest += Val; sMutex.UnLock();}

inline void  Count(long long &Dest)
                  {AtomicBeg(sMutex); AtomicInc(Dest); AtomicEnd(sMutex);}

inline void  Set(long long &Dest, long long Val)
                {sMutex.Lock(); Dest  = Val; sMutex.UnLock();}

inline void  Lock()   {sMutex.Lock();}
inline void  UnLock() {sMutex.UnLock();}

             XrdPosixStats() {memset(&X, 0, sizeof(PosixStats));}
            ~XrdPosixStats() {}
private:
XrdSysMutex sMutex;
};
#endif
