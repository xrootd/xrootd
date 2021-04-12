#ifndef _XRDOSSCSITAGSTOREFILE_H
#define _XRDOSSCSITAGSTOREFILE_H
/******************************************************************************/
/*                                                                            */
/*              X r d O s s C s i T a g s t o r e F i l e . h h               */
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

#include "XrdOss/XrdOss.hh"
#include "XrdOssCsiTagstore.hh"
#include "XrdOuc/XrdOucCRC.hh"

#include <memory>
#include <mutex>
#include <byteswap.h>

class XrdOssCsiTagstoreFile : public XrdOssCsiTagstore
{
public:
   XrdOssCsiTagstoreFile(const std::string &fn, std::unique_ptr<XrdOssDF> fd, const char *tid) : fn_(fn), fd_(std::move(fd)), trackinglen_(0), isOpen(false), tident_(tid), tident(tident_.c_str()) { }
   virtual ~XrdOssCsiTagstoreFile() { if (isOpen) { (void)Close(); } }

   virtual int Open(const char *, off_t, int, XrdOucEnv &) /* override */;
   virtual int Close() /* override */;

   virtual void Flush() /* override */;
   virtual int Fsync() /* override */;

   virtual ssize_t WriteTags(const uint32_t *, off_t, size_t) /* override */;
   virtual ssize_t ReadTags(uint32_t *, off_t, size_t) /* override */;

   virtual int Truncate(off_t, bool) /* override */;

   virtual off_t GetTrackedTagSize() const /* override */
   {
      if (!isOpen) return 0;
      return trackinglen_;
   }

   virtual off_t GetTrackedDataSize() const /* override */
   {
      if (!isOpen) return 0;
      return actualsize_;
   }

   virtual int ResetSizes(const off_t size) /* override */;

   virtual int SetTrackedSize(const off_t size) /* override */
   {
      if (!isOpen) return -EBADF;
      if (size > actualsize_)
      {
         actualsize_ = size;
      }
      if (size != trackinglen_)
      {
         const int wtt = WriteTrackedTagSize(size);
         if (wtt<0) return wtt;
      }
      return 0;
   }

   virtual bool IsVerified() const /* override */
   {
      if (!isOpen) return false;
     if ((hflags_ & XrdOssCsiTagstore::csVer)) return true;
     return false;
   }

   virtual int SetUnverified()
   {
      if (!isOpen) return -EBADF;
     if ((hflags_ & XrdOssCsiTagstore::csVer))
     {
       hflags_ &= ~XrdOssCsiTagstore::csVer;
       return MarshallAndWriteHeader();
     }
     return 0;
   }

   static ssize_t fullread(XrdOssDF &fd, void *buff, const off_t off , const size_t sz)
   {
      size_t toread = sz, nread = 0;
      uint8_t *p = (uint8_t*)buff;
      while(toread>0)
      {
         const ssize_t rret = fd.Read(&p[nread], off+nread, toread);
         if (rret<0) return rret;
         if (rret==0) break;
         toread -= rret;
         nread += rret;
      }
      if (nread != sz) return -EDOM;
      return nread;
   }

   static ssize_t fullwrite(XrdOssDF &fd, const void *buff, const off_t off , const size_t sz)
   {
      size_t towrite = sz, nwritten = 0;
      const uint8_t *p = (const uint8_t*)buff;
      while(towrite>0)
      {
         const ssize_t wret = fd.Write(&p[nwritten], off+nwritten, towrite);
         if (wret<0) return wret;
         towrite -= wret;
         nwritten += wret;
      }
      return nwritten;
   }

private:
   const std::string fn_;
   std::unique_ptr<XrdOssDF> fd_;
   off_t trackinglen_;
   off_t actualsize_;
   bool isOpen;
   const std::string tident_;
   const char *tident;
   bool machineIsBige_;
   bool fileIsBige_;
   uint8_t header_[20];
   uint32_t hflags_;

   ssize_t WriteTags_swap(const uint32_t *, off_t, size_t);
   ssize_t ReadTags_swap(uint32_t *, off_t, size_t);

   int WriteTrackedTagSize(const off_t size)
   {
      if (!isOpen) return -EBADF;
      trackinglen_ = size;
      return MarshallAndWriteHeader();
   }

   int MarshallAndWriteHeader()
   {
      if (!isOpen) return -EBADF;

      uint32_t y = cmagic_;
      if (fileIsBige_ != machineIsBige_) y = bswap_32(y);
      memcpy(header_, &y, 4);

      uint64_t x = trackinglen_;
      if (fileIsBige_ != machineIsBige_) x = bswap_64(x);
      memcpy(&header_[4], &x, 8);

      y = hflags_;
      if (fileIsBige_ != machineIsBige_) y = bswap_32(y);
      memcpy(&header_[12], &y, 4);

      uint32_t cv = XrdOucCRC::Calc32C(header_, 16, 0U);
      if (machineIsBige_ != fileIsBige_) cv = bswap_32(cv);
      memcpy(&header_[16], &cv, 4);

      ssize_t wret = fullwrite(*fd_, header_, 0, 20);
      if (wret<0) return wret;
      return 0;
   }

   static const uint32_t cmagic_ = 0x30544452U;
};

#endif
