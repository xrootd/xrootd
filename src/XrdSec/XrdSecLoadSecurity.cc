/******************************************************************************/
/*                                                                            */
/*                 X r d S e c L o a d S e c u r i t y . c c                  */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <iostream>

#include "XrdVersion.hh"

#include "XProtocol/XProtocol.hh"

#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdSec/XrdSecLoadSecurity.hh"
#include "XrdSec/XrdSecProtector.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
namespace
{
static XrdVERSIONINFODEF(myVersion, XrdSecLoader, XrdVNUMBER, XrdVERSION);

XrdSysMutex    protMutex;
}

namespace XrdSecProtection
{
XrdSecProtector *theProtector = 0;
int              protRC  = 0;
}

/******************************************************************************/
/*                                  P l u g                                   */
/******************************************************************************/

namespace
{
int Plug(XrdOucPinLoader *piP, XrdSecGetProt_t *getP, XrdSecGetServ_t *ep)
{
// If we need to load the protocol factory do so now
//
   if (getP && !(*getP=(XrdSecGetProt_t)piP->Resolve("XrdSecGetProtocol")))
      return 1;

// If we do not need to load the security service we are done
//
   if (!ep) return 0;

// Load the security service creator
//
   if ((*ep = (XrdSecGetServ_t)piP->Resolve("XrdSecgetService"))) return 0;

// We failed this is eiter soft or hard depending on what else we loaded
//
   return (getP ? -1 : 1);
}
}

/******************************************************************************/
/* Private:                         L o a d                                   */
/******************************************************************************/

namespace
{
int Load(      char       *eBuff,      int         eBlen,
         const char       *cfn,  const char       *seclib,
         XrdSecGetProt_t  *getP, XrdSecService   **secP=0,
         XrdSysError      *eDest=0)
{
   XrdSecGetServ_t  ep;
   XrdOucPinLoader *piP;
   const char *mySecLib = "libXrdSec.so";
   int rc;

// Check for default path
//
   if (!seclib) seclib = mySecLib;

// Get a plugin loader object
//
   if (eDest) piP = new XrdOucPinLoader(eDest,
                                        &myVersion, "seclib", seclib);
      else    piP = new XrdOucPinLoader(eBuff, eBlen,
                                        &myVersion, "seclib", seclib);

// Load the appropriate pointers and get required objects.
//
   rc = Plug(piP, getP, &ep);
   if (rc == 0)
      {if (secP && !(*secP = (*ep)(eDest->logger(), cfn))) rc = 1;
       if (!rc) {delete piP; return 0;}
      }

// We failed, so bail out
//
   if (eDest)
      eDest->Say("Config ","Unable to create security framework via ", seclib);
   piP->Unload(true);
   return 1;
}
}

/******************************************************************************/

namespace
{
int Load(      char       *eBuff,   int             eBlen,
         const char       *protlib, XrdSysError    *eDest=0)
{
   XrdSecProtector **protPP;
   XrdOucPinLoader *piP;
   const char *myProtLib = "libXrdSecProt.so";

// Check for default path
//
   if (!protlib) protlib = myProtLib;

// Get a plugin loader object
//
   if (eDest) piP = new XrdOucPinLoader(eDest,
                                        &myVersion, "protlib", protlib);
      else    piP = new XrdOucPinLoader(eBuff, eBlen,
                                        &myVersion, "protlib", protlib);

// Get the protection object which also is a factory object.
//
   protPP = (XrdSecProtector **)piP->Resolve("XrdSecProtObjectP");
   if (protPP)
      {XrdSecProtection::theProtector = *protPP;
       delete piP;
       return 0;
      }
      return 1;

// We failed, so bail out
//
   if (eDest)
      eDest->Say("Config ","Unable to create protection framework via ",protlib);
   piP->Unload(true);
   return ENOENT;
}
}

/******************************************************************************/
/*                     X r d S e c L o a d F a c t o r y                      */
/******************************************************************************/

XrdSecGetProt_t XrdSecLoadSecFactory(char *eBuff, int eBlen, const char *seclib)
{
   XrdSecGetProt_t getP;
   int rc;

// Load required plugin nd obtain pointers
//
   rc = Load(eBuff, eBlen, 0, seclib, &getP);
   if (!rc) return getP;

// Issue correct error message, if any
//
   if (!seclib) seclib = "default";

   if (rc < 0)
       snprintf(eBuff, eBlen,
               "Unable to create security framework via %s; invalid path.",
               seclib);
      else if (!(*eBuff))
              snprintf(eBuff, eBlen,
                       "Unable to create security framework via %s", seclib);
   return 0;
}

/******************************************************************************/
/*                   X r d S e c G e t P r o t e c t i o n                    */
/******************************************************************************/

// This is used client-side only

int XrdSecGetProtection(XrdSecProtect              *&protP,
                        XrdSecProtocol              &aprot,
                        ServerResponseBody_Protocol &resp,
                        unsigned int                 resplen)
{
   static const unsigned int hdrLen = sizeof(ServerResponseReqs_Protocol) - 2;
   static const unsigned int minLen = kXR_ShortProtRespLen + hdrLen;
   XrdSecProtector *pObj;
   unsigned int vLen;
   int rc;

// First validate the response before passing it to anyone
//
   protP = 0;
   if (resplen <= kXR_ShortProtRespLen) return 0;
   if (resplen < minLen) return -EINVAL;
   vLen = static_cast<unsigned int>(resp.secreq.secvsz)
        * sizeof(ServerResponseSVec_Protocol);
   if (vLen + minLen > resplen) return -EINVAL;

// Our first step is to see if any protection is required
//
   if (vLen == 0 && resp.secreq.seclvl == kXR_secNone) return 0;

// The next step is to see if we have a protector object. If we do not then
// we need to load the library that provides such objects. This needs to be
// MT-safe as it may be called at any time by any thread.
//
   protMutex.Lock();
   if (!(pObj = XrdSecProtection::theProtector))
      {if (!XrdSecProtection::protRC)
          {char eBuff[2048];
           if ((XrdSecProtection::protRC = Load(eBuff, sizeof(eBuff), 0)))
              std::cerr <<"SecLoad: " <<eBuff <<'\n' <<std::flush;
           else
              pObj = XrdSecProtection::theProtector;
          }
       if ((rc = XrdSecProtection::protRC))
          {protMutex.UnLock();
           return -rc;
          }
      }
   protMutex.UnLock();

// Return new protection object
//
   protP = pObj->New4Client(aprot, resp.secreq, resplen-kXR_ShortProtRespLen);
   return (protP ? 1 : 0);
}
  
/******************************************************************************/
/*                  X r d S e c L o a d P r o t e c t i o n                   */
/******************************************************************************/

// This is a one-time server-side call

XrdSecProtector *XrdSecLoadProtection(XrdSysError &erP)
{

// Load the protection object. This is done in the main thread do no mutex
//
   XrdSecProtection::protRC = Load(0, 0, 0, &erP);

// All done, return result
//
   return (XrdSecProtection::protRC ? 0 : XrdSecProtection::theProtector);
}

/******************************************************************************/
/*                  X r d S e c L o a d S e c S e r v i c e                   */
/******************************************************************************/

XrdSecService *XrdSecLoadSecService(XrdSysError     *eDest,
                                    const char      *cfn,
                                    const char      *seclib,
                                    XrdSecGetProt_t *getP,
                                    XrdSecProtector**proP)
{
   XrdSecService   *CIA;

// Load required plugin nd obtain pointers
//
   if (Load(0, 0, cfn, seclib, getP, &CIA, eDest)) return 0;

// Set the protectorobject. Note that the securityservice will load it if
// is needed and we will havecaptured its pointer. This sort of a hack but
// we can't change the SecService object as it is a public interface.
//
   if (proP) *proP = XrdSecProtection::theProtector;
   return CIA;
}
