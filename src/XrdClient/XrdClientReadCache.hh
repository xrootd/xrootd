//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdReadCache                                                         // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Classes to handle cache reading                                      //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRD_READCACHE_H
#define XRD_READCACHE_H

#include <pthread.h>
#include <vector>
#include <iostream>

#include "XrdInputBuffer.hh"
#include "XrdMessage.hh"



//
// XrdReadCacheItem
//
// An item is nothing more than an interval of bytes taken from a file.
// Since a cache object is to be associated to a single instance
// of TXNetFile, we do not have to keep here any filehandle
//

class XrdReadCacheItem {
private:
   long long        fBeginOffset;
   void             *fData;
   long long        fEndOffset;
   long             fTimestampTicks; // timestamp updated each time it's referenced

public:
   XrdReadCacheItem(const void *buffer, long long begin_offs, 
                                          long long end_offs, long long ticksnow);
   ~XrdReadCacheItem();

   // Is this obj contained in the given interval (which is going to be inserted) ?
   inline bool   ContainedInInterval(long long begin_offs, long long end_offs) {
      return ( (end_offs > begin_offs) &&
               (fBeginOffset >= begin_offs) &&
               (fEndOffset <= end_offs) );
   }
   // Does this obj contain the given interval (which is going to be requested) ?
   inline bool   ContainsInterval(long long begin_offs, long long end_offs) {
      return ( (end_offs > begin_offs) &&
               (fBeginOffset <= begin_offs) && (fEndOffset >= end_offs) );
   }
   // Get the requested interval, if possible
   inline bool   GetInterval(const void *buffer, long long begin_offs, 
                                                   long long end_offs) {
      if (!ContainsInterval(begin_offs, end_offs))
         return FALSE;
      memcpy((void *)buffer, ((char *)fData)+(begin_offs - fBeginOffset),
                                                 end_offs - begin_offs);
      return TRUE;
   }
   inline long long GetTimestampTicks() { return(fTimestampTicks); }
   long Size() { return (fEndOffset - fBeginOffset); }
   inline void     Touch(long long ticksnow) { fTimestampTicks = ticksnow; }
};

//
// TXNetReadCacheBody
//
// The content of the cache. Not cache blocks, but
// variable length Items
//
typedef vector<XrdReadCacheItem *> ItemVect;

class XrdReadCache {
private:

   long long      fBytesHit;         // Total number of bytes read with a cache hit
   long long      fBytesSubmitted;   // Total number of bytes inserted
   float         fBytesUsefulness;
   ItemVect      fItems;
   long long      fMaxCacheSize;
   long long      fMissCount;        // Counter of the cache misses
   float         fMissRate;            // Miss rate
   pthread_mutex_t  fMutex;
   long long      fReadsCounter;     // Counter of all the attempted reads (hit or miss)
   long long      fTimestampTickCounter;        // Aging mechanism yuk!
   long long      fTotalByteCount;

   long long      GetTimestampTick();
   bool        MakeFreeSpace(long long bytes);
   bool        RemoveLRUItem();
   inline void   UpdatePerfCounters() {
      if (fReadsCounter > 0)
         fMissRate = (float)fMissCount / fReadsCounter;
      if (fBytesSubmitted > 0)
         fBytesUsefulness = (float)fBytesHit / fBytesSubmitted;
   }

public:
   XrdReadCache();
   ~XrdReadCache();
  
   bool          GetDataIfPresent(const void *buffer, long long begin_offs,
                                  long long end_offs, bool PerfCalc);
   inline long long GetTotalByteCount() { return fTotalByteCount; }
   inline void     PrintPerfCounters() {
      cout << "Caching info: MissRate=" << fMissRate << " MissCount=" << 
              fMissCount << " ReadsCounter=" << fReadsCounter << endl;
      cout << "Caching info: BytesUsefulness=" << fBytesUsefulness <<
              " BytesSubmitted=" << fBytesSubmitted << " BytesHit=" << 
              fBytesHit << endl;
   }

   void            SubmitXMessage(XrdMessage *xmsg, long long begin_offs,
                                                 long long end_offs);
   void            RemoveItems();
   void            RemoveItems(long long begin_offs, long long end_offs);
   // To check if a block dimension will fit into the cache
   inline bool   WillFit(long long bc) { return (bc < fMaxCacheSize); }

};

#endif
