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

#include "XrdClientInputBuffer.hh"
#include "XrdClientMutexLocker.hh"
#include "XrdClientDebug.hh"
#include <sys/time.h>

using namespace std;

//________________________________________________________________________
int XrdClientInputBuffer::MsgForStreamidCnt(int streamid)
{
    // Counts the number of messages belonging to the given streamid

    int cnt = 0;
    XrdClientMessage *m = 0;

    for (fMsgIter = fMsgQue.begin(); fMsgIter != fMsgQue.end(); ++fMsgIter) {
       m = *fMsgIter;
       if (m->MatchStreamid(streamid))
          cnt++;
    }

    return cnt;
}

//________________________________________________________________________
pthread_cond_t *XrdClientInputBuffer::GetSyncObjOrMakeOne(int streamid) {
   // Gets the right sync obj to wait for messages for a given streamid
   // If the semaphore is not available, it creates one.

   pthread_cond_t *cnd;

   StreamidCondition::iterator iter;

   {
      XrdClientMutexLocker mtx(fMutex);

      iter = fSyncobjRepo.find(streamid);

      if (iter == fSyncobjRepo.end()) {
	 cnd = new pthread_cond_t;
	 pthread_cond_init(cnd, 0);

         fSyncobjRepo[ streamid ] = cnd;
	 return cnd;

      } else
         return iter->second;
   }

}



//_______________________________________________________________________
XrdClientInputBuffer::XrdClientInputBuffer() {
   // Constructor
   pthread_mutexattr_t attr;
   int rc;


   // Initialization of cnd mutex
   rc = pthread_mutexattr_init(&attr);
   rc = pthread_mutex_init(&fCndMutex, &attr);

   if (rc) {
      Error("InputBuffer", 
            "Can't create mutex: out of system resources.");
      abort();
   }

   // Initialization of lock mutex
   rc = pthread_mutexattr_init(&attr);
   if (rc == 0) {
      rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      if (rc == 0)
	 rc = pthread_mutex_init(&fMutex, &attr);
   }
   if (rc) {
      Error("InputBuffer", 
            "Can't create mutex: out of system resources.");
      abort();
   }

   fMsgQue.clear();
}

//_______________________________________________________________________
XrdClientInputBuffer::~XrdClientInputBuffer() {
   // Destructor

   // Delete all the syncobjs

   StreamidCondition::iterator iter;

   {
      XrdClientMutexLocker mtx(fMutex);

      for (iter = fSyncobjRepo.begin();
	   iter != fSyncobjRepo.end();
	   iter++) {

	 pthread_cond_destroy(iter->second);
	 delete(iter->second);

      }

      fSyncobjRepo.clear();

   }


   pthread_mutex_destroy(&fMutex);
}

//_______________________________________________________________________
int XrdClientInputBuffer::PutMsg(XrdClientMessage* m)
{
   // Put message in the list

  int sz;
  pthread_cond_t *cnd;

   {
      XrdClientMutexLocker mtx(fMutex);
    
      fMsgQue.push_back(m);
      sz = MexSize();
   }
    
   // Is anybody sleeping ?
   cnd = GetSyncObjOrMakeOne( m->HeaderSID() );

   pthread_mutex_lock(&fCndMutex);
   pthread_cond_signal(cnd);
   pthread_mutex_unlock(&fCndMutex);
 
   return sz;
}


//_______________________________________________________________________
XrdClientMessage *XrdClientInputBuffer::GetMsg(int streamid, int secstimeout)
{
   // Gets the first XrdClientMessage from the queue, given a matching streamid.
   // If there are no XrdClientMessages for the streamid, it waits for a number
   // of seconds for something to come

   pthread_cond_t *cv;
   XrdClientMessage *res, *m;


   res = 0;
 
   {
      XrdClientMutexLocker mtx(fMutex);

      if (MsgForStreamidCnt(streamid) > 0) {

         // If there are messages to dequeue, we pick the oldest one
         for (fMsgIter = fMsgQue.begin(); fMsgIter != fMsgQue.end(); ++fMsgIter) {
            m = *fMsgIter;
            if (m->MatchStreamid(streamid)) {
               res = *fMsgIter;
	       fMsgQue.erase(fMsgIter);
	       break;
            }
         }
      } 
   }

   if (!res) {
      time_t now;
      struct timespec timeout;

      // Find the cond where to wait for a msg
      cv = GetSyncObjOrMakeOne(streamid);

      now = time(0);
      timeout.tv_sec = now + secstimeout;
      timeout.tv_nsec = 0;

      // Remember, the wait primitive internally unlocks the mutex!
      pthread_mutex_lock(&fCndMutex);
      pthread_cond_timedwait(cv, &fCndMutex, &timeout);
      pthread_mutex_unlock(&fCndMutex);

      {
	 // Yes, we have to lock the mtx until we finish
	 XrdClientMutexLocker mtx(fMutex);

	 // We were awakened. Or the timeout elapsed. The mtx is again locked.
	 // If there are messages to dequeue, we pick the oldest one
	 for (fMsgIter = fMsgQue.begin(); fMsgIter != fMsgQue.end(); ++fMsgIter) {
	    m = *fMsgIter;
	    if (m->MatchStreamid(streamid)) {
	       res = *fMsgIter;
	       fMsgQue.erase(fMsgIter);
	       break;
	    }
	 }
      }

   }

  return res;
}
