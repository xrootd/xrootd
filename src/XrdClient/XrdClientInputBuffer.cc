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
pthread_cond_t *XrdClientInputBuffer::GetSyncObjOrMakeOne(int streamid) {
   // Gets the right sync obj to wait for messages for a given streamid
   // If the semaphore is not available, it creates one.

   pthread_cond_t *cnd;

   {
      XrdClientMutexLocker mtx(fMutex);
      char buf[20];

      snprintf(buf, 20, "%d", streamid);

      cnd = fSyncobjRepo.Find(buf);

      if (!cnd) {
	 cnd = new pthread_cond_t;
	 pthread_cond_init(cnd, 0);

         fSyncobjRepo.Rep(buf, cnd);
	 return cnd;

      } else
         return cnd;
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

   fMsgQue.Clear();
}



//_______________________________________________________________________
int DeleteHashItem(const char *key, pthread_cond_t *cnd, void *Arg) {
   if (cnd) pthread_cond_destroy(cnd);
   return -1;
}
XrdClientInputBuffer::~XrdClientInputBuffer() {
   // Destructor

   // Delete all the syncobjs
   {
      XrdClientMutexLocker mtx(fMutex);

      fSyncobjRepo.Apply(DeleteHashItem, 0);

   }


   pthread_mutex_destroy(&fMutex);
}

//_______________________________________________________________________
int XrdClientInputBuffer::PutMsg(XrdClientMessage* m)
{
   // Put message in the list
  //XrdClientMutexLocker cndmtx(fCndMutex);
  int sz;
  pthread_cond_t *cnd = 0;

   {
      XrdClientMutexLocker mtx(fMutex);
    
      fMsgQue.Push_back(m);
      sz = MexSize();
    
   // Is anybody sleeping ?
   if (m)
      cnd = GetSyncObjOrMakeOne( m->HeaderSID() );

   }

   if (cnd) {
      pthread_mutex_lock(&fCndMutex);
      pthread_cond_signal(cnd);
      pthread_mutex_unlock(&fCndMutex);
   }

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
   //XrdClientMutexLocker cndmtx(fCndMutex);

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
      time_t now;
      struct timespec timeout;

      // Find the cond where to wait for a msg
      cv = GetSyncObjOrMakeOne(streamid);

      for (int k = 0; k < secstimeout; k++) {
      now = time(0);
      timeout.tv_sec = now+2;
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
