/******************************************************************************/
/*                                                                            */
/*                 X r d S e c P r o t o c o l h o s t . c c                  */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <strings.h>
#include <cstdlib>

#include "XrdSec/XrdSecProtocolhost.hh"

/******************************************************************************/
/*                          A u t h e n t i c a t e                           */
/******************************************************************************/
  
int XrdSecProtocolhost::Authenticate(XrdSecCredentials  *cred,
                                     XrdSecParameters  **parms,
                                     XrdOucErrInfo      *einfo)
{
   strcpy(Entity.prot, "host");
   Entity.host = theHost;
   Entity.addrInfo = &epAddr;
   return 0;
}

/******************************************************************************/
/*                        g e t C r e d e n t i a l s                         */
/******************************************************************************/
  
XrdSecCredentials *XrdSecProtocolhost::getCredentials(XrdSecParameters *parm,
                                                      XrdOucErrInfo    *einfo)
{XrdSecCredentials *cp = new XrdSecCredentials;

 cp->size = 5; cp->buffer = (char *)"host";
 return cp;
}

/******************************************************************************/
/*                X r d S e c P r o t o c o l h o s t I n i t                 */
/******************************************************************************/
  
// This is a builtin protocol so we don't define an Init method. Anyway, this
// protocol need not be initialized. It works as is.

/******************************************************************************/
/*              X r d S e c P r o t o c o l h o s t O b j e c t               */
/******************************************************************************/
  
// Normally this would be defined as an extern "C", however, this function is
// statically linked into the shared library as a native protocol so there is
// no reason to define it as such. Imitators, beware! Read the comments in
// XrdSecInterface.hh
//
XrdSecProtocol *XrdSecProtocolhostObject(const char              who,
                                         const char             *hostname,
                                               XrdNetAddrInfo   &endPoint,
                                         const char             *parms,
                                               XrdOucErrInfo    *einfo)
{

// Simply return an instance of the host protocol object
//
   return new XrdSecProtocolhost(hostname, endPoint);
}
