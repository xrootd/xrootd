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

#include "XrdSec/XrdSecInterface.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPlugin.hh"

/******************************************************************************/
/*                    X r d S e c L o a d S e c u r i t y                     */
/******************************************************************************/

XrdSecService *XrdSecLoadSecurity(XrdSysError *eDest, const char *cfn,
                                  const char *seclib, XrdSecGetProt_t *getP)
{
   static XrdVERSIONINFODEF(myVersion, XrdSecLoader, XrdVNUMBER, XrdVERSION);
   XrdSecService *(*ep)(XrdSysLogger *, const char *cfn);
   XrdSecService *CIA;
   const char *libPath = "", *mySecLib = "libXrdSec" LT_MODULE_EXT, *Slash = 0;
   char theLib[2048];
   int libLen = 0;

// Check if we should version this library
//
   if (!seclib || !strcmp(seclib, mySecLib)
   ||  ((Slash = rindex(seclib, '/')) && !strcmp(Slash+1, mySecLib)))
      {if (Slash) {libLen = Slash-seclib+1; libPath = seclib;}
#if defined(__APPLE__)
       snprintf(theLib, sizeof(theLib)-1, "%.*slibXrdSec.%s%s",
                        libLen, libPath, XRDPLUGIN_SOVERSION, LT_MODULE_EXT );
#else
       snprintf(theLib, sizeof(theLib)-1, "%.*slibXrdSec%s.%s",
                        libLen, libPath, LT_MODULE_EXT, XRDPLUGIN_SOVERSION );
#endif
       theLib[sizeof(theLib)-1] = '\0';
       seclib = theLib;
      }

// Now that we have the name of the library we can declare the plugin object
// inline so that it gets deleted when we return (icky but simple)
//
   XrdSysPlugin secLib(eDest, seclib, "seclib", &myVersion, 1);

// Get the server object creator
//
   if (!(ep = (XrdSecService *(*)(XrdSysLogger *, const char *cfn))
              secLib.getPlugin("XrdSecgetService")))
      return 0;

// Get the server object
//
   if (!(CIA = (*ep)(eDest->logger(), cfn)))
      {eDest->Emsg("Config", "Unable to create security service object via",seclib);
       return 0;
      }

// Get the client object creator (in case we are acting as a client). We return
// the function pointer as a (void *) to the caller so that it can be passed
// onward via an environment object.
//
   if (getP
   &&  !(*getP = (XrdSecGetProt_t)secLib.getPlugin("XrdSecGetProtocol")))
      return 0;

// All done
//
   secLib.Persist();
   return CIA;
}
