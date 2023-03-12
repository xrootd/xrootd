#ifndef __XRDOUCMEMSLOT_HH__
#define __XRDOUCMEMSLOT_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O u c M e m S l o t . h h                       */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>

#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysShmem.hh"

template<class T>
class XrdOucMemSlot
{
public:

bool Available() {return freeSlot != 0;}

T*   Get(int* offset=0)
        {theSlot* urSlot;
         msMutex.Lock();
         if ((urSlot = freeSlot)) freeSlot = freeSlot->nxtSlot;
         msMutex.UnLock();
         if (offset) *offset = (urSlot ? urSlot - slotVec : -1);
         return (urSlot ? &(urSlot->memSlot) : 0);
        }

void Ret(T *item)
        {if (item)
            {theSlot* mySlot = slotVec + (vecSlot - item);
             msMutex.Lock();
             mySlot->nxtSlot = freeSlot;
             freeSlot = mySlot;
             msMutex.UnLock();
            }
        }

void Ret(int iOffs) {Ret(&((slotVec + iOffs)->memSlot));}

     XrdOucMemSlot(int &rc, int count, const char *shmemfn="")
                  : freeSlot(0), slotVec(0)
                  {rc = 0;
                   if (!*shmemfn) slotVec = new theSlot[count];
                      else {std::tuple<T*, size_t> tpl;
                            std::string mfn(shmemfn);
                            try {tpl = XrdSys::shm::make_array<T>(shmemfn,count);
                                 vecSlot = std::get<0>(tpl);
                                }
                                catch(XrdSys::shm_error shmerr)
                                     {rc = shmerr.errcode; return;}
                           }
                   for (int i = 0; i < count-1; i++)
                       {slotVec[i].nxtSlot = &slotVec[i+1];}
                   slotVec[count-1].nxtSlot = 0;
                   freeSlot = &slotVec[0];
                  }

    ~XrdOucMemSlot() {}

private:

union theSlot
{
   theSlot*  nxtSlot;
   T         memSlot;

   theSlot() {}
  ~theSlot() {}
};

XrdSysMutex msMutex;
theSlot*    freeSlot;
union
{
   theSlot*    slotVec;
   T*          vecSlot;
};
};
#endif
