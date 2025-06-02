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
#include <vector>

#include "XrdSsi/XrdSsiAtomics.hh"
  
template<class T>
class XrdSsiRRTable;

template<class T>
class XrdSsiRRTableItem
{
public:
      XrdSsiRRTableItem() : item(0) { }

      XrdSsiRRTableItem(T* _item, XrdSsiRRTable<T> *_tab, uint64_t itemID) :
         item(_item), tab(_tab), reqid(itemID) { }

      XrdSsiRRTableItem(XrdSsiRRTableItem &&other) : item(other.item), tab(other.tab),
         reqid(other.reqid) { other.item = 0; }

      XrdSsiRRTableItem(const XrdSsiRRTableItem &other) =delete;

      XrdSsiRRTableItem& operator=(const XrdSsiRRTableItem&) =delete;

      XrdSsiRRTableItem& operator=(XrdSsiRRTableItem &&other)
         {item       = other.item;
          tab        = other.tab;
          reqid      = other.reqid;
          other.item = 0;
          return *this;
         }

      ~XrdSsiRRTableItem() { reset(); }

      explicit operator bool() const { return item != nullptr; }

      T& operator*() { return *item; }
      T* operator->() { return item; }

      void reset() { if (item) {tab->Release(item, reqid); item=0;} }

      T* release() { T* itm=item; item=0; return itm; }

      uint64_t reqID() const { return reqid; }

private:
      T* item;
      XrdSsiRRTable<T> *tab;
      uint64_t reqid;
};

template<class T>
class XrdSsiRRTable
{
public:

// Init with refcounter to 2. One reference is for the entry in the
// baseITem or theMap and other reference for item returned.
XrdSsiRRTableItem<T>  Add(T *item, uint64_t itemID)
         {XrdSsiMutexMon lck(rrtMutex);
          if ((baseItem.item && baseKey == itemID)
          || theMap.count(itemID))
             {return XrdSsiRRTableItem<T>();}
          if (baseItem.item == 0)
             {baseItem.Init(item, 2);
              baseKey = itemID;
              return XrdSsiRRTableItem(item, this, itemID);
             }
          theMap[itemID].Init(item, 2);
          return XrdSsiRRTableItem(item, this, itemID);
         }

void  Clear() {rrtMutex.Lock(); theMap.clear(); baseItem.item = 0; rrtMutex.UnLock();}

// Called by the SsiFileReq when the request is complete. Return false indicates
// we no longer have the request in the table, so XrdSsiFileReq::Finalize() can be
// called immedatly. We return true to prevent Finalize() being called, but we
// arrange to call it as the request leaves our table.
//
bool  DeferFinalize(T *item, uint64_t itemID)
            {XrdSsiMutexMon lck(rrtMutex);
             if (baseItem.item && baseKey == itemID)
                {if (baseItem.item != item) return false;
                 baseItem.deferedFinalize = true;
                 return true;
                }
             typename std::map<uint64_t,ItemInfo>::iterator it = theMap.find(itemID);
             if (it == theMap.end()) return false;
             ItemInfo &info = it->second;
             if (info.item != item) return false;
             info.deferedFinalize = true;
             return true;
            }

// Called once XrdSsiFileReq::Finalize() has been called for an request in our table.
//
void  DeferredFinalizeDone(T *item, uint64_t itemID)
            {XrdSsiMutexMon lck(rrtMutex);
             wCond.Lock();
             nDef--;
             wCond.Broadcast();
             wCond.UnLock();
             if (baseItem.item && baseKey == itemID)
             {
               if (baseItem.item != item) return;
               baseItem.item = 0;
               return;
             }
             typename std::map<uint64_t,ItemInfo>::iterator it = theMap.find(itemID);
             if (it == theMap.end()) return;
             ItemInfo &info = it->second;
             if (info.item != item) return;
             theMap.erase(it);
            }

// Mark request as deleted from the table (LookUp will not longer return it).
// Request will stay in the table until reference count becomes zero.
//
void  Del(uint64_t itemID) {Decr(itemID, true);}

// Mark request as deleted and also to be finalized once refernce count reaches zero.
// Blocks until the refernce count reaches zero.
void  DelFinalize(XrdSsiRRTableItem<T> &&r)
            {if (!r) return;
             uint64_t itemID = r.reqID();
             T* item = r.release();
             Decr(itemID, true, item, true, 1);
             XrdSsiMutexMon lck(rrtMutex);
             while((baseItem.item && baseKey == itemID) ||
                    theMap.count(itemID))
                {wCond.Lock();
                 lck.UnLock();
                 do { wCond.Wait(); } while(nDef>0);
                 wCond.UnLock();
                 lck.Lock(&rrtMutex);
                }
            }

void  Release(T* item, uint64_t itemID) {Decr(itemID, false, item, false, 1);}

// Return a request object pointer from the table. Request pointer is wrapped in
// an XrdSsiRRTableItem to take care of decreasing the reference count when
// 'item' container is destroyed.
//
XrdSsiRRTableItem<T> LookUp(uint64_t itemID)
            {XrdSsiMutexMon lck(rrtMutex);
             if (baseItem.item && baseKey == itemID)
                {if (baseItem.deleted) return XrdSsiRRTableItem<T>();
                 baseItem.refcount++;
                 return XrdSsiRRTableItem(baseItem.item, this, itemID);
                }
             typename std::map<uint64_t,ItemInfo>::iterator it = theMap.find(itemID);
             if (it == theMap.end()) return XrdSsiRRTableItem<T>();
             ItemInfo &info = it->second;
             if (info.deleted) return XrdSsiRRTableItem<T>();
             info.refcount++;
             return XrdSsiRRTableItem(info.item, this, itemID);
            }

int   Num() {return theMap.size() + (baseItem.item ? 1 : 0);}

// Finalize all remaining requests and block until the reference counts
// have falled to zero.
//
void  Reset()
         {XrdSsiMutexMon lck(rrtMutex);
          std::vector<std::pair<T*,uint64_t>> tofin;
          if (baseItem.item && baseItem.refcount > 0)
             {if (!baseItem.deleted)
                {baseItem.deleted = true;
                 baseItem.refcount--;
                 baseItem.deferedFinalize = true;
                }
              if (baseItem.refcount <= 0)
                {tofin.push_back(std::make_pair(baseItem.item,baseKey));}
             }
          for(auto it=theMap.begin(); it!=theMap.end(); ++it)
            {ItemInfo &info = it->second;
             if (info.refcount <= 0) continue;
             if (!info.deleted)
               {info.deleted = true;
                info.refcount--;
                info.deferedFinalize=true;
               }
             if (info.refcount <= 0) tofin.push_back(std::make_pair(info.item,it->first));
            }
          lck.UnLock();
          for(auto &fpair : tofin)
            {T* f=fpair.first;
             uint64_t itemID=fpair.second;
             wCond.Lock();
             nDef++;
             wCond.UnLock();
             f->Finalize();
             DeferredFinalizeDone(f, itemID);
            }

          lck.Lock(&rrtMutex);
          while(baseItem.item || theMap.size() != 0)
            {wCond.Lock();
             lck.UnLock();
             do { wCond.Wait(); } while(nDef>0);
             wCond.UnLock();
             lck.Lock(&rrtMutex);
            }
         }

      XrdSsiRRTable() : baseKey(0), wCond(0), nDef(0) {}

     ~XrdSsiRRTable() {Reset();}

private:
void  Decr(uint64_t itemID, bool del=false, T* item=0, bool fin=false, int ecnt=0)
         {XrdSsiMutexMon lck(rrtMutex);
          if (baseItem.item && baseKey == itemID)
             {if (item && baseItem.item != item) return;
              if (baseItem.refcount <=0) return;
              if (!baseItem.deleted)
                {if (fin) baseItem.deferedFinalize=true;
                 if (del)
                   {baseItem.refcount--;
                    baseItem.deleted = true;
                   }
                }
              baseItem.refcount -= ecnt;
              T *f=0;
              if (baseItem.refcount <= 0)
                 {if (baseItem.deferedFinalize)
                    { f = baseItem.item; baseItem.deleted = true; }
                  else { wCond.Lock(); wCond.Broadcast(); wCond.UnLock(); baseItem.item = 0; }
                 }
              lck.UnLock();
              if (f)
                {wCond.Lock();
                 nDef++;
                 wCond.UnLock();
                 if (fin)
                  {f->Finalize();
                   DeferredFinalizeDone(f, itemID);
                  }
                 else f->DeferredFinalize();
                }
              return;
             }
          typename std::map<uint64_t,ItemInfo>::iterator it = theMap.find(itemID);
          if (it == theMap.end()) return;
          ItemInfo &info = it->second;
          if (item && info.item != item) return;
          if (info.refcount <= 0) return;
          if (!info.deleted)
            {if (fin) info.deferedFinalize=true;
             if (del)
               {info.refcount--;
                info.deleted = true;
               }
            }
          info.refcount -= ecnt;
          T* f=0;
          if (info.refcount <= 0)
             {if (info.deferedFinalize)
                { f=info.item; info.deleted = true; }
              else { wCond.Lock(); wCond.Broadcast(); wCond.UnLock(); theMap.erase(it); }
             }
          lck.UnLock();
          if (f)
            {wCond.Lock();
             nDef++;
             wCond.UnLock();
             if (fin)
               {f->Finalize();
                DeferredFinalizeDone(f, itemID);
               }
             else f->DeferredFinalize();
            }
         }

struct ItemInfo
      {int refcount;
       bool deferedFinalize;
       bool deleted;
       T *item;
       ItemInfo() {Init(0,0);}
       void Init(T* _item, int cnt)
          {refcount        = cnt;
           deferedFinalize = false;
           deleted         = false;
           item            = _item;
          }
      };
XrdSsiMutex                  rrtMutex;
ItemInfo                     baseItem;
uint64_t                     baseKey;
std::map<uint64_t, ItemInfo> theMap;
XrdSysCondVar                wCond;
int                          nDef;
};
#endif
