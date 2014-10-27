/******************************************************************************/
/*                                                                            */
/*                        X r d S y s X A t t r . c c                         */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysXAttr.hh"

/******************************************************************************/
/*                                  C o p y                                   */
/******************************************************************************/

int XrdSysXAttr::Copy(const char *iPath, int iFD, const char *oPath, int oFD,
                      const char *Aname)
{
   char *bP;
   int sz, rc = 0;

// Check if all attributes are to be copied. If so, do it.
//
   if (!Aname)
      {AList *aP = 0, *aNow;
       char *Buff;
       int maxSz;

    // Get all of the attributes for the input
    //
       if ((maxSz = List(&aP, iPath, iFD, 1)) <= 0)
          return maxSz == 0 || maxSz == -ENOTSUP;

    // Allocate a buffer to hold the largest attribute value (plus some)
    //
       maxSz += 4096;
       Buff = (char *)malloc(maxSz);

    // Get each value and set it
    //
       aNow = aP;
       while(aNow && (rc = Get(aNow->Name, Buff, maxSz,      iPath, iFD)) >= 0
                  && (rc = Set(aNow->Name, Buff, aNow->Vlen, oPath, oFD)) >= 0)
            {aNow = aNow->Next;}

    // Free up resources and return
    //
       Free(aP);
       free(Buff);
       return rc;
      }

// First obtain the size of the attribute (if zero ignore it)
//
   if ((sz = Get(Aname, 0, 0, iPath, iFD)) <= 0)
      return (!sz || sz == -ENOTSUP ? 0 : sz);

// Obtain storage
//
   if (!(bP = (char *)malloc(sz)))
      {if (Say)
          {char eBuff[512];
           snprintf(eBuff, sizeof(eBuff), "copy attr %s from", Aname);
           Say->Emsg("XAttr", ENOMEM, eBuff, iPath);
          }
       return -ENOMEM;
      }

// Copy over any extended attributes
//
   if ((rc =     Get(Aname, bP, sz, iPath, iFD)) > 0)
      {if ((rc = Set(Aname, bP, rc, oPath, oFD)) < 0 && rc == -ENOTSUP) rc = 0;}
      else if (rc < 0 && rc == -ENOTSUP) rc = 0;

// All done
//
   free(bP);
   return rc;
}

/******************************************************************************/
/*                           S e t M s g R o u t e                            */
/******************************************************************************/
  
XrdSysError *XrdSysXAttr::SetMsgRoute(XrdSysError *errP)
{
   XrdSysError *msgP = Say;
   Say = errP;
   return msgP;
}
