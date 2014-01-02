#include "XrdFileCacheStats.hh"
#include "XrdFileCacheLog.hh"

using namespace XrdFileCache;

void Stats::Dump() const
{
   printf("CacheStats::Dump() bCachedPrefetch = %lld, bPrefetch = %lld\n", BytesCachedPrefetch, BytesPrefetch);
}
