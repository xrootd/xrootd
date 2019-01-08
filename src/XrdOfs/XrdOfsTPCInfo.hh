#ifndef __XRDOFSTPCINFO_HH__
#define __XRDOFSTPCINFO_HH__
/******************************************************************************/
/*                                                                            */
/*                      X r d O f s T P C I n f o . h h                       */
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

#include <stdlib.h>
#include <string.h>

#include "XrdOuc/XrdOucCallBack.hh"

class XrdOucErrInfo;
class XrdSysMutex;

class XrdOfsTPCInfo
{
public:

void        Engage() {inWtR = true;} // Must be called w/ a serialization lock!

int         Fail(XrdOucErrInfo *eRR, const char *eMsg, int eCode);

void        isDest() {isDST = true;}

int         Match(const char *cKey, const char *cOrg,
                  const char *xLfn, const char *xDst);

void        Reply(int rC, int eC, const char *eMsg, XrdSysMutex *mP=0);

const char *Set(const char *cKey, const char *cOrg,
                const char *xLfn, const char *xDst,
                const char *xCks=0);

int         SetCB(XrdOucErrInfo *eRR);

void        SetCreds(const char *evar, const char *creds, int crdsz)
                    {Env = evar;
                     Crd = (char *)malloc(crdsz);
                     memcpy(Crd, creds, crdsz);
                     Csz = crdsz;
                    }

void        SetStreams(char sval) {Str = sval;}

void        Success() {isAOK = true;}

            XrdOfsTPCInfo(const char *vKey=0, const char *vOrg=0,
                          const char *vLfn=0, const char *vDst=0,
                          const char *vCks=0, const char *vSpr=0,
                          const char *vTpr=0) : cbP(0),
                      Cks(vCks ? strdup(vCks) :0),
                      Key(vKey ? strdup(vKey) :0),
                      Org(vOrg ? strdup(vOrg) :0),
                      Lfn(vLfn ? strdup(vLfn) :0),
                      Dst(vDst ? strdup(vDst) :0),
                      Spr(vSpr ? strdup(vSpr) :0),
                      Tpr(vTpr ? strdup(vTpr) :0),
                      Env(0), Crd(0), Csz(0), Str(0),
                      inWtR(false), isDST(false), isAOK(false)
                      {}

           ~XrdOfsTPCInfo();

XrdOucCallBack *cbP;   // Callback object
char           *Cks;   // Checksum information (only at dest)
char           *Key;   // Rendezvous key    or src  URL
char           *Org;   // Rendezvous origin
char           *Lfn;   // Rendezvous path   or dest LFN
char           *Dst;   // Rendezvous dest   or dest PFN
char           *Spr;   // Source protocol
char           *Tpr;   // Target protocol
const char     *Env;   // -> creds envar name
char           *Crd;   // Credentials to be forwarded dst->src
int             Csz;   // Size of credentials
char            Str;   // Number of streams to use
bool           inWtR;  // Traget in waitresp status, async reply is valid.
bool           isDST;  // This info is about the destination file (PFN)
bool           isAOK;  // The copy succeeded
};
#endif
