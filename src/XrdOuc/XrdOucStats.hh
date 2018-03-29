#ifndef _XRDOUCSTATS_HH_
#define _XRDOUCSTATS_HH_
/******************************************************************************/
/*                                                                            */
/*                        X r d O u c S t a t s . h h                         */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSys/XrdSysAtomics.hh"

#ifdef HAVE_ATOMICS
#define _statsADD(x,y) AtomicAdd(x,y)
#define _statsINC(x)   AtomicInc(x)
#else
#define _statsADD(x,y) statsMutex.Lock(); x+=y; statsMutex.UnLock()
#define _statsINC(x)   statsMutex.Lock(); x++;  statsMutex.UnLock()
#endif
  
class XrdOucStats
{
public:

inline void Bump(int &val)       {_statsINC(val);}

inline void Bump(int &val, int n){_statsADD(val,n);}

inline void Bump(long long &val)              {_statsINC(val);}

inline void Bump(long long &val, long long n) {_statsADD(val,n);}

XrdSysMutex statsMutex;   // Mutex to serialize updates

            XrdOucStats() {}
           ~XrdOucStats() {}
};
#endif
