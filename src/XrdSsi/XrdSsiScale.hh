#ifndef __XRDSSISCALE_HH__
#define __XRDSSISCALE_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i S c a l e . h h                         */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string.h>

#include "XrdSys/XrdSysPthread.hh"

class XrdSsiScale
{
public:

static const int          maxSprd =256;
static const int          maxEnt  = 32;      // Must be power of two
static const int          entShft = 8;       // Allows a spread of 256
static const unsigned int maxPend = 65500;

int   getEnt() {entMutex.Lock();
                if (pendCnt[nowEnt] < maxPend) 
                   {pendCnt[nowEnt]++;
                    if (maxSpread) return Spread(nowEnt);
                    entMutex.UnLock();
                    return nowEnt << entShft;
                   }
                int xEnt = (nowEnt < maxEnt ? nowEnt+1 : 0);
                int zEnt = maxEnt;
                do {for (int i = xEnt; i < zEnt; i++)
                        {if (pendCnt[i] < maxPend)
                            {pendCnt[i]++;
                             nowEnt = i;
                             if (maxSpread) return Spread(i);
                             entMutex.UnLock();
                             return i;

                            }
                        }
                    if (!xEnt) break;
                    xEnt = 0; zEnt = nowEnt;
                   } while(true);
                 entMutex.UnLock();
                 return -1;
                }

void  retEnt(int xEnt) {xEnt >>= entShft;
                        if (xEnt >= 0 && xEnt < maxEnt)
                           {entMutex.Lock();
                            if (pendCnt[xEnt]) pendCnt[xEnt]--;
                            entMutex.UnLock();
                           }
                       }

bool  rsvEnt(int xEnt) {xEnt >>= entShft;
                        if (xEnt < 0 && xEnt >= maxEnt) return false;
                        entMutex.Lock();
                        if (pendCnt[nowEnt] < maxPend)
                           {pendCnt[nowEnt]++;
                            entMutex.UnLock();
                            return true;
                           }
                        entMutex.UnLock();
                        return false;
                       }

void  setSpread(short sval) {if (sval <= 0) maxSpread = 0;
                                else if (sval < maxSprd) maxSpread = sval;
                                        else maxSpread = maxSprd;
                            }

      XrdSsiScale() : nowEnt(0), maxSpread(4), nowSpread(0)
                      {memset(pendCnt, 0, sizeof(uint16_t)*maxEnt);}

     ~XrdSsiScale() {}

private:

int   Spread(int ent) // Called with entMutex locked and return unlocked.
            {int n = nowSpread;
             nowSpread++;
             if (nowSpread >= maxSpread) nowSpread = 0;
             entMutex.UnLock();
             return (ent << entShft) | n;
            }

XrdSysMutex entMutex;
uint16_t    pendCnt[maxEnt];
int         nowEnt;
short       maxSpread;
short       nowSpread;
};
#endif
