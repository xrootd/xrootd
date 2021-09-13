#ifndef __SecsssEnt__
#define __SecsssEnt__
/******************************************************************************/
/*                                                                            */
/*                       X r d S e c s s s E n t . h h                        */
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

#include <set>
#include <string>
#include <cstdlib>

#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysPthread.hh"

class XrdSecEntity;

class XrdSecsssEnt
{
public:

char         *eData;    // -> RR
int           iLen;     // Length of V1 data
int           tLen;     // Length of V1 plus V2 data (follows iLen)

//-----------------------------------------------------------------------------
//! Add a new connection created by this entity.
//!
//! @param  hostID - The hostID (i.e. user[:pswd]@host:port).
//-----------------------------------------------------------------------------

void          AddContact(const std::string &hostID);

//-----------------------------------------------------------------------------
//! Delete this entity object.
//-----------------------------------------------------------------------------

void          Delete();

//-----------------------------------------------------------------------------
//! Return serialized entity infrmation.
//!
//! @param  dP     - Reference to a pointer where the serialized ID is returned.
//!                  The caller is responsible for freeing the storage.
//! @param  myIP   - Pointer to IP address of client.
//! @param  opts   - Options as follows:
//!                  addExtra - This is a V2 client, include extra info
//!                  addCreds - This is a V2 client, add credentials to extra
//!
//! @return The length of the structure pointed to by dP; zero if not found.
//-----------------------------------------------------------------------------

static const int addExtra = 0x00000001; //!< Add v2 data
static const int addCreds = 0x00000002; //!< Add v2 data plus creds
static const int v2Client = 0x00000003; //!< Data for a v2 client wanted

int           RR_Data(char *&dP, const char *hostIP, int dataOpts);


void          Ref() {AtomicBeg(eMtx); AtomicInc(refCnt); AtomicEnd(eMtx);}

void          UnRef()
                    {AtomicBeg(eMtx);
                     int x = AtomicDec(refCnt);
                     AtomicEnd(eMtx);
                     if (x < 1) delete this;
                    }

static void   setHostName(const char *hnP);

              XrdSecsssEnt(const XrdSecEntity *entity=0, bool defer=false)
                          : eData(0), iLen(0), tLen(0), eP(entity), refCnt(1)
                          {if (!defer) Serialize();}

//-----------------------------------------------------------------------------
//! Destructor cannot be directly called; use Delete() instead.
//-----------------------------------------------------------------------------

private:
             ~XrdSecsssEnt() {if (eData) free(eData);}

bool          Serialize();

#ifndef HAVE_ATOMICS
XrdSysMutex   eMtx;
#endif
std::set<std::string> Contacts;

const
XrdSecEntity *eP;
int           refCnt;
short         credLen;

static char  *myHostName;
static int    myHostNLen;
};
#endif
