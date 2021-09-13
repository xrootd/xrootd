#ifndef __SEC_PROTOCOL_HOST_H__
#define __SEC_PROTOCOL_HOST_H__
/******************************************************************************/
/*                                                                            */
/*                 X r d S e c P r o t o c o l h o s t . h h                  */
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

#include <cstdlib>
#include <strings.h>

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdSec/XrdSecInterface.hh"

class XrdSecProtocolhost : public XrdSecProtocol
{
public:

        int                Authenticate  (XrdSecCredentials  *cred,
                                          XrdSecParameters  **parms,
                                          XrdOucErrInfo      *einfo=0);

        XrdSecCredentials *getCredentials(XrdSecParameters  *parm=0,
                                          XrdOucErrInfo     *einfo=0);

        const char        *getParms(int &psize, const char *hname=0)
                                   {psize = 5; return "host";}


void                       Delete() {delete this;}

              XrdSecProtocolhost(const char *host, XrdNetAddrInfo &endPoint)
                                : XrdSecProtocol("host")
                                   {theHost = strdup(host);
                                    epAddr = endPoint;
                                   }
             ~XrdSecProtocolhost() {if (theHost) free(theHost);}
private:

XrdNetAddrInfo epAddr;
char *theHost;
};
#endif
