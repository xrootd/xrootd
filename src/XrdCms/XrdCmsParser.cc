/******************************************************************************/
/*                                                                            */
/*                       X r d C m s P a r s e r . c c                        */
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
  
#include <cstdio>
#include <cerrno>
#include <cinttypes>
#include <cstdarg>
#include <stddef.h>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sys/types.h>

#include "XrdCms/XrdCmsParser.hh"
#include "XrdCms/XrdCmsRRData.hh"
#include "XrdCms/XrdCmsTrace.hh"

#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucBuffer.hh"

#include "XrdSfs/XrdSfsInterface.hh"

#include "XrdSys/XrdSysError.hh"

using namespace XrdCms;

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/
  
class XrdCmsParseInit
{
public:

const  char **nameVec() {return (const char **)PupNVec;}

       XrdCmsParseInit(int mVal, ...)
                      {va_list ap;
                       int vp = mVal;
//                     const char *Dummy;
                       memset(PupNVec, 0, sizeof(PupNVec));
                       va_start(ap, mVal);
                       do { if (vp < XrdCmsRRData::Arg_Count)
                               PupNVec[vp] = va_arg(ap, char *);
                               else          va_arg(ap, char *);
                          } while((vp = va_arg(ap, int)));
                       va_end(ap);
                      }
      ~XrdCmsParseInit() {}

private:

static char  *PupNVec[XrdCmsRRData::Arg_Count];

};

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/

char           *XrdCmsParseInit::PupNVec[XrdCmsRRData::Arg_Count];

XrdCmsParseInit XrdCmsParseArgN(XrdCmsRRData::Arg_Null,    "",
                                XrdCmsRRData::Arg_AToken,  "authtoken",
                                XrdCmsRRData::Arg_Avoid,   "bad_host",
                                XrdCmsRRData::Arg_CGI,     "CGI",
                                XrdCmsRRData::Arg_Datlen,  "datalen",
                                XrdCmsRRData::Arg_Ident,   "ident",
                                XrdCmsRRData::Arg_Ilist,   "interfaces",
                                XrdCmsRRData::Arg_Mode,    "mode",
                                XrdCmsRRData::Arg_Notify,  "notify",
                                XrdCmsRRData::Arg_Opaque,  "opaque",
                                XrdCmsRRData::Arg_Opaque2, "opaque2",
                                XrdCmsRRData::Arg_Opts,    "opts",
                                XrdCmsRRData::Arg_Path,    "path",
                                XrdCmsRRData::Arg_Path2,   "path2",
                                XrdCmsRRData::Arg_Prty,    "prty",
                                XrdCmsRRData::Arg_Reqid,   "reqid",
                                XrdCmsRRData::Arg_dskFree, "diskfree",
                                XrdCmsRRData::Arg_dskTot,  "disktotal",
                                XrdCmsRRData::Arg_dskMinf, "diskminf",
                                XrdCmsRRData::Arg_dskUtil, "diskutil",
                                XrdCmsRRData::Arg_theLoad, "load",
                                XrdCmsRRData::Arg_Info,    "info",
                                XrdCmsRRData::Arg_Port,    "port",
                                XrdCmsRRData::Arg_SID,     "SID",
                                0,                         (const char *)0
                               );

// The structure that defines the item names to the packer/unpacker
//
XrdOucPupNames XrdCmsParser::PupName(XrdCmsParseArgN.nameVec(),
                                     XrdCmsRRData::Arg_Count);

// Common protocol data unpacker
//
XrdOucPup      XrdCmsParser::Pup(&Say, &XrdCmsParser::PupName);

// Reference array
//
XrdOucPupArgs *XrdCmsParser::vecArgs[kYR_MaxReq] = {0};

// The actual parser object
//
XrdCmsParser   XrdCms::Parser;

/******************************************************************************/
/*          S t a t i c   P a r s i n g   D e s i f i n i t i o n s           */
/******************************************************************************/
  
// {chmod, mkdir, mkpath, trunc} <id> <mode> <path> [<opq>]
//
XrdOucPupArgs XrdCmsParser::fwdArgA[] =
/*0*/        {setPUP1(XrdCmsRRData::Arg_Ident,   char, XrdCmsRRData, Ident),
/*1*/         setPUP1(XrdCmsRRData::Arg_Mode,    char, XrdCmsRRData, Mode),
/*2*/         setPUP1(XrdCmsRRData::Arg_Path,    char, XrdCmsRRData, Path),
/*3*/         setPUP0(Fence),
/*4*/         setPUP1(XrdCmsRRData::Arg_Opaque,  char, XrdCmsRRData, Opaque),
/*5*/         setPUP1(XrdCmsRRData::Arg_Datlen,EndFill,XrdCmsRRData, Request.datalen)
             };

// mv <id> <path1> <path2> [<opq1> [<opq2>]]
//
XrdOucPupArgs XrdCmsParser::fwdArgB[] =
/*0*/        {setPUP1(XrdCmsRRData::Arg_Ident,   char, XrdCmsRRData, Ident),
/*1*/         setPUP1(XrdCmsRRData::Arg_Path,    char, XrdCmsRRData, Path),
/*2*/         setPUP1(XrdCmsRRData::Arg_Path2,   char, XrdCmsRRData, Path2),
/*3*/         setPUP0(Fence),
/*4*/         setPUP1(XrdCmsRRData::Arg_Opaque,  char, XrdCmsRRData, Opaque),
/*5*/         setPUP1(XrdCmsRRData::Arg_Opaque2, char, XrdCmsRRData, Opaque2),
/*6*/         setPUP1(XrdCmsRRData::Arg_Datlen,EndFill,XrdCmsRRData, Request.datalen)
             };

// {rm, rmdir, statfs} <id> <path> <opq>
//
XrdOucPupArgs XrdCmsParser::fwdArgC[] =
/*0*/        {setPUP1(XrdCmsRRData::Arg_Ident,   char, XrdCmsRRData, Ident),
/*1*/         setPUP1(XrdCmsRRData::Arg_Path,    char, XrdCmsRRData, Path),
/*2*/         setPUP0(Fence),
/*3*/         setPUP1(XrdCmsRRData::Arg_Opaque,  char, XrdCmsRRData, Opaque),
/*4*/         setPUP1(XrdCmsRRData::Arg_Datlen,EndFill,XrdCmsRRData, Request.datalen)
            };

// {locate, select} <id> <opts> <path> [<opq> [<avoid>]]
//
XrdOucPupArgs XrdCmsParser::locArgs[] =
/*0*/        {setPUP1(XrdCmsRRData::Arg_Ident,   char, XrdCmsRRData, Ident),
/*1*/         setPUP1(XrdCmsRRData::Arg_Opts,    int,  XrdCmsRRData, Opts),
/*2*/         setPUP1(XrdCmsRRData::Arg_Path,    char, XrdCmsRRData, Path),
/*3*/         setPUP1(XrdCmsRRData::Arg_Datlen,Datlen, XrdCmsRRData, PathLen),
/*4*/         setPUP0(Fence),
/*5*/         setPUP1(XrdCmsRRData::Arg_Opaque,  char, XrdCmsRRData, Opaque),
/*6*/         setPUP1(XrdCmsRRData::Arg_Avoid,   char, XrdCmsRRData, Avoid),
/*7*/         setPUP1(XrdCmsRRData::Arg_Datlen,EndFill,XrdCmsRRData, Request.datalen)
             };

// prepadd <id> <reqid> <notify> <prty> <mode> <path> [<opaque>]
//
XrdOucPupArgs XrdCmsParser::padArgs[] =
/*0*/        {setPUP1(XrdCmsRRData::Arg_Ident,   char, XrdCmsRRData, Ident),
/*1*/         setPUP1(XrdCmsRRData::Arg_Reqid,   char, XrdCmsRRData, Reqid),
/*2*/         setPUP1(XrdCmsRRData::Arg_Notify,  char, XrdCmsRRData, Notify),
/*3*/         setPUP1(XrdCmsRRData::Arg_Prty,    char, XrdCmsRRData, Prty),
/*4*/         setPUP1(XrdCmsRRData::Arg_Mode,    char, XrdCmsRRData, Mode),
/*5*/         setPUP1(XrdCmsRRData::Arg_Path,    char, XrdCmsRRData, Path),
/*6*/         setPUP1(XrdCmsRRData::Arg_Datlen,Datlen, XrdCmsRRData, PathLen),
/*7*/         setPUP0(Fence),
/*8*/         setPUP1(XrdCmsRRData::Arg_Opaque,  char, XrdCmsRRData, Opaque),
/*9*/         setPUP1(XrdCmsRRData::Arg_Datlen,EndFill,XrdCmsRRData, Request.datalen)
             };

// prepdel <id> <reqid>
//
XrdOucPupArgs XrdCmsParser::pdlArgs[] =
/*0*/        {setPUP1(XrdCmsRRData::Arg_Ident,   char, XrdCmsRRData, Ident),
/*1*/         setPUP1(XrdCmsRRData::Arg_Reqid,   char, XrdCmsRRData, Reqid),
/*2*/         setPUP1(XrdCmsRRData::Arg_Datlen,EndFill,XrdCmsRRData, Request.datalen)
             };

// avail   <dskFree> <dskUtil>
//
XrdOucPupArgs XrdCmsParser::avlArgs[] =
/*0*/        {setPUP1(XrdCmsRRData::Arg_dskFree, int, XrdCmsRRData, dskFree),
/*1*/         setPUP1(XrdCmsRRData::Arg_dskUtil, int, XrdCmsRRData, dskUtil),
/*2*/         setPUP0(End)
             };

// try <path>
//
XrdOucPupArgs XrdCmsParser::pthArgs[] =
/*0*/        {setPUP1(XrdCmsRRData::Arg_Path,    char, XrdCmsRRData, Path),
/*1*/         setPUP1(XrdCmsRRData::Arg_Datlen,Datlen, XrdCmsRRData, PathLen),
/*2*/         setPUP1(XrdCmsRRData::Arg_Datlen,EndFill,XrdCmsRRData, Request.datalen)
             };

// load <cpu> <io> <load> <mem> <pag> <dut> <dsk>
//      0     1    2      3     5     5
XrdOucPupArgs XrdCmsParser::lodArgs[] =
/*0*/        {setPUP1(XrdCmsRRData::Arg_theLoad, char, XrdCmsRRData, Opaque),
/*1*/         setPUP1(XrdCmsRRData::Arg_dskFree, int,  XrdCmsRRData, dskFree),
/*2*/         setPUP0(End)
             };

XrdOucPupArgs XrdCmsParser::logArgs[] =
/*0*/        {setPUP1(XrdCmsRRData::Arg_Ident,   short,   CmsLoginData, Version),
/*1*/         setPUP1(XrdCmsRRData::Arg_Mode,    int,     CmsLoginData, Mode),
/*2*/         setPUP1(XrdCmsRRData::Arg_Info,    int,     CmsLoginData, HoldTime),
/*3*/         setPUP1(XrdCmsRRData::Arg_dskTot,  int,     CmsLoginData, tSpace),
/*4*/         setPUP1(XrdCmsRRData::Arg_dskFree, int,     CmsLoginData, fSpace),
/*5*/         setPUP1(XrdCmsRRData::Arg_dskMinf, int,     CmsLoginData, mSpace),
/*6*/         setPUP1(XrdCmsRRData::Arg_Info,    short,   CmsLoginData, fsNum),
/*7*/         setPUP1(XrdCmsRRData::Arg_dskUtil, short,   CmsLoginData, fsUtil),
/*8*/         setPUP1(XrdCmsRRData::Arg_Port,    short,   CmsLoginData, dPort),
/*9*/         setPUP1(XrdCmsRRData::Arg_Port,    short,   CmsLoginData, sPort),
/*0*/         setPUP0(Fence),
/*1*/         setPUP1(XrdCmsRRData::Arg_SID,     char,    CmsLoginData, SID),
/*2*/         setPUP1(XrdCmsRRData::Arg_Path,    char,    CmsLoginData, Paths),
/*3*/         setPUP1(XrdCmsRRData::Arg_Ilist,   char,    CmsLoginData, ifList),
/*4*/         setPUP1(XrdCmsRRData::Arg_CGI,     char,    CmsLoginData, envCGI),
/*5*/         setPUP1(XrdCmsRRData::Arg_Datlen,EndFill,   CmsLoginData, Size)
             };

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdCmsParser::XrdCmsParser()
{
   static int Done = 0;

// Setup the Parse vector
//
   if (!Done)
      {vecArgs[kYR_login]   =  logArgs;
       vecArgs[kYR_chmod]   =  fwdArgA;
       vecArgs[kYR_locate]  =  locArgs;
       vecArgs[kYR_mkdir]   =  fwdArgA;
       vecArgs[kYR_mkpath]  =  fwdArgA;
       vecArgs[kYR_mv]      =  fwdArgB;
       vecArgs[kYR_prepadd] =  padArgs;
       vecArgs[kYR_prepdel] =  pdlArgs;
       vecArgs[kYR_rm]      =  fwdArgC;
       vecArgs[kYR_rmdir]   =  fwdArgC;
       vecArgs[kYR_select]  =  locArgs;
       vecArgs[kYR_rm]      =  fwdArgC;
       vecArgs[kYR_statfs]  =  pthArgs;
       vecArgs[kYR_avail]   =  avlArgs;
       vecArgs[kYR_gone]    =  pthArgs;
       vecArgs[kYR_trunc]   =  fwdArgA;
       vecArgs[kYR_try]     =  pthArgs;
       vecArgs[kYR_have]    =  pthArgs;
       vecArgs[kYR_load]    =  lodArgs;
       vecArgs[kYR_state]   =  pthArgs;
       Done = 1;
      }
}

/******************************************************************************/
/*                                D e c o d e                                 */
/******************************************************************************/

// Decode responses to the redirector. Very simple lean protocol.

int XrdCmsParser::Decode(const char *Man, CmsRRHdr &hdr, XrdOucBuffer *dBuff,
                         XrdOucErrInfo *eInfo)
{
   EPNAME("Decode");
   static const int mvsz = static_cast<int>(sizeof(kXR_unt32));
   kXR_unt32    uval;
   int          Result, msgval, msglen, dlen = dBuff->DataLen();
   const char  *Path = eInfo->getErrData(), *User = eInfo->getErrUser();
   const char  *Mgr  = (Man ? Man : "?");
   char        *msg, *data = dBuff->Buffer();

// Path may be null here, fix it
//
   if (!Path) Path = "";

// Responses are always in the form of <int><string>
//
   if (dlen < mvsz) {msgval = 0; msg = (char *)""; msglen = 0;}
      else {memcpy(&uval, data, mvsz);
            msgval = static_cast<int>(ntohl(uval));
            if (dlen == mvsz) {msg = (char *)""; msglen = 0;}
               else {msg = data+mvsz; msglen = dlen - mvsz;}
           }

// Now decode the response code
//
   switch(hdr.rrCode)

   {case kYR_redirect:  Result = SFS_REDIRECT;
             TRACE(Redirect, Mgr <<" redirects " <<User <<" to "
                   <<msg <<':' <<msgval <<' ' <<Path);
             break;
    case kYR_wait:      Result = SFS_STALL;
             TRACE(Redirect, Mgr <<" delays " <<User <<' ' <<msgval <<' ' <<Path);
             break;
    case kYR_waitresp:  Result = SFS_STARTED;
             TRACE(Redirect, Mgr <<" idles " <<User <<' ' <<msgval <<' ' <<Path);
             break;
    case kYR_data:      Result = SFS_DATA; msgval = msglen;
             TRACE(Redirect, Mgr <<" sent " <<User <<" '" <<msg <<"' " <<Path);
             if (msglen > (int)XrdOucEI::Max_Error_Len)
                {XrdOucBuffer *myBuff=dBuff->Highjack(XrdOucEI::Max_Error_Len);
                 if (myBuff)
                    {myBuff->SetLen(msglen, (msglen ? mvsz : 0));
                     eInfo->setErrInfo(msglen, myBuff);
                     return Result;
                    }
                }
             break;
    case kYR_error:     Result = SFS_ERROR;
             if (msgval) msgval = -mapError(msgval);
             TRACE(Redirect, Mgr <<" gave " <<User <<" err " <<msgval
                             <<" '" <<msg <<"' " <<Path);
             break;
    default: msgval=0;  Result = SFS_ERROR;
             msg = (char *)"Redirector protocol error";
             TRACE(Redirect, User <<" given error msg '"
                      <<msg <<"' due to " << Mgr <<' ' <<Path);
   }

// Insert the information into the error object
//
   eInfo->setErrInfo(msgval, msg);
   return Result;
}

/******************************************************************************/
/*                              m a p E r r o r                               */
/******************************************************************************/
  
int XrdCmsParser::mapError(const char *ecode)
{
   if (!strcmp("ENOENT", ecode))       return ENOENT;
   if (!strcmp("EEXIST", ecode))       return EEXIST;
   if (!strcmp("EPERM", ecode))        return EPERM;
   if (!strcmp("EACCES", ecode))       return EACCES;
   if (!strcmp("EIO", ecode))          return EIO;
   if (!strcmp("ENOMEM", ecode))       return ENOMEM;
   if (!strcmp("ENOSPC", ecode))       return ENOSPC;
   if (!strcmp("ENAMETOOLONG", ecode)) return ENAMETOOLONG;
   if (!strcmp("ENETUNREACH", ecode))  return ENETUNREACH;
   if (!strcmp("ENOTBLK", ecode))      return ENOTBLK;
   if (!strcmp("EISDIR", ecode))       return EISDIR;
   return EINVAL;
}
  
int XrdCmsParser::mapError(int ecode)
{
   switch(ecode)
         {case kYR_ENOENT:             return ENOENT;
          case kYR_EPERM:              return EPERM;
          case kYR_EACCES:             return EACCES;
          case kYR_EIO:                return EIO;
          case kYR_ENOMEM:             return ENOMEM;
          case kYR_ENOSPC:             return ENOSPC;
          case kYR_ENAMETOOLONG:       return ENAMETOOLONG;
          case kYR_ENETUNREACH:        return ENETUNREACH;
          case kYR_ENOTBLK:            return ENOTBLK;
          case kYR_EISDIR:             return EISDIR;
          case kYR_FSError:            return ENODEV;
          case kYR_SrvError:           return EFAULT;
          case kYR_RWConflict:         return EEXIST;
          case kYR_noReplicas:         return EADDRNOTAVAIL;
          default:                     return EINVAL;
         }
}

/******************************************************************************/
/*                                  P a c k                                   */
/******************************************************************************/
  
int XrdCmsParser::Pack(int rnum, struct iovec *iovP, struct iovec *iovE,
                       char *Base, char *Work)
{
   XrdOucPupArgs *PArgs;
   const char    *reason;
   char           buff[16];
   int            iovcnt;

// Pack the request
//
   if ((PArgs = PupArgs(rnum)))
      if ((iovcnt = Pup.Pack(iovP, iovE, PArgs, Base, Work))) return iovcnt;
         else reason = "too much data for code";
      else    reason = "invalid request code -";

// Indicate failure (we don't translate the request code as it drags in too
// many dependent object files, sigh.
//
   sprintf(buff, "%d", rnum);
   Say.Emsg("Pack", "Unable to pack request;", reason, buff);
   return 0;
}
