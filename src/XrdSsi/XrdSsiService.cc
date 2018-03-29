/******************************************************************************/
/*                                                                            */
/*                      X r d S s i S e r v i c e . c c                       */
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

#include "XrdSsiProvider.hh"
#include "XrdSsiService.hh"

/******************************************************************************/
/*                          G l o b a l   I t e m s                           */
/******************************************************************************/

namespace XrdSsi
{
XrdSsiProvider *Provider = 0;
}
  
/******************************************************************************/
/*                               P r e p a r e                                */
/******************************************************************************/
  
bool XrdSsiService::Prepare(XrdSsiErrInfo &eInfo, const XrdSsiResource &rDesc)
{
// The default implementation simply asks the proviuder if the resource exists
//
   if (XrdSsi::Provider
   &&  XrdSsi::Provider->QueryResource(rDesc.rName.c_str()) !=
       XrdSsiProvider::notPresent) return true;

// Indicate we do not have the resource
//
   eInfo.Set("Resource not available.", ENOENT);
   return false;
}
