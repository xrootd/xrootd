/******************************************************************************/
/*                                                                            */
/*             X r d S s i G e t C l i e n t S e r v i c e . c c              */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <string.h>
  
#include "XrdNet/XrdNetAddr.hh"
#include "XrdSsi/XrdSsiServReal.hh"
  
XrdSsiService *XrdSsiGetClientService(XrdSsiErrInfo &eInfo,
                                      const char    *contact,
                                      int            oHold)
{
   XrdNetAddr netAddr;
   const char *eText;
   char buff[512];
   int  n;

// If no contact is given then declare an error
//
   if (!contact || !(*contact))
      {eInfo.Set("Contact not specified.", EINVAL); return 0;}

// Validate the given contact
//
   if ((eText = netAddr.Set(contact)))
      {eInfo.Set(eText, EINVAL); return 0;}

// Construct new binding
//
   if (!(n = netAddr.Format(buff, sizeof(buff), XrdNetAddrInfo::fmtName)))
      {eInfo.Set("Unable to validate contact.", EINVAL); return 0;}

// Allocate a service object and return it
//
   return new XrdSsiServReal(buff, oHold);
}
