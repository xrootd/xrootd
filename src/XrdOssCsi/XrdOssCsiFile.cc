/******************************************************************************/
/*                                                                            */
/*                    X r d O s s C s i F i l e . c c                         */
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

#include "XrdOssCsi.hh"
#include "XrdOssCsiTrace.hh"
#include "XrdOssCsiTagstoreFile.hh"
#include "XrdOssCsiPages.hh"
#include "XrdOssCsiRanges.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSys/XrdSysPageSize.hh"
#include "XrdVersion.hh"
#include "XrdSfs/XrdSfsAio.hh"

#include <string>
#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <assert.h>

extern XrdSysError  OssCsiEroute;
extern XrdOucTrace  OssCsiTrace;

// storage for class members
XrdSysMutex XrdOssCsiFile::pumtx_;
std::unordered_map<std::string, std::shared_ptr<XrdOssCsiFile::puMapItem_t> > XrdOssCsiFile::pumap_;

//
// If no others hold a pointer to Pages object, close it and remoe the pagemap info object.
//
int XrdOssCsiFile::pageMapClose()
{
   if (!pmi_) return -EBADF;
   bool doclose = false;

   XrdSysMutexHelper lck(pmi_->mtx);
   if (mapRelease(pmi_)) doclose = true;

   int cpret = 0;
   if (doclose)
   {
      if (pmi_->pages)
      {
         cpret = pmi_->pages->Close();
         pmi_->pages.reset();
      }
   }

   lck.UnLock();
   pmi_.reset();

   return cpret;
}

void XrdOssCsiFile::mapTake(const std::string &key, std::shared_ptr<puMapItem_t> &pmi, const bool create)
{
   XrdSysMutexHelper lck(pumtx_);
   auto mapidx = pumap_.find(key);
   if (mapidx == pumap_.end())
   {
      if (!create) return;
      pmi.reset(new puMapItem_t());
      pmi->tpath = key;
      if (!key.empty())
      {
         pumap_.insert(std::make_pair(key, pmi));
      }
   }
   else
   {
      pmi = mapidx->second;
   }
   pmi->refcount++;
}

int XrdOssCsiFile::mapRelease(std::shared_ptr<puMapItem_t> &pmi, XrdSysMutexHelper *plck)
{
   XrdSysMutexHelper lck(pumtx_);
   pmi->refcount--;
   auto mapidx = pumap_.find(pmi->tpath);
   if (pmi->refcount == 0 || pmi->unlinked)
   {
      if (mapidx != pumap_.end() && mapidx->second == pmi)
      {
         pumap_.erase(mapidx);
      }
   }
   if (plck) plck->UnLock();
   return (pmi->refcount == 0) ? 1 : 0;
}

int XrdOssCsiFile::pageAndFileOpen(const char *fn, const int dflags, const int Oflag, const mode_t Mode, XrdOucEnv &Env)
{
   if (pmi_) return -EBADF;

   {
      std::string tpath = config_.tagParam_.makeTagFilename(fn);
      mapTake(tpath, pmi_);
   }

   XrdSysMutexHelper lck(pmi_->mtx);
   pmi_->dpath = fn;
   if (pmi_->unlinked)
   {
     mapRelease(pmi_, &lck);
     // filename replaced since check, try again
     pmi_.reset();
     return pageAndFileOpen(fn, dflags, Oflag, Mode, Env);
   }

   if ((dflags & O_TRUNC) && pmi_->pages)
   {
      // truncate of already open file at open() not supported
      mapRelease(pmi_, &lck);
      pmi_.reset();
      return -EDEADLK;
   }

   const int dataret = successor_->Open(pmi_->dpath.c_str(), dflags, Mode, Env);
   int pageret = XrdOssOK;
   if (dataret == XrdOssOK)
   {
      if (pmi_->pages)
      {
         return XrdOssOK;
      }
     
      pageret = createPageUpdater(Oflag, Env);
      if (pageret == XrdOssOK)
      {
         return XrdOssOK;
      }

      // failed to open the datafile or create the page object.
      // close datafile if needed
      (void) successor_->Close();
   }

   mapRelease(pmi_, &lck);
   pmi_.reset();

   return (dataret != XrdOssOK) ? dataret : pageret;
}

XrdOssCsiFile::~XrdOssCsiFile()
{
   if (pmi_)
   {
      (void)Close();
   }
}

int XrdOssCsiFile::Close(long long *retsz)
{
   if (!pmi_)
   {
      return -EBADF;
   }

   // wait for any ongoing aios to finish
   aioWait();

   const int cpret = pageMapClose();

   const int csret = successor_->Close(retsz);
   if (cpret<0) return cpret;
   return csret;
}

int XrdOssCsiFile::createPageUpdater(const int Oflag, XrdOucEnv &Env)
{
   std::unique_ptr<XrdOucEnv> tagEnv = XrdOssCsi::tagOpenEnv(config_, Env);

   // get information about data file size
   off_t dsize = 0;
   if (!(Oflag & O_EXCL) && !(Oflag & O_TRUNC))
   {
      struct stat sb;
      const int sstat = successor_->Fstat(&sb);
      if (sstat<0)
      {
         return sstat;
      }
      dsize = sb.st_size;
   }

   // tag file always opened O_RDWR as the Tagstore/Pages object associated will be shared
   // between any File instances which concurrently access the file
   // (some of which may be RDWR, some RDONLY)
   int tagFlags = O_RDWR;

   // data file was truncated, do same to tag file and let it be reset
   if ((Oflag & O_TRUNC)) tagFlags |= O_TRUNC;

   // The concern with allowing creation of a new tag file is that the data file may
   // already exist. Creating a new empty tag file would usually cause subsequent access
   // errors, but not if the data file starts empty. In addition we may have been
   // configured to ignore missing tag files. Approach taken is that:
   //   If the data file creation was wanted and it is currently zero length then
   //   allow creation of tag file.
   if ((Oflag & O_CREAT) && dsize == 0)
   {
      tagFlags |= O_CREAT;
   }

   // be sure the leading directories exist for the tag file
   if ((tagFlags & O_CREAT))
   {
      int mkdret = XrdOssOK;
      {
         std::string base = pmi_->tpath;
         const size_t idx = base.rfind("/");
         base = base.substr(0,idx);
         if (!base.empty())
         {
            const int AMode = S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH; // 775
            mkdret = parentOss_->Mkdir(base.c_str(), AMode, 1, tagEnv.get());
         }
      }
      if (mkdret != XrdOssOK && mkdret != -EEXIST)
      {
         return mkdret;
      }
   }

   std::unique_ptr<XrdOssDF> integFile(parentOss_->newFile(tident));
   std::unique_ptr<XrdOssCsiTagstore> ts(new
      XrdOssCsiTagstoreFile(pmi_->dpath, std::move(integFile), tident));
   std::unique_ptr<XrdOssCsiPages> pages(new
      XrdOssCsiPages(pmi_->dpath, std::move(ts), config_.fillFileHole(), config_.allowMissingTags(),
                     config_.disablePgExtend(), config_.disableLooseWrite(), tident));

   int puret = pages->Open(pmi_->tpath.c_str(), dsize, tagFlags, *tagEnv);
   if (puret<0)
   {
      if ((puret == -EROFS || puret == -EACCES) && rdonly_)
      {
         // try to open tag file readonly
         puret = pages->Open(pmi_->tpath.c_str(), dsize, O_RDONLY, *tagEnv);
      }
   }

   if (puret<0)
   {
      return puret;
   }

   pages->BasicConsistencyCheck(successor_);
   pmi_->pages = std::move(pages);
   return XrdOssOK;
}

int XrdOssCsiFile::Open(const char *path, const int Oflag, const mode_t Mode, XrdOucEnv &Env)
{
   char cxid[4];

   if (pmi_)
   {
      // already open
      return -EINVAL;
   }

   if (!path)
   {
      return -EINVAL;
   }
   if (config_.tagParam_.isTagFile(path))
   {
      if ((Oflag & O_CREAT)) return -EACCES;
      return -ENOENT;
   }

   int dflags = Oflag;
   if ((dflags & O_ACCMODE) == O_WRONLY)
   {
      // for non-aligned writes it may be needed to do read-modify-write
      dflags &= ~O_ACCMODE;
      dflags |= O_RDWR;
   }

   rdonly_ = true;
   if ((dflags & O_ACCMODE) != O_RDONLY)
   {
      rdonly_ = false;
   }

   const int oret = pageAndFileOpen(path, dflags, Oflag, Mode, Env);
   if (oret<0)
   {
      return oret;
   }

   if (successor_->isCompressed(cxid)>0)
   {
      (void)Close();
      return -ENOTSUP;
   }

   if (Pages()->IsReadOnly() && !rdonly_)
   {
      (void)Close();
      return -EACCES;
   }
   return XrdOssOK;
}

ssize_t XrdOssCsiFile::Read(off_t offset, size_t blen)
{
   return successor_->Read(offset, blen);
}

ssize_t XrdOssCsiFile::Read(void *buff, off_t offset, size_t blen)
{
   if (!pmi_) return -EBADF;

   XrdOssCsiRangeGuard rg;
   Pages()->LockTrackinglen(rg, offset, offset+blen, true);

   const ssize_t bread = successor_->Read(buff, offset, blen);
   if (bread<0 || blen==0) return bread;

   const ssize_t puret = Pages()->VerifyRange(successor_, buff, offset, bread, rg);
   if (puret<0) return puret;
   return bread;
}

ssize_t XrdOssCsiFile::ReadRaw(void *buff, off_t offset, size_t blen)
{
   if (!pmi_) return -EBADF;

   XrdOssCsiRangeGuard rg;
   Pages()->LockTrackinglen(rg, offset, offset+blen, true);

   const ssize_t bread = successor_->ReadRaw(buff, offset, blen);
   if (bread<0 || blen==0) return bread;

   const ssize_t puret = Pages()->VerifyRange(successor_, buff, offset, bread, rg);
   if (puret<0) return puret;
   return bread;
}

ssize_t XrdOssCsiFile::ReadV(XrdOucIOVec *readV, int n)
{
   if (!pmi_) return -EBADF;
   if (n==0) return 0;

   XrdOssCsiRangeGuard rg;
   off_t start = readV[0].offset;
   off_t end = start + (off_t)readV[0].size;
   for(int i=1; i<n; i++)
   {
      const off_t p1 = readV[i].offset;
      const off_t p2 = p1 + (off_t)readV[i].size;
      if (p1<start) start = p1;
      if (p2>end) end = p2;
   }
   Pages()->LockTrackinglen(rg, start, end, true);

   // standard OSS gives -ESPIPE in case of partial read of an element
   ssize_t rret = successor_->ReadV(readV, n);
   if (rret<0) return rret;
   for (int i=0; i<n; i++)
   {
      if (readV[i].size == 0) continue;
      ssize_t puret = Pages()->VerifyRange(successor_, readV[i].data, readV[i].offset, readV[i].size, rg);
      if (puret<0) return puret;
   }
   return rret;
}

ssize_t XrdOssCsiFile::Write(const void *buff, off_t offset, size_t blen)
{
   if (!pmi_) return -EBADF;
   if (rdonly_) return -EBADF;

   XrdOssCsiRangeGuard rg;
   Pages()->LockTrackinglen(rg, offset, offset+blen, false);

   int puret = Pages()->UpdateRange(successor_, buff, offset, blen, rg);
   if (puret<0)
   {
      rg.ReleaseAll();
      resyncSizes();
      return (ssize_t)puret;
   }
   ssize_t towrite = blen;
   ssize_t bwritten = 0;
   const uint8_t *p = (uint8_t*)buff;
   while(towrite>0)
   {
      ssize_t wret = successor_->Write(&p[bwritten], offset+bwritten, towrite);
      if (wret<0)
      {
         rg.ReleaseAll();
         resyncSizes();
         return wret;
      }
      towrite -= wret;
      bwritten += wret;
   }
   return bwritten;
}

ssize_t XrdOssCsiFile::WriteV(XrdOucIOVec *writeV, int n)
{
   if (!pmi_) return -EBADF;
   if (rdonly_) return -EBADF;
   if (n==0) return 0;

   XrdOssCsiRangeGuard rg;
   off_t start = writeV[0].offset;
   off_t end = start + (off_t)writeV[0].size;
   for(int i=1; i<n; i++)
   {
      const off_t p1 = writeV[i].offset;
      const off_t p2 = p1 + (off_t)writeV[i].size;
      if (p1<start) start = p1;
      if (p2>end) end = p2;
   }
   Pages()->LockTrackinglen(rg, start, end, false);

   for (int i=0; i<n; i++)
   {
      int ret = Pages()->UpdateRange(successor_, writeV[i].data, writeV[i].offset, writeV[i].size, rg);
      if (ret<0)
      {
         rg.ReleaseAll();
         resyncSizes();
         return ret;
      }
   }
   // standard OSS gives -ESPIPE in case of partial write of an element
   ssize_t wret = successor_->WriteV(writeV, n);
   if (wret<0)
   {
      rg.ReleaseAll();
      resyncSizes();
   }
   return wret;
}

ssize_t XrdOssCsiFile::pgRead(void *buffer, off_t offset, size_t rdlen, uint32_t *csvec, uint64_t opts)
{
   if (!pmi_) return -EBADF;

   XrdOssCsiRangeGuard rg;
   Pages()->LockTrackinglen(rg, offset, offset+rdlen, true);

   // if we return a short amount of data the caller will have to deal with
   // joining csvec values from repeated reads: for simplicity try to read as
   // such as possible up to the request read length
   ssize_t toread = rdlen;
   ssize_t bread = 0;
   uint8_t *const p = (uint8_t*)buffer;
   do
   {
      ssize_t rret = successor_->Read(&p[bread], offset+bread, toread);
      if (rret<0) return rret;
      if (rret==0) break;
      toread -= rret;
      bread += rret;
   } while(toread>0);
   if (rdlen == 0) return bread;

   ssize_t puret = Pages()->FetchRange(successor_, buffer, offset, bread, csvec, opts, rg);
   if (puret<0) return puret;
   return bread;
}

ssize_t XrdOssCsiFile::pgWrite(void *buffer, off_t offset, size_t wrlen, uint32_t *csvec, uint64_t opts)
{
   if (!pmi_) return -EBADF;
   if (rdonly_) return -EBADF;
   uint64_t pgopts = opts;

   const int prec = XrdOssCsiPages::pgWritePrelockCheck(buffer, offset, wrlen, csvec, opts);
   if (prec < 0)
   {
      return prec;
   }

   XrdOssCsiRangeGuard rg;
   Pages()->LockTrackinglen(rg, offset, offset+wrlen, false);

   int puret = Pages()->StoreRange(successor_, buffer, offset, wrlen, csvec, pgopts, rg);
   if (puret<0) {
      rg.ReleaseAll();
      resyncSizes();
      return (ssize_t)puret;
   }
   ssize_t towrite = wrlen;
   ssize_t bwritten = 0;
   const uint8_t *p = (uint8_t*)buffer;
   do
   {
      ssize_t wret = successor_->Write(&p[bwritten], offset+bwritten, towrite);
      if (wret<0)
      {
         rg.ReleaseAll();
         resyncSizes();
         return wret;
      }
      towrite -= wret;
      bwritten += wret;
   } while(towrite>0);
   return bwritten;
}

int XrdOssCsiFile::Fsync()
{
   if (!pmi_) return -EBADF;

   const int psret = Pages()->Fsync();
   const int ssret = successor_->Fsync();
   if (psret<0) return psret;
   return ssret;
}

int XrdOssCsiFile::Ftruncate(unsigned long long flen)
{
   if (!pmi_) return -EBADF;
   if (rdonly_) return -EBADF;

   XrdOssCsiRangeGuard rg;
   Pages()->LockTrackinglen(rg, flen, LLONG_MAX, false);
   int ret = Pages()->truncate(successor_, flen, rg);
   if (ret<0)
   {
      rg.ReleaseAll();
      resyncSizes();
      return ret;
   }
   ret = successor_->Ftruncate(flen);
   if (ret<0)
   {
      rg.ReleaseAll();
      resyncSizes();
   }
   return ret;
}

int XrdOssCsiFile::Fstat(struct stat *buff)
{
   if (!pmi_) return -EBADF;
   XrdOssCsiPages::Sizes_t sizes;
   const int tsret = Pages()->TrackedSizesGet(sizes, false);
   const int fsret = successor_->Fstat(buff);
   if (fsret<0) return fsret;
   if (tsret<0) return 0;
   buff->st_size = std::max(sizes.first, sizes.second);
   return 0;
}

int XrdOssCsiFile::resyncSizes()
{
   XrdOssCsiRangeGuard rg;
   Pages()->LockTrackinglen(rg, 0, LLONG_MAX, false);
   struct stat sbuff;
   int ret = successor_->Fstat(&sbuff);
   if (ret<0) return ret;
   Pages()->LockResetSizes(successor_, sbuff.st_size);
   return 0;
}

void XrdOssCsiFile::Flush()
{
   if (!pmi_) return;

   Pages()->Flush();
   successor_->Flush();
}

int XrdOssCsiFile::VerificationStatus()
{
   if (!pmi_) return 0;
   return Pages()->VerificationStatus();
}
