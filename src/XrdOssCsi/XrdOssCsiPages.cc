/******************************************************************************/
/*                                                                            */
/*                    X r d O s s C s i P a g e s . c c                       */
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

#include "XrdOssCsiTrace.hh"
#include "XrdOssCsiPages.hh"
#include "XrdOuc/XrdOucCRC.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <assert.h>

extern XrdOucTrace  OssCsiTrace;

XrdOssCsiPages::XrdOssCsiPages(const std::string &fn, std::unique_ptr<XrdOssCsiTagstore> ts, bool wh, bool am, bool dpe, bool dlw, const char *tid) :
        ts_(std::move(ts)),
        writeHoles_(wh),
        allowMissingTags_(am),
        disablePgExtend_(dpe),
        hasMissingTags_(false),
        rdonly_(false),
        loosewriteConfigured_(!dlw),
        loosewrite_(false),
        tscond_(0),
        tsforupdate_(false),
        fn_(fn),
        tident_(tid),
        tident(tident_.c_str()),
        lastpgforloose_(0),
        checklastpg_(false)
{
   // empty constructor
}

int XrdOssCsiPages::Open(const char *path, off_t dsize, int flags, XrdOucEnv &envP)
{
   EPNAME("Pages::Open");
   hasMissingTags_ = false;
   rdonly_ = false;
   int ret = ts_->Open(path, dsize, flags, envP);
   if (ret == -ENOENT)
   {
      // no existing tag
      if (allowMissingTags_)
      {
         TRACE(Info, "Opening with missing tagfile: " << fn_);
         hasMissingTags_ = true;
         return 0;
      }
      TRACE(Warn, "Could not open tagfile for " << fn_ << " error " << ret);
      return -EDOM;
   }
   if (ret<0) return ret;
   if ((flags & O_ACCMODE) == O_RDONLY) rdonly_ = true;
   loosewrite_ = (dsize==0 && ts_->GetTrackedTagSize()==0) ? false : loosewriteConfigured_;
   return 0;
}

int XrdOssCsiPages::Close()
{
   if (hasMissingTags_)
   {
      hasMissingTags_ = false;
      return 0;
   }
   return ts_->Close();
}

void XrdOssCsiPages::Flush()
{
   if (!hasMissingTags_) ts_->Flush();
}

int XrdOssCsiPages::Fsync()
{
   if (hasMissingTags_) return 0;
   return ts_->Fsync();
}

int XrdOssCsiPages::TrackedSizesGet(XrdOssCsiPages::Sizes_t &rsizes, const bool forupdate)
{
   if (hasMissingTags_) return -ENOENT;

   XrdSysCondVarHelper lck(&tscond_);
   while (tsforupdate_)
   {
      tscond_.Wait();
   }
   off_t tagsize =  ts_->GetTrackedTagSize();
   off_t datasize =  ts_->GetTrackedDataSize();
   if (forupdate)
   {
      tsforupdate_ = true;
   }
   rsizes = std::make_pair(tagsize,datasize);
   return 0;
}

int XrdOssCsiPages::LockSetTrackedSize(const off_t sz)
{
   XrdSysCondVarHelper lck(&tscond_);
   return ts_->SetTrackedSize(sz);
}

int XrdOssCsiPages::LockResetSizes(XrdOssDF *fd, const off_t sz)
{
   // nothing to do is no tag file
   if (hasMissingTags_) return 0;

   XrdSysCondVarHelper lck(&tscond_);
   const int ret = ts_->ResetSizes(sz);
   loosewrite_ = loosewriteConfigured_;
   BasicConsistencyCheck(fd);
   return ret;
}

int XrdOssCsiPages::LockTruncateSize(const off_t sz, const bool datatoo)
{
   XrdSysCondVarHelper lck(&tscond_);
   return ts_->Truncate(sz,datatoo);
}

int XrdOssCsiPages::LockMakeUnverified()
{
   XrdSysCondVarHelper lck(&tscond_);
   return ts_->SetUnverified();
}

void XrdOssCsiPages::TrackedSizeRelease()
{
   XrdSysCondVarHelper lck(&tscond_);
   assert(tsforupdate_ == true);

   tsforupdate_ = false;
   tscond_.Broadcast();
}

// Used by Write: At this point the user's data has not yet been written to the file.
//
int XrdOssCsiPages::UpdateRange(XrdOssDF *const fd, const void *buff, const off_t offset, const size_t blen, XrdOssCsiRangeGuard &rg)
{
   if (offset<0)
   {
      return -EINVAL;
   }

   if (blen == 0)
   {
     return 0;
   }

   // if the tag file is missing we don't need to store anything
   if (hasMissingTags_)
   {
      return 0;
   }

   // update of file were checksums are based on the file data suppplied: as there's no separate
   // source of checksum information mark this file as having unverified checksums
   LockMakeUnverified();

   const Sizes_t sizes = rg.getTrackinglens();

   const off_t trackinglen = sizes.first;
   if (offset+blen > static_cast<size_t>(trackinglen))
   {
      LockSetTrackedSize(offset+blen);
      rg.unlockTrackinglen();
   }

   int ret;
   if ((offset % XrdSys::PageSize) != 0 ||
       (offset+blen < static_cast<size_t>(trackinglen) && (blen % XrdSys::PageSize) != 0) ||
       ((trackinglen % XrdSys::PageSize) !=0 && offset > trackinglen))
   {
      ret = UpdateRangeUnaligned(fd, buff, offset, blen, sizes);
   }
   else
   {
      ret = UpdateRangeAligned(buff, offset, blen, sizes);
   }

   return ret;
}

// Used by Read: At this point the user's buffer has already been filled from the file.
// offset: offset within the file at which the read starts
// blen  : the length of the read already read into the buffer
//         (which may be less than what was originally requested)
//
int XrdOssCsiPages::VerifyRange(XrdOssDF *const fd, const void *buff, const off_t offset, const size_t blen, XrdOssCsiRangeGuard &rg)
{
   EPNAME("VerifyRange");

   if (offset<0)
   {
      return -EINVAL;
   }

   // if the tag file is missing we don't verify anything
   if (hasMissingTags_)
   {
      return 0;
   }

   const Sizes_t sizes = rg.getTrackinglens();
   const off_t trackinglen = sizes.first;

   if (offset >= trackinglen && blen == 0)
   {
      return 0;
   }

   if (blen == 0)
   {
      // if offset is before the tracked len we should not be requested to verify zero bytes:
      // the file may have been truncated
      TRACE(Warn, "Verify request for zero bytes " << fn_ << ", file may be truncated");
      return -EDOM;
   }

   if (offset+blen > static_cast<size_t>(trackinglen))
   {
      TRACE(Warn, "Verify request for " << (offset+blen-trackinglen) << " bytes from " << fn_ << " beyond tracked lengh");
      return -EDOM;
   }

   int vret;
   if ((offset % XrdSys::PageSize) != 0 || (offset+blen != static_cast<size_t>(trackinglen) && (blen % XrdSys::PageSize) != 0))
   {
      vret = VerifyRangeUnaligned(fd, buff, offset, blen, sizes);
   }
   else
   {
      vret = VerifyRangeAligned(buff, offset, blen, sizes);
   }

   return vret;
}

// apply_sequential_aligned_modify: Internal func used during Write/pgWrite
//                                  (both aligned/unaligned cases) to update multiple tags.
//
// Write series of crc32c values to a file's tag file, with optional pre-block or lastblock
// values. Tries to reduce the number of calls to WriteTags() by using an internal buffer
// to make all the crc32c values available from a contiguous start location.
//
// buff:        start of data buffer: Page aligned within the file, used if calculating crc32
// startp:      page index corresponding to the start of buff
// nbytes:      length of buffer
// csvec:       optional pre-computed crc32 values covering nbytes of buffer
// preblockset: true/false. A value for a crc32 value to be placed at startp-1
// lastblockset:true/false. If true the last page of buff must be partial, and instead of
//                          calculating or fetching a crc32 value from csvec[], a supplied
//                          value is used.
// cspre:       value to use for preblock crc32 (used if preblockset is true)
// cslast:      value to use for last partial-block crc32 (used if lastblockset is true)
// 
ssize_t XrdOssCsiPages::apply_sequential_aligned_modify(
   const void *const buff, const off_t startp, const size_t nbytes, const uint32_t *csvec,
   const bool preblockset, const bool lastblockset, const uint32_t cspre, const uint32_t cslast)
{
   EPNAME("apply_sequential_aligned_modify");

   if (lastblockset && (nbytes % XrdSys::PageSize)==0)
   {
      return -EINVAL;
   }
   if (preblockset && startp==0)
   {
      return -EINVAL;
   }

   uint32_t calcbuf[stsize_];
   const size_t calcbufsz = sizeof(calcbuf)/sizeof(uint32_t);
   const uint8_t *const p = (uint8_t*)buff;

   // will be using calcbuf
   bool useinternal = true;
   if (csvec && !preblockset && !lastblockset)
   {
      useinternal = false;
   }

   bool dopre = preblockset;
   const off_t sp = preblockset ? startp-1 : startp;

   size_t blktowrite = ((nbytes+XrdSys::PageSize-1)/XrdSys::PageSize) + (preblockset ? 1 : 0);
   size_t nblkwritten = 0;
   size_t calcbytot = 0;
   while(blktowrite>0)
   {
      size_t blkwcnt = blktowrite;
      if (useinternal)
      {
         size_t cidx = 0;
         size_t calcbycnt = nbytes - calcbytot;
         if (nblkwritten == 0 && dopre)
         {
            calcbycnt = std::min(calcbycnt, (calcbufsz-1)*XrdSys::PageSize);
            blkwcnt = (calcbycnt+XrdSys::PageSize-1)/XrdSys::PageSize;
            calcbuf[cidx] = cspre;
            cidx++;
            blkwcnt++;
            dopre = false;
         }
         else
         {
            calcbycnt = std::min(calcbycnt, calcbufsz*XrdSys::PageSize);
            blkwcnt = (calcbycnt+XrdSys::PageSize-1)/XrdSys::PageSize;
         }
         if ((calcbycnt % XrdSys::PageSize)!=0 && lastblockset)
         {
            const size_t x = calcbycnt / XrdSys::PageSize;
            calcbycnt = XrdSys::PageSize * x;
            calcbuf[cidx + x] = cslast;
         }
         if (csvec)
         {
            memcpy(&calcbuf[cidx], &csvec[calcbytot/XrdSys::PageSize], 4*((calcbycnt+XrdSys::PageSize-1)/XrdSys::PageSize));
         }
         else
         {
            XrdOucCRC::Calc32C(&p[calcbytot], calcbycnt, &calcbuf[cidx]);
         }
         calcbytot += calcbycnt;
      }
      const ssize_t wret = ts_->WriteTags(useinternal ? calcbuf : &csvec[nblkwritten], sp+nblkwritten, blkwcnt);
      if (wret<0)
      {
         TRACE(Warn, TagsWriteError(sp+nblkwritten, blkwcnt, wret));
         return wret;
      }
      blktowrite -= blkwcnt;
      nblkwritten += blkwcnt;
   }
   return nblkwritten;
}

//
// FetchRangeAligned
//
// Used by pgRead or Read (via VerifyRangeAligned) when the read offset is at a page boundary within the file
// AND the length is a multiple of page size or the read is up to exactly the end of file.
//
int XrdOssCsiPages::FetchRangeAligned(const void *const buff, const off_t offset, const size_t blen, const Sizes_t & /* sizes */, uint32_t *const csvec, const uint64_t opts)
{
   EPNAME("FetchRangeAligned");
   uint32_t rdvec[stsize_],vrbuf[stsize_];

   const off_t p1 = offset / XrdSys::PageSize;
   const off_t p2 = (offset+blen) / XrdSys::PageSize;
   const size_t p2_off = (offset+blen) % XrdSys::PageSize;
   const size_t nfull = p2-p1;

   uint32_t *rdbuf;
   size_t rdbufsz;
   if (csvec == NULL)
   {
      // use fixed sized stack buffer
      rdbuf = rdvec;
      rdbufsz = sizeof(rdvec)/sizeof(uint32_t);
   }
   else
   {
      // use supplied buffer, assumed to be large enough
      rdbuf = csvec;
      rdbufsz = (p2_off==0) ? nfull : (nfull+1);
   }

   // always use stack based, fixed sized buffer for verify
   const size_t vrbufsz = sizeof(vrbuf)/sizeof(uint32_t);

   // pointer to data
   const uint8_t *const p = (uint8_t*)buff;
  
   // process full pages + any partial page
   size_t toread = (p2_off>0) ? nfull+1 : nfull;
   size_t nread = 0;
   while(toread>0)
   {
      const size_t rcnt = std::min(toread, rdbufsz-(nread%rdbufsz));
      const ssize_t rret = ts_->ReadTags(&rdbuf[nread%rdbufsz], p1+nread, rcnt);
      if (rret<0)
      {
         TRACE(Warn, TagsReadError(p1+nread, rcnt, rret));
         return rret;
      }
      if ((opts & XrdOssDF::Verify))
      {
         size_t toverif = rcnt;
         size_t nverif = 0;
         while(toverif>0)
         {
            const size_t vcnt = std::min(toverif, vrbufsz);
            const size_t databytes = (nread+nverif+vcnt <= nfull) ? (vcnt*XrdSys::PageSize) : ((vcnt-1)*XrdSys::PageSize+p2_off);
            XrdOucCRC::Calc32C(&p[XrdSys::PageSize*(nread+nverif)],databytes,vrbuf);
            if (memcmp(vrbuf, &rdbuf[(nread+nverif)%rdbufsz], 4*vcnt))
            {
               size_t badpg;
               for(badpg=0;badpg<vcnt;++badpg) { if (memcmp(&vrbuf[badpg],&rdbuf[(nread+nverif+badpg)%rdbufsz],4)) break; }
               TRACE(Warn, CRCMismatchError( (nread+nverif+badpg<nfull) ? XrdSys::PageSize : p2_off,
                                             (p1+nread+nverif+badpg),
                                             vrbuf[badpg], 
                                             rdbuf[(nread+nverif+badpg)%rdbufsz] ));
               return -EDOM;
            }
            toverif -= vcnt;
            nverif += vcnt;
         }
      }
      toread -= rcnt;
      nread += rcnt;
   }

   return 0;
}

int XrdOssCsiPages::VerifyRangeAligned(const void *const buff, const off_t offset, const size_t blen, const Sizes_t &sizes)
{
   return FetchRangeAligned(buff,offset,blen,sizes,NULL,XrdOssDF::Verify);
}

int XrdOssCsiPages::StoreRangeAligned(const void *const buff, const off_t offset, const size_t blen, const Sizes_t &sizes, uint32_t *csvec)
{
   EPNAME("StoreRangeAligned");

   // if csvec given store those values
   // if no csvec then calculate against data and store

   const off_t p1 = offset / XrdSys::PageSize;
   const off_t trackinglen = sizes.first;

   if (offset > trackinglen)
   {
      const int ret = UpdateRangeHoleUntilPage(NULL, p1, sizes);
      if (ret<0)
      {
         TRACE(Warn, "Error updating tags for holes, error=" << ret);
         return ret;
      }
   }

   const ssize_t aret = apply_sequential_aligned_modify(buff, p1, blen, csvec, false, false, 0U, 0U);
   if (aret<0)
   {
      TRACE(Warn, "Error updating tags, error=" << aret);
      return aret;
   }

   return 0;
}

// Used by Read for aligned reads. See StoreRangeAligned for conditions.
//
int XrdOssCsiPages::UpdateRangeAligned(const void *const buff, const off_t offset, const size_t blen, const Sizes_t &sizes)
{
   return StoreRangeAligned(buff, offset, blen, sizes, NULL);
}

//
// LockTrackinglen: obtain current tracking counts and lock the following as necessary:
//                  tracking counts and file byte range [offset, offend). Lock will be applied
//                  at the page level.
//
// offset - byte offset of first page to apply lock
// offend - end of range byte (excluding byte at end) of page at which to end lock
// rdonly - will be a read-only operation
//
void XrdOssCsiPages::LockTrackinglen(XrdOssCsiRangeGuard &rg, const off_t offset, const off_t offend, const bool rdonly)
{
   // no need to lock if we don't have a tag file
   if (hasMissingTags_) return;

   // in case of empty range the tracking len is not copied
   if (offset == offend) return;

   {
      XrdSysMutexHelper lck(rangeaddmtx_);

      Sizes_t sizes;
      (void)TrackedSizesGet(sizes, !rdonly);

      // tag tracking data filesize, as recorded in the tagfile and for which the tagfile
      // should be approprately sized, is sizes.first: usually the same as the in
      // memory "actual" data filesize (sizes.second), but may differ after crashes or write failure.
      const off_t trackinglen = sizes.first;

      const off_t p1 = (offset>trackinglen ? trackinglen : offset) / XrdSys::PageSize;
      bool unlock = false;
      if (!rdonly && offend <= trackinglen)
      {
         unlock = true;
      }

      off_t p2 = offend / XrdSys::PageSize;
      const size_t p2_off = offend % XrdSys::PageSize;

      // range is exclusive
      if (p2_off ==0) p2--;

      ranges_.AddRange(p1, p2, rg, rdonly);

      if (unlock)
      {
         TrackedSizeRelease();
      }
      rg.SetTrackingInfo(this, sizes, (!rdonly && !unlock));
   }

   rg.Wait();
}

int XrdOssCsiPages::truncate(XrdOssDF *const fd, const off_t len, XrdOssCsiRangeGuard &rg)
{
   EPNAME("truncate");

   if (len<0) return -EINVAL;

   // nothing to truncate if there is no tag file
   if (hasMissingTags_) return 0;

   const Sizes_t sizes = rg.getTrackinglens();

   const off_t trackinglen = sizes.first;
   const off_t p_until = len / XrdSys::PageSize;
   const size_t p_off = len % XrdSys::PageSize;

   if (len>trackinglen)
   {
      int ret = UpdateRangeHoleUntilPage(fd,p_until,sizes);
      if (ret<0)
      {
         TRACE(Warn, "Error updating tags for holes, error=" << ret);
         return ret;
      }
   }

   if (len != trackinglen && p_off != 0)
   {
      const off_t tracked_page = trackinglen / XrdSys::PageSize;
      const size_t tracked_off = trackinglen % XrdSys::PageSize;
      size_t toread = tracked_off;
      if (len>trackinglen)
      {
         if (p_until != tracked_page) toread = 0;
      }
      else
      {
         if (p_until != tracked_page) toread = XrdSys::PageSize;
      }
      uint8_t b[XrdSys::PageSize];
      if (toread>0)
      {
         ssize_t rret = XrdOssCsiPages::fullread(fd, b, p_until*XrdSys::PageSize, toread);
         if (rret<0)
         {
            TRACE(Warn, PageReadError(toread, p_until, rret));
            return rret;
         }
         const uint32_t crc32c = XrdOucCRC::Calc32C(b, toread, 0U);
         uint32_t crc32v;
         rret = ts_->ReadTags(&crc32v, p_until, 1);
         if (rret<0)
         {
            TRACE(Warn, TagsReadError(p_until, 1, rret));
            return rret;
         }
         if (crc32v != crc32c)
         {
            TRACE(Warn, CRCMismatchError(toread, p_until, crc32c, crc32v));
            return -EDOM;
         }
      }
      if (p_off > toread)
      {
         memset(&b[toread],0,p_off-toread);
      }
      const uint32_t crc32c = XrdOucCRC::Calc32C(b, p_off, 0U);
      const ssize_t wret = ts_->WriteTags(&crc32c, p_until, 1);
      if (wret < 0)
      {
         TRACE(Warn, TagsWriteError(p_until, 1, wret));
         return wret;
      }
   }

   LockTruncateSize(len,true);
   rg.unlockTrackinglen();
   return 0;
}

// used by pgRead: At this point the user's buffer has already been filled from the file
// offset: offset within the file at which the read starts
// blen  : the length of the read already read into the buffer
//         (which may be less than what was originally requested)
//
int XrdOssCsiPages::FetchRange(
   XrdOssDF *const fd, const void *buff, const off_t offset, const size_t blen,
   uint32_t *csvec, const uint64_t opts, XrdOssCsiRangeGuard &rg)
{
   EPNAME("FetchRange");
   if (offset<0)
   {
      return -EINVAL;
   }

   // if the tag file is missing there is nothing to fetch or verify
   // but if we should return a list of checksums calculate them from the data
   if (hasMissingTags_)
   {
      if (csvec)
      {
         pgDoCalc(buff, offset, blen, csvec);
      }
      return 0;
   }

   const Sizes_t sizes = rg.getTrackinglens();
   const off_t trackinglen = sizes.first;

   if (offset >= trackinglen && blen == 0)
   {
      return 0;
   }

   if (blen == 0)
   {
      // if offset is before the tracked len we should not be requested to verify zero bytes:
      // the file may have been truncated
      TRACE(Warn, "Fetch request for zero bytes " << fn_ << ", file may be truncated");
      return -EDOM;
   }

   if (offset+blen > static_cast<size_t>(trackinglen))
   {
      TRACE(Warn, "Fetch request for " << (offset+blen-trackinglen) << " bytes from " << fn_ << " beyond tracked lengh");
      return -EDOM;
   }

   if (csvec == NULL && !(opts & XrdOssDF::Verify))
   {
      // if the crc values are not wanted nor checks against data, then
      // there's nothing more to do here
      return 0;
   }

   int fret;
   if ((offset % XrdSys::PageSize) != 0 || (offset+blen != static_cast<size_t>(trackinglen) && (blen % XrdSys::PageSize) != 0))
   {
     fret = FetchRangeUnaligned(fd, buff, offset, blen, sizes, csvec, opts);
   }
   else
   {
     fret = FetchRangeAligned(buff,offset,blen,sizes,csvec,opts);
   }
   return fret;
}

// Used by pgWrite: At this point the user's data has not yet been written to the file.
//
int XrdOssCsiPages::StoreRange(XrdOssDF *const fd, const void *buff, const off_t offset, const size_t blen, uint32_t *csvec, const uint64_t opts, XrdOssCsiRangeGuard &rg)
{
   if (offset<0)
   {
      return -EINVAL;
   }

   if (blen == 0)
   {
      return 0;
   }

   // if the tag file is missing there is nothing to store
   // but do calculate checksums to return, if requested to do so
   if (hasMissingTags_)
   {
      if (csvec && (opts & XrdOssDF::doCalc))
      {
         pgDoCalc(buff, offset, blen, csvec);
      }
      return 0;
   }

   const Sizes_t sizes = rg.getTrackinglens();
   const off_t trackinglen = sizes.first;

   // in the original specification of pgWrite there was the idea of a logical-eof, set by
   // the a pgWrite with non-page aligned length: We support an option to approximate that
   // by disallowing pgWrite past the current (non page aligned) eof.
   if (disablePgExtend_ && (trackinglen % XrdSys::PageSize) !=0 && offset+blen > static_cast<size_t>(trackinglen))
   {
      return -ESPIPE;
   }

   // if doCalc is set and we have a csvec buffer fill it with calculated values
   if (csvec && (opts & XrdOssDF::doCalc))
   {
      pgDoCalc(buff, offset, blen, csvec);
   }

   // if no vector of crc have been given and not specifically requested to calculate,
   // then mark this file as having unverified checksums
   if (!csvec && !(opts & XrdOssDF::doCalc))
   {
      LockMakeUnverified();
   }

   if (offset+blen > static_cast<size_t>(trackinglen))
   {
      LockSetTrackedSize(offset+blen);
      rg.unlockTrackinglen();
   }

   int ret;
   if ((offset % XrdSys::PageSize) != 0 ||
       (offset+blen < static_cast<size_t>(trackinglen) && (blen % XrdSys::PageSize) != 0) ||
       ((trackinglen % XrdSys::PageSize) !=0 && offset > trackinglen))
   {
      ret = StoreRangeUnaligned(fd,buff,offset,blen,sizes,csvec);
   }
   else
   {
      ret = StoreRangeAligned(buff,offset,blen,sizes,csvec);
   }

   return ret;
}

int XrdOssCsiPages::VerificationStatus()
{
   if (hasMissingTags_)
   {
      return 0;
   }
   bool iv;
   {
      XrdSysCondVarHelper lck(&tscond_);
      iv = ts_->IsVerified();
   }
   if (iv)
   {
      return XrdOss::PF_csVer;
   }
   return XrdOss::PF_csVun;
}

void XrdOssCsiPages::pgDoCalc(const void *buffer, off_t offset, size_t wrlen, uint32_t *csvec)
{
   const size_t p_off = offset % XrdSys::PageSize;
   const size_t p_alen = (p_off > 0) ? (XrdSys::PageSize - p_off) : wrlen;
   if (p_alen < wrlen)
   {
      XrdOucCRC::Calc32C((uint8_t *)buffer+p_alen, wrlen-p_alen, &csvec[1]);
   }
   XrdOucCRC::Calc32C((void*)buffer, std::min(p_alen, wrlen), csvec);
}

int XrdOssCsiPages::pgWritePrelockCheck(const void *buffer, off_t offset, size_t wrlen, const uint32_t *csvec, uint64_t opts)
{
   // do verify before taking locks to allow for faster fail
   if (csvec && (opts & XrdOssDF::Verify))
   {
      uint32_t valcs;
      const size_t p_off = offset % XrdSys::PageSize;
      const size_t p_alen = (p_off > 0) ? (XrdSys::PageSize - p_off) : wrlen;
      if (p_alen < wrlen)
      {
         if (XrdOucCRC::Ver32C((uint8_t *)buffer+p_alen, wrlen-p_alen, &csvec[1], valcs)>=0)
         {
            return -EDOM;
         }
      }
      if (XrdOucCRC::Ver32C((void*)buffer, std::min(p_alen, wrlen), csvec, valcs)>=0)
      {
         return -EDOM;
      }
   }

   return 0;
}

//
// Do some consistency checks and repair in case the datafile length is inconsistent
// with the length in the tag file. Called on open() and after write failures.
// Disabled if loosewrite_ off, or file readonly. Sets lastpgforloose_.
//
// May be called under tscond_ lock from LockResetSizes, or directly from
// CsiFile just after open.
//
void XrdOssCsiPages::BasicConsistencyCheck(XrdOssDF *fd)
{
   EPNAME("BasicConsistencyCheck");

   if (!loosewrite_ || rdonly_) return;

   uint8_t b[XrdSys::PageSize];
   static const uint8_t bz[XrdSys::PageSize] = {0};

   const off_t tagsize =  ts_->GetTrackedTagSize();
   const off_t datasize =  ts_->GetTrackedDataSize();

   off_t taglp = 0, datalp = 0;
   size_t tag_len = 0, data_len = 0;

   if (tagsize>0)
   {
      taglp = (tagsize - 1) / XrdSys::PageSize;
      tag_len = tagsize % XrdSys::PageSize;
      tag_len = tag_len ? tag_len : XrdSys::PageSize;
   }
   if (datasize>0)
   {
      datalp = (datasize - 1) / XrdSys::PageSize;
      data_len = datasize % XrdSys::PageSize;
      data_len = data_len ? data_len : XrdSys::PageSize;
   }

   lastpgforloose_ = taglp;
   checklastpg_ = true;

   if (datasize>0 && taglp > datalp)
   {
      ssize_t rlen = XrdOssCsiPages::maxread(fd, b, XrdSys::PageSize * datalp, XrdSys::PageSize);
      if (rlen<0)
      {
         TRACE(Warn, PageReadError(XrdSys::PageSize, datalp, rlen));
         return;
      }

      memset(&b[rlen], 0, XrdSys::PageSize-rlen);
      const uint32_t data_crc = XrdOucCRC::Calc32C(b, data_len, 0u);
      const uint32_t data_crc_z = XrdOucCRC::Calc32C(b, XrdSys::PageSize, 0u);
      uint32_t tagv;
      ssize_t rret = ts_->ReadTags(&tagv, datalp, 1);
      if (rret<0)
      {
         TRACE(Warn, TagsReadError(datalp, 1, rret));
         return;
      }

      if (tagv == data_crc_z)
      {
         // expected
      }
      else if (tagv == data_crc)
      {
         // should set tagv to data_crc_z
         TRACE(Warn, "Resetting tag for page at " << datalp*XrdSys::PageSize << " to zero-extended");
         const ssize_t wret = ts_->WriteTags(&data_crc_z, datalp, 1);
         if (wret < 0)
         {
            TRACE(Warn, TagsWriteError(datalp, 1, wret));
            return;
         }
      }
      else
      {
         // something else wrong
         TRACE(Warn, CRCMismatchError(data_len, datalp, data_crc, tagv) << " (ignoring)");
      }
   }
   else if (tagsize>0 && taglp < datalp)
   {
      // datafile has more pages than recorded in the tag file:
      // the tag file should have a crc corresponding to the relevant data fragment that is tracked in the last page.
      // If it has the crc for a whole page (and there no non-zero content later in the page) reset it.
      // This is so that a subsequnt UpdateRangeHoleUntilPage can zero-extend the CRC and get a consistent CRC.

      ssize_t rlen = XrdOssCsiPages::maxread(fd, b, XrdSys::PageSize * taglp, XrdSys::PageSize);
      if (rlen<0)
      {
         TRACE(Warn, PageReadError(XrdSys::PageSize, taglp, rlen));
         return;
      }

      memset(&b[rlen], 0, XrdSys::PageSize-rlen);
      const uint32_t tag_crc = XrdOucCRC::Calc32C(b, tag_len, 0u);
      const uint32_t tag_crc_z = XrdOucCRC::Calc32C(b, XrdSys::PageSize, 0u);
      const uint32_t dp_ext_is_zero = !memcmp(&b[tag_len], bz, XrdSys::PageSize-tag_len);
      uint32_t tagv;
      ssize_t rret = ts_->ReadTags(&tagv, taglp, 1);
      if (rret<0)
      {
         TRACE(Warn, TagsReadError(taglp, 1, rret));
         return;
      }

      if (tagv == tag_crc)
      {
         // expected
      }
      else if (tagv == tag_crc_z && dp_ext_is_zero)
      {
         // should set tagv to tag_crc
         TRACE(Warn, "Resetting tag for page at " << taglp*XrdSys::PageSize << " to not zero-extended");
         const ssize_t wret = ts_->WriteTags(&tag_crc, taglp, 1);
         if (wret < 0)
         {
            TRACE(Warn, TagsWriteError(taglp, 1, wret));
            return;
         }
      }
      else
      {
         // something else wrong
         TRACE(Warn, CRCMismatchError(tag_len, taglp, tag_crc, tagv) << " dp_ext_is_zero=" << dp_ext_is_zero << " (ignoring)");
      }
   }
}
