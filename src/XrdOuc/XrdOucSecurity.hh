#ifndef __OOUC_SECURITY__
#define __OOUC_SECURITY__
/******************************************************************************/
/*                                                                            */
/*                       o o u c _ S e c u r i t y . h                        */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

#include <ctype.h>
#include <stdlib.h>
  
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucNList.hh"
#include "XrdOuc/XrdOucPthread.hh"

class XrdOucTextList;
class XrdOucTrace;

class XrdOucSecurity
{
public:
  
void  AddHost(char *hname);

void  AddNetGroup(char *hname);

char *Authorize(struct sockaddr_in *addr);

void  Trace(XrdOucTrace *et=0) {eTrace = et;}

     XrdOucSecurity() {NetGroups = 0; eTrace = 0; lifetime = 8*60*60;}
    ~XrdOucSecurity() {}

private:

char *getHostName(struct sockaddr_in &addr);
char *hostOK(char *hname, char *ipname, const char *why);
char *LowCase(char *str);

XrdOucNList_Anchor        HostList;

XrdOucTextList           *NetGroups;

XrdOucHash<char>          OKHosts;
XrdOucMutex               okHMutex;
XrdOucTrace              *eTrace;

int                      lifetime;
static const char       *TraceID;
};
#endif
