#ifndef __XRDCMSPARSER_H__
#define __XRDCMSPARSER_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d C m s P a r s e r . h h                        */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XProtocol/YProtocol.hh"

#include "XrdCms/XrdCmsRRData.hh"
#include "XrdOuc/XrdOucPup.hh"

/******************************************************************************/
/*                    C l a s s   X r d C m s P a r s e r                     */
/******************************************************************************/

class XrdOucErrInfo;
class XrdOucBuffer;
  
class XrdCmsParser
{
public:

static int            Decode(const char *Man, XrdCms::CmsRRHdr &hdr, 
                                   XrdOucBuffer *dBuff, XrdOucErrInfo *eInfo);

static int            mapError(const char *ecode);

static int            mapError(int ecode);

static int            Pack(int rnum, struct iovec *iovP, struct iovec *iovE,
                           char *Base, char *Work);

inline int            Parse(XrdCms::CmsLoginData *Data, 
                            const char *Aps, const char *Apt)
                           {Data->SID = Data->Paths = 0;
                            return Pup.Unpack(Aps,Apt,vecArgs[XrdCms::kYR_login],
                                              (char *)Data);
                           }

inline int            Parse(int rnum, const char *Aps, const char *Apt, 
                            XrdCmsRRData *Data)
                           {Data->Opaque = Data->Opaque2 = Data->Path = 0;
                            return rnum < XrdCms::kYR_MaxReq 
                                   && vecArgs[rnum] != 0
                                   && Pup.Unpack(Aps, Apt,
                                      vecArgs[rnum], (char *)Data);
                           }

static XrdOucPup      Pup;

static XrdOucPupArgs *PupArgs(int rnum)
                             {return rnum < XrdCms::kYR_MaxReq ? vecArgs[rnum] : 0;}

       XrdCmsParser();
      ~XrdCmsParser() {}

private:

static const char   **PupNVec;
static XrdOucPupNames PupName;

static XrdOucPupArgs  fwdArgA[];  // chmod | mkdir | mkpath | trunc
static XrdOucPupArgs  fwdArgB[];  // mv
static XrdOucPupArgs  fwdArgC[];  // rm | rmdir
static XrdOucPupArgs  locArgs[];  // locate | select
static XrdOucPupArgs  padArgs[];  // prepadd
static XrdOucPupArgs  pdlArgs[];  // prepdel
static XrdOucPupArgs  avlArgs[];  // avail
static XrdOucPupArgs  pthArgs[];  // statfs | try
static XrdOucPupArgs  lodArgs[];  // load
static XrdOucPupArgs  logArgs[];  // login

static XrdOucPupArgs *vecArgs[XrdCms::kYR_MaxReq];
};

namespace XrdCms
{
extern    XrdCmsParser Parser;
}
#endif
