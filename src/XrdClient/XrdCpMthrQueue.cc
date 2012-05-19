//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdCpMthrQueue                                                       //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A thread safe queue to be used for multithreaded producers-consumers //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdCpMthrQueue.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdClient/XrdClientDebug.hh"

XrdCpMthrQueue::XrdCpMthrQueue(): fWrWait(0), fReadSem(0), fWriteSem(0) {
   // Constructor

   fMsgQue.Clear();
   fTotSize = 0;
}

int XrdCpMthrQueue::PutBuffer(void *buf, long long offs, int len) {
   XrdCpMessage *m;

   fMutex.Lock();
   if (len && fTotSize > CPMTQ_BUFFSIZE)
      {fWrWait++;
       fMutex.UnLock();
       fWriteSem.Wait();
      }

   m = new XrdCpMessage;
   m->offs = offs;
   m->buf = buf;
   m->len = len;

   // Put message in the list
   //
      fMsgQue.Push_back(m);
      fTotSize += len;
    
   fMutex.UnLock();
   fReadSem.Post(); 

   return 0;
}

int XrdCpMthrQueue::GetBuffer(void **buf, long long &offs, int &len) {
   XrdCpMessage *res;

   res = 0;
 
   // If there is no data for one hour, then give up with an error
   if (!fReadSem.Wait(3600))
      {fMutex.Lock();
      // If there are messages to dequeue, we pick the oldest one
       if (fMsgQue.GetSize() > 0)
          {res = fMsgQue.Pop_front();
           if (res) {fTotSize -= res->len;
                     if (fWrWait) {fWrWait--; fWriteSem.Post();}
                    }
          }
       fMutex.UnLock();
      }


   if (res) {
      *buf = res->buf;
      len = res->len;
      offs = res->offs;
      delete res;
   }

   return (res != 0);
}


void XrdCpMthrQueue::Clear() {
   void *buf;
   int len;
   long long offs;

   while (fMsgQue.GetSize() && GetBuffer(&buf, offs, len)) {
      free(buf);
   }

   fTotSize = 0;

}

   
