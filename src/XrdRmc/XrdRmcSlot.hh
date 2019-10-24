#ifndef __XRDRMCSLOT_HH__
#define __XRDRMCSLOT_HH__
/******************************************************************************/
/*                                                                            */
/*                         X r d R m c S l o t . h h                          */
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
  
/* This class is used to support a memory cache used by an XrdOucCache actual
   implementation.
*/

class XrdRmcData;
class XrdOucCacheIO;
class XrdSysSemaphore;

class XrdRmcSlot
{
public:

inline void      File(XrdOucCacheIO *kV, int you)
                     {Status.Data = 0; Key = kV; HLink = you; Count = 1;}

static inline int Find(XrdRmcSlot *Base, long long What, int n)
                      {while(n && Base[n].Contents != What) n=Base[n].HLink;
                       return n;
                      }

inline void       Hide(XrdRmcSlot *Base, int *hTab, int hI)
                      {int j, Slot = this-Base;
                       if (hTab[hI] == Slot) hTab[hI] = HLink;
                          else if ((j = hTab[hI]))
                                  {while((hI=Base[j].HLink) && hI != Slot) j=hI;
                                   if (hI) Base[j].HLink = Base[hI].HLink;
                                  }
                       Count = 0; Contents = -1;
                      }

static void       Init(XrdRmcSlot *Base, int Num)
                     {int i;
                      Base->Status.LRU.Next = Base->Status.LRU.Prev = 0;
                      Base->Own.Next        = Base->Own.Prev = 0;
                      for (i = 1; i < Num; i++)
                          {Base[i].Status.LRU.Next = Base[i].Status.LRU.Prev = i;
                           Base[i].Own.Next = Base[i].Own.Prev = i;
                           Base->Push(Base, &Base[i]);
                          }
                     }

inline int        Pull(XrdRmcSlot *Base)
                      {Base[Status.LRU.Prev].Status.LRU.Next = Status.LRU.Next;
                       Base[Status.LRU.Next].Status.LRU.Prev = Status.LRU.Prev;
                       Status.LRU.Next = Status.LRU.Prev = this-Base;
                       return Status.LRU.Next;
                      }

inline int        Push(XrdRmcSlot *Base, XrdRmcSlot *sP)
                      {int UrNum = sP-Base, MyNum = this-Base;
                       sP->Status.LRU.Next = MyNum;
                       sP->Status.LRU.Prev = Status.LRU.Prev;
                       Base[Status.LRU.Prev].Status.LRU.Next = UrNum;
                       Status.LRU.Prev = UrNum;
                       return UrNum;
                      }

inline void       Owner(XrdRmcSlot *Base)
                      {Base[Own.Prev].Own.Next = Own.Next;
                       Base[Own.Next].Own.Prev = Own.Prev;
                       Own.Next = Own.Prev = this-Base;
                      }

inline void       Owner(XrdRmcSlot *Base, XrdRmcSlot *sP)
                      {int UrNum = sP-Base, MyNum = this-Base;
                       sP->Own.Next = MyNum;        sP->Own.Prev = Own.Prev;
                       Base[Own.Prev].Own.Next = UrNum; Own.Prev = UrNum;
                      }

inline void       reRef(XrdRmcSlot *Base)
                      {      Status.LRU.Prev           = Base->Status.LRU.Prev;
                       Base[ Status.LRU.Prev].Status.LRU.Next = this-Base;
                       Base->Status.LRU.Prev           = this-Base;
                             Status.LRU.Next           = 0;
                      }

inline void       unRef(XrdRmcSlot *Base)
                      {      Status.LRU.Next           = Base->Status.LRU.Next;
                       Base [Status.LRU.Next].Status.LRU.Prev = this-Base;
                       Base->Status.LRU.Next           = this-Base;
                             Status.LRU.Prev           = 0;
                      }

struct SlotList
      {
       int              Next;
       int              Prev;
      };

struct ioQ
      {ioQ             *Next;
       XrdSysSemaphore *ioEnd;
                        ioQ(ioQ *First, XrdSysSemaphore *ioW)
                           : Next(First), ioEnd(ioW) {}
      };

union  SlotState
      {struct  ioQ     *waitQ;
       XrdRmcData     *Data;
       struct  SlotList LRU;
       int              inUse;
      };

union {long long        Contents;
       XrdOucCacheIO   *Key;
      };
SlotState               Status;
SlotList                Own;
int                     HLink;
int                     Count;

static const int  lenMask = 0x01ffffff; // Mask to get true value in Count
static const int  isShort = 0x80000000; // Short page, Count & lenMask == size
static const int  inTrans = 0x40000000; // Segment is in transit
static const int  isSUSE  = 0x20000000; // Segment is single use
static const int  isNew   = 0x10000000; // Segment is new (not yet referenced)

                  XrdRmcSlot() : Contents(-1), HLink(0), Count(0) {}

                 ~XrdRmcSlot() {}
};
#endif
