#ifndef _XRDOSSCSIPAGES_H
#define _XRDOSSCSIPAGES_H
/******************************************************************************/
/*                                                                            */
/*                    X r d O s s C s i P a g e s . h h                       */
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

#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysPageSize.hh"

#include "XrdOssCsiTagstore.hh"
#include "XrdOssCsiRanges.hh"
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <cinttypes>
#include <cstdio>

class XrdOssCsiPages
{
public:
   typedef std::pair<off_t,off_t> Sizes_t;

   XrdOssCsiPages(const std::string &fn, std::unique_ptr<XrdOssCsiTagstore> ts, bool wh, bool am, bool dpe, bool dlw, const char *);
   ~XrdOssCsiPages() { (void)Close(); }

   int Open(const char *path, off_t dsize, int flags, XrdOucEnv &envP);
   int Close();

   int UpdateRange(XrdOssDF *, const void *, off_t, size_t, XrdOssCsiRangeGuard&);
   int VerifyRange(XrdOssDF *, const void *, off_t, size_t, XrdOssCsiRangeGuard&);
   void Flush();
   int Fsync();

   void BasicConsistencyCheck(XrdOssDF *);

   int FetchRange(XrdOssDF *, const void *, off_t, size_t, uint32_t *, uint64_t, XrdOssCsiRangeGuard&);
   int StoreRange(XrdOssDF *, const void *, off_t, size_t, uint32_t *, uint64_t, XrdOssCsiRangeGuard&);
   void LockTrackinglen(XrdOssCsiRangeGuard &, off_t, off_t, bool);

   bool IsReadOnly() const { return rdonly_; }
   int truncate(XrdOssDF *, off_t, XrdOssCsiRangeGuard&);
   int TrackedSizesGet(Sizes_t &, bool);
   int LockResetSizes(XrdOssDF *, off_t);
   void TrackedSizeRelease();
   int VerificationStatus();

   static void pgDoCalc(const void *, off_t, size_t, uint32_t *);
   static int pgWritePrelockCheck(const void *, off_t, size_t, const uint32_t *, uint64_t);

protected:
   ssize_t apply_sequential_aligned_modify(const void *, off_t, size_t, const uint32_t *, bool, bool, uint32_t, uint32_t);
   std::unique_ptr<XrdOssCsiTagstore> ts_;
   XrdSysMutex rangeaddmtx_;
   XrdOssCsiRanges ranges_;
   bool writeHoles_;
   bool allowMissingTags_;
   bool disablePgExtend_;
   bool hasMissingTags_;
   bool rdonly_;
   const bool loosewriteConfigured_;
   bool loosewrite_;

   XrdSysCondVar tscond_;
   bool tsforupdate_;

   // fn_ is the associated data filename when the page object is made.
   // if renamed while the page object exists fn_ is not updated
   const std::string fn_;
   const std::string tident_;
   const char *tident;

   // used by the loosewrite checks
   off_t lastpgforloose_;
   bool checklastpg_;

   int LockSetTrackedSize(off_t);
   int LockTruncateSize(off_t,bool);
   int LockMakeUnverified();

   int UpdateRangeAligned(const void *, off_t, size_t, const Sizes_t &);
   int UpdateRangeUnaligned(XrdOssDF *, const void *, off_t, size_t, const Sizes_t &);
   int UpdateRangeHoleUntilPage(XrdOssDF *, off_t, const Sizes_t &);
   int VerifyRangeAligned(const void *, off_t, size_t, const Sizes_t &);
   int VerifyRangeUnaligned(XrdOssDF *, const void *, off_t, size_t, const Sizes_t &);
   int FetchRangeAligned(const void *, off_t, size_t, const Sizes_t &, uint32_t *, uint64_t);
   int FetchRangeUnaligned(XrdOssDF *, const void *, off_t, size_t, const Sizes_t &, uint32_t *, uint64_t);
   int FetchRangeUnaligned_preblock(XrdOssDF *, const void *, off_t, size_t, off_t, uint32_t *, uint32_t *, uint64_t);
   int FetchRangeUnaligned_postblock(XrdOssDF *, const void *, off_t, size_t, off_t, uint32_t *, uint32_t *, size_t, uint64_t);
   int StoreRangeAligned(const void *, off_t, size_t, const Sizes_t &, uint32_t *);
   int StoreRangeUnaligned(XrdOssDF *, const void *, off_t, size_t, const Sizes_t &, const uint32_t *);
   int StoreRangeUnaligned_preblock(XrdOssDF *, const void *, size_t, off_t, off_t, const uint32_t *, uint32_t &);
   int StoreRangeUnaligned_postblock(XrdOssDF *, const void *, size_t, off_t, off_t, const uint32_t *, uint32_t &);


   static ssize_t fullread(XrdOssDF *fd, void *buff, const off_t off , const size_t sz)
   {
      ssize_t rret = maxread(fd, buff, off, sz);
      if (rret<0) return rret;
      if (static_cast<size_t>(rret) != sz) return -EDOM;
      return rret;
   }

   // keep calling read until EOF, an error or the number of bytes read is between tg and sz.
   static ssize_t maxread(XrdOssDF *fd, void *buff, const off_t off , const size_t sz, size_t tg=0)
   {
      size_t toread = sz, nread = 0;
      uint8_t *p = (uint8_t*)buff;
      tg = tg ? tg : sz;
      while(toread>0 && nread<tg)
      {
         const ssize_t rret = fd->Read(&p[nread], off+nread, toread);
         if (rret<0) return rret;
         if (rret==0) break;
         toread -= rret;
         nread += rret;
      }
      return nread;
   }

   std::string CRCMismatchError(size_t blen, off_t pgnum, uint32_t got, uint32_t expected)
   {
      char buf[256],buf2[256];
      snprintf(buf, sizeof(buf),
               "bad crc32c/0x%04" PRIx32 " checksum in file ",
               (uint32_t)blen);
      snprintf(buf2, sizeof(buf2),
               " at offset 0x%" PRIx64 ", got 0x%08" PRIx32 ", expected 0x%08" PRIx32,
               (uint64_t)(pgnum*XrdSys::PageSize),
               got, expected);
      return buf + fn_ + buf2;
   }

   std::string ByteMismatchError(size_t blen, off_t off, uint8_t user, uint8_t page)
   {
      char buf[256],buf2[256];
      snprintf(buf, sizeof(buf),
               "unexpected byte mismatch between user-buffer and page/0x%04" PRIx32 " in file ",
               (uint32_t)blen);
      snprintf(buf2, sizeof(buf2),
               " at offset 0x%" PRIx64 ", user-byte 0x%02" PRIx8 ", page-byte 0x%02" PRIx8,
               (uint64_t)off,
               user, page);
      return buf + fn_ + buf2;
   }

   std::string PageReadError(size_t blen, off_t pgnum, int ret)
   {
      char buf[256],buf2[256];
      snprintf(buf, sizeof(buf),
               "error %d while reading page/0x%04" PRIx32 " in file ",
               ret, (uint32_t)blen);
      snprintf(buf2, sizeof(buf2),
               " at offset 0x%" PRIx64,
               (uint64_t)(pgnum*XrdSys::PageSize));
      return buf + fn_ + buf2;
   }

   std::string TagsReadError(off_t start, size_t n, int ret)
   {
      char buf[256];
      snprintf(buf, sizeof(buf),
               "error %d while reading crc32c values for pages [0x%" PRIx64":0x%" PRIx64 "] for file ",
               ret, (uint64_t)start, (uint64_t)(start + n - 1));
      return buf + fn_;
   }

   std::string TagsWriteError(off_t start, size_t n, int ret)
   {
      char buf[256];
      snprintf(buf, sizeof(buf),
               "error %d while writing crc32c values for pages [0x%" PRIx64":0x%" PRIx64 "] for file ",
               ret, (uint64_t)start, (uint64_t)(start + n - 1));
      return buf + fn_;
   }

   static const size_t stsize_ = 1024;
};

#endif
