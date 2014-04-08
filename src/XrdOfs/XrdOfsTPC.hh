#ifndef __XRDOFSTPC_HH__
#define __XRDOFSTPC_HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d O f s T P C . h h                           */
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
  
#include "XrdOfs/XrdOfsTPCInfo.hh"

class XrdAccAuthorize;
class XrdOfsTPCAllow;
class XrdOfsTPCJob;
class XrdOucEnv;
class XrdOucErrInfo;
class XrdOucPListAnchor;
class XrdOucTList;
class XrdSecEntity;

class XrdOfsTPC
{
public:

struct Facts
      {const char          *Key;
       const char          *Lfn;
       const char          *Pfn;
       const char          *Org;
       const char          *Dst;
       const XrdSecEntity  *Usr;
             XrdOucErrInfo *eRR;
             XrdOucEnv     *Env;

       Facts(const XrdSecEntity *vEnt, XrdOucErrInfo *vInf, XrdOucEnv *vEnv,
             const char *vKey, const char *vLfn, const char *vPfn=0)
            : Key(vKey), Lfn(vLfn), Pfn(vPfn), Org(0), Dst(0),
              Usr(vEnt), eRR(vInf), Env(vEnv) {}
      };

static  void  Allow(char *vDN, char *vGN, char *vHN, char *vVO);

static  int   Authorize(XrdOfsTPC **theTPC,
                        Facts      &Args,
                        int         isPLE=0);

virtual void  Del() {}

struct  iParm {char *Pgm;
               char *Ckst;
               int   Dflttl;
               int   Maxttl;
               int   Logok;
               int   Strm;
               int   Xmax;
               int   Grab;
               int   xEcho;
               int   autoRM;
                     iParm() : Pgm(0), Ckst(0), Dflttl(-1), Maxttl(-1),
                               Logok(-1), Strm(-1), Xmax(-1), Grab(0), 
                               xEcho(-1), autoRM(-1) {}
              };

static  void  Init(iParm &Parms);

static  void  Init(XrdAccAuthorize *accP) {fsAuth = accP;}

static  const int reqALL = 0;
static  const int reqDST = 1;
static  const int reqORG = 2;

static  void  Require(const char *Auth, int RType);

static  int   Restrict(const char *Path);

static  int   Start();

virtual int   Sync(XrdOucErrInfo *error) {return 0;}

static  int   Validate (XrdOfsTPC **theTPC, Facts &Args);

              XrdOfsTPC() : Refs(1), inQ(0) {}

              XrdOfsTPC(const char *Url, const char *Org,
                        const char *Lfn, const char *Pfn, const char *Cks=0)
                       : Info(Url, Org, Lfn, Pfn, Cks), Refs(1), inQ(0) {}

virtual      ~XrdOfsTPC() {}

XrdOfsTPCInfo Info;

protected:

static int    Fatal(Facts &Args, const char *eMsg, int eCode, int nomsg=0);
static int    genOrg(const XrdSecEntity *client, char *Buff, int Blen);
static int    getTTL(XrdOucEnv *Env);
static int    Screen(Facts &Args, XrdOucTList *tP, int wasEnc=0);
static char  *Verify(const char *Who,const char *Name,char *Buf,int Blen);

static XrdAccAuthorize   *fsAuth;

static XrdOucTList       *AuthDst;
static XrdOucTList       *AuthOrg;

static XrdOfsTPCAllow    *ALList;
static XrdOucPListAnchor *RPList;
static int                maxTTL;
static int                dflTTL;

       char               Refs;      // Reference count
       char               inQ;       // Object in queue
};
#endif
