/******************************************************************************/
/*                                                                            */
/*                     X r d S e c P M a n a g e r . c c                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

const char *XrdSecPManagerCVSID = "$Id$";

// Bypass Solaris ELF madness
//
#if (defined(SUNCC) || defined(SUN)) 
#include <sys/isa_defs.h>
#if defined(_ILP32) && (_FILE_OFFSET_BITS != 32)
#undef  _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#undef  _LARGEFILE_SOURCE
#endif
#endif

#include <dlfcn.h>
#include <link.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <iostream.h>
  
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecPManager.hh"
#include "XrdSec/XrdSecProtocolhost.hh"
#include "XrdOuc/XrdOucErrInfo.hh"

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

XrdSecPMask_t      protnum;
char               protid[8];
const char        *protargs;
XrdSecProtocol    *protp;
XrdSecProtList    *Next;

                XrdSecProtList(char *pid, XrdSecProtocol *pp)
                      {int i;
                       strncpy(protid, pid, sizeof(protid)-1);
                       protid[4] = '\0';   protp = pp; Next = 0;
                       protargs = pp->getParms(i);
                      }
               ~XrdSecProtList() {} // ProtList objects never get freed!
};

/******************************************************************************/
/*                X r d S e c P M a n a g e r   M e t h o d s                 */
/******************************************************************************/
/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/
  
XrdSecProtocol *XrdSecPManager::Find(const char *pid,   // In
                                     const char *parg)  // In
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
   if (plp)
      do {if (!strcmp(plp->protid,   pid) && parg
          &&  !strcmp(plp->protargs, parg)) break;
         } while(plp = plp->Next);

   if (plp) return plp->protp;
   return (XrdSecProtocol *)0;
}
  
XrdSecProtocol *XrdSecPManager::Find(const         char  *pid,   // In
                                                   char **parg,  // Out
                                     XrdSecPMask_t       *pnum)  // Out
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
   if (plp)
      do {if (!strcmp(plp->protid,   pid)) break;
         } while(plp = plp->Next);

   if (!plp) return (XrdSecProtocol *)0;
   if (parg) *parg = (char *)plp->protargs;
   if (pnum) *pnum = plp->protnum;
   return plp->protp;
}

/******************************************************************************/
/*                                   G e t                                    */
/******************************************************************************/

XrdSecProtocol *XrdSecPManager::Get(char *sectoken)
{
   char savec, *nscan, *pname, *pargs, *bp = sectoken;
   XrdSecProtocol *pp;
   XrdOucErrInfo erp;

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
         if ((pp = Find(pname, pargs))
         || ( pp = Load(&erp, 0, pname, pargs, 'c')))
            {DEBUG("Reusing " <<pname <<" protocol, args='"
                   <<(pargs ? pargs : "") <<"'");
             return pp;
            }
         if (erp.getErrInfo() != ENOENT)
            cerr <<erp.getErrText() <<endl;
         if (!nscan) break;
         *nscan = '&'; bp = nscan;
         }
    return (XrdSecProtocol *)0;
}

/******************************************************************************/
/*                                  L o a d                                   */
/******************************************************************************/

XrdSecProtocol *XrdSecPManager::Load(XrdOucErrInfo *eMsg,  // In
                                     const char    *spath, // In
                                     const char    *pid,   // In
                                     const char    *parg,  // In
                                     const char     pmode) // In 'c' | 's'
{
   static XrdSecProtocolhost HostProtocol;
   void *libhandle;
   XrdSecProtocol *(*ep)(XrdOucErrInfo *, const char, const char *, const char *);
   XrdSecProtocol *Prot;
   char *tlist[8], poname[80], libfn[80], libpath[2048], *libloc;
   int i, k = 1;

// The "host" protocol is builtin.
//
   if (!strcmp(pid, "host")) return Add(eMsg, pid, &HostProtocol);

// Preset the tlist array and name of routine to load
//
   tlist[0] = (char *)"XrdSec: ";
   sprintf(poname, "XrdSecProtocol%sObject", pid);

// Form library name
//
   snprintf(libfn, sizeof(libfn)-1, "libXrdSec%s.so", pid);
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

// For clients, verify if the library exists (don't complain, if not)
//
   if (pmode == 'c')
      {struct stat buf;
       if (!stat((const char *)libloc, &buf) && errno == ENOENT)
          {eMsg->setErrInfo(ENOENT, ""); return 0;}
      }

// Open the security library
//
   if (!(libhandle = dlopen(libloc, RTLD_NOW)))
      {tlist[k++] = dlerror();
       tlist[k++] = (char *)" opening shared library ";
       tlist[k++] = libloc;
       eMsg->setErrInfo(-1, tlist, k);
       return 0;
      }

// Get the protocol object creator
//
   if (!(ep = (XrdSecProtocol *(*)(XrdOucErrInfo *, const char, const char *,
               const char *))dlsym(libhandle, poname)))
      {tlist[k++] = dlerror();
       tlist[k++] = (char *)" finding ";
       tlist[k++] = poname;
       tlist[k++] = (char *)" in ";
       tlist[k++] = libloc;
       eMsg->setErrInfo(-1, tlist, k);
       return 0;
      }

// Get the protocol object
//
   if (!(Prot = (*ep)(eMsg, pmode, pid, parg))) return 0;

// Add this protocol to our protocol stack
//
   return Add(eMsg, pid, Prot);
}
 
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                   A d d                                    */
/******************************************************************************/
  
XrdSecProtocol *XrdSecPManager::Add(XrdOucErrInfo  *eMsg, const char *pid,
                                    XrdSecProtocol *Prot)
{
   XrdSecProtList *plp;

// Add this protocol to our protocol stack
//
   plp = new XrdSecProtList((char *)pid, Prot);
   myMutex.Lock();
   if (Last) {Last->Next = plp; Last = plp;}
      else First = Last = plp;
   plp->protnum = protnum; protnum = protnum<<1;
   myMutex.UnLock();

// Make sure we did not overflow the protocol stack
//
   if (!protnum)
      {eMsg->setErrInfo(-1, "XrdSec: Too many protocols defined.");
       return 0;
      }
   return Prot;
}
