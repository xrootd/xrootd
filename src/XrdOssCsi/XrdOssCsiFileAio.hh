#ifndef _XRDOSSCSIFILEAIO_H
#define _XRDOSSCSIFILEAIO_H
/******************************************************************************/
/*                                                                            */
/*                 X r d O s s C s i F i l e A i o . h h                      */
/*                                                                            */
/* (C) Copyright 2021 CERN.                                                   */
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

#include "Xrd/XrdScheduler.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdOssCsi.hh"
#include "XrdSys/XrdSysPageSize.hh"

#include <mutex>
#include <thread>

class XrdOssCsiFileAioJob : public XrdJob
{
public:

   XrdOssCsiFileAioJob() { }
   virtual ~XrdOssCsiFileAioJob() { }

   void Init(XrdOssCsiFile *fp, XrdOssCsiFileAio *nio, XrdSfsAio *aiop, bool isPg, bool read)
   {
      fp_   = fp;
      nio_  = nio;
      aiop_ = aiop;
      pg_   = isPg;
      read_ = read;
      jobtype_ = (read_) ? JobReadStep1 : JobWriteStep1;
   }

   void PrepareWrite2()
   {
      jobtype_ = JobWriteStep2;
   }

   void PrepareRead2()
   {
      jobtype_ = JobReadStep2;
   }

   void DoIt() /* override */
   {
      switch(jobtype_)
      {
         case JobReadStep1:
            // take rangelock, then submit aio read
            DoItRead1();
            break;

         case JobReadStep2:
            // fetch any extra bytes then verify/fetch csvec
            DoItRead2();
            break;

         case JobWriteStep1:
            // lock byte range, update/store csvec and queue aio write
            DoItWrite1();
            break;

         case JobWriteStep2:
            // check return from aio write, write any extra
            DoItWrite2();
            break;
      }
   }
   
   void DoItRead1();
   void DoItRead2();
   void DoItWrite1();
   void DoItWrite2();

private:
   XrdOssCsiFile *fp_;
   XrdOssCsiFileAio *nio_;
   XrdSfsAio *aiop_;
   bool pg_;
   bool read_;
   enum { JobReadStep1, JobReadStep2, JobWriteStep1, JobWriteStep2 } jobtype_;
};

class XrdOssCsiFileAio : public XrdSfsAio
{
friend class XrdOssCsiFileAioStore;
public:

   XrdOssCsiRangeGuard rg_;
   uint64_t pgOpts_;

   virtual void doneRead() /* override */
   {
      parentaio_->Result = this->Result;
      // schedule the result check and verify/fetchrange
      SchedReadJob2();
   }

   virtual void doneWrite() /* override */
   {
      parentaio_->Result = this->Result;
      // schedule the result check and write any extra
      SchedWriteJob2();
   }

   virtual void Recycle()
   {
      rg_.ReleaseAll();
      parentaio_ = NULL;
      XrdOssCsiFile *f = file_;
      file_ = NULL;
      if (store_)
      {
         std::lock_guard<std::mutex> guard(store_->mtx_);
         next_ = store_->list_;
         store_->list_ = this;
      }
      else
      {
         delete this;
      }
      if (f)
      {
         f->aioDec();
      }
   }
  
   void Init(XrdSfsAio *aiop, XrdOssCsiFile *file, bool isPgOp, uint64_t opts, bool isread)
   {
      parentaio_               = aiop;
      this->sfsAio.aio_fildes  = aiop->sfsAio.aio_fildes;
      this->sfsAio.aio_buf     = aiop->sfsAio.aio_buf;
      this->sfsAio.aio_nbytes  = aiop->sfsAio.aio_nbytes;
      this->sfsAio.aio_offset  = aiop->sfsAio.aio_offset;
      this->sfsAio.aio_reqprio = aiop->sfsAio.aio_reqprio;
      this->cksVec             = aiop->cksVec;
      this->TIdent             = aiop->TIdent;
      file_                    = file;
      isPgOp_                  = isPgOp;
      pgOpts_                  = opts;
      Sched_                   = XrdOssCsi::Sched_;
      job_.Init(file, this, aiop, isPgOp, isread);
      file_->aioInc();
   }

   static XrdOssCsiFileAio *Alloc(XrdOssCsiFileAioStore *store)
   {
      XrdOssCsiFileAio *p=NULL;
      if (store)
      {
         std::lock_guard<std::mutex> guard(store->mtx_);
         if ((p = store->list_)) store->list_ = p->next_;
      }
      if (!p) p = new XrdOssCsiFileAio(store);
      return p;
   }

   void SchedWriteJob2()
   {
      job_.PrepareWrite2();
      Sched_->Schedule((XrdJob *)&job_);
   }

   void SchedWriteJob()
   {
      Sched_->Schedule((XrdJob *)&job_);
   }

   void SchedReadJob2()
   {
      job_.PrepareRead2();
      Sched_->Schedule((XrdJob *)&job_);
   }

   void SchedReadJob()
   {
      Sched_->Schedule((XrdJob *)&job_);
   }

   XrdOssCsiFileAio(XrdOssCsiFileAioStore *store) : store_(store) { }
   ~XrdOssCsiFileAio() { }

private:
   XrdOssCsiFileAioStore *store_;
   XrdSfsAio *parentaio_;
   XrdOssCsiFile *file_;
   bool isPgOp_;
   XrdOssCsiFileAioJob job_;
   XrdScheduler *Sched_;
   XrdOssCsiFileAio *next_;
};

void XrdOssCsiFileAioJob::DoItRead2()
{
   // this job runs after async Read
   // range was already locked read-only before the read

   if (aiop_->Result<0 || nio_->sfsAio.aio_nbytes==0)
   {
      aiop_->doneRead();
      nio_->Recycle();
      return;
   }

   // if this is a pg operation and this was a short read, try to complete,
   // otherwise caller will have to deal with joining csvec values from repeated reads

   ssize_t toread = nio_->sfsAio.aio_nbytes - nio_->Result;
   ssize_t nread = nio_->Result;

   if (!pg_)
   {
      // not a pg operation, no need to read more
      toread = 0;
   }
   char *p = (char*)nio_->sfsAio.aio_buf;
   while(toread>0)
   {
      const ssize_t rret = fp_->successor_->Read(&p[nread], nio_->sfsAio.aio_offset+nread, toread);
      if (rret == 0) break;
      if (rret<0)
      {
         aiop_->Result = rret;
         aiop_->doneRead();
         nio_->Recycle();
         return;
      }
      toread -= rret;
      nread += rret;
   }
   aiop_->Result = nread;

   ssize_t puret;
   if (pg_)
   {
      puret = fp_->Pages()->FetchRange(fp_->successor_,
                                      (void *)nio_->sfsAio.aio_buf,
                                      (off_t)nio_->sfsAio.aio_offset,
                                      (size_t)nio_->Result,
                                      (uint32_t*)nio_->cksVec,
                                      nio_->pgOpts_,
                                      nio_->rg_);
   }
   else
   {
      puret = fp_->Pages()->VerifyRange(fp_->successor_,
                                       (void *)nio_->sfsAio.aio_buf,
                                       (off_t)nio_->sfsAio.aio_offset,
                                       (size_t)nio_->Result,
                                       nio_->rg_);
   }
   if (puret<0)
   {
      aiop_->Result = puret;
   }
   aiop_->doneRead();
   nio_->Recycle();
}

void XrdOssCsiFileAioJob::DoItRead1()
{
   // this job takes rangelock and then queues aio read

   // lock range
   fp_->Pages()->LockTrackinglen(nio_->rg_, (off_t)aiop_->sfsAio.aio_offset,
                                (off_t)(aiop_->sfsAio.aio_offset+aiop_->sfsAio.aio_nbytes), true);

   const int ret = fp_->successor_->Read(nio_);
   if (ret<0)
   {
      aiop_->Result = ret;
      aiop_->doneRead();
      nio_->Recycle();
      return;
   }
}

void XrdOssCsiFileAioJob::DoItWrite1()
{
   // this job runs before async Write

   // lock range
   fp_->Pages()->LockTrackinglen(nio_->rg_, (off_t)aiop_->sfsAio.aio_offset,
                                (off_t)(aiop_->sfsAio.aio_offset+aiop_->sfsAio.aio_nbytes), false);
   int puret;
   if (pg_) {
      puret = fp_->Pages()->StoreRange(fp_->successor_,
                                      (const void *)aiop_->sfsAio.aio_buf, (off_t)aiop_->sfsAio.aio_offset,
                                      (size_t)aiop_->sfsAio.aio_nbytes, (uint32_t*)aiop_->cksVec, nio_->pgOpts_, nio_->rg_);

   }
   else
   {
      puret = fp_->Pages()->UpdateRange(fp_->successor_,
                                       (const void *)aiop_->sfsAio.aio_buf, (off_t)aiop_->sfsAio.aio_offset,
                                       (size_t)aiop_->sfsAio.aio_nbytes, nio_->rg_);
   }
   if (puret<0)
   {
      nio_->rg_.ReleaseAll();
      fp_->resyncSizes();
      aiop_->Result = puret;
      aiop_->doneWrite();
      nio_->Recycle();
      return;
   }

   const int ret = fp_->successor_->Write(nio_);
   if (ret<0)
   {
      nio_->rg_.ReleaseAll();
      fp_->resyncSizes();
      aiop_->Result = ret;
      aiop_->doneWrite();
      nio_->Recycle();
      return;
   }
}

void XrdOssCsiFileAioJob::DoItWrite2()
{
   // this job runs after the async Write

   if (aiop_->Result<0)
   {
      nio_->rg_.ReleaseAll();
      fp_->resyncSizes();
      aiop_->doneWrite();
      nio_->Recycle();
      return;
   }

   // in case there was a short write during the async write, finish
   // writing the data now, otherwise the crc values will be inconsistent
   ssize_t towrite = nio_->sfsAio.aio_nbytes - nio_->Result;
   ssize_t nwritten = nio_->Result;
   const char *p = (const char*)nio_->sfsAio.aio_buf;
   while(towrite>0)
   {
      const ssize_t wret = fp_->successor_->Write(&p[nwritten], nio_->sfsAio.aio_offset+nwritten, towrite);
      if (wret<0)
      {
         aiop_->Result = wret;
         nio_->rg_.ReleaseAll();
         fp_->resyncSizes();
         aiop_->doneWrite();
         nio_->Recycle();
         return;
      }
      towrite -= wret;
      nwritten += wret;
   }
   aiop_->Result = nwritten;
   aiop_->doneWrite();
   nio_->Recycle();
}

#endif
