#ifndef __XRDSSIRRTABLE_HH__
#define __XRDSSIRRTABLE_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d S s i R R T a b l e . h h                       */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <map>
#include <cstdint>

#include "XrdSsi/XrdSsiAtomics.hh"
  
template<class T>
class XrdSsiRRTable
{
public:

void  Add(T *item, uint64_t itemID)
         {rrtMutex.Lock();
          if (baseItem != 0) theMap[itemID] = item;
             else {baseKey  = itemID;
                   baseItem = item;
                  }
          rrtMutex.UnLock();
         }

void  Clear() {rrtMutex.Lock(); theMap.clear(); rrtMutex.UnLock();}

void  Del(uint64_t itemID, bool finit=false)
         {XrdSsiMutexMon lck(rrtMutex);
          if (baseItem && baseKey == itemID)
             {if (finit) baseItem->Finalize();
              baseItem = 0;
             } else {
              if (!finit) theMap.erase(itemID);
                 else {typename std::map<uint64_t,T*>::iterator it = theMap.find(itemID);
                       if (it != theMap.end()) it->second->Finalize();
                       theMap.erase(it);
                      }
             }
         }

T    *LookUp(uint64_t itemID)
            {XrdSsiMutexMon lck(rrtMutex);
             if (baseItem && baseKey == itemID) return baseItem;
             typename std::map<uint64_t,T*>::iterator it = theMap.find(itemID);
             return (it == theMap.end() ? 0 : it->second);
            }

int   Num() {return theMap.size() + (baseItem ? 1 : 0);}

void  Reset()
           {XrdSsiMutexMon lck(rrtMutex);
            typename std::map<uint64_t, T*>::iterator it = theMap.begin();
            while(it != theMap.end())
                 {it->second->Finalize();
                  it++;
                 }
            theMap.clear();
            if (baseItem)
               {baseItem->Finalize();
                baseItem = 0;
               }
           }

      XrdSsiRRTable() : baseItem(0), baseKey(0) {}

     ~XrdSsiRRTable() {Reset();}

private:
XrdSsiMutex              rrtMutex;
T                       *baseItem;
uint64_t                 baseKey;
std::map<uint64_t, T*>   theMap;
};
#endif
