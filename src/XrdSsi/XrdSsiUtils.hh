#ifndef __SSI_UTILS_H__
#define __SSI_UTILS_H__
/******************************************************************************/
/*                                                                            */
/*                        X r d S s i U t i l s . h h                         */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <string>

namespace XrdCl {class XRootDStatus;}

class XrdOucErrInfo;
class XrdSsiErrInfo;
class XrdSsiRequest;

class XrdSsiUtils
{
public:

static char *b2x(const char *ibuff, int ilen, char *obuff, int olen,
                       char xbuff[4]);

static int  Emsg(const char    *pfx,    // Message prefix value
                 int            ecode,  // The error code
                 const char    *op,     // Operation being performed
                 const char    *path,   // Operation target
                 XrdOucErrInfo &eDest); // Plase to put error

static int  GetErr(XrdCl::XRootDStatus &Status, std::string &eText);

static int  MapErr(int xEnum);

static void RetErr(XrdSsiRequest &reqP, const char *eTxt, int eNum);

static void SetErr(XrdCl::XRootDStatus &Status, XrdSsiErrInfo &eInfo);

            XrdSsiUtils() {}
           ~XrdSsiUtils() {}
};
#endif
