//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientReadCache                                                   // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Classes to handle cache reading                                      //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include "XrdClientReadCache.hh"
#include "XrdClientMutexLocker.hh"
#include "XrdClientDebug.hh"
#include "XrdClientEnv.hh"


//________________________________________________________________________
XrdClientReadCacheItem::XrdClientReadCacheItem(const void *buffer, long long begin_offs,
				       long long end_offs, long long ticksnow)
{
   // Constructor

   fData = (void *)buffer;
   Touch(ticksnow);
   fBeginOffset = begin_offs;
   fEndOffset = end_offs;
}

//________________________________________________________________________
XrdClientReadCacheItem::~XrdClientReadCacheItem()
{
   // Destructor

   if (fData)
      free(fData);
}

//
// XrdClientReadCache
//

//________________________________________________________________________
long long XrdClientReadCache::GetTimestampTick()
{
   // Return timestamp

   // Mutual exclusion man!
   XrdClientMutexLocker mtx(fMutex);
   return ++fTimestampTickCounter;
}
  
//________________________________________________________________________
XrdClientReadCache::XrdClientReadCache()
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
      Error("XrdClientReadCache", 
            "Can't create mutex: out of system resources.");
      abort();
   }

   fMissRate = 0.0;
   fMissCount = 0;
   fReadsCounter = 0;

   fBytesSubmitted = 0;
   fBytesHit = 0;
   fBytesUsefulness = 0.0;

   fMaxCacheSize = EnvGetLong(NAME_READCACHESIZE);
}

//________________________________________________________________________
XrdClientReadCache::~XrdClientReadCache()
{
   // Destructor

   RemoveItems();
   pthread_mutex_destroy(&fMutex);
}

//________________________________________________________________________
void XrdClientReadCache::SubmitXMessage(XrdClientMessage *xmsg, long long begin_offs,
				  long long end_offs)
{
   // To populate the cache of items, newly received

   XrdClientReadCacheItem *itm;

   // We remove all the blocks contained in the one we are going to put
   RemoveItems(begin_offs, end_offs);

   if (MakeFreeSpace(end_offs - begin_offs)) {
      itm = new XrdClientReadCacheItem(xmsg->DonateData(), begin_offs, end_offs,
                                   GetTimestampTick());
      // Mutual exclusion man!
      XrdClientMutexLocker mtx(fMutex);

      fItems.Push_back(itm);
      fTotalByteCount += itm->Size();
      fBytesSubmitted += itm->Size();
   } // if
}

//________________________________________________________________________
bool XrdClientReadCache::GetDataIfPresent(const void *buffer,
					    long long begin_offs,
					    long long end_offs,
					    bool PerfCalc)
{
   // Copies the requested data from the cache. False if not possible

   int it;
   XrdClientMutexLocker mtx(fMutex);

   if (PerfCalc)
      fReadsCounter++;

   // We search an item containing all the data we need in one shot.
   // A future refinement could try to get the data in small blocks from
   //  multiple items
   for (it = 0; it < fItems.GetSize(); it++)
      if (fItems[it] && fItems[it]->GetInterval(buffer, begin_offs, end_offs)) {
         fItems[it]->Touch(GetTimestampTick());

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
void XrdClientReadCache::RemoveItems(long long begin_offs, long long end_offs)
{
   // To remove all the items contained in the given interval

   int it;
   XrdClientMutexLocker mtx(fMutex);

   it = 0;
   while (it < fItems.GetSize())
      if (fItems[it] && fItems[it]->ContainedInInterval(begin_offs, end_offs)) {
         fTotalByteCount -= fItems[it]->Size();
         delete fItems[it];
         fItems.Erase(it);
      }
      else it++;
}

//________________________________________________________________________
void XrdClientReadCache::RemoveItems()
{
   // To remove all the items
   int it;
   XrdClientMutexLocker mtx(fMutex);

   it = 0;

   while (it < fItems.GetSize()) {
      delete fItems[it];
      fItems.Erase(it);
   }

   fTotalByteCount = 0;

}


//________________________________________________________________________
bool XrdClientReadCache::RemoveLRUItem()
{
   // Finds the LRU item and removes it

   int it, lruit;
   long long minticks = -1;
   XrdClientReadCacheItem *item;

   XrdClientMutexLocker mtx(fMutex);

   lruit = 0;
   for (it = 0; it < fItems.GetSize(); it++) {
      if (fItems[it]) {
         if ((minticks < 0) || (fItems[it]->GetTimestampTicks() < minticks)) {
            minticks = fItems[it]->GetTimestampTicks();
            lruit = it;
         }      
      }
   }

   item = fItems[lruit];

   if (minticks >= 0) {
      fTotalByteCount -= item->Size();
      delete item;
      fItems.Erase(lruit);
   }

   return TRUE;
}

//________________________________________________________________________
bool XrdClientReadCache::MakeFreeSpace(long long bytes)
{
   // False if not possible (requested space exceeds max size!)

   if (!WillFit(bytes))
      return FALSE;

   XrdClientMutexLocker mtx(fMutex);

   while (fMaxCacheSize - fTotalByteCount < bytes)
      RemoveLRUItem();

   return TRUE;
}
