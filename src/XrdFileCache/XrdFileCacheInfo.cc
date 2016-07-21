//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman
//----------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------------------

#include <sys/file.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucSxeq.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdFileCacheInfo.hh"
#include "XrdFileCache.hh"
#include "XrdFileCacheStats.hh"
#include "XrdFileCacheTrace.hh"

namespace
{
   struct FpHelper
   {
      XrdOssDF    *f_fp;
      off_t        f_off;
      XrdOucTrace *f_trace;
      const char  *m_traceID;
      std::string  f_ttext;

      XrdOucTrace* GetTrace() const { return f_trace; }

      FpHelper(XrdOssDF* fp, off_t off,
               XrdOucTrace *trace, const char *tid, const std::string &ttext) :
         f_fp(fp), f_off(off),
         f_trace(trace), m_traceID(tid), f_ttext(ttext)
      {}

      // Returns true on error
      bool Read(void *buf, ssize_t size)
      {
         ssize_t ret = f_fp->Read(buf, f_off, size);
         if (ret != size)
         {
            TRACE(Warning, f_ttext << " off=" << f_off << " size=" << size
                  << " ret=" << ret << " error=" << ((ret < 0) ? strerror(errno) : "<no error>"));
            return true;
         }
         f_off += ret;
         return false;
      }

      template<typename T> bool Read(T &loc)
      {
         return Read(&loc, sizeof(T));
      }

      // Returns true on error
      bool Write(void *buf, ssize_t size)
      {
         ssize_t ret = f_fp->Write(buf, f_off, size);
         if (ret != size)
         {
            TRACE(Warning, f_ttext << " off=" << f_off << " size=" << size
                  << " ret=" << ret << " error=" << ((ret < 0) ? strerror(errno) : "<no error>"));
            return true;
         }
         f_off += ret;
         return false;
      }

      template<typename T> bool Write(T &loc)
      {
         return Write(&loc, sizeof(T));
      }
   };
}

using namespace XrdFileCache;

const char* Info::m_infoExtension = ".cinfo";
const char* Info::m_traceID       = "Cinfo";

//------------------------------------------------------------------------------

Info::Info(XrdOucTrace* trace, bool prefetchBuffer) :
   m_trace(trace),
   m_version(1),
   m_bufferSize(-1),
   m_hasPrefetchBuffer(prefetchBuffer),
   m_fileSize(0),
   m_sizeInBits(0),
   m_buff_fetched(0), m_buff_write_called(0), m_buff_prefetch(0),
   m_accessCnt(0),
   m_complete(false)
{}

Info::~Info()
{
   if (m_buff_fetched)      free(m_buff_fetched);
   if (m_buff_write_called) free(m_buff_write_called);
   if (m_buff_prefetch)     free(m_buff_prefetch);
}

//------------------------------------------------------------------------------

void Info::SetBufferSize(long long bs)
{
   // Needed only info is created first time in File::Open()
   m_bufferSize = bs;
}

//------------------------------------------------------------------------------s

void Info::SetFileSize(long long fs)
{
   m_fileSize = fs;
   if (m_version >= 0)
      ResizeBits((m_fileSize - 1)/m_bufferSize + 1) ;
}

//------------------------------------------------------------------------------

void Info::ResizeBits(int s)
{
    // drop buffer in case of failed/partial reads
   if (m_buff_fetched)      free(m_buff_fetched);
   if (m_buff_write_called) free(m_buff_write_called);
   if (m_buff_prefetch)     free(m_buff_prefetch);

   m_sizeInBits = s;
   m_buff_fetched      = (unsigned char*) malloc(GetSizeInBytes());
   m_buff_write_called = (unsigned char*) malloc(GetSizeInBytes());
   memset(m_buff_fetched,      0, GetSizeInBytes());
   memset(m_buff_write_called, 0, GetSizeInBytes());

   if (m_hasPrefetchBuffer)
   {
      m_buff_prefetch = (unsigned char*) malloc(GetSizeInBytes());
      memset(m_buff_prefetch, 0, GetSizeInBytes());
   }
}

//------------------------------------------------------------------------------

bool Info::Read(XrdOssDF* fp, const std::string &fname)
{
   // does not need lock, called only in File::Open
   // before File::Run() starts

   std::string trace_pfx("Info:::Read() ");
   trace_pfx += fname + " ";

   FpHelper r(fp, 0, m_trace, m_traceID, trace_pfx + "oss read failed");

   int version;
   if (r.Read(version)) return false;
   if (abs(version) != abs(m_version))
   {
      TRACE(Warning, trace_pfx << " incompatible file version " << version);
      return false;
   }
   m_version = version;

   if (r.Read(m_bufferSize)) return false;

   long long fs;
   if (r.Read(fs)) return false;
   SetFileSize(fs);

   if (m_version > 0) 
   {
      if (r.Read(m_buff_fetched, GetSizeInBytes())) return false;
      memcpy(m_buff_write_called, m_buff_fetched, GetSizeInBytes());
   }

   m_complete = ! IsAnythingEmptyInRng(0, m_sizeInBits);

   if (r.Read(m_accessCnt)) m_accessCnt = 0; // was: return false;
   TRACE(Dump, trace_pfx << " complete "<< m_complete << " access_cnt " << m_accessCnt);

   return true;
}

//------------------------------------------------------------------------------
void Info::DisableDownloadStatus()
{
    // use version sign to skip downlaod status
    m_version = -m_version;
}

int Info::GetHeaderSize() const
{
   // version + buffersize + file-size + download-status-array
   return sizeof(int) + sizeof(long long) + sizeof(long long) + GetSizeInBytes();
}

//------------------------------------------------------------------------------

bool Info::WriteHeader(XrdOssDF* fp, const std::string &fname)
{
   std::string trace_pfx("Info:::WriteHeader() ");
   trace_pfx += fname + " ";

   if (XrdOucSxeq::Serialize(fp->getFD(), XrdOucSxeq::noWait))
   {
      TRACE(Error, trace_pfx << " lock failed " << strerror(errno));
      return false;
   }

   FpHelper w(fp, 0, m_trace, m_traceID, trace_pfx + "oss write failed");

   if (w.Write(m_version))    return false;
   if (w.Write(m_bufferSize)) return false;
   if (w.Write(m_fileSize))   return false;

   if ( m_version >= 0 )
   {
      if (w.Write(m_buff_write_called, GetSizeInBytes())) 
          return false;
   }

   // Can this really fail?
   if (XrdOucSxeq::Release(fp->getFD()))
   {
      TRACE(Error, trace_pfx << "un-lock failed");
   }

   return true;
}

//------------------------------------------------------------------------------

bool Info::AppendIOStat(AStat& as, XrdOssDF* fp, const std::string &fname)
{
   std::string trace_pfx("Info:::AppendIOStat() ");
   trace_pfx += fname + " ";

   TRACE(Dump, trace_pfx);

   if (XrdOucSxeq::Serialize(fp->getFD(), 0))
   {
      TRACE(Error, trace_pfx << "lock failed");
      return false;
   }

   m_accessCnt++;

   FpHelper w(fp, GetHeaderSize(), m_trace, m_traceID, trace_pfx + "oss write failed");

   if (w.Write(m_accessCnt)) return false;
   w.f_off += (m_accessCnt-1)*sizeof(AStat);
 
   if (w.Write(as)) return false;

   if (XrdOucSxeq::Release(fp->getFD()))
   {
      TRACE(Error, trace_pfx << "un-lock failed");
   }

   return true;
}

//------------------------------------------------------------------------------

bool Info::GetLatestDetachTime(time_t& t, XrdOssDF* fp) const
{
   bool res = false;

   int flr = XrdOucSxeq::Serialize(fp->getFD(), XrdOucSxeq::Share);
   if (flr)
   {
       TRACE(Error, "Info::GetLatestAttachTime() lock failed");
       return false;
   }
   if (m_accessCnt)
   {
      AStat     stat;
      long long off      = GetHeaderSize() + sizeof(int) + (m_accessCnt-1)*sizeof(AStat);
      ssize_t   read_res = fp->Read(&stat, off, sizeof(AStat));
      if (read_res == sizeof(AStat))
      {
         t = stat.DetachTime;
         res = true;
      }
      else
      {
         TRACE(Error, " Info::GetLatestAttachTime() can't get latest access stat ");
         return false;
      }
   }

   flr = XrdOucSxeq::Release(fp->getFD());
   if (flr) TRACE(Error, "Info::GetLatestAttachTime() lock failed");

   return res;
}
