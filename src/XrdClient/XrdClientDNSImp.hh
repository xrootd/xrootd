//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientDNSImp                                                      //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
//                                                                      //
// Abstract class for concrete implementations of the DNS wrapper       //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_DNSIMP_H
#define XRC_DNSIMP_H

class XrdClientString;

class XrdClientDNSImp {

public:
   XrdClientDNSImp() {}
   virtual ~XrdClientDNSImp() {}

   // Get array of addresses assosiated 
   virtual int HostAddr(int nmx, XrdClientString *ha, XrdClientString *hn) = 0;
};

#endif
