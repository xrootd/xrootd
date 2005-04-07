//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientXrdDNS                                                      //
//                                                                      //
// Author: G. Ganis (CERN, 2005)                                        //
//                                                                      //
// Xrd-based implementations of the DNS wrapper                         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <XrdClient/XrdClientDNS.hh>
#include <XrdClient/XrdClientString.hh>
#include <XrdNet/XrdNetDNS.hh>

static const char kIPmax = 10;

//_____________________________________________________________________________
XrdClientDNS::XrdClientDNS(const char *h)
{
   // Constructor

   host = 0;
   if (h) {
      int lh = strlen(h);
      if (lh > 0)
         if ((host = new char[lh+1]))
            strcpy(host,h);
   }
}

//_____________________________________________________________________________
int XrdClientDNS::HostAddr(int nmx, XrdClientString *ha, XrdClientString *nn)
{
   // Retrieve info about nmx (<= kIPmax) associated addresses and names

   if (!host)
      // Host undefined
      return 0;

   // Check nmx
   nmx = (nmx > 0 && nmx <= kIPmax) ? nmx : 1;

   // Number of addresses
   struct sockaddr_in ip[10];
   char *emsg;
   int n = XrdNetDNS::getHostAddr(host,(struct sockaddr *)ip, nmx, &emsg);

   // Fill address / name strings, if required
   if (ha && nn) {
      int i = 0;
      for (; i < n; i++ ) {
 
          // The address
          char buf[255];
          inet_ntop(ip[i].sin_family, &ip[i].sin_addr, buf, sizeof(buf));
          ha[i] = buf;

          // The name
          char *names[1] = {0};
          int hn = XrdNetDNS::getHostName((struct sockaddr&)ip[i],
                                           names, 1, &emsg);
          if (hn)
             nn[i] = names[0];
          else
             nn[i] = ha[i];

          // Cleanup
          if (names[0])
             free(names[0]);
      }
   }

   // We are done
   return n;
}
