#ifndef __SSI_ENTITY_H__
#define __SSI_ENTITY_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d S s i E n t i t y . h h                        */
/*                                                                            */
/* (c) 2014 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//------------------------------------------------------------------------------
//! This object describes the authenticated identification of a client and may
//! be used to restrct certain functions based on the identification. Presence
//! of certain members is determined by the actual authnetication method used.
//! This object, if supplied, is only supplied server-side.
//------------------------------------------------------------------------------

#include <cstring>

#define XrdSsiPROTOIDSIZE 8

class  XrdSsiEntity
{
public:
         char    prot[XrdSsiPROTOIDSIZE]; //!< Protocol used
const    char   *name;                    //!< Entity's name
const    char   *host;                    //!< Entity's host name or address
const    char   *vorg;                    //!< Entity's virtual organization
const    char   *role;                    //!< Entity's role
const    char   *grps;                    //!< Entity's group names
const    char   *endorsements;            //!< Protocol specific endorsements
const    char   *creds;                   //!< Raw client credentials or cert
         int     credslen;                //!< Length of the 'creds' field
         int     rsvd;                    //!< Reserved field
const    char   *tident;                  //!< Trace identifier always preset

         XrdSsiEntity(const char *pName = "")
                     : name(0), host(0), vorg(0), role(0), grps(0),
                       endorsements(0), creds(0), credslen(0),
                       rsvd(0), tident("")
                     {memset(prot, 0, XrdSsiPROTOIDSIZE);
	              strncpy(prot, pName, XrdSsiPROTOIDSIZE-1);
                      prot[XrdSsiPROTOIDSIZE-1] = '\0';
                     }
        ~XrdSsiEntity() {}
};
#endif
