/******************************************************************************/
/*                                                                            */
/*                     X r d S e c P M a n a g e r . c c                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "XrdVersion.hh"

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecPManager.hh"
#include "XrdSec/XrdSecProtocolhost.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdNet/XrdNetAddrInfo.hh"

#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                 M i s c e l l a n e o u s   D e f i n e s                  */
/******************************************************************************/

#define DEBUG(x) {if (DebugON) cerr <<"sec_PM: " <<x <<endl;}
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdSecProtList
{
public:

XrdSecPMask_t    protnum;
char             protid[XrdSecPROTOIDSIZE+1];
char            *protargs;
XrdSecProtocol  *(*ep)(PROTPARMS);
XrdSecProtList  *Next;

                XrdSecProtList(const char *pid, const char *parg)
                      {strncpy(protid, pid, sizeof(protid)-1);
                       protid[XrdSecPROTOIDSIZE] = '\0'; ep = 0; Next = 0;
                       protargs = (parg ? strdup(parg): (char *)"");
                      }
               ~XrdSecProtList() {} // ProtList objects never get freed!
};

/******************************************************************************/
/*                        V e r s i o n   N u m b e r                         */
/******************************************************************************/

// Note that these would properly belong in XrdSecClient.cc and XrdSecServer.cc
// However, as this is the object common to both, we consolidate them here.
  
XrdVERSIONINFO(XrdSecGetProtocol,secprot);

XrdVERSIONINFO(XrdSecgetService,secserv);

/******************************************************************************/
/*                X r d S e c P M a n a g e r   M e t h o d s                 */
/******************************************************************************/
/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/
  
XrdSecPMask_t XrdSecPManager::Find(const char *pid, char **parg)
{
   XrdSecProtList *plp;

   if ((plp = Lookup(pid)))
      {if (parg) *parg = plp->protargs;
       return plp->protnum;
      }
   return 0;
}

/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/

XrdSecProtocol *XrdSecPManager::Get(const char     *hname,
                                    XrdNetAddrInfo &endPoint,
                                    const char     *pname,
                                    XrdOucErrInfo  *erp)
{
   XrdSecProtList *pl;
   const char *msgv[2];

// Find the protocol and get an instance of the protocol object
//
   if ((pl = Lookup(pname)))
      {DEBUG("Using " <<pname <<" protocol, args='"
              <<(pl->protargs ? pl->protargs : "") <<"'");
       return pl->ep('s', hname, endPoint, 0, erp);
      }

// Protocol is not supported
//
   msgv[0] = pname;
   msgv[1] = " security protocol is not supported.";
   erp->setErrInfo(EPROTONOSUPPORT, msgv, 2);
   return 0;
}

XrdSecProtocol *XrdSecPManager::Get(const char       *hname,
                                    XrdNetAddrInfo   &endPoint,
                                    XrdSecParameters &secparm,
                                    XrdOucErrInfo    *eri)
{
   char secbuff[4096], *nscan, *pname, *pargs, *bp = secbuff;
   char pcomp[XrdSecPROTOIDSIZE+4], *compProt;
   XrdSecProtList *pl;
   XrdSecProtocol *pp;
   XrdOucErrInfo   ei;
   XrdOucErrInfo  *erp = (eri) ? eri : &ei;
   int i;

// We support passing the list of protocols via Url parameter
//
   char *wp = (eri && eri->getEnv()) ? eri->getEnv()->Get("xrd.wantprot") : 0;
   const char *wantProt = wp ? (const char *)wp : getenv("XrdSecPROTOCOL");

// We only scan the buffer once
//
   if (secparm.size <= 0) return (XrdSecProtocol *)0;

// Copy out the wanted protocols and frame them for easy comparison
//
   if (wantProt)
      {i = strlen(wantProt);
       compProt = (char *)malloc(i+3);
       *compProt = ',';
       strcpy(compProt+1, wantProt);
       compProt[i+1] = ','; compProt[i+2] = 0; *pcomp = ',';
      } else compProt = 0;

// Copy the string into a local buffer so that we can simplify some comparisons
// and isolate ourselves from server protocol errors.
//
   if (secparm.size < (int)sizeof(secbuff)) i = secparm.size;
      else i = sizeof(secbuff)-1;
   strncpy(secbuff, secparm.buffer, i);
   secbuff[i] = '\0';

// Find a protocol marker in the info block and check if acceptable
//
   while(*bp)
        {if (*bp != '&') {bp++; continue;}
            else if (!*(++bp) || *bp != 'P' || !*(++bp) || *bp != '=') continue;
         bp++; pname = bp; pargs = 0;
         while(*bp && *bp != ',' && *bp != '&') bp++;
         if (!*bp) nscan = 0;
            else {if (*bp == '&') {*bp = '\0'; pargs = 0; nscan = bp;}
                     else {*bp = '\0'; pargs = ++bp;
                           while (*bp && *bp != '&') bp++;
                           if (*bp) {*bp ='\0'; nscan = bp;}
                              else nscan = 0;
                          }
                  }
         if (wantProt)
            {strncpy(pcomp+1, pname, XrdSecPROTOIDSIZE);
             pcomp[XrdSecPROTOIDSIZE+1] = 0;
             strcat(pcomp, ",");
            }
         if (!wantProt || strstr(compProt, pcomp))
            {if ((pl = Lookup(pname)) || (pl = ldPO(erp, 'c', pname)))
                {DEBUG("Using " <<pname <<" protocol, args='"
                       <<(pargs ? pargs : "") <<"'");
                 if ((pp = pl->ep('c', hname, endPoint, pargs, erp)))
                    {if (nscan) {i = nscan - secbuff;
                                 secparm.buffer += i; secparm.size -= i;
                                } else secparm.size = -1;
                     if (compProt) free(compProt);
                     return pp;
                    }
                }
             if (erp->getErrInfo() != ENOENT) cerr <<erp->getErrText() <<endl;
            } else {DEBUG("Skipping " <<pname <<" only want " <<wantProt);}
         if (!nscan) break;
         *nscan = '&'; bp = nscan;
         }
    secparm.size = -1;
    if (compProt) free(compProt);
    return (XrdSecProtocol *)0;
}
 
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
XrdSecProtList *XrdSecPManager::Add(XrdOucErrInfo  *eMsg, const char *pid,
                                    XrdSecProtocol *(*ep)(PROTPARMS),
                                    const char *parg)
{
   XrdSecProtList *plp;

// Make sure we did not overflow the protocol stack
//
   if (!protnum)
      {eMsg->setErrInfo(-1, "XrdSec: Too many protocols defined.");
       return 0;
      }

// Add this protocol to our protocol stack
//
   plp = new XrdSecProtList((char *)pid, parg);
   plp->ep = ep;
   myMutex.Lock();
   if (Last) {Last->Next = plp; Last = plp;}
      else First = Last = plp;
   plp->protnum = protnum; 
   if (protnum & 0x40000000) protnum = 0;
      else protnum = protnum<<1;
   myMutex.UnLock();

// All went well
//
   return plp;
}

/******************************************************************************/
/*                                  l d P O                                   */
/******************************************************************************/

#define INITPARMS const char, const char *, XrdOucErrInfo *
  
XrdSecProtList *XrdSecPManager::ldPO(XrdOucErrInfo *eMsg,  // In
                                     const char     pmode, // In 'c' | 's'
                                     const char    *pid,   // In
                                     const char    *parg,  // In
                                     const char    *spath) // In
{
   extern XrdSecProtocol *XrdSecProtocolhostObject(PROTPARMS);
   static XrdVERSIONINFODEF(clVer, SecClnt, XrdVNUMBER, XrdVERSION);
   static XrdVERSIONINFODEF(srVer, SecSrvr, XrdVNUMBER, XrdVERSION);
   XrdVersionInfo *myVer = (pmode == 'c' ? &clVer : &srVer);
   XrdSysPlugin   *secLib;
   XrdSecProtocol *(*ep)(PROTPARMS);
   char           *(*ip)(INITPARMS);
   char  poname[80], libfn[80], libpath[2048], *libloc, *newargs, *bP;
   int i;

// Set plugin debugging if needed (this only applies to client calls)
//
   if (DebugON && pmode == 'c' && !DebugON) XrdOucEnv::Export("XRDPIHUSH", "1");

// The "host" protocol is builtin.
//
   if (!strcmp(pid, "host")) return Add(eMsg,pid,XrdSecProtocolhostObject,0);

// Form library name
//
   snprintf(libfn, sizeof(libfn)-1, "libXrdSec%s%s", pid, LT_MODULE_EXT );
   libfn[sizeof(libfn)-1] = '\0';

// Determine path
//
   if (!spath || (i = strlen(spath)) < 2) libloc = libfn;
      else {char *sep = (spath[i-1] == '/' ? (char *)"" : (char *)"/");
            snprintf(libpath, sizeof(libpath)-1, "%s%s%s", spath, sep, libfn);
            libpath[sizeof(libpath)-1] = '\0';
            libloc = libpath;
           }
   DEBUG("Loading " <<pid <<" protocol object from " <<libloc);

// For clients, verify if the library exists (don't complain, if not).
//
   if (pmode == 'c')
      {struct stat buf;
       if (!stat(libloc, &buf) && errno == ENOENT)
          {eMsg->setErrInfo(ENOENT, ""); return 0;}
      }

// Get the plugin object. We preferentially use a message object if it exists
//
   if (errP) secLib = new XrdSysPlugin(errP, libloc, "sec.protocol", myVer);
       else {bP = eMsg->getMsgBuff(i);
             secLib = new XrdSysPlugin(bP,i, libloc, "sec.protocol", myVer);
            }
   eMsg->setErrInfo(0, "");

// Get the protocol object creator
//
   sprintf(poname, "XrdSecProtocol%sObject", pid);
   if (!(ep = (XrdSecProtocol *(*)(PROTPARMS))secLib->getPlugin(poname)))
      {delete secLib;
       return 0;
      }

// Get the protocol initializer
//
   sprintf(poname, "XrdSecProtocol%sInit", pid);
   if (!(ip = (char *(*)(INITPARMS))secLib->getPlugin(poname)))
      {delete secLib;
       return 0;
      }

// Invoke the one-time initialization
//
   if (!(newargs = ip(pmode, (pmode == 'c' ? 0 : parg), eMsg)))
      {if (!*(eMsg->getErrText()))
          {const char *eTxt[] = {"XrdSec: ", pid, 
                       " initialization failed in sec.protocol ", libloc};
           eMsg->setErrInfo(-1, eTxt, sizeof(eTxt));
          }
       delete secLib;
       return 0;
      }

// Add this protocol to our protocol stack
//
   secLib->Persist();
   delete secLib;
   return Add(eMsg, pid, ep, newargs);
}
 
/******************************************************************************/
/*                                L o o k u p                                 */
/******************************************************************************/
  
XrdSecProtList *XrdSecPManager::Lookup(const char *pid)   // In
{
   XrdSecProtList *plp;

// Since we only add protocols and never remove them, we need only to lock
// the protocol list to get the first item.
//
   myMutex.Lock();
   plp = First;
   myMutex.UnLock();

// Now we can go and find a matching protocol
//
   while(plp && strcmp(plp->protid, pid)) plp = plp->Next;

   return plp;
}
