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

#include "XrdClient/XrdClientReadCache.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientEnv.hh"


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
   XrdOucMutexHelper mtx(fMutex);
   return ++fTimestampTickCounter;
}
  
//________________________________________________________________________
XrdClientReadCache::XrdClientReadCache()
{
   // Constructor

   fTimestampTickCounter = 0;
   fTotalByteCount = 0;

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

}

//________________________________________________________________________
void XrdClientReadCache::SubmitXMessage(XrdClientMessage *xmsg, long long begin_offs,
				  long long end_offs)
{
   // To populate the cache of items, newly received

   XrdClientReadCacheItem *itm;
   const void *buffer = xmsg->DonateData();
   
   if (!buffer) return;

   // We remove all the blocks contained in the one we are going to put
   RemoveItems(begin_offs, end_offs);

   if (MakeFreeSpace(end_offs - begin_offs)) {
      itm = new XrdClientReadCacheItem(buffer, begin_offs, end_offs,
                                   GetTimestampTick());
      // Mutual exclusion man!
      XrdOucMutexHelper mtx(fMutex);

      fItems.Push_back(itm);
      fTotalByteCount += itm->Size();
      fBytesSubmitted += itm->Size();
   } // if
}

//________________________________________________________________________
bool XrdClientReadCache::GetDataIfPresent(const void *buffer,
					    long long begin_offs,
					    long long end_offs,
					    bool PerfCalc, 
                                            long long &lasttakenbyte)
{
   // Copies the requested data from the cache. False if not possible

   int it;
   long bytesgot = 0;
   bool advancing = true;

   XrdOucMutexHelper mtx(fMutex);

   if (PerfCalc)
      fReadsCounter++;

   // We try to compose the requested data block by concatenating smaller
   //  blocks

   do {
      advancing = false;

      // Find a block helping us to go forward
      for (it = 0; it < fItems.GetSize(); it++) {
	 long l;

	 if (!fItems[it]) continue;

	 l = fItems[it]->GetPartialInterval(((char *)buffer)+bytesgot,
					    begin_offs+bytesgot, end_offs);

	 if (l > 0) {
	    bytesgot += l;
	    advancing = true;

	    fItems[it]->Touch(GetTimestampTick());

	    if (PerfCalc) {
	       fBytesHit += l;
	       UpdatePerfCounters();
	    }

	    if (bytesgot >= end_offs - begin_offs + 1) {
	       lasttakenbyte = begin_offs+bytesgot-1;
	       return TRUE;
	    }

	 }

      }

   } while (advancing);

   
   if (PerfCalc) {
      fMissCount++;
      UpdatePerfCounters();
   }

   if (bytesgot > 0)
      lasttakenbyte = begin_offs+bytesgot-1;

   return FALSE;
}

//________________________________________________________________________
void XrdClientReadCache::RemoveItems(long long begin_offs, long long end_offs)
{
   // To remove all the items contained in the given interval

   int it;
   XrdOucMutexHelper mtx(fMutex);

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
   XrdOucMutexHelper mtx(fMutex);

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

   XrdOucMutexHelper mtx(fMutex);

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

   XrdOucMutexHelper mtx(fMutex);

   while (fMaxCacheSize - fTotalByteCount < bytes)
      RemoveLRUItem();

   return TRUE;
}
