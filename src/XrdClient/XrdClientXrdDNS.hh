//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientXrdDNS                                                      //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
//                                                                      //
// Xrd-based implementations of the DNS wrapper                         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_XRDDNS_H
#define XRC_XRDDNS_H

#include <XrdClient/XrdClientDNSImp.hh>

class XrdClientString;

class XrdClientXrdDNS : public XrdClientDNSImp {
private:
   char *host;

public:
   XrdClientXrdDNS(const char *h);
   virtual ~XrdClientXrdDNS() { if (host) delete[] host; }

   // Get array of addresses assosiated 
   int HostAddr(int nmx, XrdClientString *ha, XrdClientString *hn);
};

#endif
