/******************************************************************************/
/*                                                                            */
/*                     X r d N e t S e c u r i t y . c c                      */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//      $Id$

const char *XrdNetSecurityCVSID = "$Id$";

#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "XrdNet/XrdNetDNS.hh"
#include "XrdNet/XrdNetSecurity.hh"
#include "XrdOuc/XrdOucTrace.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

class XrdNetTextList 
{
public:

XrdNetTextList *next; 
char           *text;

     XrdNetTextList(char *newtext) {next = 0; text = strdup(newtext);}
    ~XrdNetTextList() {if (text) free(text);}
};

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define DEBUG(x) if (eTrace) {eTrace->Beg(TraceID); cerr <<x; eTrace->End();}

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
const char *XrdNetSecurity::TraceID = "NetSecurity";

/******************************************************************************/
/*                               A d d H o s t                                */
/******************************************************************************/
  
void XrdNetSecurity::AddHost(char *hname)
{
  XrdOucNList *nlp = new XrdOucNList(hname);

// Add host object to list of authorized hosts
//
   HostList.Insert(nlp);
   DEBUG(hname <<" added to authorized hosts.");
}

/******************************************************************************/
/*                           A d d N e t G r o u p                            */
/******************************************************************************/

void XrdNetSecurity::AddNetGroup(char *gname)
{
  XrdNetTextList *tlp = new XrdNetTextList(gname);

// Add netgroup to list of valid ones
//
   tlp->next = NetGroups;
   NetGroups = tlp;

// All done
//
   DEBUG(gname <<" added to authorized netgroups.");
}

/******************************************************************************/
/*                             A u t h o r i z e                              */
/******************************************************************************/

char *XrdNetSecurity::Authorize(struct sockaddr *addr)
{
   struct sockaddr_in *ip = (struct sockaddr_in *)addr;
   char ipbuff[64], *hname, *ipname;
   XrdNetTextList *tlp;

// Convert IP address to characters (eventually,
//
   if (!(ipname = (char *)inet_ntop(ip->sin_family, (void *)&(ip->sin_addr),
       ipbuff, sizeof(ipbuff)))) return (char *)0;

// Check if we have seen this host before
//
   okHMutex.Lock();
   if ((hname = OKHosts.Find(ipname)))
      {okHMutex.UnLock(); return strdup(hname);}

// Get the hostname for this IP address
//
   if (!(hname = XrdNetDNS::getHostName(*addr))) hname = strdup(ipname);

// Check if this host is in the the appropriate netgroup, if any
//
   if ((tlp = NetGroups))
      do {if (innetgr(tlp->text, hname, 0, 0))
          return hostOK(hname, ipname, "netgroup");
         } while ((tlp = tlp->next));

// Plow through the specific host list to see if the host
//
   if (HostList.Find(hname)) return hostOK(hname, ipname, "host");

// Host is not authorized
//
   okHMutex.UnLock();
   DEBUG(hname <<" not authorized");
   free(hname);
   return 0;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                h o s t O K                                 */
/******************************************************************************/
  
char *XrdNetSecurity::hostOK(char *hname, char *ipname, const char *why)
{

// Add host to valid host table and return true. Note that the okHMutex must
// be locked upon entry and it will be unlocked upon exit.
//
   OKHosts.Add(strdup(ipname), strdup(hname), lifetime, Hash_dofree);
   okHMutex.UnLock();
   DEBUG(hname <<" authorized via " <<why);
   return hname;
}
