//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientInputBuffer                                                 //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Buffer for incoming messages (responses)                             //
//  Handles the waiting (with timeout) for a message to come            //
//   belonging to a logical streamid                                    //
//  Multithread friendly                                                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include "XrdClient/XrdClientInputBuffer.hh"
#include "XrdClient/XrdClientMutexLocker.hh"
#include "XrdClient/XrdClientDebug.hh"
#include <sys/time.h>
#include <stdio.h>

using namespace std;

//________________________________________________________________________
int XrdClientInputBuffer::MsgForStreamidCnt(int streamid)
{
    // Counts the number of messages belonging to the given streamid

    int cnt = 0;
    XrdClientMessage *m = 0;

    for (fMsgIter = 0; fMsgIter < fMsgQue.GetSize(); ++fMsgIter) {
       m = fMsgQue[fMsgIter];
       if (m->MatchStreamid(streamid))
          cnt++;
    }

    return cnt;
}

//________________________________________________________________________
XrdClientCond *XrdClientInputBuffer::GetSyncObjOrMakeOne(int streamid) {
   // Gets the right sync obj to wait for messages for a given streamid
   // If the semaphore is not available, it creates one.

   XrdClientCond *cnd;

   {
      XrdClientMutexLocker mtx(fMutex);
      char buf[20];

      snprintf(buf, 20, "%d", streamid);

      cnd = fSyncobjRepo.Find(buf);

      if (!cnd) {
	 cnd = new XrdClientCond();

         fSyncobjRepo.Rep(buf, cnd);
	 return cnd;

      } else
         return cnd;
   }

}



//_______________________________________________________________________
XrdClientInputBuffer::XrdClientInputBuffer() {
   // Constructor

   fMsgQue.Clear();
}



//_______________________________________________________________________
int DeleteHashItem(const char *key, XrdClientCond *cnd, void *Arg) {
   if (cnd) delete cnd;

   // This makes the Apply method delete the entry
   return -1;
}

XrdClientInputBuffer::~XrdClientInputBuffer() {
   // Destructor

   // Delete all the syncobjs
   {
      XrdClientMutexLocker mtx(fMutex);


      // Delete the content of the queue
      for (fMsgIter = 0; fMsgIter < fMsgQue.GetSize(); ++fMsgIter) {
	 delete fMsgQue[fMsgIter];
	 fMsgQue[fMsgIter] = 0;
      }

      fMsgQue.Clear();

      // Delete all the syncobjs
      fSyncobjRepo.Apply(DeleteHashItem, 0);

   }


}

//_______________________________________________________________________
int XrdClientInputBuffer::PutMsg(XrdClientMessage* m)
{
   // Put message in the list
  int sz;
  XrdClientCond *cnd = 0;

   {
      XrdClientMutexLocker mtx(fMutex);
    
      fMsgQue.Push_back(m);
      sz = MexSize();
    
   // Is anybody sleeping ?
   if (m)
      cnd = GetSyncObjOrMakeOne( m->HeaderSID() );

   }

   if (cnd) {
      cnd->Signal();
   }

   return sz;
}


//_______________________________________________________________________
XrdClientMessage *XrdClientInputBuffer::GetMsg(int streamid, int secstimeout)
{
   // Gets the first XrdClientMessage from the queue, given a matching streamid.
   // If there are no XrdClientMessages for the streamid, it waits for a number
   // of seconds for something to come

   XrdClientCond *cv;
   XrdClientMessage *res, *m;

   res = 0;

 
   {
      XrdClientMutexLocker mtx(fMutex);

      if (MsgForStreamidCnt(streamid) > 0) {

         // If there are messages to dequeue, we pick the oldest one
         for (fMsgIter = 0; fMsgIter < fMsgQue.GetSize(); ++fMsgIter) {
            m = fMsgQue[fMsgIter];
            
            if ((!m) || m->IsError() || m->MatchStreamid(streamid)) {
               res = fMsgQue[fMsgIter];
	       fMsgQue.Erase(fMsgIter);
               if (!m) return 0;
	       break;
            }
         }
      } 
   }

   if (!res) {

      // Find the cond where to wait for a msg
      cv = GetSyncObjOrMakeOne(streamid);

      for (int k = 0; k < secstimeout; k++) {

      cv->TimedWait(1);

      {
	 // Yes, we have to lock the mtx until we finish
	 XrdClientMutexLocker mtx(fMutex);

	 // We were awakened. Or the timeout elapsed. The mtx is again locked.
	 // If there are messages to dequeue, we pick the oldest one
	 for (fMsgIter = 0; fMsgIter < fMsgQue.GetSize(); ++fMsgIter) {
	    m = fMsgQue[fMsgIter];
	    if ((!m) || m->IsError() || m->MatchStreamid(streamid)) {
	       res = fMsgQue[fMsgIter];
	       fMsgQue.Erase(fMsgIter);
               if (!m) return 0;
	       break;
	    }
	 }
      }

      if (res) break;
      } // for

   }


  return res;
}
