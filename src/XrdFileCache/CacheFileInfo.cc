#include "CacheFileInfo.hh"
#include "Cache.hh"
#include "Log.hh"
#include "CacheStats.hh"
#include <sys/file.h>


#include <XrdOss/XrdOss.hh>
#include <assert.h>

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>


#define BIT(n)       (1ULL << (n))
using namespace XrdFileCache;


CacheFileInfo::CacheFileInfo():
   m_bufferSize(PrefetchDefaultBufferSize),
   m_sizeInBits(0), m_buff(0), 
   m_accessCnt(0), 
   m_complete(false)
{
}

CacheFileInfo::~CacheFileInfo() {
   if (m_buff) delete [] m_buff;
}

//______________________________________________________________________________


void  CacheFileInfo::resizeBits(int s)
{
   m_sizeInBits = s;
   m_buff = (char*)malloc(getSizeInBytes());
   memset(m_buff, 0, getSizeInBytes());
}

//______________________________________________________________________________


int CacheFileInfo::Read(XrdOssDF* fp)
{
    // does not need lock, called only in Prefetch::Open
    // before Prefetch::Run() starts

   int off = 0;
   off += fp->Read(&m_bufferSize, off, sizeof(long long));
   if (off <= 0) return off;

   int sb;
   off += fp->Read(&sb, off, sizeof(int));
   resizeBits(sb);

   off += fp->Read(m_buff, off, getSizeInBytes());
   m_complete = isAnythingEmptyInRng(0, sb-1) ? false : true;

   assert (off = getHeaderSize());

   off += fp->Read(&m_accessCnt, off, sizeof(int));

   return off;
}

//______________________________________________________________________________


int CacheFileInfo::getHeaderSize() const
{
   return  sizeof(long long) + sizeof(int) + getSizeInBytes();
}

//______________________________________________________________________________
void  CacheFileInfo::WriteHeader(XrdOssDF* fp)
{
   // m_writeMutex.Lock();
   int fl = flock(fp->getFD(),  LOCK_EX);
   if (fl) aMsg(kError, "WriteHeader() lock failed %s \n", strerror(errno));

   long long  off = 0;
   off += fp->Write(&m_bufferSize, off, sizeof(long long));

   int nb = getSizeInBits();
   off += fp->Write(&nb, off, sizeof(int));
   off += fp->Write(m_buff, off, getSizeInBytes());

   int flu = flock(fp->getFD(),  LOCK_UN);
   if (flu) aMsg(kError,"WriteHeader() un-lock failed \n");

   assert (off == getHeaderSize());
   //m_writeMutex.UnLock();

}

//______________________________________________________________________________
void  CacheFileInfo::AppendIOStat(const CacheStats* caches, XrdOssDF* fp)
{
   // m_writeMutex.Lock();

   int fl = flock(fp->getFD(),  LOCK_EX);
   if (fl) aMsg(kError,"AppendIOStat() lock failed \n");

   m_accessCnt++;

   // get offset to append
   // not: XrdOssDF FStat doesn not sets stat 
 
   long long off = getHeaderSize();
   off += fp->Write(&m_accessCnt, off, sizeof(int));
   off += (m_accessCnt-1)*sizeof(AStat);
   AStat as;
   as.AppendTime = caches->AppendTime;
   as.DetachTime = time(0);
   as.BytesRead = caches->BytesCachedPrefetch + caches->BytesPrefetch;
   as.Hits = caches->Hits; // num blocks
   as.Miss = caches->Miss;

   if(Dbg < kInfo) as.Dump();

   int flu = flock(fp->getFD(),  LOCK_UN);
   if (flu) aMsg(kError,"AppendStat() un-lock failed \n");

   long long ws = fp->Write(&as, off, sizeof(AStat));
   assert(ws == sizeof(AStat));
   //  m_writeMutex.UnLock();
}

//______________________________________________________________________________
bool  CacheFileInfo::getLatestAttachTime(time_t& t, XrdOssDF* fp) const
{
   bool res = false;
   int fl = flock(fp->getFD(),  LOCK_SH);
   if (fl) aMsg(kError,"CacheFileInfo::getLatestAttachTime() lock failed \n");
   if (m_accessCnt) {
      AStat stat;
      long long off = getHeaderSize() + sizeof(int) + (m_accessCnt-1)*sizeof(AStat);
      int res = fp->Read(&stat, off, sizeof(AStat));
      if (res == sizeof(AStat))
      {
         t = stat.AppendTime;
         res = true;
      }
      else
      {
         aMsg(kError, " CacheFileInfo::getLatestAttachTime() can't get latest access stat. read bytes = %d", res);
      }
   }

   int fu = flock(fp->getFD(),  LOCK_UN);
   if (fu) aMsg(kError,"CacheFileInfo::getLatestAttachTime() lock failed \n");
   return res;
}

//______________________________________________________________________________


void  CacheFileInfo::print() const
{
   printf("blocksSize %lld \n",m_bufferSize );
   printf("printing [%d] blocks \n", m_sizeInBits);
   for (int i = 0; i < m_sizeInBits; ++i)
   {
      printf("%d ", testBit(i));
   }
   printf("\n");
   printf("printing complete %d\n", m_complete);
}
