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

class BL_Grip;

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
//! @param  chkt   Seconds between checks for blacklist changes. If the value
//!                is negative, the blacklist is treated as a whitelist.
//------------------------------------------------------------------------------

static void  Init(XrdScheduler *sP,   XrdCmsCluster *cP,
                  const char   *blfn, int chkt=600);

//------------------------------------------------------------------------------
//! Check if host is in the black list and how it should be managed.
//!
//! @param  hName  Pointer to the host name or address.
//! @param  bList  Optional pointer to a private black list.
//! @param  rbuff  Pointer to the buffer to contain the redirect response. If
//!                nil, the host is not redirected.
//! @param  rblen  The size of rbuff. If zero or insufficiently large the host
//!                is not redirected.
//!
//! @return < -1   Host is  in the black list and would      be redirected;
//!                but either rbuff was nil or the buffer was too small. The
//!                abs(returned value) is the size the buffer should have been.
//! @return = -1   Host is  in the black list and should not be redirected.
//! @return =  0   Host not in the black list.
//! @return >  0   Host is  in the black list and should     be redirected.
//!                The return value is the size of the redirect response placed
//!                in the supplied buffer.
//------------------------------------------------------------------------------

static int  Present(const char *hName, XrdOucTList *bList=0,
                          char *rbuff=0, int rblen=0);

//------------------------------------------------------------------------------
//! Constructor and Destructor
//------------------------------------------------------------------------------

            XrdCmsBlackList() : XrdJob("Black List Check") {}
           ~XrdCmsBlackList() {}
private:
static bool  AddBL(BL_Grip &bAnchor, char *hSpec,
                   BL_Grip *rAnchor, char *rSpec);
static int   AddRD(BL_Grip *rAnchor,    char *rSpec, char *hSpec);
static bool  AddRD(XrdOucTList **rList, char *rSpec, char *hSpec);
static
XrdOucTList *Flatten(XrdOucTList *tList, int tPort);
static bool  GetBL(XrdOucTList *&bList, XrdOucTList **&rList, int &rcnt);
};
#endif
