
#include "XrdClient/XrdClientThreadImp.hh"
#include "XrdClient/XrdClientThread.hh"

//_____________________________________________________________________________
extern "C" void * XrdClientThreadDispatcher(void * arg)
{
   // This function is launched by the thread implementation. Its purpose
   // is to call the actual thread body, passing to it the original arg and
   // a pointer to the thread object which launched it.

   XrdClientThreadArgs *args = (XrdClientThreadArgs *)arg;

   args->threadobj->SetCancelDeferred();
   args->threadobj->SetCancelOn();

   if (args->threadobj->fThreadImp->ThreadFunc)
      return args->threadobj->fThreadImp->ThreadFunc(args->arg, args->threadobj);

   return 0;

}



