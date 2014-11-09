/******************************************************************************/
/*                                                                            */
/*                      X r d O u c P r e l o a d . c c                       */
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

#include <stdio.h>

#include "XrdVersion.hh"

#include "XrdOuc/XrdOucVerName.hh"
#include "XrdOuc/XrdOucPreload.hh"
#include "XrdSys/XrdSysPlugin.hh"

/******************************************************************************/
/*                         X r d O u c P r e l o a d                          */
/******************************************************************************/

bool XrdOucPreload(const char *plib, char *eBuff, int eBlen, bool retry)
{
   char theLib[2048];
   bool dummy;

// Perform versioning
//
   if (!XrdOucVerName::Version(XRDPLUGIN_SOVERSION, plib, dummy,
                               theLib, sizeof(theLib)))
      {snprintf(eBuff, eBlen,
                "Unable to preload plugin via %s; path too long.", plib);
       return false;
      }

// Preload the library. If we failed, we will try to fallback to the
// unversioned name is we are allowed to do so.
//
   *eBuff = 0;
   if (XrdSysPlugin::Preload(theLib, eBuff, eBlen)
   || (retry && XrdSysPlugin::Preload(plib, eBuff, eBlen))) return true;
   return false;
}
