#ifndef __SecsssID__
#define __SecsssID__
/******************************************************************************/
/*                                                                            */
/*                        X r d S e c s s s I D . h h                         */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

//-----------------------------------------------------------------------------
/*! The XrdSecsssID class allows you to establish a registery to map loginid's
    to arbitrary entities. By default, the sss security protocol uses the
    username as the authenticated username and, if possible, the corresponding
    primary group membership of username (i.e., static mapping). The server
    will ignore the username and/or the groupname unless the key is designated
    as anyuser, anygroup, respectively. By creating an instance of this class
    you can over-ride the default and map the loginid (i.e., the id supplied
    at login time which is normally the first 8-characters of the username or
    the id specified in the url; i.e., id@host) to arbitrary entities using
    the Register() method. You must create one, and only one, such instance
    prior to connecting to an sss security enabled server.

    In order to use XrdSecsssID methods, you should link with libXrdUtils.so
*/

class  XrdSecEntity;
class  XrdSecsssCon;
class  XrdSecsssEnt;

class XrdSecsssID
{
public:
friend class XrdSecProtocolsss;

//-----------------------------------------------------------------------------
//! Create a single instance of this class. Once created it cannot be deleted.
//!
//! @param  aType  - The type of authentication to perform (see authType enum).
//! @param  Ident  - Pointer to the default entity to use. If nil, a generic
//!                  entity is created based on the process uid and gid.
//! @param  Tracker- pointer to the connection tracker objec if connection
//!                  tracking is desired. If nil, connections are not tracked.
//! @param  isOK   - if not nil sets the variable to true if successful and
//!                  false, otherwise. Strongly recommended it be supplied.
//!
//! @note Mutual authnetication requires that the server send an encrypted
//!       message proving that it holds the key before an identity is sent.
//!       For idDynamic this is the default and the message must be the
//!       login which must correspond to the key used to register the entity.
//!       This works well when keys are no more than 8 characters and consist
//!       only of letters and digits. The idMapped types provide greater
//!       freedom by using whatever userid was specified on the URL performing
//!       the login as the lookup key (i.e. the returned loginid is not used).
//-----------------------------------------------------------------------------

enum authType
         {idDynamic = 0, //!< Mutual: Map loginid to registered identity
                         //!<         Ident is default; if 0 nobody/nogroup
          idMapped  = 3, //!< 1Sided: Map loginid to registered identity
                         //!<         Ident is default; if 0 nobody/nogroup
          idMappedM = 4, //!< Mutual: Map loginid to registered identity
                         //!<         Ident is default; if 0 process uid/gid
          idStatic  = 1, //!< 1Sided: fixed identity sent to the server
                         //!<         Ident as specified; if 0 process uid/gid
                         //!<         Default if XrdSecsssID not instantiated!
          idStaticM = 2  //!< Mutual: fixed identity sent to the server
                         //!<         Ident as specified; if 0 process uid/gid
         };

         XrdSecsssID(authType aType=idStatic, const XrdSecEntity *Ident=0,
                     XrdSecsssCon *Tracker=0, bool *isOK=0);

//-----------------------------------------------------------------------------
//! Create or delete a mapping from a loginid to an entity description.
//!
//! @param  lgnid  - Pointer to the login ID.
//! @param  Ident  - Pointer to the entity object to be registstered. If the
//!                  pointer is NIL, then the mapping is deleted.
//! @param  doRep  - When true, any existing mapping is replaced.
//! @param  defer  - When true, the entity object is recorded but serialization
//!                  is deferred until the object is needed. The entity object
//!                  must remain valid until the mapping is deleted. The entity
//!                  may not be modified during this period.
//!
//! @return true   - Mapping registered.
//! @return false  - Mapping not registered because this object was not created
//!                  as idDynamic idMapped, or idMappedM; or the mapping exists
//!                  and doRep is false.
//-----------------------------------------------------------------------------

bool     Register(const char *lgnid, const XrdSecEntity *Ident,
                  bool doReplace=false, bool defer=false);

private:

        ~XrdSecsssID();

//-----------------------------------------------------------------------------
//! Find and return a id mapping.
//!
//! @param  lid    - Pointer to the login ID to search for.
//! @param  dP     - Reference to a pointer where the serialized ID is returned.
//!                  The caller is responsible for freeing the storage.
//! @param  myIP   - Pointer to IP address of client.
//! @param  opts   - Options to pass to the XrdSecsssEnt data extractor.
//!                  See XrdSecsssEnt::rr_Data for details.
//!
//! @return The length of the structure pointed to by dP; zero if not found.
//-----------------------------------------------------------------------------

int      Find(const char *lid, char *&dP, const char *myIP, int dataOpts=0);

//-----------------------------------------------------------------------------
//! Get initial parameters for sss ID mapping.
//!
//! @param  atype  - The authentication type used by this object.
//! @param  idP    - Reference to a pointer where the default ID is returned.
//!
//! @return A pointer to this object if it was instantiated, otherwise nil.
//-----------------------------------------------------------------------------
static
XrdSecsssID  *getObj(authType &aType, XrdSecsssEnt *&idP);

static
XrdSecsssEnt *genID(bool Secure);

XrdSecsssEnt *defaultID;
authType      myAuth;
bool          isStatic;
bool          trackOK;
};
#endif
