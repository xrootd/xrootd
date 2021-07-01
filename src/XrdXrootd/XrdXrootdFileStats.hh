#ifndef __XRDXROOTDFILESTATS__
#define __XRDXROOTDFILESTATS__
/******************************************************************************/
/*                                                                            */
/*                 X r d X r o o t d F i l e S t a t s . h h                  */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdXrootd/XrdXrootdMonData.hh"

class XrdXrootdFileStats
{
public:

kXR_unt32           FileID;   // Unique file id used for monitoring
short               MonEnt;   // Set by mon: entry in reporting table or -1
char                monLvl;   // Set by mon: level of data collection needed
char                xfrXeq;   // Transfer has occurred
long long           fSize;    // Size of file when opened
XrdXrootdMonStatXFR xfr;
XrdXrootdMonStatOPS ops;
XrdXrootdMonStatPRW prw;
struct {double      read;     // sum(read_size[i] **2) i = 1 to Ops.read
        double      readv;    // sum(readv_size[i]**2) i = 1 to Ops.readv
        double      rsegs;    // sum(readv_segs[i]**2) i = 1 to Ops.readv
        double      write;    // sum(write_size[i]**2) i = 1 to Ops.write
       }            ssq;

enum monLevel {monOff = 0, monOn = 1, monOps = 2, monSsq = 3};

       void Init()
                {FileID = 0; MonEnt = -1; monLvl = xfrXeq = 0;
                 memset(&xfr, 0, sizeof(xfr));
                 memset(&ops, 0, sizeof(ops));
                 memset(&prw, 0, sizeof(prw));
                 ops.rsMin = 0x7fff;
                 ops.rdMin = ops.rvMin = ops.wrMin = 0x7fffffff;
                 ssq.read  = ssq.readv = ssq.write = ssq.rsegs = 0.0;
                };

inline void pgrOps(int rsz, bool isRetry=false)
                  {if (monLvl)
                      {prw.rBytes += rsz;
                       prw.rCount++;
                       if(isRetry) prw.rRetry++;
                      }
                   }

inline void pgwOps(int wsz, bool isRetry=false)
                  {if (monLvl)
                      {prw.wBytes += wsz;
                       prw.wCount++;
                       if(isRetry) prw.wRetry++;
                      }
                   }

inline void pgUpdt(int wErrs, int wFixd, int wUnc)
                  {if (monLvl)
                      {prw.wcsErr = wErrs;
                       prw.wRetry = wFixd;
                       prw.wcsUnc = wUnc;
                      }
                  }

inline void rdOps(int rsz)
                 {if (monLvl)
                     {xfr.read += rsz; ops.read++; xfrXeq = 1;
                      if (monLvl > 1)
                         {if (rsz < ops.rdMin) ops.rdMin = rsz;
                          if (rsz > ops.rdMax) ops.rdMax = rsz;
                          if (monLvl > 2)
                             ssq.read  += static_cast<double>(rsz)
                                        * static_cast<double>(rsz);
                         }
                     }
                 }

inline void rvOps(int rsz, int ssz)
                 {if (monLvl)
                     {xfr.readv += rsz; ops.readv++; ops.rsegs += ssz; xfrXeq=1;
                      if (monLvl > 1)
                         {if (rsz < ops.rvMin) ops.rvMin = rsz;
                          if (rsz > ops.rvMax) ops.rvMax = rsz;
                          if (ssz < ops.rsMin) ops.rsMin = ssz;
                          if (ssz > ops.rsMax) ops.rsMax = ssz;
                          if (monLvl > 2)
                             {ssq.readv += static_cast<double>(rsz)
                                         * static_cast<double>(rsz);
                              ssq.rsegs += static_cast<double>(ssz)
                                         * static_cast<double>(ssz);
                             }
                         }
                     }
                 }

inline void wrOps(int wsz)
                 {if (monLvl)
                     {xfr.write += wsz; ops.write++; xfrXeq = 1;
                      if (monLvl > 1)
                         {if (wsz < ops.wrMin) ops.wrMin = wsz;
                          if (wsz > ops.wrMax) ops.wrMax = wsz;
                          if (monLvl > 2)
                             ssq.write += static_cast<double>(wsz)
                                        * static_cast<double>(wsz);
                         }
                     }
                 }

inline void wvOps(int wsz, int ssz) {wrOps(wsz);}
/* !!! When we start reporting detail of writev's we will uncomment this
   !!! For now writev's are treated as single write, not correct but at least
   !!! the data gets counted.
                 {if (monLvl)
                     {xfr.writev += wsz; ops.writev++; ops.wsegs += ssz; xfrXeq=1;
                      if (monLvl > 1)
                         {if (wsz < ops.wvMin) ops.wvMin = wsz;
                          if (wsz > ops.wvMax) ops.wvMax = wsz;
                          if (ssz < ops.wsMin) ops.wsMin = ssz;
                          if (ssz > ops.wsMax) ops.wsMax = ssz;
                          if (monLvl > 2)
                             {ssq.writev+= static_cast<double>(wsz)
                                         * static_cast<double>(wsz);
                              ssq.wsegs += static_cast<double>(ssz)
                                         * static_cast<double>(ssz);
                             }
                         }
                     }
                 }
*/
       XrdXrootdFileStats() {Init();}
      ~XrdXrootdFileStats() {}
};
#endif
