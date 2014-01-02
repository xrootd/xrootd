#include "XrdFileCacheStats.hh"
#include "Log.hh"

using namespace XrdFileCache;

void Stats::Dump() const
{
   printf("CacheStats::Dump() bCachedPrefetch = %lld, bPrefetch = %lld\n", BytesCachedPrefetch, BytesPrefetch);
}
