//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientDNS                                                         //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
//                                                                      //
// Bridge class to use the appropriate package for DNS services         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_DNS_H
#define XRC_DNS_H

#include <XrdClient/XrdClientDNSImp.hh>

class XrdClientDNS {

private:

   XrdClientDNSImp  *fDNSImp;

public:
   XrdClientDNS(const char *h);
   virtual ~XrdClientDNS() { if (fDNSImp) delete fDNSImp;}

   // Get array of addresses assosiated 
   int HostAddr(int nmx, XrdClientString *haddr = 0, XrdClientString *hnam = 0);
};

#endif
