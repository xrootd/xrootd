/******************************************************************************/
/*                                                                            */
/*                       X r d S e c C l i e n t . c c                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

const char *XrdSecClientCVSID = "$Id$";

#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <iostream.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Experiment/Experiment.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdSec/XrdSecPManager.hh"
#include "XrdSec/XrdSecInterface.hh"

/******************************************************************************/
/*                 M i s c e l l a n e o u s   D e f i n e s                  */
/******************************************************************************/

#define DEBUG(x) {if (DebugON) cerr <<"sec_Client: " <<x <<endl;}

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdSecProtNone : public XrdSecProtocol
{
public:
int                Authenticate  (XrdSecCredentials  *cred,
                                  XrdSecParameters  **parms,
                                  XrdSecClientName   &client,
                                  XrdOucErrInfo    *einfo=0) {return 0;}

XrdSecCredentials *getCredentials(XrdSecParameters  *parm=0,       // In
                                  XrdOucErrInfo     *einfo=0)
                                 {return new XrdSecCredentials();}

const char        *getParms(int &psize, const char *hname=0)
                           {psize = 0; return (const char *)0;}

              XrdSecProtNone() {}
             ~XrdSecProtNone() {}
};
 
/******************************************************************************/
/*                     X r d S e c G e t P r o t o c o l                      */
/******************************************************************************/
  
XrdSecProtocol *XrdSecGetProtocol(const struct sockaddr  &netaddr,
                                  const XrdSecParameters &parms,
                                        XrdOucErrInfo    *einfo)
{
   static int DebugON = (getenv("XrdSecDEBUG") ? 1 : 0);
   static XrdSecProtNone ProtNone;
   static XrdSecPManager PManager(DebugON);
   const char *noperr = "XrdSec: No authentication protocols are available.";
   struct sockaddr_in *sp = (sockaddr_in *)&netaddr;

   char sectoken[4096];
   int i;
   XrdSecProtocol *protp;

// Perform any required debugging
//
   DEBUG("protocol request for host " <<inet_ntoa(sp->sin_addr) <<" token='"
         <<(parms.size ? parms.buffer : "") <<"'");

// Check if the server wants no security.
//
   if (!parms.size || !parms.buffer[0]) return (XrdSecProtocol *)&ProtNone;

// Copy the string into a local buffer so that we can simplify some comparisons
// and isolate ourselves from server protocol errors.
//
   if (parms.size < sizeof(sectoken)) i = parms.size;
      else i = sizeof(sectoken)-1;
   strncpy(sectoken, parms.buffer, i);
   sectoken[i] = '\0';

// Find a supported protocol.
//
   if (!(protp = PManager.Get(sectoken)))
      if (einfo) einfo->setErrInfo(ENOPROTOOPT, noperr);
         else cerr <<noperr <<endl;

// All done
//
   return protp;
}

/******************************************************************************/
/*                     X r d S e c D e l P r o t o c o l                      */
/******************************************************************************/
  
void XrdSecDelProtocol(XrdSecProtocol *pp) {}
