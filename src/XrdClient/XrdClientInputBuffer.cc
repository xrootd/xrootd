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
#include "XrdOuc/XrdOucPthread.hh"
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
XrdOucSemWait *XrdClientInputBuffer::GetSyncObjOrMakeOne(int streamid) {
   // Gets the right sync obj to wait for messages for a given streamid
   // If the semaphore is not available, it creates one.

   XrdOucSemWait *sem;

   {
      XrdOucMutexHelper mtx(fMutex);
      char buf[20];

      snprintf(buf, 20, "%d", streamid);

      sem = fSyncobjRepo.Find(buf);

      if (!sem) {
	 sem = new XrdOucSemWait(0);

         fSyncobjRepo.Rep(buf, sem);
	 return sem;

      } else
         return sem;
   }

}



//_______________________________________________________________________
XrdClientInputBuffer::XrdClientInputBuffer() {
   // Constructor

   fMsgQue.Clear();
}



//_______________________________________________________________________
int DeleteHashItem(const char *key, XrdOucSemWait *sem, void *Arg) {
   if (sem) delete sem;

   // This makes the Apply method delete the entry
   return -1;
}

XrdClientInputBuffer::~XrdClientInputBuffer() {
   // Destructor

   // Delete all the syncobjs
   {
      XrdOucMutexHelper mtx(fMutex);


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
  XrdOucSemWait *sem = 0;

   {
      XrdOucMutexHelper mtx(fMutex);
    
      fMsgQue.Push_back(m);
      sz = MexSize();
    
      // Is anybody sleeping ?
      if (m)
	 sem = GetSyncObjOrMakeOne( m->HeaderSID() );

   }

   if (sem) {
      sem->Post();
   }

   return sz;
}


//_______________________________________________________________________
XrdClientMessage *XrdClientInputBuffer::GetMsg(int streamid, int secstimeout)
{
   // Gets the first XrdClientMessage from the queue, given a matching streamid.
   // If there are no XrdClientMessages for the streamid, it waits for a number
   // of seconds for something to come

   XrdOucSemWait *sem = 0;
   XrdClientMessage *res = 0, *m = 0;

   // Find the sem where to wait for a msg
   sem = GetSyncObjOrMakeOne(streamid);

   sem->Wait(secstimeout);

   {
      // Yes, we have to lock the mtx until we finish
      XrdOucMutexHelper mtx(fMutex);

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


  return res;
}
