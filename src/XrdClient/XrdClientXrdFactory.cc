//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientXrdFactory                                                  //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
//                                                                      //
// XRD factory implementation                                           //
//                                                                      //
//////////////////////////////////////////////////////////////////////////
#include <string.h>

#include "XrdClient/XrdClientXrdFactory.hh"
#include "XrdClient/XrdClientXrdSock.hh"
#include "XrdClient/XrdClientXrdDNS.hh"
#include "XrdClient/XrdClientXrdMutex.hh"
#include "XrdClient/XrdClientXrdCond.hh"
#include "XrdClient/XrdClientXrdThread.hh"

extern "C" {
XrdClientFactory *XrdClientGetFactory()
{
   // Return a static instantiation of this kind of factory
   static XrdClientXrdFactory gClientXRDFactory;
   
   return &gClientXRDFactory;
}}

//______________________________________________________________________________
XrdClientXrdFactory::XrdClientXrdFactory(const char *n)
{
   // Constructor: only called by derived classes

   if (n)
      if (strlen(n))
         if ((name = new char[strlen(n)+1]))
            strcpy(name,n);
}

//______________________________________________________________________________
XrdClientSockImp *XrdClientXrdFactory::CreateSockImp(XrdClientUrlInfo u, int ws)
{
   // Return a XRD socket.

   return new XrdClientXrdSock(u, ws);
}

//______________________________________________________________________________
XrdClientDNSImp *XrdClientXrdFactory::CreateDNSImp(const char *h)
{
   // Return a XRD DNS object.

   return new XrdClientXrdDNS(h);
}

//______________________________________________________________________________
XrdClientMutexImp *XrdClientXrdFactory::CreateMutexImp()
{
   // Return an XRD Mutex object

   return new XrdClientXrdMutex();
}

//______________________________________________________________________________
XrdClientCondImp *XrdClientXrdFactory::CreateCondImp()
{
   // Return an XRD Cond object

   return new XrdClientXrdCond();
}

//______________________________________________________________________________
XrdClientThreadImp *XrdClientXrdFactory::CreateThreadImp()
{
   // Return an XRD Thread object

   return new XrdClientXrdThread();
}
