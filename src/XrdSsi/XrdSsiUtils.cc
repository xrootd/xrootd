/******************************************************************************/
/*                                                                            */
/*                        X r d S s i U t i l s . c c                         */
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucERoute.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSsi/XrdSsiUtils.hh"
#include "XrdSys/XrdSysError.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdSsi
{
extern XrdSysError       Log;
};

using namespace XrdSsi;

/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/

int XrdSsiUtils::Emsg(const char    *pfx,    // Message prefix value
                      int            ecode,  // The error code
                      const char    *op,     // Operation being performed
                      const char    *path,   // Operation target
                      XrdOucErrInfo &eDest)  // Plase to put error
{
   char buffer[2048];

// Get correct error code and path
//
    if (ecode < 0) ecode = -ecode;
    if (!path) path = "???";

// Format the error message
//
   XrdOucERoute::Format(buffer, sizeof(buffer), ecode, op, path);

// Put the message in the log
//
   Log.Emsg(pfx, eDest.getErrUser(), buffer);

// Place the error message in the error object and return
//
   eDest.setErrInfo(ecode, buffer);
   return SFS_ERROR;
}
