//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientFactory                                                     //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
//                                                                      //
// Factory for a few object types whose real implementations depend     //
// on the environment                                                   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_FACTORY_H
#define XRC_FACTORY_H

class XrdClientUrlInfo;

class XrdClientSockImp;
class XrdClientDNSImp;
class XrdClientMutexImp;
class XrdClientCondImp;
class XrdClientThreadImp;

class XrdClientFactory {
public:
   XrdClientFactory() {}
   virtual ~XrdClientFactory() {}

   virtual const char *Name() const = 0;

   virtual XrdClientSockImp *CreateSockImp(XrdClientUrlInfo u, int ws = 0) = 0;
   virtual XrdClientDNSImp *CreateDNSImp(const char *h) = 0;
   virtual XrdClientMutexImp *CreateMutexImp() = 0;
   virtual XrdClientCondImp *CreateCondImp() = 0;
   virtual XrdClientThreadImp *CreateThreadImp() = 0;
};

extern "C" {
   XrdClientFactory *XrdClientGetFactory();
}

#endif



