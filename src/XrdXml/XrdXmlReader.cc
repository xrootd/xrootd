/******************************************************************************/
/*                                                                            */
/*                       X r d X m l R e a d e r . c c                        */
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

#include <errno.h>
#include <string.h>

#include "XrdXml/XrdXmlRdrTiny.hh"

#ifdef HAVE_XML2
#include "XrdXml/XrdXmlRdrXml2.hh"
#endif

/******************************************************************************/
/*                             G e t R e a d e r                              */
/******************************************************************************/
  
XrdXmlReader *XrdXmlReader::GetReader(const char *fname, const char *enc,
                                      const char *impl)
{
   XrdXmlReader *rP;
   int rc;
   bool aOK;

// Check if this is the default implementation
//                                                                             c
   if (!impl || !strcmp(impl, "tinyxml"))
      {rP = new XrdXmlRdrTiny(aOK, fname, enc);
       if (aOK) return rP;
       rP->GetError(rc);
       delete rP;
       errno = (rc ? rc : ENOTSUP);
       return 0;
      }

// Check for he full blown xml implementation
//
#ifdef HAVE_XML2
   if (!strcmp(impl, "libxml2"))
      {rP = new XrdXmlRdrXml2(aOK, fname, enc);
       if (aOK) return rP;
       rP->GetError(rc);
       delete rP;
       errno = (rc ? rc : ENOTSUP);
       return 0;
      }
#endif

// Add additional implementations here
//

// Not supported
//
   errno = ENOTSUP;
   return 0;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
bool XrdXmlReader::Init(const char *impl)
{
// Check if this is the default implementation
//
   if (!impl || !strcmp(impl, "tinyxml")) return true;

// Check for the whole hog implmenetation
//
#ifdef HAVE_XML2
   if (!strcmp(impl, "libxml2")) {return XrdXmlRdrXml2::Init();}
#endif

// Add additional implementations here
//

// Not supported
//
   errno = ENOTSUP;
   return false;
}
