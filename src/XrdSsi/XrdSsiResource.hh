#ifndef __XRDSSIRESOURCE_HH__
#define __XRDSSIRESOURCE_HH__
/******************************************************************************/
/*                                                                            */
/*                     X r d S s i R e s o u r c e . h h                      */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
//-----------------------------------------------------------------------------
//! The XrdSsiResource object is used by the Scalable Service Interface to
//! describe a resource that may be provisioned by XrdSsiService.
//-----------------------------------------------------------------------------

class XrdSsiEntity;

class XrdSsiResource
{
public:

const char    *rName;   //!< -> Name of the resource to be provisioned
const char    *rUser;   //!< -> Name of the resource user (nil if anonymous)
const char    *rInfo;   //!< -> Additional information in CGI format
const char    *hAvoid;  //!< -> Comma separated list of hosts to avoid
XrdSsiEntity  *client;  //!< -> Pointer to client identification (server-side)

enum Affinity {Default, //!< Use configured affinity
               None,    //!< Resource has no affinity, any endpoint will do
               Weak,    //!< Use resource on same node if possible, don't wait
               Strong,  //!< Use resource on same node even if wait required
               Strict   //!< Always use same node for resource no matter what
              };
Affinity       affinity;//!< Resource affinity

static const
unsigned short autoUnP = 0x0001; //!< Auto unprovision on Finish()

unsigned short rOpts;   //!< Resource options (see above)
unsigned short rsvd;

//-----------------------------------------------------------------------------
//! Constructor
//!
//! @param  rname    points to the name of the resource. If using directory
//!                  notation (i.e. slash separated names); duplicate slashes
//!                  and dot-shashes are compressed out.
//!
//! @param  havoid   if not null then points to a comma separated list of
//!                  hostnames to avoid when provisioning the resource. This
//!                  argument is only meaningful client-side.
//!
//! @param  ruser    points to the name of the resource user. If nil the user
//!                  is anonymous (unnamed). By default, all resources share
//!                  the TCP connection to any endpoint. Different users have
//!                  separate connections only if so requested during the
//!                  provision call (see XrdSsiService::Provision()).
//!
//! @param  rinfo    points to additional information to be passed to the
//!                  endpoint that provides the resource. The string should be
//!                  in cgi format (e.g. var=val&var2-val2&....).
//!
//! @param  raff     resource affinity (see Affinity enum).
//-----------------------------------------------------------------------------

               XrdSsiResource(const char *rname,
                              const char *havoid=0,
                              const char *ruser=0,
                              const char *rinfo=0,
                              Affinity    raff=Default
                             ) : rName(rname), rUser(ruser), rInfo(rinfo),
                                 hAvoid(havoid), client(0), affinity(Default),
                                 rOpts(0), rsvd(0) {}

//-----------------------------------------------------------------------------
//! Destructor
//-----------------------------------------------------------------------------

              ~XrdSsiResource() {}
};
#endif
