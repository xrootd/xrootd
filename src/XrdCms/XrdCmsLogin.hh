#ifndef __CMS_LOGIN_H__
#define __CMS_LOGIN_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d C m s L o g i n . h h                         */
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

#include <sys/uio.h>

#include "XProtocol/XPtypes.hh"
#include "XProtocol/YProtocol.hh"

class XrdLink;

class XrdCmsLogin
{
public:

       int  Admit(XrdLink *Link, XrdCms::CmsLoginData &Data);

static int  Login(XrdLink *Link, XrdCms::CmsLoginData &Data, int timeout=-1);

       XrdCmsLogin(char *Buff = 0, int Blen = 0) {myBuff = Buff; myBlen = Blen;}

      ~XrdCmsLogin() {}

private:

static int Authenticate(XrdLink *Link, XrdCms::CmsLoginData &Data);
static int Emsg(XrdLink *, const char *, int ecode=XrdCms::kYR_EINVAL);
static int sendData(XrdLink *Link, XrdCms::CmsLoginData &Data);
static int SendErrorBL(XrdLink *Link);

         char       *myBuff;
         int         myBlen;
};
#endif
