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

#include <XrdClient/XrdClientString.hh>

class XrdClientDNS {
private:
   char *host;

public:
   XrdClientDNS(const char *h);
   virtual ~XrdClientDNS() { if (host) delete[] host; }

   // Get array of addresses assosiated 
   int HostAddr(int nmx, XrdClientString *ha, XrdClientString *hn);
};

#endif
