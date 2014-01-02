#ifndef __XRDOUCCACHESTATS_HH__
#define __XRDOUCCACHESTATS_HH__

#include <XrdOuc/XrdOucCache.hh>

namespace XrdFileCache
{
class CacheStats : public XrdOucCacheStats
{
public:
    long long    BytesCachedPrefetch;
    long long    BytesPrefetch;
    long long    BytesDisk;
    int          HitsPrefetch;
    int          HitsDisk;
    time_t       AppendTime;

    inline void AddStat(CacheStats &Src)
    {
        XrdOucCacheStats::Add(Src);

        //  sMutex1.Lock();
        BytesCachedPrefetch += Src.BytesCachedPrefetch;
        BytesPrefetch       += Src.BytesPrefetch;
        BytesDisk           += Src.BytesDisk;

        HitsPrefetch += Src.HitsPrefetch;
        HitsDisk     += Src.HitsDisk;

        // sMutex1.UnLock();
    }

    CacheStats() :
        BytesCachedPrefetch(0), 
        BytesPrefetch(0),
        BytesDisk(0),
        HitsPrefetch(0), 
        HitsDisk(0) {
        AppendTime = time(0);
    }

   void Dump() const;

private:    
   //   XrdSysMutex sMutex1;

};
}



#endif
