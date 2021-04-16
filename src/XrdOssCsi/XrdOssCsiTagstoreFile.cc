/******************************************************************************/
/*                                                                            */
/*              X r d O s s C s i T a g s t o r e F i l e . c c               */
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

#include "XrdOssCsiTrace.hh"
#include "XrdOssCsiTagstoreFile.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

extern XrdOucTrace  OssCsiTrace;

int XrdOssCsiTagstoreFile::Open(const char *path, const off_t dsize, const int Oflag, XrdOucEnv &Env)
{
   EPNAME("TagstoreFile::Open");

   const int ret = fd_->Open(path, Oflag, 0666, Env);
   if (ret<0)
   {
      return ret;
   }
   isOpen = true;

   struct guard_s
   {
      guard_s(XrdOssDF *fd, bool &isopen) : fd_(fd), isopen_(isopen) { }
      ~guard_s() { if (fd_) { fd_->Close(); isopen_ = false; } }
      void disarm() { fd_ = NULL; }

      XrdOssDF *fd_;
      bool &isopen_;
   } fdguard(fd_.get(),isOpen);

   const static int little = 1;
   if (*(char const *)&little)
   {
      machineIsBige_ = false;
   }
   else
   {
      machineIsBige_ = true;
   }

   uint32_t magic;

   const ssize_t mread = XrdOssCsiTagstoreFile::fullread(*fd_, header_, 0, 20);
   bool mok = false;
   if (mread >= 0)
   {
      memcpy(&magic, header_, 4);
      if (magic == cmagic_)
      {
         fileIsBige_ = machineIsBige_;
         mok = true;
      }
      else if (magic == bswap_32(cmagic_))
      {
         fileIsBige_ = !machineIsBige_;
         mok = true;
      }
   }

   if (!mok)
   {
      fileIsBige_ = machineIsBige_;
      hflags_ = (dsize==0) ? XrdOssCsiTagstore::csVer : 0;
      trackinglen_ = 0;
      const int stret = MarshallAndWriteHeader();
      if (stret<0) return stret;
   }
   else
   {
      memcpy(&trackinglen_, &header_[4], 8);
      if (fileIsBige_ != machineIsBige_)
      {
         trackinglen_ = bswap_64(trackinglen_);
      }
      memcpy(&hflags_,&header_[12], 4);
      if (fileIsBige_ != machineIsBige_)
      {
         hflags_ = bswap_32(hflags_);
      }
      const uint32_t cv = XrdOucCRC::Calc32C(header_, 16, 0U);
      uint32_t rv;
      memcpy(&rv, &header_[16], 4);
      if (fileIsBige_ != machineIsBige_) rv = bswap_32(rv);
      if (rv != cv)
      {
         return -EDOM;
      }
   }

   if (trackinglen_ != dsize)
   {
      TRACE(Warn, "Tagfile disagrees with actual filelength for " << fn_ << " expected " << trackinglen_ << " actual " << dsize);
   }

   const int rsret = ResetSizes(dsize);
   if (rsret<0) return rsret;

   fdguard.disarm();
   return 0;
}

int XrdOssCsiTagstoreFile::ResetSizes(const off_t size)
{
   EPNAME("ResetSizes");
   if (!isOpen) return -EBADF;
   actualsize_ = size;
   struct stat sb;
   const int ssret = fd_->Fstat(&sb);
   if (ssret<0) return ssret;
   const off_t expected_tagfile_size = 20LL + 4*((trackinglen_+XrdSys::PageSize-1)/XrdSys::PageSize);
   // truncate can be relatively slow
   if (expected_tagfile_size < sb.st_size)
   {
      TRACE(Warn, "Truncating tagfile to " << expected_tagfile_size <<
         ", from current size " << sb.st_size << " for " << fn_);
      const int tret = fd_->Ftruncate(expected_tagfile_size);
      if (tret<0) return tret;
   }
   else if (expected_tagfile_size > sb.st_size)
   {
      off_t nb = 0;
      if (sb.st_size>20) nb = (sb.st_size - 20)/4;
      TRACE(Warn, "Reducing tracked size to " << (nb*XrdSys::PageSize) <<
         " instead of " << trackinglen_ << ", because of short tagfile for " << fn_);
      const int stret = WriteTrackedTagSize(nb*XrdSys::PageSize);
      if (stret<0) return stret;
      const int tret = fd_->Ftruncate(20LL + 4*nb);
      if (tret<0) return tret;
   }
   return 0;
}

int XrdOssCsiTagstoreFile::Fsync()
{
   if (!isOpen) return -EBADF;
   return fd_->Fsync();
}

void XrdOssCsiTagstoreFile::Flush()
{
   if (!isOpen) return;
   fd_->Flush();
}

int XrdOssCsiTagstoreFile::Close()
{
   if (!isOpen) return -EBADF;
   isOpen = false;
   return fd_->Close();
}

ssize_t XrdOssCsiTagstoreFile::WriteTags(const uint32_t *const buf, const off_t off, const size_t n)
{
   if (!isOpen) return -EBADF;
   if (machineIsBige_ != fileIsBige_) return WriteTags_swap(buf, off, n);

   const ssize_t nwritten = XrdOssCsiTagstoreFile::fullwrite(*fd_, buf, 20LL+4*off, 4*n);
   if (nwritten<0) return nwritten;
   return nwritten/4;
}

ssize_t XrdOssCsiTagstoreFile::ReadTags(uint32_t *const buf, const off_t off, const size_t n)
{
   if (!isOpen) return -EBADF;
   if (machineIsBige_ != fileIsBige_) return ReadTags_swap(buf, off, n);

   const ssize_t nread = XrdOssCsiTagstoreFile::fullread(*fd_, buf, 20LL+4*off, 4*n);
   if (nread<0) return nread;
   return nread/4;
}

int XrdOssCsiTagstoreFile::Truncate(const off_t size, bool datatoo)
{
   if (!isOpen)
   {
      return -EBADF;
   }

   // set tag file to correct length for value of size
   const off_t expected_tagfile_size = 20LL + 4*((size+XrdSys::PageSize-1)/XrdSys::PageSize);
   const int tret = fd_->Ftruncate(expected_tagfile_size);

   // if failed to set the tagfile length return error before updating header
   if (tret != XrdOssOK) return tret;

   // truncating down to zero, so reset to content verified
   if (datatoo && size==0) hflags_ |= XrdOssCsiTagstore::csVer;

   int wtt = WriteTrackedTagSize(size);
   if (wtt<0) return wtt;

   if (datatoo) actualsize_ = size;
   return 0;
}

ssize_t XrdOssCsiTagstoreFile::WriteTags_swap(const uint32_t *const buf, const off_t off, const size_t n)
{
   uint32_t b[1024];
   const size_t bsz = sizeof(b)/sizeof(uint32_t);
   size_t towrite = n;
   size_t nwritten = 0;
   while(towrite>0)
   {
      const size_t bs = std::min(towrite, bsz);
      for(size_t i=0;i<bs;i++)
      {
         b[i] = bswap_32(buf[i+nwritten]);
      }
      const ssize_t wret = XrdOssCsiTagstoreFile::fullwrite(*fd_, b, 20LL+4*(nwritten+off), 4*bs);
      if (wret<0) return wret;
      towrite -= wret/4;
      nwritten += wret/4;
   }
   return n;
}

ssize_t XrdOssCsiTagstoreFile::ReadTags_swap(uint32_t *const buf, const off_t off, const size_t n)
{
   uint32_t b[1024];
   const size_t bsz = sizeof(b)/sizeof(uint32_t);
   size_t toread = n;
   size_t nread = 0;
   while(toread>0)
   {
      const size_t bs = std::min(toread, bsz);
      const ssize_t rret = XrdOssCsiTagstoreFile::fullread(*fd_, b, 20LL+4*(nread+off), 4*bs);
      if (rret<0) return rret;
      for(size_t i=0;i<bs;i++)
      {
         buf[i+nread] = bswap_32(b[i]);
      }
      toread -= rret/4;
      nread += rret/4;
   }
   return n;
}
