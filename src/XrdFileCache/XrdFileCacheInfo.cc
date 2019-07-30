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
#include "XrdCks/XrdCksCalcmd5.hh"
#include "XrdOuc/XrdOucSxeq.hh"
#include "XrdSys/XrdSysTrace.hh"
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
   off_t f_off;
   XrdSysTrace *f_trace;
   const char  *m_traceID;
   std::string f_ttext;

   XrdSysTrace* GetTrace() const { return f_trace; }

   FpHelper(XrdOssDF* fp, off_t off,
            XrdSysTrace *trace, const char *tid, const std::string &ttext) :
      f_fp(fp), f_off(off),
      f_trace(trace), m_traceID(tid), f_ttext(ttext)
   {}

   // Returns true on error
   bool ReadRaw(void *buf, ssize_t size, bool warnp = true)
   {
      ssize_t ret = f_fp->Read(buf, f_off, size);
      if (ret != size)
      {
         if (warnp)
         {
            TRACE(Warning, f_ttext << " off=" << f_off << " size=" << size
                                   << " ret=" << ret << " error=" << ((ret < 0) ? strerror(-ret) : "<no error>"));
         }
         return true;
      }
      f_off += ret;
      return false;
   }

   template<typename T> bool Read(T &loc, bool warnp = true)
   {
      return ReadRaw(&loc, sizeof(T), warnp);
   }

   // Returns true on error
   bool WriteRaw(void *buf, ssize_t size)
   {
      ssize_t ret = f_fp->Write(buf, f_off, size);
      if (ret != size)
      {
         TRACE(Warning, f_ttext << " off=" << f_off << " size=" << size
                                << " ret=" << ret << " error=" << ((ret < 0) ? strerror(ret) : "<no error>"));
         return true;
      }
      f_off += ret;
      return false;
   }

   template<typename T> bool Write(T &loc)
   {
      return WriteRaw(&loc, sizeof(T));
   }
};
}

using namespace XrdFileCache;

const char*  Info::m_infoExtension  = ".cinfo";
const char*  Info::m_traceID        = "Cinfo";
const int    Info::m_defaultVersion = 2;
const size_t Info::m_maxNumAccess   = 20;

//------------------------------------------------------------------------------

Info::Info(XrdSysTrace* trace, bool prefetchBuffer) :
   m_trace(trace),
   m_hasPrefetchBuffer(prefetchBuffer),
   m_buff_written(0),  m_buff_prefetch(0),
   m_sizeInBits(0),
   m_complete(false),
   m_cksCalc(0)
{}

Info::~Info()
{
   if (m_store.m_buff_synced) free(m_store.m_buff_synced);
   if (m_buff_written) free(m_buff_written);
   if (m_buff_prefetch) free(m_buff_prefetch);
   delete m_cksCalc;
}

//------------------------------------------------------------------------------

void Info::SetAllBitsSynced()
{
   // The following should be:
   //   memset(m_store.m_buff_synced, 255, GetSizeInBytes());
   // but GCC produces an overzealous 'possible argument transpose warning' and
   // xrootd build uses warnings->errors escalation.
   // This workaround can be removed for gcc >= 5.
   // See also: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61294
   const int nb = GetSizeInBytes();
   for (int i = 0; i < nb; ++i)
      m_store.m_buff_synced[i] = 255;

   m_complete = true;
}

//------------------------------------------------------------------------------

void Info::SetBufferSize(long long bs)
{
   // Needed only info is created first time in File::Open()
   m_store.m_bufferSize = bs;
}

//------------------------------------------------------------------------------s

void Info::SetFileSize(long long fs)
{
   m_store.m_fileSize = fs;
   ResizeBits((m_store.m_fileSize - 1) / m_store.m_bufferSize + 1);
   m_store.m_creationTime = time(0);
}

//------------------------------------------------------------------------------

void Info::ResizeBits(int s)
{
   // drop buffer in case of failed/partial reads

   if (m_store.m_buff_synced) free(m_store.m_buff_synced);
   if (m_buff_written)        free(m_buff_written);
   if (m_buff_prefetch)       free(m_buff_prefetch);

   m_sizeInBits = s;
   m_buff_written        = (unsigned char*) malloc(GetSizeInBytes());
   m_store.m_buff_synced = (unsigned char*) malloc(GetSizeInBytes());
   memset(m_buff_written,        0, GetSizeInBytes());
   memset(m_store.m_buff_synced, 0, GetSizeInBytes());

   if (m_hasPrefetchBuffer)
   {
      m_buff_prefetch = (unsigned char*) malloc(GetSizeInBytes());
      memset(m_buff_prefetch, 0, GetSizeInBytes());
   }
   else
   {
      m_buff_prefetch = 0;
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

   if (r.Read(m_store.m_version)) return false;

   if (m_store.m_version == 0)
   {
      TRACE(Warning, trace_pfx << " File version 0 non supported");
      return false;
   }
   else if (abs(m_store.m_version) == 1)
      return ReadV1(fp, fname);

   if (r.Read(m_store.m_bufferSize)) return false;

   long long fs;
   if (r.Read(fs)) return false;
   SetFileSize(fs);

   if (r.ReadRaw(m_store.m_buff_synced, GetSizeInBytes())) return false;
   memcpy(m_buff_written, m_store.m_buff_synced, GetSizeInBytes());


   if (r.ReadRaw(m_store.m_cksum, 16)) return false;
   char tmpCksum[16];
   GetCksum(&m_store.m_buff_synced[0], &tmpCksum[0]);

   /*
      // debug print cksum
      for (int i =0; i < 16; ++i)
      printf("%x", tmpCksum[i] & 0xff);

      for (int i =0; i < 16; ++i)
      printf("%x", m_store.m_cksum[i] & 0xff);
    */
   if (memcmp(m_store.m_cksum, &tmpCksum[0], 16))
   {
      TRACE(Error, trace_pfx << " buffer cksum and saved cksum don't match \n");
      return false;
   }

   // cache complete status
   m_complete = ! IsAnythingEmptyInRng(0, m_sizeInBits);

   // read creation time
   if (r.Read(m_store.m_creationTime)) return false;

   // get number of accessess
   if (r.Read(m_store.m_accessCnt, false)) m_store.m_accessCnt = 0;  // was: return false;
   TRACE(Dump, trace_pfx << " complete "<< m_complete << " access_cnt " << m_store.m_accessCnt);

   // read access statistics
   int vs = m_store.m_accessCnt < m_maxNumAccess ? m_store.m_accessCnt : m_maxNumAccess;
   m_store.m_astats.resize(vs);
   for (std::vector<AStat>::iterator it = m_store.m_astats.begin(); it != m_store.m_astats.end(); ++it)
   {
      if (r.Read(*it, sizeof(AStat))) return false;
   }

   return true;
}

bool Info::ReadV1(XrdOssDF* fp, const std::string &fname)
{
   struct AStatV1 {
      time_t DetachTime;       //! close time
      long long BytesDisk;     //! read from disk
      long long BytesRam;      //! read from ram
      long long BytesMissed;   //! read remote client
   };

   std::string trace_pfx("Info:::ReadV1() ");
   trace_pfx += fname + " ";

   FpHelper r(fp, 0, m_trace, m_traceID, trace_pfx + "oss read failed");



   if (r.Read(m_store.m_version)) return false;
   if (r.Read(m_store.m_bufferSize)) return false;

   long long fs;
   if (r.Read(fs)) return false;
   SetFileSize(fs);

   if (r.ReadRaw(m_store.m_buff_synced, GetSizeInBytes())) return false;
   memcpy(m_buff_written, m_store.m_buff_synced, GetSizeInBytes());


   m_complete = ! IsAnythingEmptyInRng(0, m_sizeInBits);
   if (r.ReadRaw(&m_store.m_accessCnt, sizeof(int), false)) m_store.m_accessCnt = 0;  // was: return false;
   TRACE(Dump, trace_pfx << " complete "<< m_complete << " access_cnt " << m_store.m_accessCnt);


   size_t startFillIdx = m_store.m_accessCnt < m_maxNumAccess ? 0 : m_store.m_accessCnt - m_maxNumAccess;
   AStatV1 av1;
   for (size_t i = 0; i < m_store.m_accessCnt; ++i)
   {
      if (r.ReadRaw(&av1, sizeof(AStatV1))) return false;

      if (i >= startFillIdx)
      {
         AStat av2;
         av2.AttachTime  =  av1.DetachTime;
         av2.DetachTime  =  av1.DetachTime;
         av2.BytesDisk   =  av1.BytesDisk;
         av2.BytesRam    =  av1.BytesRam;
         av2.BytesMissed =  av1.BytesMissed;

         m_store.m_astats.push_back(av2);
      }

      if (i == 0) m_store.m_creationTime = av1.DetachTime;
   }

   return true;
}

//------------------------------------------------------------------------------
void Info::GetCksum( unsigned char* buff, char* digest)
{
   if (m_cksCalc)
      m_cksCalc->Init();
   else
      m_cksCalc = new XrdCksCalcmd5();

   m_cksCalc->Update((const char*)buff, GetSizeInBytes());
   memcpy(digest, m_cksCalc->Final(), 16);
}

//------------------------------------------------------------------------------
void Info::DisableDownloadStatus()
{
   // use version sign to skip download status
   m_store.m_version = -m_store.m_version;
}
//------------------------------------------------------------------------------

bool Info::Write(XrdOssDF* fp, const std::string &fname)
{
   std::string trace_pfx("Info:::Write() ");
   trace_pfx += fname + " ";

   int rc;
   if ((rc = XrdOucSxeq::Serialize(fp->getFD(), XrdOucSxeq::noWait)))
   {
      TRACE(Error, trace_pfx << " lock failed " << strerror(rc));
      return false;
   }

   FpHelper w(fp, 0, m_trace, m_traceID, trace_pfx + "oss write failed");

   m_store.m_version = m_defaultVersion;
   if (w.Write(m_store.m_version))    return false;
   if (w.Write(m_store.m_bufferSize)) return false;
   if (w.Write(m_store.m_fileSize))   return false;

   if (w.WriteRaw(m_store.m_buff_synced, GetSizeInBytes())) return false;

   GetCksum(&m_store.m_buff_synced[0], &m_store.m_cksum[0]);
   if (w.Write(m_store.m_cksum)) return false;

   if (w.Write(m_store.m_creationTime)) return false;

   if (w.Write(m_store.m_accessCnt)) return false;
   for (std::vector<AStat>::iterator it = m_store.m_astats.begin(); it != m_store.m_astats.end(); ++it)
   {
      if (w.WriteRaw(&(*it), sizeof(AStat))) return false;
   }

   // Can this really fail?
   if (XrdOucSxeq::Release(fp->getFD()))
   {
      TRACE(Error, trace_pfx << "un-lock failed");
   }

   return true;
}

//------------------------------------------------------------------------------

void Info::ResetAllAccessStats()
{
   m_store.m_accessCnt = 0;
   m_store.m_astats.clear();
}

void Info::WriteIOStatAttach()
{
   m_store.m_accessCnt++;
   if (m_store.m_astats.size() >= m_maxNumAccess)
      m_store.m_astats.erase(m_store.m_astats.begin());

   AStat as;
   as.AttachTime = time(0);
   m_store.m_astats.push_back(as);
}

void Info::WriteIOStat(Stats& s)
{
   m_store.m_astats.back().BytesDisk   = s.m_BytesDisk;
   m_store.m_astats.back().BytesRam    = s.m_BytesRam;
   m_store.m_astats.back().BytesMissed = s.m_BytesMissed;
}

void Info::WriteIOStatDetach(Stats& s)
{
   m_store.m_astats.back().DetachTime  = time(0);
   m_store.m_astats.back().BytesDisk   = s.m_BytesDisk;
   m_store.m_astats.back().BytesRam    = s.m_BytesRam;
   m_store.m_astats.back().BytesMissed = s.m_BytesMissed;
}

void Info::WriteIOStatSingle(long long bytes_disk)
{
   m_store.m_accessCnt++;
   if (m_store.m_astats.size() >= m_maxNumAccess)
      m_store.m_astats.erase(m_store.m_astats.begin());

   AStat as;
   as.AttachTime = as.DetachTime = time(0);
   as.BytesDisk  = bytes_disk;
   m_store.m_astats.push_back(as);
}

void Info::WriteIOStatSingle(long long bytes_disk, time_t att, time_t dtc)
{
   m_store.m_accessCnt++;
   if (m_store.m_astats.size() >= m_maxNumAccess)
      m_store.m_astats.erase(m_store.m_astats.begin());

   AStat as;
   as.AttachTime = att;
   as.DetachTime = dtc;
   as.BytesDisk  = bytes_disk;
   m_store.m_astats.push_back(as);
}

//------------------------------------------------------------------------------

bool Info::GetLatestDetachTime(time_t& t) const
{
   if (! m_store.m_accessCnt) return false;

   size_t entry = std::min(m_store.m_accessCnt, m_maxNumAccess) - 1;
   t =  m_store.m_astats[entry].DetachTime;
   return true;
}
