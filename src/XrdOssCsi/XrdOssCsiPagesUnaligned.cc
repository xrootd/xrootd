/******************************************************************************/
/*                                                                            */
/*           X r d O s s C s i P a g e s U n a l i g n e d . c c              */
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
#include "XrdOssCsiCrcUtils.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdSys/XrdSysPageSize.hh"

#include <vector>
#include <assert.h>

extern XrdOucTrace  OssCsiTrace;
static XrdOssCsiCrcUtils CrcUtils;

//
// UpdateRangeHoleUntilPage
//
// Used pgWrite/Write (both aligned and unaligned cases) when extending a file
// with implied zeros after then current end of file and the new one.
// fd (data file descriptor pointer) required only when last page in file is partial.
//   current implementation does not use fd in this case, but requires it be set.
//
int XrdOssCsiPages::UpdateRangeHoleUntilPage(XrdOssDF *fd, const off_t until, const Sizes_t &sizes)
{
   EPNAME("UpdateRangeHoleUntilPage");

   static const uint32_t crczero = CrcUtils.crc32c_extendwith_zero(0u, XrdSys::PageSize);
   static const std::vector<uint32_t> crc32Vec(stsize_, crczero);

   const off_t trackinglen = sizes.first;
   const off_t tracked_page = trackinglen / XrdSys::PageSize;
   if (until <= tracked_page) return 0;

   const size_t tracked_off = trackinglen % XrdSys::PageSize;

   // if last tracked page is before page "until" extend it
   if (tracked_off>0)
   {
      if (fd == NULL)
      {
         TRACE(Warn, "Unexpected partially filled last page " << fn_);
         return -EDOM;
      }

      uint32_t prevtag;
      const ssize_t rret = ts_->ReadTags(&prevtag, tracked_page, 1);
      if (rret < 0)
      {
         TRACE(Warn, TagsReadError(tracked_page, 1, rret));
         return rret;
      }

      // extend prevtag up to PageSize. If there is a mismatch it will only be
      // discovered during a later read (but this saves a read now).
      const uint32_t crc32c = CrcUtils.crc32c_extendwith_zero(prevtag, XrdSys::PageSize - tracked_off);
      const ssize_t wret = ts_->WriteTags(&crc32c, tracked_page, 1);
      if (wret < 0)
      {
         TRACE(Warn, TagsWriteError(tracked_page, 1, wret) << " (prev)");
         return wret;
      }
   }

   if (!writeHoles_) return 0;

   const off_t nAllEmpty = (tracked_off>0) ? (until - tracked_page - 1) : (until - tracked_page);
   const off_t firstEmpty = (tracked_off>0) ? (tracked_page + 1) : tracked_page;

   off_t towrite = nAllEmpty;
   off_t nwritten = 0;
   while(towrite>0)
   {
      const size_t nw = std::min(towrite, (off_t)crc32Vec.size());
      const ssize_t wret = ts_->WriteTags(&crc32Vec[0], firstEmpty+nwritten, nw);
      if (wret<0)
      {
         TRACE(Warn, TagsWriteError(firstEmpty+nwritten, nw, wret) << " (new)");
         return wret;
      }
      towrite -= wret;
      nwritten += wret;
   }

   return 0;
}

// UpdateRangeUnaligned
// 
// Used by Write for various cases with mis-alignment that need checksum recalculation. See StoreRangeUnaligned for list of conditions.
//
int XrdOssCsiPages::UpdateRangeUnaligned(XrdOssDF *const fd, const void *buff, const off_t offset, const size_t blen, const Sizes_t &sizes)
{
   return StoreRangeUnaligned(fd, buff, offset, blen, sizes, NULL);
}

//
// used by StoreRangeUnaligned when the supplied data does not cover the whole of the first corresponding page in the file
//
// offset: offset in file for start of write
// blen:   length of write in first page
//
int XrdOssCsiPages::StoreRangeUnaligned_preblock(XrdOssDF *const fd, const void *const buff, const size_t blen,
                                                 const off_t offset, const off_t trackinglen,
                                                 const uint32_t *const csvec, uint32_t &prepageval)
{
   EPNAME("StoreRangeUnaligned_preblock");
   const off_t p1 = offset / XrdSys::PageSize;
   const size_t p1_off = offset % XrdSys::PageSize;

   const off_t tracked_page = trackinglen / XrdSys::PageSize;
   const size_t tracked_off = trackinglen % XrdSys::PageSize;

   if (p1 > tracked_page)
   {
      // the start of will have a number of implied zero bytes
      uint32_t crc32c = CrcUtils.crc32c_extendwith_zero(0u, p1_off);
      if (csvec)
      {
         crc32c = CrcUtils.crc32c_combine(crc32c, csvec[0], blen);
      }
      else
      {
         crc32c = XrdOucCRC::Calc32C(buff, blen, crc32c);
      }
      prepageval = crc32c;
      return 0;
   }

   // we're appending, or appending within the last page after a gap of zeros
   if (p1 == tracked_page && p1_off >= tracked_off)
   {
      // appending: with or without some implied zeros.

      // zero initialised value may be used
      uint32_t crc32v = 0;
      if (tracked_off > 0)
      {
         const ssize_t rret = ts_->ReadTags(&crc32v, p1, 1);
         if (rret<0)
         {
            TRACE(Warn, TagsReadError(p1, 1, rret) << " (append)");
            return rret;
         }
      }

      uint32_t crc32c = 0;

      // only do the loosewrite extending check one time for the page which was the
      // last page according to the trackinglen at time the check was configured (open or size-resync).
      // don't do the check every time because it needs an extra read compared to the non loose case;
      // checklastpg_ is checked and modified here, but is protected from concurrent
      // access because of the condition that p1==lastpgforloose_

      if (loosewrite_ && p1==lastpgforloose_ && checklastpg_)
      {
         checklastpg_ = false;
         uint8_t b[XrdSys::PageSize];

         // this will reissue read() until eof, or tracked_off bytes read but accept up to PageSize
         const ssize_t rlen = XrdOssCsiPages::maxread(fd, b, XrdSys::PageSize * p1, XrdSys::PageSize, tracked_off);

         if (rlen<0)
         {
            TRACE(Warn, PageReadError(tracked_off, p1, rlen));
            return rlen;
         }
         memset(&b[rlen], 0, XrdSys::PageSize - rlen);

         // in the loose-write mode, the new crc is based on the crc of data
         // read from file up to p1_off, not on the previously stored tag.
         // However must check if the data read were consistent with stored tag (crc32v)

         uint32_t crc32x = XrdOucCRC::Calc32C(b, tracked_off, 0u);
         crc32c = XrdOucCRC::Calc32C(&b[tracked_off], p1_off-tracked_off, crc32x);

         do
         {
            if (static_cast<size_t>(rlen) == tracked_off)
            {
               // this is the expected match
               if (tracked_off==0 || crc32x == crc32v) break;
            }

            // any bytes on disk beyond p1_off+blan would not be included in the new crc.
            // if tracked_off==0 we have no meaningful crc32v value.
            if ((tracked_off>0 || p1_off==0) && static_cast<size_t>(rlen) <= p1_off+blen)
            {
               
               if (tracked_off != 0)
               {
                  TRACE(Warn, CRCMismatchError(tracked_off, p1, crc32x, crc32v) << " (loose match, still trying)");
               }

               // there was no tag recorded for the page, and we're completely overwriting anything on disk in the page
               if (tracked_off==0)
               {
                  TRACE(Warn, "Recovered page with no tag at offset " << (XrdSys::PageSize * p1) <<
                              " of file " << fn_ << " rlen=" << rlen << " (append)");
                  break;
               }

               if (static_cast<size_t>(rlen) != tracked_off && rlen>0)
               {
                  crc32x = XrdOucCRC::Calc32C(b, rlen, 0u);
                  if (crc32x == crc32v)
                  {
                     TRACE(Warn, "Recovered page at offset " << (XrdSys::PageSize * p1)+p1_off << " of file " << fn_ << " (append)");
                     break;
                  }
                  TRACE(Warn, CRCMismatchError(rlen, p1, crc32x, crc32v) << " (loose match, still trying)");
               }

               memcpy(&b[p1_off], buff, blen);
               crc32x = XrdOucCRC::Calc32C(b, p1_off+blen, 0u);
               if (crc32x == crc32v)
               {
                  TRACE(Warn, "Recovered matching write at offset " << (XrdSys::PageSize * p1)+p1_off <<
                              " of file " << fn_ << " (append)");
                  break;
               }
               TRACE(Warn, CRCMismatchError(p1_off+blen, p1, crc32x, crc32v) << " (append)");
            }
            else
            {
               if (tracked_off>0)
               {
                  TRACE(Warn, CRCMismatchError(tracked_off, p1, crc32x, crc32v) << " (append)");
               }
               else
               {
                  TRACE(Warn, "Unexpected content, write at page at offset " << (XrdSys::PageSize * p1) <<
                              " of file " << fn_ << ", offset-in-page=" << p1_off << " rlen=" << rlen << " (append)");
               }
            }
            return -EDOM;
         } while(0);
      }
      else
      {  
         // non-loose case;
         // can recalc crc with new data without re-reading existing partial block's data
         const size_t nz = p1_off - tracked_off;
         crc32c = CrcUtils.crc32c_extendwith_zero(crc32v, nz);
      }

      // crc32c is crc up to p1_off. Now add the user's data.
      if (csvec)
      {
         crc32c = CrcUtils.crc32c_combine(crc32c, csvec[0], blen);
      }
      else
      {
         crc32c = XrdOucCRC::Calc32C(buff, blen, crc32c);
      }
      prepageval = crc32c;
      return 0;
   }

   const size_t bavail = (p1==tracked_page) ? tracked_off : XrdSys::PageSize;

   // assert we're overwriting some (or all) of the previous data (other case was above)
   assert(p1_off < bavail);

   // case p1_off==0 && blen>=bavail is either handled by aligned case (p1==tracked_page)
   // or not sent to preblock, so will need to read some preexisting data
   assert(p1_off !=0 || blen<bavail);
   uint8_t b[XrdSys::PageSize];

   uint32_t crc32v;
   ssize_t rret = ts_->ReadTags(&crc32v, p1, 1);
   if (rret<0)
   {
      TRACE(Warn, TagsReadError(p1, 1, rret) << " (overwrite)");
      return rret;
   }

   // in either loosewrite or non-loosewrite a read-modify-write sequence is done and the
   // final crc is that of the modified block. The difference between loose and non-loose
   // case if that the looser checks are done on the block.
   //
   // in either case there are implicit verification(s) (e.g. pgWrite may return EDOM without Verify requested)
   // as it's not clear if there is a meaningful way to crc a mismatching page during a partial overwrite

   if (loosewrite_)
   {
      // this will reissue read() until eof, or bavail bytes read but accept up to PageSize
      const ssize_t rlen = XrdOssCsiPages::maxread(fd, b, XrdSys::PageSize * p1, XrdSys::PageSize, bavail);
      if (rlen<0)
      {
         TRACE(Warn, PageReadError(bavail, p1, rlen));
         return rlen;
      }
      memset(&b[rlen], 0, XrdSys::PageSize - rlen);
      do
      {
         uint32_t crc32c = XrdOucCRC::Calc32C(b, bavail, 0U);
         // this is the expected case
         if (static_cast<size_t>(rlen) == bavail && crc32c == crc32v) break;

         // after this write there will be nothing changed between p1_off+blen
         // and bavail; if there is nothing on disk in this range it will not
         // be added by the write. So don't try to match crc with implied zero
         // in this range. Beyond bavail bytes on disk will not be included
         // in the new crc.
         const size_t rmin = (p1_off+blen < bavail) ? bavail : 0;
         if (static_cast<size_t>(rlen) >= rmin && static_cast<size_t>(rlen)<=bavail)
         {
            if (crc32c == crc32v)
            {
               TRACE(Warn, "Recovered page at offset " << (XrdSys::PageSize * p1) << " of file " << fn_ << " (overwrite)");
               break;
            }
            TRACE(Warn, CRCMismatchError(bavail, p1, crc32c, crc32v) << " (loose match, still trying)");

            if (static_cast<size_t>(rlen) != bavail && rlen > 0)
            {
               crc32c = XrdOucCRC::Calc32C(b, rlen, 0U);
               if (crc32c == crc32v)
               {
                  TRACE(Warn, "Recovered page (2) at offset " << (XrdSys::PageSize * p1) << " of file " << fn_ << " (overwrite)");
                  break;
               }
               TRACE(Warn, CRCMismatchError(rlen, p1, crc32c, crc32v) << " (loose match, still trying)");
            }

            memcpy(&b[p1_off], buff, blen);
            const size_t vl = std::max(bavail, p1_off+blen);
            crc32c = XrdOucCRC::Calc32C(b, vl, 0U);
            if (crc32c == crc32v)
            {
               TRACE(Warn, "Recovered matching write at offset " << (XrdSys::PageSize * p1)+p1_off << " of file " << fn_ << " (overwrite)");
               break;
            }
            TRACE(Warn, CRCMismatchError(vl, p1, crc32c, crc32v) << " (overwrite)");
         }
         else
         {
            TRACE(Warn, CRCMismatchError(bavail, p1, crc32c, crc32v) << " (overwrite)");
         }
         return -EDOM;
      } while(0);
   }
   else
   {
      // non-loose case
      rret = XrdOssCsiPages::fullread(fd, b, XrdSys::PageSize * p1, bavail);
      if (rret<0)
      {
         TRACE(Warn, PageReadError(bavail, p1, rret));
         return rret;
      }
      const uint32_t crc32c = XrdOucCRC::Calc32C(b, bavail, 0U);
      if (crc32v != crc32c)
      {
         TRACE(Warn, CRCMismatchError(bavail, p1, crc32c, crc32v));
         return -EDOM;
      }
   }

   uint32_t crc32c = XrdOucCRC::Calc32C(b, p1_off, 0U);
   if (csvec)
   {
      crc32c = CrcUtils.crc32c_combine(crc32c, csvec[0], blen);
   }
   else
   {
      crc32c = XrdOucCRC::Calc32C(buff, blen, crc32c);
   }
   if (p1_off+blen < bavail)
   {
      const uint32_t cl = XrdOucCRC::Calc32C(&b[p1_off+blen], bavail-p1_off-blen, 0U);
      crc32c = CrcUtils.crc32c_combine(crc32c, cl, bavail-p1_off-blen);
   }
   prepageval = crc32c;
   return 0;
}

//
// used by StoreRangeUnaligned when the end of supplied data is not page aligned
// and is before the end of file
//
// offset: first offset in file at which write is page aligned
// blen:   length of write after offset
//
int XrdOssCsiPages::StoreRangeUnaligned_postblock(XrdOssDF *const fd, const void *const buff, const size_t blen,
                                                  const off_t offset, const off_t trackinglen,
                                                  const uint32_t *const csvec, uint32_t &lastpageval)
{
   EPNAME("StoreRangeUnaligned_postblock");

   const uint8_t *const p = (uint8_t*)buff;
   const off_t p2 = (offset+blen) / XrdSys::PageSize;
   const size_t p2_off = (offset+blen) % XrdSys::PageSize;

   const off_t tracked_page = trackinglen / XrdSys::PageSize;
   const size_t tracked_off = trackinglen % XrdSys::PageSize;

   // we should not be called in this case
   assert(p2_off != 0);

   // how much existing data this last (p2) page
   const size_t bavail = (p2==tracked_page) ? tracked_off : XrdSys::PageSize;

   // how much of that data will not be overwritten
   const size_t bremain = (p2_off < bavail) ? bavail-p2_off : 0;

   // we should not be called if it is a complete overwrite
   assert(bremain>0);

   // need to use remaining data to calculate the crc of the new p2 page.
   // read and verify it now.

   uint32_t crc32v;
   ssize_t rret = ts_->ReadTags(&crc32v, p2, 1);
   if (rret<0)
   {
      TRACE(Warn, TagsReadError(p2, 1, rret));
      return rret;
   }

   uint8_t b[XrdSys::PageSize];
   rret = XrdOssCsiPages::fullread(fd, b, XrdSys::PageSize * p2, bavail);
   if (rret<0)
   {
      TRACE(Warn, PageReadError(bavail, p2, rret));
      return rret;
   }

   // calculate crc of new data with remaining data at the end:
   uint32_t crc32c = 0;
   if (csvec)
   {
      crc32c = csvec[(blen-1)/XrdSys::PageSize];
   }
   else
   {
      crc32c = XrdOucCRC::Calc32C(&p[blen-p2_off], p2_off, 0U);
   }

   const uint32_t cl = XrdOucCRC::Calc32C(&b[p2_off], bremain, 0U);
   // crc of page with new data
   crc32c = CrcUtils.crc32c_combine(crc32c, cl, bremain);
   // crc of current page (before write)
   const uint32_t crc32prev = XrdOucCRC::Calc32C(b, bavail, 0U);

   // check(s) to see if remaining data was valid

   // usual check; unmodified block is consistent with stored crc
   // for loose write we allow case were the new crc has already been stored in the tagfile

   // this may be an implicit verification (e.g. pgWrite may return EDOM without Verify requested)
   // however, it's not clear if there is a meaningful way to crc a mismatching page during a partial overwrite
   if (crc32v != crc32prev)
   {
      if (loosewrite_ && crc32c != crc32prev)
      {
         // log that we chceked if the tag was matching the previous data
         TRACE(Warn, CRCMismatchError(bavail, p2, crc32prev, crc32v) << " (loose match, still trying)");
         if (crc32c == crc32v)
         {
            TRACE(Warn, "Recovered matching write at offset " << (XrdSys::PageSize * p2) << " of file " << fn_);
            lastpageval = crc32c;
            return 0;
         }
         TRACE(Warn, CRCMismatchError(bavail, p2, crc32c, crc32v));
      }
      else
      {
         TRACE(Warn, CRCMismatchError(bavail, p2, crc32prev, crc32v));
      }
      return -EDOM;
   }

   lastpageval = crc32c;
   return 0;
}

//
// StoreRangeUnaligned
// 
// Used by pgWrite or Write (via UpdateRangeUnaligned) where the start of this update is not page aligned within the file
// OR where the end of this update is before the end of the file and is not page aligned
// OR where end of the file is not page aligned and this update starts after it
// i.e. where checksums of last current page of file, or the first or last pages after writing this buffer will need to be recomputed
//
int XrdOssCsiPages::StoreRangeUnaligned(XrdOssDF *const fd, const void *buff, const off_t offset, const size_t blen, const Sizes_t &sizes, const uint32_t *const csvec)
{
   EPNAME("StoreRangeUnaligned");
   const off_t p1 = offset / XrdSys::PageSize;

   const off_t trackinglen = sizes.first;
   if (offset > trackinglen)
   {
      const int ret = UpdateRangeHoleUntilPage(fd, p1, sizes);
      if (ret<0)
      {
         TRACE(Warn, "Error updating tags for holes, error=" << ret);
         return ret;
      }
   }

   const size_t p1_off = offset % XrdSys::PageSize;
   const size_t p2_off = (offset+blen) % XrdSys::PageSize;

   bool hasprepage = false;
   uint32_t prepageval;

   // deal with partial first page
   if ( p1_off>0 || blen < static_cast<size_t>(XrdSys::PageSize) )
   {
      const size_t bavail = (XrdSys::PageSize-p1_off > blen) ? blen : (XrdSys::PageSize-p1_off);
      const int ret = StoreRangeUnaligned_preblock(fd, buff, bavail, offset, trackinglen, csvec, prepageval);
      if (ret<0)
      {
         return ret;
      }
      hasprepage = true;
   }

   // next page (if any)
   const off_t np = hasprepage ? p1+1 : p1;
   // next page starts at buffer offset
   const size_t npoff = hasprepage ? (XrdSys::PageSize - p1_off) : 0;

   // anything in next page?
   if (blen <= npoff)
   {
      // only need to write the first, partial page
      if (hasprepage)
      {
         const ssize_t wret = ts_->WriteTags(&prepageval, p1, 1);
         if (wret<0)
         {
            TRACE(Warn, TagsWriteError(p1, 1, wret));
            return wret;
         }
      }
      return 0;
   }

   const uint8_t *const p = (uint8_t*)buff;
   const uint32_t *csp = csvec;
   if (csp && hasprepage) csp++;

   // see if there will be no old data to account for in the last page
   if (p2_off == 0 || (offset + blen >= static_cast<size_t>(trackinglen)))
   {
      // write any precomputed prepage, then write full pages and last partial page (computing or using supplied csvec)
      const ssize_t aret = apply_sequential_aligned_modify(&p[npoff], np, blen-npoff, csp, hasprepage, false, prepageval, 0U);
      if (aret<0)
      {
         TRACE(Warn, "Error updating tags, error=" << aret);
         return aret;
      }
      return 0;
   }

   // last page contains existing data that has to be read to modify it

   uint32_t lastpageval;
   const int ret = StoreRangeUnaligned_postblock(fd, &p[npoff], blen-npoff, offset+npoff, trackinglen, csp, lastpageval);
   if (ret<0)
   {
      return ret;
   }

   // write any precomputed prepage, then write full pages (computing or using supplied csvec) and finally write precomputed last page
   const ssize_t aret = apply_sequential_aligned_modify(&p[npoff], np, blen-npoff, csp, hasprepage, true, prepageval, lastpageval);
   if (aret<0)
   {
      TRACE(Warn, "Error updating tags, error=" << aret);
      return aret;
   }

   return 0;
}

// VerifyRangeUnaligned
// 
// Used by Read for various cases with mis-alignment. See FetchRangeUnaligned for list of conditions.
//
int XrdOssCsiPages::VerifyRangeUnaligned(XrdOssDF *const fd, const void *const buff, const off_t offset, const size_t blen, const Sizes_t &sizes)
{
  return FetchRangeUnaligned(fd, buff, offset, blen, sizes, NULL, XrdOssDF::Verify);
}

//
// used by FetchRangeUnaligned when only part of the data in the first page is needed, or the page is short
//
// offset: offset in file for start of read
// blen:   total length of read
//
int XrdOssCsiPages::FetchRangeUnaligned_preblock(XrdOssDF *const fd, const void *const buff, const off_t offset, const size_t blen,
                                                 const off_t trackinglen, uint32_t *const tbuf, uint32_t *const csvec, const uint64_t opts)
{
   EPNAME("FetchRangeUnaligned_preblock");

   const off_t p1 = offset / XrdSys::PageSize;
   const size_t p1_off = offset % XrdSys::PageSize;

   // bavail is length of data in this page
   const size_t bavail = std::min(trackinglen - (XrdSys::PageSize*p1), (off_t)XrdSys::PageSize);

   // bcommon is length of data in this page that user wants
   const size_t bcommon = std::min(bavail - p1_off, blen);

   uint8_t b[XrdSys::PageSize];
   const uint8_t *ub = (uint8_t*)buff;
   if (bavail>bcommon)
   {
      // will need more data to either verify or return crc of the user's data
      // (in case of no verify and no csvec FetchRange() returns early)
      const ssize_t rret = XrdOssCsiPages::fullread(fd, b, XrdSys::PageSize*p1, bavail);
      if (rret<0)
      {
         TRACE(Warn, PageReadError(bavail, p1, rret));
         return rret;
      }
      // if we're going to verify, make sure we just read the same overlapping data as that in the user's buffer
      if ((opts & XrdOssDF::Verify))
      {
         if (memcmp(buff, &b[p1_off], bcommon))
         {
            size_t badoff;
            for(badoff=0;badoff<bcommon;badoff++) { if (((uint8_t*)buff)[badoff] != b[p1_off+badoff]) break; }
            badoff = (badoff < bcommon) ? badoff : 0; // may be possible with concurrent modification
            TRACE(Warn, ByteMismatchError(bavail, XrdSys::PageSize*p1+p1_off+badoff, ((uint8_t*)buff)[badoff], b[p1_off+badoff]));
            return -EDOM;
         }
      }
      ub = b;
   }
   // verify; based on whole block, or user's buffer (if it contains the whole block)
   if ((opts & XrdOssDF::Verify))
   {
      const uint32_t crc32calc = XrdOucCRC::Calc32C(ub, bavail, 0U);
      if (tbuf[0] != crc32calc)
      {
         TRACE(Warn, CRCMismatchError(bavail, p1, crc32calc, tbuf[0]));
         return -EDOM;
      }
   }

   // if we're returning csvec values and this first block
   // needs adjustment because user requested a subset..
   if (bavail>bcommon && csvec)
   {
     // make sure csvec[0] corresponds to only the data the user wanted, not whole page.
     // if we have already verified the page + common part matches user's, take checksum of common.
     // (Use local copy of page, perhaps less chance of accidental concurrent modification than buffer)
     // Otherwise base on saved checksum.
     if ((opts & XrdOssDF::Verify))
     {
        csvec[0] = XrdOucCRC::Calc32C(&b[p1_off], bcommon, 0u);
     }
     else
     {
       // calculate expected user checksum based on block's recorded checksum, adjusting
       // for data not included in user's request. If either the returned data or the
       // data not included in the user's request are corrupt the returned checksum and
       // returned data will (probably) mismatch.

       // remove block data before p1_off from checksum
       uint32_t crc32c = XrdOucCRC::Calc32C(b, p1_off, 0u);
       csvec[0] = CrcUtils.crc32c_split2(csvec[0], crc32c, bavail-p1_off);

       // remove block data after p1_off+bcommon upto bavail
       crc32c = XrdOucCRC::Calc32C(&b[p1_off+bcommon], bavail-p1_off-bcommon, 0u);
       csvec[0] = CrcUtils.crc32c_split1(csvec[0], crc32c, bavail-p1_off-bcommon);
     }
   }
   return 0;
}

//
// used by FetchRangeUnaligned when only part of a page of data is needed from the last page
//
// offset: offset in file for start of read
// blen:   total length of read
//
int XrdOssCsiPages::FetchRangeUnaligned_postblock(XrdOssDF *const fd, const void *const buff, const off_t offset, const size_t blen,
                                                 const off_t trackinglen, uint32_t *const tbuf, uint32_t *const csvec, const size_t tidx, const uint64_t opts)
{
   EPNAME("FetchRangeUnaligned_postblock");

   const off_t p2 = (offset+blen) / XrdSys::PageSize;
   const size_t p2_off = (offset+blen) % XrdSys::PageSize;

   // length of data in last (p2) page
   const size_t bavail = std::min(trackinglen - (XrdSys::PageSize*p2), (off_t)XrdSys::PageSize);

   // how much of that data is not being returned
   const size_t bremain = (p2_off < bavail) ? bavail-p2_off : 0;
   uint8_t b[XrdSys::PageSize];
   const uint8_t *ub = &((uint8_t*)buff)[blen-p2_off];
   if (bremain>0)
   {
      const ssize_t rret = XrdOssCsiPages::fullread(fd, b, XrdSys::PageSize*p2, bavail);
      if (rret<0)
      {
         TRACE(Warn, PageReadError(bavail, p2, rret));
         return rret;
      }
      // if we're verifying make sure overlapping part of data just read matches user's buffer
      if ((opts & XrdOssDF::Verify))
      {
         const uint8_t *const p = (uint8_t*)buff;
         if (memcmp(&p[blen-p2_off], b, p2_off))
         {
            size_t badoff;
            for(badoff=0;badoff<p2_off;badoff++) { if (p[blen-p2_off+badoff] != b[badoff]) break; }
            badoff = (badoff < p2_off) ? badoff : 0; // may be possible with concurrent modification
            TRACE(Warn, ByteMismatchError(bavail, XrdSys::PageSize*p2+badoff, p[blen-p2_off+badoff], b[badoff]));
            return -EDOM;
         }
      }
      ub = b;
   }
   if ((opts & XrdOssDF::Verify))
   {
      const uint32_t crc32calc = XrdOucCRC::Calc32C(ub, bavail, 0U);
      if (tbuf[tidx] != crc32calc)
      {
         TRACE(Warn, CRCMismatchError(bavail, p2, crc32calc, tbuf[tidx]));
         return -EDOM;
      }
   }
   // if we're returning csvec and user only request part of page
   // adjust the crc
   if (csvec && bremain>0)
   {
      if ((opts & XrdOssDF::Verify))
      {
         // verified; calculate crc based on common part of page.
         csvec[tidx] = XrdOucCRC::Calc32C(b, p2_off, 0u);
      }
      else
      {
         // recalculate crc based on recorded checksum and adjusting for part of data not returned.
         // If either the returned data or the data not included in the user's request are
         // corrupt the returned checksum and returned data will (probably) mismatch.

         const uint32_t crc32c = XrdOucCRC::Calc32C(&b[p2_off], bremain, 0u);
         csvec[tidx] = CrcUtils.crc32c_split1(csvec[tidx], crc32c, bremain);
      }
   }

   return 0;
}

//
// FetchRangeUnaligned
//
// Used by pgRead/Read when reading a range not starting at a page boundary within the file
// OR when the length is not a multiple of the page-size and the read finishes not at the end of file.
//
int XrdOssCsiPages::FetchRangeUnaligned(XrdOssDF *const fd, const void *const buff, const off_t offset, const size_t blen, const Sizes_t &sizes, uint32_t *const csvec, const uint64_t opts)
{
   EPNAME("FetchRangeUnaligned");

   const off_t p1 = offset / XrdSys::PageSize;
   const size_t p1_off = offset % XrdSys::PageSize;
   const off_t p2 = (offset+blen) / XrdSys::PageSize;
   const size_t p2_off = (offset+blen) % XrdSys::PageSize;

   const off_t trackinglen = sizes.first;

   size_t ntagstoread = (p2_off>0) ? p2-p1+1 : p2-p1;
   size_t ntagsbase = p1;
   uint32_t tbufint[stsize_], *tbuf=0;
   size_t tbufsz = 0;
   if (!csvec)
   {
     tbuf = tbufint;
     tbufsz = sizeof(tbufint)/sizeof(uint32_t);
   }
   else
   {
     tbuf = csvec;
     tbufsz = ntagstoread;
   }

   size_t tcnt = std::min(ntagstoread, tbufsz);
   ssize_t rret = ts_->ReadTags(tbuf, ntagsbase, tcnt);
   if (rret<0)
   {
      TRACE(Warn, TagsReadError(ntagsbase, tcnt, rret) << " (first)");
      return rret;
   }
   ntagstoread -= tcnt;

   // deal with partial first page
   if ( p1_off>0 || blen < static_cast<size_t>(XrdSys::PageSize) )
   {
      const int ret = FetchRangeUnaligned_preblock(fd, buff, offset, blen, trackinglen, tbuf, csvec, opts);
      if (ret<0)
      {
         return ret;
      }
   }

   // first (inclusive) and last (exclusive) full page
   const off_t fp = (p1_off != 0) ? p1+1 : p1;
   const off_t lp = p2;

   // verify full pages if wanted
   if (fp<lp && (opts & XrdOssDF::Verify))
   {
      const uint8_t *const p = (uint8_t*)buff;
      uint32_t calcbuf[stsize_];
      const size_t cbufsz = sizeof(calcbuf)/sizeof(uint32_t);
      size_t toread = lp-fp;
      size_t nread = 0;
      while(toread>0)
      {
         const size_t ccnt = std::min(toread, cbufsz);
         XrdOucCRC::Calc32C(&p[(p1_off ? XrdSys::PageSize-p1_off : 0)+XrdSys::PageSize*nread],ccnt*XrdSys::PageSize,calcbuf);
         size_t tovalid = ccnt;
         size_t nvalid = 0;
         while(tovalid>0)
         {
            const size_t tidx=fp+nread+nvalid - ntagsbase;
            const size_t nv = std::min(tovalid, tbufsz-tidx);
            if (nv == 0)
            {
               assert(csvec == NULL);
               ntagsbase += tbufsz;
               tcnt = std::min(ntagstoread, tbufsz);
               rret = ts_->ReadTags(tbuf, ntagsbase, tcnt);
               if (rret<0)
               {
                  TRACE(Warn, TagsReadError(ntagsbase, tcnt, rret) << " (mid)");
                  return rret;
               }
               ntagstoread -= tcnt;
               continue;
            }
            if (memcmp(&calcbuf[nvalid], &tbuf[tidx], 4*nv))
            {
               size_t badpg;
               for(badpg=0;badpg<nv;badpg++) { if (memcmp(&calcbuf[nvalid+badpg], &tbuf[tidx+badpg],4)) break; }
               TRACE(Warn, CRCMismatchError(XrdSys::PageSize,
                                            (ntagsbase+tidx+badpg),
                                            calcbuf[nvalid+badpg], tbuf[tidx+badpg]));
               return -EDOM;
            }
            tovalid -= nv;
            nvalid += nv;
         }
         toread -= ccnt;
         nread += ccnt;
      }
   }

   // last partial page
   if (p2>p1 && p2_off > 0)
   {
      // make sure we have last tag;
      // (should already have all of them if we're returning them in csvec)
      size_t tidx = p2 - ntagsbase;
      if (tidx >= tbufsz)
      {
         assert(csvec == NULL);
         tidx = 0;
         ntagsbase = p2;
         rret = ts_->ReadTags(tbuf, ntagsbase, 1);
         if (rret<0)
         {
            TRACE(Warn, TagsReadError(ntagsbase, 1, rret) << " (last)");
            return rret;
         }
         ntagstoread = 0;
      }

      const int ret = FetchRangeUnaligned_postblock(fd, buff, offset, blen, trackinglen, tbuf, csvec, tidx, opts);
      if (ret<0)
      {
         return ret;
      }
   }

   return 0;
}
