#ifndef __XRDCMSBLACKLIST_HH__
#define __XRDCMSBLACKLIST_HH__
/******************************************************************************/
/*                                                                            */
/*                    X r d C m s B l a c k L i s t . h h                     */
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
  
#include "Xrd/XrdJob.hh"

class XrdCmsCluster;
class XrdOucTList;
class XrdScheduler;

class XrdCmsBlackList : public XrdJob
{
public:

//------------------------------------------------------------------------------
//! Time driven method for checking black list file.
//------------------------------------------------------------------------------

       void  DoIt();

//------------------------------------------------------------------------------
//! Initialize the black list
//!
//! @param  sP     Pointer to the scheduler object.
//! @param  cP     Pointer to the cluster   object.
//! @param  blfn   The path to the black list file or null.
//! @param  chkt   Seconds between checks for blacklist changes.
//------------------------------------------------------------------------------

static void  Init(XrdScheduler *sP,   XrdCmsCluster *cP,
                  const char   *blfn, int chkt=600);

//------------------------------------------------------------------------------
//! Check if host is in the black list.
//!
//! @param  hName  Pointer to the host name or address.
//! @param  bList  Optional pointer to a private black list.
//!
//! @return true   Host is     in the black list.
//! @return false  Host is not in the black list.
//------------------------------------------------------------------------------

static bool Present(const char *hName, XrdOucTList *bList=0);

//------------------------------------------------------------------------------
//! Constructor and Destructor
//------------------------------------------------------------------------------

            XrdCmsBlackList() : XrdJob("Black List Check") {}
           ~XrdCmsBlackList() {}
private:
static void AddBL(XrdOucTList *&bAnchor, char *hSpec);
static bool GetBL(XrdOucTList *&bAnchor);
};
#endif
