//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientXrdFactory                                                  //
//                                                                      //
// Author: F.Furano (INFN, 2005), G. Ganis (CERN, 2005)                 //
//                                                                      //
// XRD factory implementation                                           //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_XRDFACTORY_H
#define XRC_XRDFACTORY_H

#include <XrdClient/XrdClientFactory.hh>

class XrdClientUrlInfo;
class XrdClientSockImp;
class XrdClientDNSImp;
class XrdClientMutexImp;
class XrdClientCondImp;
class XrdClientThreadImp;
class XrdClientSemaphoreImp;

class XrdClientXrdFactory : public XrdClientFactory {

private:
   char *name;

public:
   XrdClientXrdFactory(const char *n = "XRD");
   virtual ~XrdClientXrdFactory() { if (name) delete[] name; }

   const char *Name() const { return (const char *)name; }

   XrdClientSockImp *CreateSockImp(XrdClientUrlInfo u, int ws = 0);
   XrdClientDNSImp  *CreateDNSImp(const char *h);
   XrdClientMutexImp *CreateMutexImp();
   XrdClientCondImp *CreateCondImp();
   XrdClientThreadImp *CreateThreadImp();
   XrdClientSemaphoreImp *CreateSemaphoreImp(int value);
};

#endif
