#ifndef _XRDOSSCSIRANGES_H
#define _XRDOSSCSIRANGES_H
/******************************************************************************/
/*                                                                            */
/*                  X r d O s s C s i R a n g e s . h h                       */
/*                                                                            */
/* (C) Copyright 2020 CERN.                                                   */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* In applying this licence, CERN does not waive the privileges and           */
/* immunities granted to it by virtue of its status as an Intergovernmental   */
/* Organization or submit itself to any jurisdiction.                         */
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

#include "XrdSys/XrdSysPthread.hh"

#include <mutex>
#include <list>
#include <condition_variable>
#include <memory>

// forward decl
class XrdOssCsiPages;

struct XrdOssCsiRange_s
{
   off_t start;
   off_t end;
   bool rdonly;
   int nBlockedBy;
   std::mutex mtx;
   std::condition_variable cv;
   XrdOssCsiRange_s *next;
};

class XrdOssCsiRanges;

class XrdOssCsiRangeGuard
{
public:
   XrdOssCsiRangeGuard() : r_(NULL), rp_(NULL), pages_(NULL), trackinglenlocked_(false) { }
   ~XrdOssCsiRangeGuard();

   void SetRange(XrdOssCsiRanges *r, XrdOssCsiRange_s *rp)
   {
      r_ = r;
      rp_ = rp;
      pages_ = NULL;
      trackinglenlocked_ = false;
   }

   const std::pair<off_t,off_t>& getTrackinglens() const
   {
      return trackingsizes_;
   }

   void SetTrackingInfo(XrdOssCsiPages *p, const std::pair<off_t,off_t> &tsizes, bool locked)
   {
      trackingsizes_ = tsizes;
      if (locked)
      {
         trackinglenlocked_ = true;
         pages_ = p;
      }
   }

   void Wait();

   void unlockTrackinglen();
   void ReleaseAll();

private:
   XrdOssCsiRanges *r_;
   XrdOssCsiRange_s *rp_;
   XrdOssCsiPages *pages_;
   std::pair<off_t,off_t> trackingsizes_;
   bool trackinglenlocked_;
};


class XrdOssCsiRanges
{
public:
   XrdOssCsiRanges() : allocList_(NULL) { }

   ~XrdOssCsiRanges()
   {
      XrdOssCsiRange_s *p;
      while((p = allocList_))
      {
         allocList_ = allocList_->next;
         delete p;
      }
   }

   //
   // AddRange: add an inclusive range lock on pages [start, end]
   //
   void AddRange(const off_t start, const off_t end, XrdOssCsiRangeGuard &rg, bool rdonly)
   {
      std::unique_lock<std::mutex> lck(rmtx_);
    
      int nblocking = 0;
      for(auto itr = ranges_.begin(); itr != ranges_.end(); ++itr)
      {
         if ((*itr)->start <= end && start <= (*itr)->end)
         {
            if (!(rdonly && (*itr)->rdonly))
            {
               nblocking++;
            }
         }
      }

      XrdOssCsiRange_s *nr = AllocRange();
      nr->start = start;
      nr->end = end;
      nr->rdonly = rdonly;
      nr->nBlockedBy = nblocking;
      ranges_.push_back(nr);
      lck.unlock();

      rg.SetRange(this, nr);
   }

   void Wait(XrdOssCsiRange_s *rp)
   {
      std::unique_lock<std::mutex> l(rp->mtx);
      while (rp->nBlockedBy>0)
      {
         rp->cv.wait(l);
      }
   }

   void RemoveRange(XrdOssCsiRange_s *rp)
   {
      std::lock_guard<std::mutex> guard(rmtx_);
      for(auto itr=ranges_.begin();itr!=ranges_.end();++itr)
      {
         if (*itr == rp)
         {
            ranges_.erase(itr);
            break;
         }
      }

      for(auto itr=ranges_.begin(); itr != ranges_.end(); ++itr)
      {
         if ((*itr)->start <= rp->end && rp->start <= (*itr)->end)
         {
            if (!(rp->rdonly && (*itr)->rdonly))
            {
               std::unique_lock<std::mutex> l((*itr)->mtx);
               (*itr)->nBlockedBy--;
               if ((*itr)->nBlockedBy == 0)
               {
                  (*itr)->cv.notify_one();
               }
            }
         }
     }

     RecycleRange(rp);
     rp = NULL;
   }

private:
   std::mutex rmtx_;
   std::list<XrdOssCsiRange_s *> ranges_;
   XrdOssCsiRange_s *allocList_;

   // must be called with rmtx_ locked
   XrdOssCsiRange_s* AllocRange()
   {
      XrdOssCsiRange_s *p;
      if ((p = allocList_)) allocList_ = p->next;
      if (!p) p = new XrdOssCsiRange_s();
      p->next = NULL;
      return p;
   }

   // must be called with rmtx_ locked
   void RecycleRange(XrdOssCsiRange_s* rp)
   {
     rp->next = allocList_;
     allocList_ = rp;
   }
};

#endif
