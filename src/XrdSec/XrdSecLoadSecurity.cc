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

#include "XrdVersion.hh"

#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdSec/XrdSecLoadSecurity.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/
  
namespace
{
static XrdVERSIONINFODEF(myVersion, XrdSecLoader, XrdVNUMBER, XrdVERSION);
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
/*                  X r d S e c L o a d S e c S e r v i c e                   */
/******************************************************************************/

XrdSecService *XrdSecLoadSecService(XrdSysError     *eDest,
                                    const char      *cfn,
                                    const char      *seclib,
                                    XrdSecGetProt_t *getP)
{
   XrdSecService   *CIA;

// Load required plugin nd obtain pointers
//
   return (Load(0, 0, cfn, seclib, getP, &CIA, eDest) ? 0 : CIA);
}
