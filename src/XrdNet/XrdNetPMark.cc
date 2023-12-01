/******************************************************************************/
/*                                                                            */
/*                        X r d N e t P M a r k . c c                         */
/*                                                                            */
/* (c) 2022 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <stdlib.h>
#include <string.h>
  
#include "XrdNet/XrdNetPMark.hh"

/******************************************************************************/
/*                                 g e t E A                                  */
/******************************************************************************/
  
bool XrdNetPMark::getEA(const char *cgi, int &ecode, int &acode)
{

  ecode = acode = 0;
// If we have cgi, see if we can extract rge codes from there
//
  if (cgi) {
    const char *stP = strstr(cgi, "scitag.flow=");
    if (stP) {
      char *eol;
      int eacode = strtol(stP + 12, &eol, 10);
      if (*eol == '&' || *eol == 0) {
        if (eacode >= XrdNetPMark::minTotID && eacode <= XrdNetPMark::maxTotID) {
          ecode = eacode >> XrdNetPMark::btsActID;
          acode = eacode & XrdNetPMark::mskActID;
        }
        // According to the specification, if the provided scitag.flow has an incorrect value
        // the packets will be marked with a scitag = 0
        return true;
      }
    }
  }

   // No go
   //
   return false;
}
