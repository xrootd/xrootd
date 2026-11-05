#ifndef _XRDPSS_PRTYQ_H
#define _XRDPSS_PRTYQ_H
/******************************************************************************/
/*                                                                            */
/*                        X r d P s s P r t y Q . h h                         */
/*                                                                            */
/* (c) 2026 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <set>
#include <utility>
#include <string.h>

#include "XrdSys/XrdSysPthread.hh"

class XrdPssPrtyQ
{
public:

int   Assign() {XrdSysMutexHelper mHelp(pqMtx);
                if (pqAll) return pqAll++;
                auto nh = pqSet.extract(pqSet.begin());
                int kn = nh.value().second;
                nh.value().first = ++pqVec[kn];
                pqSet.insert(std::move(nh));
                pqMtx.UnLock();
                return kn;
               }

void  unAssign(int kn) {if (!pqAll)
                           {pqMtx.Lock();
                            auto it = pqSet.find({pqVec[kn], kn});
                            if (it != pqSet.end())
                               {auto nh = pqSet.extract(it);
                                nh.value().first = --pqVec[kn];
                                pqSet.insert(std::move(nh));
                               }
                            pqMtx.UnLock();
                           }
                        }

      XrdPssPrtyQ(int items)
                 {if (items <= 0)
                     {pqAll = 1;
                      pqVec = 0;
                     } else {
                      pqAll = 0;
                      pqVec = new int[items];
                      memset(pqVec, 0, items*sizeof(int));
                      for (int i = 0; i < items; i++) pqSet.insert({0, i});
                     }
                 }

     ~XrdPssPrtyQ() {delete pqVec;}


private:
std::set<std::pair<int, int>> pqSet;
int*        pqVec;
XrdSysMutex pqMtx;
int         pqAll;
};
#endif
