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

#include "XrdReadCache.hh"
#include "XrdMutexLocker.hh"
#include "XrdDebug.hh"


//________________________________________________________________________
XrdReadCacheItem::XrdReadCacheItem(const void *buffer, long long begin_offs,
				       long long end_offs, long long ticksnow)
{
   // Constructor

   fData = (void *)buffer;
   Touch(ticksnow);
   fBeginOffset = begin_offs;
   fEndOffset = end_offs;
}

//________________________________________________________________________
XrdReadCacheItem::~XrdReadCacheItem()
{
   // Destructor

   if (fData)
      free(fData);
}

//
// XrdReadCache
//

//________________________________________________________________________
long long XrdReadCache::GetTimestampTick()
{
   // Return timestamp

   // Mutual exclusion man!
   XrdMutexLocker mtx(fMutex);
   return ++fTimestampTickCounter;
}
  
//________________________________________________________________________
XrdReadCache::XrdReadCache()
{
   // Constructor

   fTimestampTickCounter = 0;
   fTotalByteCount = 0;

   // Initialization of lock mutex
   pthread_mutexattr_t attr;
   int rc;

   // Initialization of lock mutex
   rc = pthread_mutexattr_init(&attr);
   if (rc == 0) {
      rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      if (rc == 0)
	 rc = pthread_mutex_init(&fMutex, &attr);
   }
   if (rc) {
      Error("XrdReadCache", 
            "Can't create mutex: out of system resources.");
      abort();
   }

   fMissRate = 0.0;
   fMissCount = 0;
   fReadsCounter = 0;

   fBytesSubmitted = 0;
   fBytesHit = 0;
   fBytesUsefulness = 0.0;

   fMaxCacheSize = TXNETREADCACHE_DFLTSIZE;
}

//________________________________________________________________________
XrdReadCache::~XrdReadCache()
{
   // Destructor

   RemoveItems();
   pthread_mutex_destroy(&fMutex);
}

//________________________________________________________________________
void XrdReadCache::SubmitXMessage(XrdMessage *xmsg, long long begin_offs,
				  long long end_offs)
{
   // To populate the cache of items, newly received

   XrdReadCacheItem *itm;

   // We remove all the blocks contained in the one we are going to put
   RemoveItems(begin_offs, end_offs);

   if (MakeFreeSpace(end_offs - begin_offs)) {
      itm = new XrdReadCacheItem(xmsg->DonateData(), begin_offs, end_offs,
                                   GetTimestampTick());
      // Mutual exclusion man!
      XrdMutexLocker mtx(fMutex);

      fItems.push_back(itm);
      fTotalByteCount += itm->Size();
      fBytesSubmitted += itm->Size();
   } // if
}

//________________________________________________________________________
bool XrdReadCache::GetDataIfPresent(const void *buffer,
					    long long begin_offs,
					    long long end_offs,
					    bool PerfCalc)
{
   // Copies the requested data from the cache. False if not possible

   ItemVect::iterator it;
   XrdMutexLocker mtx(fMutex);

   if (PerfCalc)
      fReadsCounter++;

   // We search an item containing all the data we need in one shot.
   // A future refinement could try to get the data in small blocks from
   //  multiple items
   for (it = fItems.begin(); it != fItems.end(); it++)
      if ((*it) && (*it)->GetInterval(buffer, begin_offs, end_offs)) {
         (*it)->Touch(GetTimestampTick());

         if (PerfCalc) {
            fBytesHit += (end_offs - begin_offs);
            UpdatePerfCounters();
         }
         return TRUE;
      }

   if (PerfCalc) {
      fMissCount++;
      UpdatePerfCounters();
   }

   return FALSE;
}

//________________________________________________________________________
void XrdReadCache::RemoveItems(long long begin_offs, long long end_offs)
{
   // To remove all the items contained in the given interval

   ItemVect::iterator it;
   XrdMutexLocker mtx(fMutex);

   it = fItems.begin();
   while (it != fItems.end())
      if ((*it) && (*it)->ContainedInInterval(begin_offs, end_offs)) {
         fTotalByteCount -= (*it)->Size();
         SafeDelete(*it);
         it = fItems.erase(it);
      }
      else it++;
}

//________________________________________________________________________
void XrdReadCache::RemoveItems()
{
   // To remove all the items
   ItemVect::iterator it;
   XrdMutexLocker mtx(fMutex);

   it = fItems.begin();

   while (it != fItems.end()) {
      SafeDelete(*it);
      it = fItems.erase(it);
   }

   fTotalByteCount = 0;

}


//________________________________________________________________________
bool XrdReadCache::RemoveLRUItem()
{
   // Finds the LRU item and removes it

   ItemVect::iterator it, lruit;
   long long minticks = -1;
   XrdReadCacheItem *item;

   XrdMutexLocker mtx(fMutex);

   lruit = fItems.begin();
   for (it = fItems.begin(); it != fItems.end(); it++) {
      if (*it) {
         if ((minticks < 0) || ((*it)->GetTimestampTicks() < minticks)) {
            minticks = (*it)->GetTimestampTicks();
            lruit = it;
         }      
      }
   }

   item = *lruit;

   if (minticks >= 0) {
      fTotalByteCount -= item->Size();
      SafeDelete(item);
      it = fItems.erase(lruit);
   }

   return TRUE;
}

//________________________________________________________________________
bool XrdReadCache::MakeFreeSpace(long long bytes)
{
   // False if not possible (requested space exceeds max size!)

   if (!WillFit(bytes))
      return FALSE;

   XrdMutexLocker mtx(fMutex);

   while (fMaxCacheSize - fTotalByteCount < bytes)
      RemoveLRUItem();

   return TRUE;
}
