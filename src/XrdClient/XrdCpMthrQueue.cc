//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdCpMthrQueue                                                       //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A thread safe queue to be used for multithreaded producers-consumers //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include "XrdClient/XrdCpMthrQueue.hh"
#include "XrdClient/XrdClientMutexLocker.hh"
#include "XrdClient/XrdClientDebug.hh"

XrdCpMthrQueue::XrdCpMthrQueue() {
   // Constructor

   fMsgQue.Clear();
   fTotSize = 0;
}

XrdCpMthrQueue::~XrdCpMthrQueue() {
   // Destructor


}

int XrdCpMthrQueue::PutBuffer(void *buf, int len) {
   XrdCpMessage *m;
   bool wantstowait = FALSE;

   {
      XrdClientMutexLocker mtx(fMutex);
      
      if (fTotSize > CPMTQ_BUFFSIZE) wantstowait = TRUE;
   }
   
   if (wantstowait) fWriteCnd.Wait(60);

   m = new XrdCpMessage;
   m->buf = buf;
   m->len = len;

   // Put message in the list
   {
      XrdClientMutexLocker mtx(fMutex);
    
      fMsgQue.Push_back(m);
      fTotSize += len;
   }
    
   fCnd.Signal(); 

   return 0;
}

int XrdCpMthrQueue::GetBuffer(void **buf, int &len) {
   XrdCpMessage *res;

   res = 0;
 
   {
      XrdClientMutexLocker mtx(fMutex);
      
      if (fMsgQue.GetSize() > 0) {

         // If there are messages to dequeue, we pick the oldest one
	 res = fMsgQue.Pop_front();
	 if (res) fTotSize -= res->len;
      }
   }

   if (!res) {

      fCnd.Wait(300);

      {
	 XrdClientMutexLocker mtx(fMutex);

      	 if (fMsgQue.GetSize() > 0) {

	    // If there are messages to dequeue, we pick the oldest one
	    res = fMsgQue.Pop_front();
	    if (res) fTotSize -= res->len;
	 }
      }

   }

   if (res) {
      *buf = res->buf;
      len = res->len;
      delete res;
      fWriteCnd.Signal();
   }

   return (res != 0);
}


void XrdCpMthrQueue::Clear() {
   void *buf;
   int len;

   while (GetBuffer(&buf, len)) {
      free(buf);
   }

   fTotSize = 0;

}

   
