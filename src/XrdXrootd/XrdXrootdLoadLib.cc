/******************************************************************************/
/*                                                                            */
/*                   X r d X r o o t d L o a d L i b . c c                    */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdio.h>

#include "XrdVersion.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPinLoader.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                 x r o o t d _ l o a d F i l e s y s t e m                  */
/******************************************************************************/

XrdSfsFileSystem *XrdXrootdloadFileSystem(XrdSysError *eDest,
                                          XrdSfsFileSystem *prevFS,
                                          char *fslib, int fsver,
                                          const char *cfn, XrdOucEnv *envP)
{
   static XrdVERSIONINFODEF(myVersion, XrdOfsLoader, XrdVNUMBER, XrdVERSION);
   XrdOucPinLoader ofsLib(eDest, &myVersion, "fslib", fslib);
   XrdSfsFileSystem_t  ep;
   XrdSfsFileSystem2_t ep2;
   XrdSfsFileSystem *FS = 0;
   const char *epname = "XrdSfsGetFileSystem";
   char  epbuff[64];

// Record the library path in the environment
//
   if (!prevFS) XrdOucEnv::Export("XRDOFSLIB", fslib);

// If a different version is to used for initialization, generate the name
//
   if (fsver)
      {sprintf(epbuff, "XrdSfsGetFileSystem%d", fsver); // Always fits
       epname = epbuff;
      }

// Get the file system object creator and the object
//
   if (fsver)
      {if ((ep2 = (XrdSfsFileSystem2_t)ofsLib.Resolve(epname)))
          FS = (*ep2)(prevFS, eDest->logger(), cfn, envP);
      } else {
       if ((ep  = (XrdSfsFileSystem_t )ofsLib.Resolve(epname)))
          FS = (*ep) (prevFS, eDest->logger(), cfn);
      }

// Issue message if we could not load it
//
   if (!FS) eDest->Emsg("Config","Unable to create file system object via",fslib);

// All done
//
   return FS;
}
