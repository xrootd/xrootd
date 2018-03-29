/******************************************************************************/
/*                                                                            */
/*                       X r d C m s C l i e n t . c c                        */
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

#include "XrdCms/XrdCmsClient.hh"
#include "XrdCms/XrdCmsFinder.hh"

/******************************************************************************/
/*                      G e t D e f a u l t C l i e n t                       */
/******************************************************************************/

namespace XrdCms
{
XrdCmsClient *GetDefaultClient(XrdSysLogger *Logger,
                               int           opMode,
                               int           myPort
                              )
{
// Determine which client to generate for the caller. This function is
// provided as an ABI-compatible interface to obtaining a default client.
//
   if (opMode & IsRedir)
      return (XrdCmsClient *)new XrdCmsFinderRMT(Logger, opMode, myPort);
   if (opMode & IsTarget)
      return (XrdCmsClient *)new XrdCmsFinderTRG(Logger, opMode, myPort);
   return 0;
}
};
