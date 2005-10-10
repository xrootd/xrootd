//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientThread                                                      //
//                                                                      //
// An user friendly thread wrapper                                      //
// Author: F.Furano (INFN, 2005)                                        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//           $Id$

const char *XrdClientThreadCVSID = "$Id$";

#include "XrdClient/XrdClientThread.hh"

//_____________________________________________________________________________
void * XrdClientThreadDispatcher(void * arg)
{
   // This function is launched by the thread implementation. Its purpose
   // is to call the actual thread body, passing to it the original arg and
   // a pointer to the thread object which launched it.

   XrdClientThread::XrdClientThreadArgs *args = (XrdClientThread::XrdClientThreadArgs *)arg;

   args->threadobj->SetCancelDeferred();
   args->threadobj->SetCancelOn();

   if (args->threadobj->ThreadFunc)
      return args->threadobj->ThreadFunc(args->arg, args->threadobj);

   return 0;

}



