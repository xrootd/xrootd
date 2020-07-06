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
#include "XrdSys/XrdSysTrace.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdPfcInfo.hh"
#include "XrdPfc.hh"
#include "XrdPfcStats.hh"
#include "XrdPfcTrace.hh"

namespace
{
struct FpHelper
{
   XrdOssDF    *f_fp;
   off_t        f_off;
   XrdSysTrace *f_trace;
   const char  *m_traceID;
   std::string  f_ttext;

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
                                   << " ret=" << ret << " error=" << ((ret < 0) ? XrdSysE2T(-ret) : "<no error>"));
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
                                << " ret=" << ret << " error=" << ((ret < 0) ? XrdSysE2T(ret) : "<no error>"));
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

using namespace XrdPfc;

const char*  Info::m_traceID        = "CInfo";
const char*  Info::s_infoExtension  = ".cinfo";
const int    Info::s_defaultVersion = 3;
      size_t Info::s_maxNumAccess   = 20; // default, can be changed through configuration

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
   m_store.m_buffer_size = bs;
}

//------------------------------------------------------------------------------s

void Info::SetFileSize(long long fs)
{
   m_store.m_file_size = fs;
   ResizeBits((m_store.m_file_size - 1) / m_store.m_buffer_size + 1);
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

   if (m_store.m_version != s_defaultVersion)
   {
      if (abs(m_store.m_version) == 1)
      {
         return ReadV1(fp, fname);
      }
      else if (m_store.m_version == 2)
      {
         return ReadV2(fp, fname);
      }
      else
      {
         TRACE(Warning, trace_pfx << " File version " << m_store.m_version << " not supported.");
         return false;
      }
   }

   if (r.Read(m_store.m_buffer_size)) return false;

   long long fs;
   if (r.Read(fs)) return false;
   SetFileSize(fs);

   if (r.ReadRaw(m_store.m_buff_synced, GetSizeInBytes())) return false;
   memcpy(m_buff_written, m_store.m_buff_synced, GetSizeInBytes());

   if (r.ReadRaw(m_store.m_cksum, 16)) return false;
   char tmpCksum[16];
   GetCksum(&m_store.m_buff_synced[0], &tmpCksum[0]);

   // Debug print cksum:
   // for (int i =0; i < 16; ++i)
   //    printf("%x", tmpCksum[i] & 0xff);
   // for (int i =0; i < 16; ++i)
   //    printf("%x", m_store.m_cksum[i] & 0xff);

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
   m_store.m_astats.reserve(std::min(m_store.m_accessCnt, s_maxNumAccess));
   AStat as;
   while ( ! r.Read(as, false))
   {
      m_store.m_astats.emplace_back(as);
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

   if (m_store.m_astats.size() > s_maxNumAccess) CompactifyAccessRecords();

   FpHelper w(fp, 0, m_trace, m_traceID, trace_pfx + "oss write failed");

   m_store.m_version = s_defaultVersion;
   if (w.Write(m_store.m_version))    return false;
   if (w.Write(m_store.m_buffer_size)) return false;
   if (w.Write(m_store.m_file_size))   return false;

   if (w.WriteRaw(m_store.m_buff_synced, GetSizeInBytes())) return false;

   GetCksum(&m_store.m_buff_synced[0], &m_store.m_cksum[0]);
   if (w.Write(m_store.m_cksum)) return false;

   if (w.Write(m_store.m_creationTime)) return false;

   if (w.Write(m_store.m_accessCnt)) return false;
   for (std::vector<AStat>::iterator it = m_store.m_astats.begin(); it != m_store.m_astats.end(); ++it)
   {
      if (w.Write(*it)) return false;
   }

   return true;
}

//------------------------------------------------------------------------------

void Info::ResetAllAccessStats()
{
   m_store.m_accessCnt = 0;
   m_store.m_astats.clear();
}

void Info::AStat::MergeWith(const Info::AStat &b)
{
   // Access in b assumed to happen after the one in this.

   DetachTime     = b.DetachTime;
   NumIos        += b.NumIos;
   Duration      += b.Duration;
   NumMerged     += b.NumMerged + 1;
   BytesHit      += b.BytesHit;
   BytesMissed   += b.BytesMissed;
   BytesBypassed += b.BytesBypassed;
}

void Info::CompactifyAccessRecords()
{
   time_t now = time(0);

   std::vector<AStat> &v = m_store.m_astats;

   for (int i = 0; i < (int) v.size() - 1; ++i)
   {
      if (v[i].DetachTime == 0)
         v[i].DetachTime = v[i].AttachTime + v[i].Duration / v[i].NumIos;
   }

   while (v.size() > s_maxNumAccess)
   {
      double min_s = 1e10;
      int    min_i = -1;

      int M = (int) v.size() - 2;
      for (int i = 0; i < M; ++i)
      {
         AStat &a = v[i], &b = v[i + 1];

         time_t t = std::max((time_t) 1, (now - b.AttachTime) / 2 + (now - a.DetachTime) / 2);
         double s = (double) (b.AttachTime - a.DetachTime) / t;

         if (s < min_s)
         {
            min_s = s;
            min_i = i;
         }
      }
      assert(min_i != -1);

      v[min_i].MergeWith(v[min_i + 1]);

      v.erase(v.begin() + (min_i + 1));
   }
}

//------------------------------------------------------------------------------

void Info::WriteIOStatAttach()
{
   m_store.m_accessCnt++;

   AStat as;
   as.AttachTime = time(0);
   m_store.m_astats.push_back(as);
}

void Info::WriteIOStat(Stats& s)
{
   m_store.m_astats.back().NumIos        = s.m_NumIos;
   m_store.m_astats.back().Duration      = s.m_Duration;
   m_store.m_astats.back().BytesHit      = s.m_BytesHit;
   m_store.m_astats.back().BytesMissed   = s.m_BytesMissed;
   m_store.m_astats.back().BytesBypassed = s.m_BytesBypassed;
}

void Info::WriteIOStatDetach(Stats& s)
{
   m_store.m_astats.back().DetachTime  = time(0);
   WriteIOStat(s);
}

void Info::WriteIOStatSingle(long long bytes_disk)
{
   m_store.m_accessCnt++;

   AStat as;
   as.AttachTime = as.DetachTime = time(0);
   as.NumIos     = 1;
   as.BytesHit  = bytes_disk;
   m_store.m_astats.push_back(as);
}

void Info::WriteIOStatSingle(long long bytes_disk, time_t att, time_t dtc)
{
   m_store.m_accessCnt++;

   AStat as;
   as.AttachTime = att;
   as.DetachTime = dtc;
   as.NumIos     = 1;
   as.Duration   = dtc - att;
   as.BytesHit  = bytes_disk;
   m_store.m_astats.push_back(as);
}

//------------------------------------------------------------------------------

bool Info::GetLatestDetachTime(time_t& t) const
{
   if (m_store.m_astats.empty())
   {
      t = m_store.m_creationTime;
   }
   else
   {
      const AStat& ls = m_store.m_astats.back();

      if (ls.DetachTime == 0)
         t = ls.AttachTime + ls.Duration;
      else
         t = ls.DetachTime;
   }

   return t != 0;
}

const Info::AStat* Info::GetLastAccessStats() const
{
   return m_store.m_astats.empty() ? 0 : & m_store.m_astats.back();
}

//==============================================================================
// Support for reading of previous cinfo versions
//==============================================================================

bool Info::ReadV2(XrdOssDF* fp, const std::string &fname)
{
   struct AStatV2
   {
      time_t    AttachTime;      //! open time
      time_t    DetachTime;      //! close time
      long long BytesHit;        //! read from disk
      long long BytesMissed;     //! read from ram
      long long BytesBypassed;   //! read remote client
   };

   std::string trace_pfx("Info:::ReadV2() ");
   trace_pfx += fname + " ";

   FpHelper r(fp, 0, m_trace, m_traceID, trace_pfx + "oss read failed");

   if (r.Read(m_store.m_version))    return false;
   if (r.Read(m_store.m_buffer_size)) return false;

   long long fs;
   if (r.Read(fs)) return false;
   SetFileSize(fs);

   if (r.ReadRaw(m_store.m_buff_synced, GetSizeInBytes())) return false;
   memcpy(m_buff_written, m_store.m_buff_synced, GetSizeInBytes());

   if (r.ReadRaw(m_store.m_cksum, 16)) return false;
   char tmpCksum[16];
   GetCksum(&m_store.m_buff_synced[0], &tmpCksum[0]);

   // Debug print cksum:
   // for (int i =0; i < 16; ++i)
   //    printf("%x", tmpCksum[i] & 0xff);
   // for (int i =0; i < 16; ++i)
   //    printf("%x", m_store.m_cksum[i] & 0xff);

   if (strncmp(m_store.m_cksum, &tmpCksum[0], 16))
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
   m_store.m_astats.reserve(std::min(m_store.m_accessCnt, s_maxNumAccess));
   AStatV2 av2;
   while ( ! r.ReadRaw(&av2, sizeof(AStatV2), false))
   {

      AStat as;
      as.AttachTime    = av2.AttachTime;
      as.DetachTime    = av2.DetachTime;
      as.NumIos        = 1;
      as.Duration      = av2.DetachTime - av2.AttachTime;
      as.NumMerged     = 0;
      as.BytesHit      = av2.BytesHit;
      as.BytesMissed   = av2.BytesMissed;
      as.BytesBypassed = av2.BytesBypassed;

      m_store.m_astats.emplace_back(as);
   }

   return true;
}

//------------------------------------------------------------------------------

bool Info::ReadV1(XrdOssDF* fp, const std::string &fname)
{
   struct AStatV1
   {
      time_t    DetachTime;    //! close time
      long long BytesHit;      //! read from disk
      long long BytesMissed;   //! read from ram
      long long BytesBypassed; //! read remote client
   };

   std::string trace_pfx("Info:::ReadV1() ");
   trace_pfx += fname + " ";

   FpHelper r(fp, 0, m_trace, m_traceID, trace_pfx + "oss read failed");

   if (r.Read(m_store.m_version)) return false;
   if (r.Read(m_store.m_buffer_size)) return false;

   long long fs;
   if (r.Read(fs)) return false;
   SetFileSize(fs);

   if (r.ReadRaw(m_store.m_buff_synced, GetSizeInBytes())) return false;
   memcpy(m_buff_written, m_store.m_buff_synced, GetSizeInBytes());

   m_complete = ! IsAnythingEmptyInRng(0, m_sizeInBits);
   if (r.ReadRaw(&m_store.m_accessCnt, sizeof(int), false)) m_store.m_accessCnt = 0;  // was: return false;
   TRACE(Dump, trace_pfx << " complete "<< m_complete << " access_cnt " << m_store.m_accessCnt);

   m_store.m_astats.reserve(std::min(m_store.m_accessCnt, s_maxNumAccess));
   AStatV1 av1;
   while ( ! r.ReadRaw(&av1, sizeof(AStatV1), false))
   {
      AStat as;
      as.AttachTime    = av1.DetachTime;
      as.DetachTime    = av1.DetachTime;
      as.NumIos        = 1;
      as.Duration      = 0;
      as.NumMerged     = 0;
      as.BytesHit      = av1.BytesHit;
      as.BytesMissed   = av1.BytesMissed;
      as.BytesBypassed = av1.BytesBypassed;

      m_store.m_astats.emplace_back(as);
   }

   if ( ! m_store.m_astats.empty())
   {
      m_store.m_creationTime = m_store.m_astats.front().AttachTime;
   }

   return true;
}
