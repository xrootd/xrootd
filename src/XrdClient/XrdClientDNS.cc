//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientDNS                                                         //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
//                                                                      //
// Bridge class to use the appropriate package for DNS services         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientDNS.hh"
#include "XrdClient/XrdClientFactory.hh"

//_____________________________________________________________________________
XrdClientDNS::XrdClientDNS(const char *h)
{
   // Constructor

   fDNSImp = XrdClientGetFactory()->CreateDNSImp(h);
}

//_____________________________________________________________________________
int XrdClientDNS::HostAddr(int nmx, XrdClientString *ha, XrdClientString *hn)
{
   // Get the first DNS address and names (max 10) associated with host
   // Arrays ha and hn are allocated by the caller.

   if (fDNSImp)
      return fDNSImp->HostAddr(nmx,ha,hn);
   return 0;
}
