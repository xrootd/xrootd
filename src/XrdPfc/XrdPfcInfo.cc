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
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>

#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucCRC32C.hh"
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

struct TraceHeader
{
   const char  *f_tpf, *f_tdir, *f_tfile , *f_tmsg;

   // tdir is supposed to be '/' terminated. Check can be added in ctor and a bool flag to mark if it is needed.
   TraceHeader(const char *tpf, const char *tdir, const char *tfile = 0, const char *tmsg = 0) :
      f_tpf(tpf), f_tdir(tdir), f_tfile(tfile), f_tmsg(tmsg) {}
};

XrdSysTrace& operator<<(XrdSysTrace& s, const TraceHeader& th)
{
    s << th.f_tpf << " " << th.f_tdir;
    if (th.f_tfile) s << th.f_tfile;
    if (th.f_tmsg)  s << " " << th.f_tmsg;
    s << " ";
    return s;
}

struct FpHelper
{
   XrdOssDF    *f_fp;
   off_t        f_off;
   XrdSysTrace *f_trace;
   const char  *m_traceID;
   const TraceHeader &f_trace_hdr;

   XrdSysTrace* GetTrace() const { return f_trace; }

   FpHelper(XrdOssDF* fp, off_t off, XrdSysTrace *trace, const char *tid, const TraceHeader &thdr) :
      f_fp(fp), f_off(off), f_trace(trace), m_traceID(tid), f_trace_hdr(thdr) 
   {}

   // Returns true on error
   bool ReadRaw(void *buf, ssize_t size, bool warnp = true)
   {
      ssize_t ret = f_fp->Read(buf, f_off, size);
      if (ret != size)
      {
         if (warnp)
         {
            TRACE(Warning, f_trace_hdr << "Oss Read failed at off=" << f_off << " size=" << size
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
   bool WriteRaw(const void *buf, ssize_t size)
   {
      ssize_t ret = f_fp->Write(buf, f_off, size);
      if (ret != size)
      {
         TRACE(Warning, f_trace_hdr << "Oss Write failed at off=" << f_off << " size=" << size
                        << " ret=" << ret << " error=" << ((ret < 0) ? XrdSysE2T(ret) : "<no error>"));
         return true;
      }
      f_off += ret;
      return false;
   }

   template<typename T> bool Write(const T &loc)
   {
      return WriteRaw(&loc, sizeof(T));
   }
};
}

using namespace XrdPfc;

const char*  Info::m_traceID          = "CInfo";
const char*  Info::s_infoExtension    = ".cinfo";
const size_t Info::s_infoExtensionLen = strlen(Info::s_infoExtension);
      size_t Info::s_maxNumAccess     = 20; // default, can be changed through configuration
const int    Info::s_defaultVersion   = 4;

//------------------------------------------------------------------------------

Info::Info(XrdSysTrace* trace, bool prefetchBuffer) :
   m_trace(trace),
   m_buff_synced(0), m_buff_written(0),  m_buff_prefetch(0),
   m_version(0),
   m_bitvecSizeInBits(0),
   m_complete(false),
   m_hasPrefetchBuffer(prefetchBuffer),
   m_cksCalcMd5(0)
{}

Info::~Info()
{
   if (m_buff_synced)   free(m_buff_synced);
   if (m_buff_written)  free(m_buff_written);
   if (m_buff_prefetch) free(m_buff_prefetch);
   delete m_cksCalcMd5;
}

//------------------------------------------------------------------------------

void Info::SetAllBitsSynced()
{
   // The following should be:
   //   memset(m_buff_synced, 255, GetBitvecSizeInBytes());
   // but GCC produces an overzealous 'possible argument transpose warning' and
   // xrootd build uses warnings->errors escalation.
   // This workaround can be removed for gcc >= 5.
   // See also: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61294
   const int nb = GetBitvecSizeInBytes();
   for (int i = 0; i < nb; ++i)
      m_buff_synced[i] = 255;

   m_complete = true;
}

//------------------------------------------------------------------------------

void Info::SetBufferSize(long long bs)
{
   // Needed only info is created first time in File::Open()
   m_store.m_buffer_size = bs;
}

//------------------------------------------------------------------------------s

void Info::SetFileSizeAndCreationTime(long long fs)
{
   m_store.m_file_size = fs;
   ResizeBits();
   m_store.m_creationTime = time(0);
}

//------------------------------------------------------------------------------

void Info::ResizeBits()
{
   // drop buffer in case of failed/partial reads

   if (m_buff_synced)   free(m_buff_synced);
   if (m_buff_written)  free(m_buff_written);
   if (m_buff_prefetch) free(m_buff_prefetch);

   m_bitvecSizeInBits = (m_store.m_file_size - 1) / m_store.m_buffer_size + 1;

   m_buff_written = (unsigned char*) malloc(GetBitvecSizeInBytes());
   m_buff_synced  = (unsigned char*) malloc(GetBitvecSizeInBytes());
   memset(m_buff_written, 0, GetBitvecSizeInBytes());
   memset(m_buff_synced,  0, GetBitvecSizeInBytes());

   if (m_hasPrefetchBuffer)
   {
      m_buff_prefetch = (unsigned char*) malloc(GetBitvecSizeInBytes());
      memset(m_buff_prefetch, 0, GetBitvecSizeInBytes());
   }
   else
   {
      m_buff_prefetch = 0;
   }
}

//------------------------------------------------------------------------------

void Info::ResetCkSumCache()
{
   if (IsCkSumCache())
   {
      m_store.m_status.f_cksum_check &= ~CSChk_Cache;
      if ( ! HasNoCkSumTime())
         m_store.m_noCkSumTime = time(0);
   }
}

void Info::ResetCkSumNet()
{
   if (IsCkSumNet())
   {
      m_store.m_status.f_cksum_check &= ~CSChk_Net;
      if ( ! HasNoCkSumTime())
         m_store.m_noCkSumTime = time(0);
   }
}

//------------------------------------------------------------------------------
// Write / Read cinfo file
//------------------------------------------------------------------------------

uint32_t Info::CalcCksumStore()
{
   return crc32c(0, &m_store, sizeof(Store));
}

uint32_t Info::CalcCksumSyncedAndAStats()
{
   uint32_t cks = crc32c(0, m_buff_synced, GetBitvecSizeInBytes());
   return crc32c(cks, m_astats.data(), m_astats.size() * sizeof(AStat));
}

void Info::CalcCksumMd5(unsigned char* buff, char* digest)
{
   if (m_cksCalcMd5)
      m_cksCalcMd5->Init();
   else
      m_cksCalcMd5 = new XrdCksCalcmd5();

   m_cksCalcMd5->Update((const char*)buff, GetBitvecSizeInBytes());
   memcpy(digest, m_cksCalcMd5->Final(), 16);
}

const char* Info::GetCkSumStateAsText() const
{
   switch (m_store.m_status.f_cksum_check) {
      case CSChk_None   : return "none";
      case CSChk_Cache  : return "cache";
      case CSChk_Net    : return "net";
      case CSChk_Both   : return "both";
      default           : return "unknown";
   }
}

//------------------------------------------------------------------------------

// std::string wrapper ?
// bool Info::Write(XrdOssDF* fp, const std::string &fname)
// {}

bool Info::Write(XrdOssDF* fp, const char *dname, const char *fname)
{
   TraceHeader trace_pfx("Write()", dname, fname);

   if (m_astats.size() > s_maxNumAccess) CompactifyAccessRecords();
   m_store.m_astatSize = (int32_t) m_astats.size();

   FpHelper w(fp, 0, m_trace, m_traceID, trace_pfx);

   if (w.Write(s_defaultVersion) ||
       w.Write(m_store) ||
       w.Write(CalcCksumStore()) ||
       w.WriteRaw(m_buff_synced, GetBitvecSizeInBytes()) ||
       w.WriteRaw(m_astats.data(), m_store.m_astatSize * sizeof(AStat)) ||
       w.Write(CalcCksumSyncedAndAStats()))
   {
      return false;
   }

   return true;
}

//------------------------------------------------------------------------------

// Potentially provide std::string wrapper.
// bool Info::Read(XrdOssDF* fp, const std::string &fname)
// {}

bool Info::Read(XrdOssDF *fp, const char *dname, const char *fname)
{
   // Does not need lock, called only in File::Open before File::Run() starts.
   // XXXX Wait, how about Purge, and LocalFilePath, Stat?

   TraceHeader trace_pfx("Read()", dname, fname);

   FpHelper r(fp, 0, m_trace, m_traceID, trace_pfx);

   if (r.Read(m_version)) return false;

   if (m_version != s_defaultVersion)
   {
      if (m_version == 2)
      {
         return ReadV2(fp, r.f_off, dname, fname);
      }
      else if (m_version == 3)
      {
         return ReadV3(fp, r.f_off, dname, fname);
      }
      else
      {
         TRACE(Warning, trace_pfx << "File version " << m_version << " not supported.");
         return false;
      }
   }

   uint32_t cksum;

   if (r.Read(m_store) || r.Read(cksum)) return false;

   if (cksum != CalcCksumStore())
   {
      TRACE(Error, trace_pfx << "Checksum Store mismatch.");
      return false;
   }

   ResizeBits();
   m_astats.resize(m_store.m_astatSize);

   if (r.ReadRaw(m_buff_synced, GetBitvecSizeInBytes()) ||
       r.ReadRaw(m_astats.data(), m_store.m_astatSize * sizeof(AStat)) ||
       r.Read(cksum))
   {
      return false;
   }

   if (cksum != CalcCksumSyncedAndAStats())
   {
      TRACE(Error, trace_pfx << "Checksum Synced or AStats mismatch.");
      return false;
   }

   memcpy(m_buff_written, m_buff_synced, GetBitvecSizeInBytes());

   m_complete = ! IsAnythingEmptyInRng(0, m_bitvecSizeInBits);

   return true;
}

//------------------------------------------------------------------------------
// Access stats / records
//------------------------------------------------------------------------------

void Info::ResetAllAccessStats()
{
   m_store.m_accessCnt = 0;
   m_store.m_astatSize = 0;
   m_astats.clear();
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

   std::vector<AStat> &v = m_astats;

   for (int i = 0; i < (int) v.size() - 1; ++i)
   {
      if (v[i].DetachTime == 0)
         v[i].DetachTime = std::min(v[i].AttachTime + v[i].Duration / v[i].NumIos, v[i+1].AttachTime);
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
   m_astats.push_back(as);
}

void Info::WriteIOStat(Stats& s)
{
   m_astats.back().NumIos        = s.m_NumIos;
   m_astats.back().Duration      = s.m_Duration;
   m_astats.back().BytesHit      = s.m_BytesHit;
   m_astats.back().BytesMissed   = s.m_BytesMissed;
   m_astats.back().BytesBypassed = s.m_BytesBypassed;
}

void Info::WriteIOStatDetach(Stats& s)
{
   m_astats.back().DetachTime  = time(0);
   WriteIOStat(s);
}

void Info::WriteIOStatSingle(long long bytes_disk)
{
   m_store.m_accessCnt++;

   AStat as;
   as.AttachTime = as.DetachTime = time(0);
   as.NumIos     = 1;
   as.BytesHit  = bytes_disk;
   m_astats.push_back(as);
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
   m_astats.push_back(as);
}

//------------------------------------------------------------------------------

bool Info::GetLatestDetachTime(time_t& t) const
{
   if (m_astats.empty())
   {
      t = m_store.m_creationTime;
   }
   else
   {
      const AStat& ls = m_astats.back();

      if (ls.DetachTime == 0)
         t = ls.AttachTime + ls.Duration;
      else
         t = ls.DetachTime;
   }

   return t != 0;
}

const Info::AStat* Info::GetLastAccessStats() const
{
   return m_astats.empty() ? 0 : & m_astats.back();
}

//==============================================================================
// Support for reading of previous cinfo versions
//==============================================================================

bool Info::ReadV3(XrdOssDF* fp, off_t off, const char *dname, const char *fname)
{
   TraceHeader trace_pfx("ReadV3()", dname, fname);

   FpHelper r(fp, off, m_trace, m_traceID, trace_pfx);

   if (r.Read(m_store.m_buffer_size)) return false;
   if (r.Read(m_store.m_file_size)) return false;
   ResizeBits();

   if (r.ReadRaw(m_buff_synced, GetBitvecSizeInBytes())) return false;
   memcpy(m_buff_written, m_buff_synced, GetBitvecSizeInBytes());

   char fileCksum[16], tmpCksum[16];
   if (r.ReadRaw(&fileCksum[0], 16)) return false;
   CalcCksumMd5(&m_buff_synced[0], &tmpCksum[0]);

   if (memcmp(&fileCksum[0], &tmpCksum[0], 16))
   {
      TRACE(Error, trace_pfx << "buffer cksum and saved cksum don't match.");
      return false;
   }

   // cache complete status
   m_complete = ! IsAnythingEmptyInRng(0, m_bitvecSizeInBits);

   // read creation time
   if (r.Read(m_store.m_creationTime)) return false;

   // get number of accessess
   if (r.Read(m_store.m_accessCnt, false)) m_store.m_accessCnt = 0;  // was: return false;

   // read access statistics
   m_astats.reserve(std::min(m_store.m_accessCnt, s_maxNumAccess));
   AStat as;
   while ( ! r.Read(as, false))
   {
      // Consistency check ... weird stuff seen at UCSD StashCache.
      if (as.NumIos <= 0 || as.AttachTime < 3600*24*365 ||
          (as.DetachTime != 0 && (as.DetachTime < 3600*24*365 || as.DetachTime < as.AttachTime)))
      {
         TRACE(Warning, trace_pfx << "Corrupted access record, skipping.");
         continue;
      }

      as.Reserved = 0;
      m_astats.emplace_back(as);
   }

   // Comment for V4: m_store.m_noCkSumTime and m_store_mstatus.f_cksum_check
   // are left as 0 (default values in Info ctor).

   return true;
}

bool Info::ReadV2(XrdOssDF* fp, off_t off, const char *dname, const char *fname)
{
   struct AStatV2
   {
      time_t    AttachTime;      //! open time
      time_t    DetachTime;      //! close time
      long long BytesHit;        //! read from disk
      long long BytesMissed;     //! read from ram
      long long BytesBypassed;   //! read remote client
   };

   TraceHeader trace_pfx("ReadV2()", dname, fname);

   FpHelper r(fp, off, m_trace, m_traceID, trace_pfx);

   if (r.Read(m_store.m_buffer_size)) return false;
   if (r.Read(m_store.m_file_size)) return false;
   ResizeBits();

   if (r.ReadRaw(m_buff_synced, GetBitvecSizeInBytes())) return false;
   memcpy(m_buff_written, m_buff_synced, GetBitvecSizeInBytes());

   char fileCksum[16], tmpCksum[16];
   if (r.ReadRaw(&fileCksum[0], 16)) return false;
   CalcCksumMd5(&m_buff_synced[0], &tmpCksum[0]);

   if (memcmp(&fileCksum[0], &tmpCksum[0], 16))
   {
      TRACE(Error, trace_pfx << "buffer cksum and saved cksum don't match.");
      return false;
   }

   // cache complete status
   m_complete = ! IsAnythingEmptyInRng(0, m_bitvecSizeInBits);

   // read creation time
   if (r.Read(m_store.m_creationTime)) return false;

   // get number of accessess
   if (r.Read(m_store.m_accessCnt, false)) m_store.m_accessCnt = 0;  // was: return false;

   // read access statistics
   m_astats.reserve(std::min(m_store.m_accessCnt, s_maxNumAccess));
   AStatV2 av2;
   while ( ! r.ReadRaw(&av2, sizeof(AStatV2), false))
   {
      AStat as;
      as.AttachTime    = av2.AttachTime;
      as.DetachTime    = av2.DetachTime;
      as.NumIos        = 1;
      as.Duration      = av2.DetachTime - av2.AttachTime;
      as.NumMerged     = 0;
      as.Reserved      = 0;
      as.BytesHit      = av2.BytesHit;
      as.BytesMissed   = av2.BytesMissed;
      as.BytesBypassed = av2.BytesBypassed;

      // Consistency check ... weird stuff seen at UCSD StashCache.
      if (as.AttachTime < 3600*24*365 ||
          (as.DetachTime != 0 && (as.DetachTime < 3600*24*365 || as.DetachTime < as.AttachTime)))
      {
         TRACE(Warning, trace_pfx << "Corrupted access record, skipping.");
         continue;
      }

      m_astats.emplace_back(as);
   }

   return true;
}


//==============================================================================
// Test bitfield ops and masking of non-cksum fields
//==============================================================================
#ifdef XRDPFC_CKSUM_TEST

void Info::TestCksumStuff()
{
   static const char* names[] = { "--", "-C", "N-", "NC" };

   const Configuration &conf = Cache::GetInstance().RefConfiguration();

   printf("Doing cksum tests for config %s\n", names[conf.m_cs_Chk]);

   Info cfi(0);
   printf("cksum %d, raw %x\n", cfi.m_store.m_status.f_cksum_check, cfi.m_store.m_status._raw_);

   cfi.SetCkSumState(CSChk_Both);
   printf("cksum %d, raw %x\n", cfi.m_store.m_status.f_cksum_check, cfi.m_store.m_status._raw_);

   cfi.m_store.m_status._raw_ |= 0xff0000;
   printf("cksum %d, raw %x\n", cfi.m_store.m_status.f_cksum_check, cfi.m_store.m_status._raw_);

   cfi.ResetCkSumCache();
   printf("cksum %d, raw %x\n", cfi.m_store.m_status.f_cksum_check, cfi.m_store.m_status._raw_);

   cfi.ResetCkSumNet();
   printf("cksum %d, raw %x\n", cfi.m_store.m_status.f_cksum_check, cfi.m_store.m_status._raw_);

   for (int cs = CSChk_None; cs <= CSChk_Both; ++cs)
   {
      cfi.SetCkSumState((CkSumCheck_e) cs);
      bool hasmb = conf.does_cschk_have_missing_bits(cfi.GetCkSumState());
      cfi.DowngradeCkSumState(conf.get_cs_Chk());
      printf("-- File conf %s -- does_cschk_have_missing_bits:%d, downgraded_state:%s\n",
             names[cs], hasmb, names[cfi.GetCkSumState()]);
   }
}

#endif