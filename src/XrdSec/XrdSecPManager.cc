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

#include <string>
#include <cstring>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

#include "XrdVersion.hh"
#include "XrdVersionPlugin.hh"

#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecPManager.hh"
#include "XrdSec/XrdSecProtocolhost.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdOuc/XrdOucVerName.hh"
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
bool             needTLS;
char             protid[XrdSecPROTOIDSIZE+1];
char            *protargs;
XrdSecProtocol  *(*ep)(PROTPARMS);
XrdSecProtList  *Next;

                XrdSecProtList(const char *pid, const char *parg, bool tls)
                              : needTLS(tls), ep(0), Next(0)
                      {strncpy(protid, pid, sizeof(protid)-1);
                       protid[XrdSecPROTOIDSIZE] = '\0';
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
/*                          S t a t i c   I t e m s                           */
/******************************************************************************/
  
namespace
{
XrdSysMutex pmMutex;
}

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
   XrdOucErrInfo  *erp;
   char *wp;
   int i;

// We support passing the list of protocols via Url parameter unless this is
// a proxy server as the url should be merely passed hrough. If the proxy is
// not forwarding creds, then we use our error object to prevent security
// yet from using anything but the proxy's credentials.
// to become more clever
//
   if (isProxy)
      {wp = 0;
       if (!fwdCreds) eri = 0;
      } else {
       XrdOucEnv *envP;
       if (!eri || (envP = eri->getEnv()) == 0) wp = 0;
          else wp = envP->Get("xrd.wantprot");
      }

// Get the appropriate protocol list as well as the right error object
//
   const char *wantProt = wp ? (const char *)wp : getenv("XrdSecPROTOCOL");
   erp = (eri) ? eri : &ei;

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
            {XrdSysMutexHelper pmHelper(pmMutex);
             if ((pl = Lookup(pname)) || (pl = ldPO(erp, 'c', pname)))
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
   bool reqTLS = false;

// Make sure we did not overflow the protocol stack
//
   if (!protnum)
      {eMsg->setErrInfo(-1, "XrdSec: Too many protocols defined.");
       return 0;
      }

// Check if this protocol need TLS
//
   if (parg && !strncmp(parg, "TLS:",4))
      {char pBuff[XrdSecPROTOIDSIZE+2];
       *pBuff = ' ';
       strcpy(pBuff+1, pid);  // We know it fits
       if (!tlsProt) tlsProt = strdup(pBuff);
          else {std::string tmp(tlsProt);
                tmp.append(pBuff);
                free(tlsProt);
                tlsProt = strdup(tmp.c_str());
               }
        parg += 4; // Skip 'TLS:'
        reqTLS = true;
       }

// Add this protocol to our protocol stack
//
   plp = new XrdSecProtList((char *)pid, parg, reqTLS);
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
   XrdOucPinLoader *secLib;
   XrdSecProtocol *(*ep)(PROTPARMS);
   char           *(*ip)(INITPARMS);
   const char     *sep, *libloc;
   char  poname[80], libpath[2048], *newargs, *bP;
   int i;

// Set plugin debugging if needed (this only applies to client calls)
//
   if (DebugON && pmode == 'c' && !DebugON) XrdOucEnv::Export("XRDPIHUSH", "1");

// The "host" protocol is builtin.
//
   if (!strcmp(pid, "host")) return Add(eMsg,pid,XrdSecProtocolhostObject,0);

// Form library name (versioned) and object creator name and bundle id
//
   snprintf(poname, sizeof(poname), "libXrdSec%s.so", pid);
   i = (spath ? strlen(spath) : 0);
   if (!i) {spath = ""; sep = "";}
      else sep = (spath[i-1] == '/' ? "" : "/");
   snprintf(libpath, sizeof(libpath), "%s%s%s", spath, sep, poname);
   libloc = libpath;

// Get the plugin loader.
//
   if (errP) secLib = new XrdOucPinLoader(errP, myVer, "sec.protocol", libloc);
      else   {bP = eMsg->getMsgBuff(i);
              secLib = new XrdOucPinLoader(bP,i,myVer, "sec.protocol", libloc);
             }

// Get the protocol object creator.
//
   if (eMsg) eMsg->setErrInfo(0, "");
   snprintf(poname, sizeof(poname), "XrdSecProtocol%sObject", pid);
   if (!(ep = (XrdSecProtocol *(*)(PROTPARMS))secLib->Resolve(poname)))
      {secLib->Unload(true); return 0;}

// Get the protocol initializer
//
   sprintf(poname, "XrdSecProtocol%sInit", pid);
   if (!(ip = (char *(*)(INITPARMS))secLib->Resolve(poname)))
      {secLib->Unload(true); return 0;}

// Get the true path and do some debugging
//
   libloc = secLib->Path();
   DEBUG("Loaded " <<pid <<" protocol object from " <<libpath);

// Invoke the one-time initialization
//
   if (!(newargs = ip(pmode, (pmode == 'c' ? 0 : parg), eMsg)))
      {if (!*(eMsg->getErrText()))
          {const char *eTxt[] = {"XrdSec: ", pid, 
                       " initialization failed in sec.protocol ", libloc};
           eMsg->setErrInfo(-1, eTxt, sizeof(eTxt));
          }
       secLib->Unload(true);
       return 0;
      }

// Add this protocol to our protocol stack
//
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
