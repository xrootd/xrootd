/******************************************************************************/
/*                                                                            */
/*                       X r d O u c E R o u t e . h h                        */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "XrdOuc/XrdOucERoute.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                                F o r m a t                                 */
/******************************************************************************/
  
int XrdOucERoute::Format(char *buff, int blen, int ecode, const char *etxt1,
                                                          const char *etxt2)
{
   const char *esep = " ", *estr = XrdSysError::ec2text(ecode);
   char ebuff[256];
   int n;

// Substitute something of no ecode translation exists
//
   if (!estr) estr = "reason unknown";
      else if (isupper(static_cast<int>(*estr)))
              {strlcpy(ebuff, estr, sizeof(ebuff));
               *ebuff = static_cast<char>(tolower(static_cast<int>(*estr)));
               estr = ebuff;
              }

// Set format elements
//
   if (!etxt2) etxt2 = esep = "";

// Format the message
//
   n = snprintf(buff, blen, "Unable to %s%s%s; %s",etxt1,esep,etxt2,estr);
   return (n < blen ? n : blen-1);
}

/******************************************************************************/
/*                                 R o u t e                                  */
/******************************************************************************/
  
int XrdOucERoute::Route(XrdSysError *elog,  XrdOucStream *estrm,
                        const char  *esfx,  int           ecode,
                        const char  *etxt1, const char   *etxt2)
{
   char ebuff[2048];
   int elen;

// Format the error message
//
   elen = Format(ebuff, sizeof(ebuff), ecode, etxt1, etxt2);

// Route appropriately
//
   if (elog)  elog->Emsg(esfx, ebuff);
   if (estrm) estrm->Put(ebuff, elen);

// Return the error number
//
   if (ecode) return (ecode < 0 ? ecode : -ecode);
   return -1;
}
