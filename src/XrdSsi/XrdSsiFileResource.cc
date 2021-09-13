/******************************************************************************/
/*                                                                            */
/*                 X r d S s i F i l e R e s o u r c e . c c                  */
/*                                                                            */
/* (c) 2017 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdlib>

#include "XrdOuc/XrdOucEnv.hh"

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSsi/XrdSsiFileResource.hh"

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
void XrdSsiFileResource::Init(const char *path, XrdOucEnv &envX, bool aDNS)
{
   const XrdSecEntity *entP = envX.secEnv();
   const char *rVal;
   int n;

// Construct the security information
//
   if (entP)
      {strncpy(mySec.prot, entP->prot, XrdSsiPROTOIDSIZE);
       mySec.name = entP->name;
       mySec.host = (!aDNS ? entP->host : entP->addrInfo->Name(entP->host));
       mySec.role = entP->vorg;
       mySec.role = entP->role;
       mySec.grps = entP->grps;
       mySec.endorsements = entP->endorsements;
       mySec.creds = entP->creds;
       mySec.credslen = entP->credslen;
      } else mySec.tident = "ssi";
   client = &mySec;

// Fill out the resource name and user
//
   rName = path;
   if ((rVal = envX.Get("ssi.user"))) rUser = rVal;
      else rUser.clear();

// Fill out the the optional cgi info
//
   if (!(rVal = envX.Get("ssi.cgi"))) rInfo.clear();
      else {rVal = envX.Env(n);
            if (!(rVal = strstr(rVal, "ssi.cgi="))) rInfo.clear();
               else rInfo = rVal+8;
           }
}
