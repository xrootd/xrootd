#ifndef _CMS_SECURITY_H
#define _CMS_SECURITY_H
/******************************************************************************/
/*                                                                            */
/*                     X r d C m s S e c u r i t y . h h                      */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <sys/types.h>

#include "XrdSec/XrdSecInterface.hh"

class XrdLink;
class XrdOucTList;
class XrdSysError;

class XrdCmsSecurity
{
public:

static int             Authenticate(XrdLink *Link, const char *Token, int tlen);

static int             Configure(const char *Lib, const char *Cfn=0);

static char           *getVnId(XrdSysError &eDest, const char *cfgFN,
                               const char *nidlib, const char *nidparm,
                               char nidType);

static const char     *getToken(int &size, XrdNetAddrInfo *endPoint);

static int             Identify(XrdLink *Link, XrdCms::CmsRRHdr &inHdr,
                                char *authBuff, int abLen);

static void            setSecFunc(void *secfP);

static char           *setSystemID(XrdOucTList *tp,    const char *iVNID,
                                   const char  *iTag,        char  iType);

      XrdCmsSecurity() {}
     ~XrdCmsSecurity() {}

private:
static XrdSecService *DHS;
static char *chkVnId(XrdSysError &eDest, const char *vnid, const char *what);
};
#endif
