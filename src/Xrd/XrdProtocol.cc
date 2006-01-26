/******************************************************************************/
/*                                                                            */
/*                        X r d P r o t o c o l . c c                         */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//        $Id$  

const char *XrdProtocolCVSID = "$Id$";

// Bypass Solaris ELF madness
//
#if defined(__solaris__)
#include <sys/isa_defs.h>
#if defined(_ILP32) && (_FILE_OFFSET_BITS != 32)
#undef  _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#undef  _LARGEFILE_SOURCE
#endif
#endif

#include <dlfcn.h>
#ifndef __macos__
#include <link.h>
#endif

#include "XrdOuc/XrdOucError.hh"
#include "Xrd/XrdLink.hh"
#include "Xrd/XrdPoll.hh"
#include "Xrd/XrdProtocol.hh"
#include "Xrd/XrdTrace.hh"
 
/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

extern XrdOucError XrdLog;

extern XrdOucTrace XrdTrace;

XrdProtocol *XrdProtocol_Select::Protocol[XRD_PROTOMAX] = {0};
char        *XrdProtocol_Select::ProtName[XRD_PROTOMAX] = {0};
int          XrdProtocol_Select::ProtPort[XRD_PROTOMAX] = {0};

int          XrdProtocol_Select::ProtoCnt = 0;
  
/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/
  
#define DISCARD_LINK(x,y) x->setEtext(y); x->Close(); return -1

/******************************************************************************/
/*       x r d _ P r o t o c o l _ I n f o   C o p y   O p e r a t o r        */
/******************************************************************************/
  
XrdProtocol_Config::XrdProtocol_Config(XrdProtocol_Config &rhs)
{
eDest     = rhs.eDest;
NetTCP    = rhs.NetTCP;
BPool     = rhs.BPool;
Sched     = rhs.Sched;
Trace     = rhs.Trace;

Format    = rhs.Format;
Port      = rhs.Port;
ConnOptn  = rhs.ConnOptn;
ConnLife  = rhs.ConnLife;
readWait  = rhs.readWait;
idleWait  = rhs.idleWait;
argc      = rhs.argc;
argv      = rhs.argv;
DebugON   = rhs.DebugON;
}

/******************************************************************************/
/*                   x r d _ P r o t o c o l _ S e l e c t                    */
/******************************************************************************/
/******************************************************************************/
/*            C o n s t r u c t o r   a n d   D e s t r u c t o r             */
/******************************************************************************/
  
 XrdProtocol_Select::XrdProtocol_Select(int port) :
                     XrdProtocol("protocol selection") {myPort = port;}

XrdProtocol_Select::~XrdProtocol_Select() {}
 
/******************************************************************************/
/*                                  L o a d                                   */
/******************************************************************************/

int XrdProtocol_Select::Load(const char *lname, const char *pname,
                             char *parms, XrdProtocol_Config *pi)
{
   XrdProtocol *xp;
   int i, j, port = pi->Port;

// Trace this load if so wanted
//
   if (TRACING(TRACE_DEBUG))
      {XrdTrace.Beg("Protocol");
       cerr <<"loading protocol " <<pname;
       XrdTrace.End();
      }

// First check to see that we haven't exceeded our protocol count
//
   if (ProtoCnt >= XRD_PROTOMAX)
      {XrdLog.Emsg("Protocol", "Too many protocols have been defined.");
       return 0;
      }

// Obtain an instance of this protocol
//
   if (lname)  xp =    getProtocol(lname, pname, parms, pi);
      else     xp = XrdgetProtocol(pname, parms, pi);
   if (!xp) {XrdLog.Emsg("Protocol","Protocol", pname, "could not be loaded");
             return 0;
            }

// Find a port associated slot in the table
//
   for (i = ProtoCnt-1; i >= 0; i--) if (port == ProtPort[i]) break;
   for (j = ProtoCnt-1; j > i; j--)
       {ProtName[j+1] = ProtName[j];
        ProtPort[j+1] = ProtPort[j];
        Protocol[j+1] = Protocol[j];
       }

// Add protocol to our table of protocols
//
   ProtName[j+1] = strdup(pname);
   ProtPort[j+1] = port;
   Protocol[j+1] = xp;
   ProtoCnt++;
   return 1;
}
  
/******************************************************************************/
/*                               P r o c e s s                                */
/******************************************************************************/
  
int XrdProtocol_Select::Process(XrdLink *lp)
{
     XrdProtocol *pp = 0;
     int i;

// We check each protocol we have until we find one that works with this link
//
   for (i = 0; i < ProtoCnt; i++) 
       if (myPort == ProtPort[i] && (pp = Protocol[i]->Match(lp))) break;
          else if (lp->isFlawed()) {lp->Close(); return -1;}
   if (!pp) {DISCARD_LINK(lp, "matching protocol not found");}

// Now attach the new protocol object to the link
//
   lp->setProtocol(pp);

// Trace this load if so wanted
//                                                x
   if (TRACING(TRACE_DEBUG))
      {XrdTrace.Beg("Protocol");
       cerr <<"matched protocol " <<ProtName[i];
       XrdTrace.End();
      }

// Attach this link to the appropriate poller and enable it.
//
   if (!XrdPoll::Attach(lp)) {DISCARD_LINK(lp, "attach failed");}

// Take a short-cut and process the initial request as a sticky request
//
   return pp->Process(lp);
}
 
/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdProtocol_Select::Recycle(XrdLink *lp, int ctime, const char *reason)
{

// Document non-protocol errors
//
   if (lp && reason)
      XrdLog.Emsg("Protocol", lp->ID, "terminated", reason);
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/

int XrdProtocol_Select::Stats(char *buff, int blen, int do_sync)
{
    int i, k, totlen = 0;

    for (i = 0; i <ProtoCnt && blen > 0; i++)
        {k = Protocol[i]->Stats(buff, blen, do_sync);
         totlen += k; buff += k; blen -= k;
        }

    return totlen;
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                           g e t P r o t o c o l                            */
/******************************************************************************/
  
XrdProtocol *XrdProtocol_Select::getProtocol(const char *lname,
                                             const char *pname,
                                                   char *parms,
                                      XrdProtocol_Config *pi)
{
static char         *  liblist[XRD_PROTOMAX];
static void         *  libhndl[XRD_PROTOMAX];
static XrdProtocol *(*libfunc[XRD_PROTOMAX])(const char *, char *,
                                             XrdProtocol_Config *);
static int libcnt = 0;
       int i;

void *ep;

// See if the library is already opened, if not open it
//
   for (i = 0; i < libcnt; i++) if (!strcmp(lname, liblist[i])) break;
   if (i >= libcnt)
      {if (!(libhndl[i] = dlopen(lname, RTLD_NOW)))
          {XrdLog.Emsg("Protocol", dlerror(), "opening shared library", lname);
           return 0;
          }
        if (!(ep = dlsym(libhndl[i], "XrdgetProtocol")))
          {XrdLog.Emsg("Protocol",dlerror(),"finding XrdgetProtocol() in",lname);
           return 0;
          }
       libfunc[i]=(XrdProtocol *(*)(const char*,char*,XrdProtocol_Config*))ep;
       liblist[i] = strdup(lname);
       libcnt++;
      }

// Obtain an instance of the protocol object and return it
//
return libfunc[i](pname, parms, pi);
}
