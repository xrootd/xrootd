/******************************************************************************/
/*                                                                            */
/*                     X r d O u c S e c u r i t y . c c                      */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//      $Id$ 

const char *XrdOucSecurityCVSID = "$Id$";

#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "XrdOuc/XrdOucSecurity.hh"
#include "XrdOuc/XrdOucTrace.hh"

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

class XrdOucTextList 
{
public:

XrdOucTextList *next; 
char          *text;

     XrdOucTextList(char *newtext) {next = 0; text = strdup(newtext);}
    ~XrdOucTextList() {if (text) free(text);}
};

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define DEBUG(x) if (eTrace) {eTrace->Beg(TraceID); cerr <<x; eTrace->End();}

#ifdef __linux__
#define GETHOSTBYADDR(haddr,hlen,htype,rbuff,cbuff,cblen, rpnt,pretc) \
     (gethostbyaddr_r(haddr,hlen,htype,rbuff,cbuff,cblen,&rpnt,pretc) == 0)
#else
#define GETHOSTBYADDR(haddr, hlen, htype, rbuff, cbuff, cblen, rpnt, pretc) \
(rpnt=gethostbyaddr_r(haddr, hlen, htype, rbuff, cbuff, cblen,       pretc))
#endif

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
const char *XrdOucSecurity::TraceID = "OucSecurity: ";

/******************************************************************************/
/*                               A d d H o s t                                */
/******************************************************************************/
  
void XrdOucSecurity::AddHost(char *hname)
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

void XrdOucSecurity::AddNetGroup(char *gname)
{
  XrdOucTextList *tlp = new XrdOucTextList(gname);

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

char *XrdOucSecurity::Authorize(struct sockaddr_in *addr)
{
   char *hname, *ipname = inet_ntoa(addr->sin_addr);
   XrdOucTextList *tlp;

// Check if we have seen this host before
//
   okHMutex.Lock();
   if (hname = OKHosts.Find((const char *)ipname))
      {okHMutex.UnLock(); return strdup(hname);}

// Get the hostname for this IP address
//
   if (!(hname = getHostName(*addr))) hname = strdup(ipname);

// Check if this host is in the the appropriate netgroup, if any
//
   if (tlp = NetGroups)
      do {if (innetgr((const char *)tlp->text, (const char *)hname,
                      (const char *)0,         (const char *)0))
          return hostOK(hname, ipname, "netgroup");
         } while (tlp = tlp->next);

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
/*                           g e t H o s t N a m e                            */
/******************************************************************************/

char *XrdOucSecurity::getHostName(struct sockaddr_in &addr)
{
   struct hostent hent, *hp;
   char *hname, hbuff[1024];
   int rc;

// Convert it to a host name
//
   if (GETHOSTBYADDR((const char *)&addr.sin_addr, sizeof(addr.sin_addr),
                     AF_INET, &hent, hbuff, sizeof(hbuff), hp, &rc))
             hname = LowCase(strdup(hp->h_name));
        else hname = strdup(inet_ntoa(addr.sin_addr));

// Return the name
//
   return hname;
}

/******************************************************************************/
/*                                h o s t O K                                 */
/******************************************************************************/
  
char *XrdOucSecurity::hostOK(char *hname, char *ipname, const char *why)
{

// Add host to valid host table and return true. Note that the okHMutex must
// be locked upon entry and it will be unlocked upon exit.
//
   OKHosts.Add((const char *)strdup(ipname), strdup(hname),
               (const int)lifetime, Hash_dofree);
   okHMutex.UnLock();
   DEBUG(hname <<" authorized via " <<why);
   return hname;
}

/******************************************************************************/
/*                               L o w C a s e                                */
/******************************************************************************/
  
char *XrdOucSecurity::LowCase(char *str)
{
   char *sp = str;

   while(*sp) {if (isupper((int)*sp)) *sp = (char)tolower((int)*sp); sp++;}

   return str;
}
