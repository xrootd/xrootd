/******************************************************************************/
/*                                                                            */
/*                        X r d O f s F S c t l . c c                         */
/*                                                                            */
/* (c) 2018 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC02-76-SFO0515 with the Deprtment of Energy              */
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

#include <unistd.h>
#include <cerrno>
#include <fcntl.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdNet/XrdNetIF.hh"

#include "XrdOfs/XrdOfs.hh"
#include "XrdOfs/XrdOfsFSctl_PI.hh"
#include "XrdOfs/XrdOfsTrace.hh"
#include "XrdOfs/XrdOfsSecurity.hh"

#include "XrdCms/XrdCmsClient.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFAttr.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucERoute.hh"
#include "XrdOuc/XrdOucExport.hh"

#include "XrdSec/XrdSecEntity.hh"
#include "XrdSfs/XrdSfsFAttr.hh"
#include "XrdSfs/XrdSfsFlags.hh"
#include "XrdSfs/XrdSfsInterface.hh"

#ifdef AIX
#include <sys/mode.h>
#endif

/******************************************************************************/
/*                  E r r o r   R o u t i n g   O b j e c t                   */
/******************************************************************************/

extern XrdSysError      OfsEroute;

extern XrdSysTrace      OfsTrace;

/******************************************************************************/
/*                    F i l e   S y s t e m   O b j e c t                     */
/******************************************************************************/
  
extern XrdOfs* XrdOfsFS;

/******************************************************************************/
/*                 S t o r a g e   S y s t e m   O b j e c t                  */
/******************************************************************************/
  
extern XrdOss *XrdOfsOss;
  
/******************************************************************************/
/*                     f s c t l   ( V e r s i o n   1 )                      */
/******************************************************************************/

int XrdOfs::fsctl(      int               cmd,
                  const char             *args,
                  XrdOucErrInfo          &einfo,
                  const XrdSecEntity     *client)
/*
  Function: Perform filesystem operations:

  Input:    cmd       - Operation command (currently supported):
                        SFS_FSCTL_LOCATE - locate file
                        SFS_FSCTL_STATCC - return cluster config status
                        SFS_FSCTL_STATFS - return file system info (physical)
                        SFS_FSCTL_STATLS - return file system info (logical)
                        SFS_FSCTL_STATXA - return file extended attributes
            arg       - Command dependent argument:
                      - Locate: The path whose location is wanted
            buf       - The stat structure to hold the results
            einfo     - Error/Response information structure.
            client    - Authentication credentials, if any.

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   EPNAME("fsctl");
   static int PrivTab[]     = {XrdAccPriv_Delete, XrdAccPriv_Insert,
                               XrdAccPriv_Lock,   XrdAccPriv_Lookup,
                               XrdAccPriv_Rename, XrdAccPriv_Read,
                               XrdAccPriv_Write};
   static char PrivLet[]    = {'d',               'i',
                               'k',               'l',
                               'n',               'r',
                               'w'};
   static const int PrivNum = sizeof(PrivLet);

   int retc, i, blen, privs, opcode = cmd & SFS_FSCTL_CMD;
   const char *tident = einfo.getErrUser();
   char *bP, *cP;

// Process the LOCATE request
//
   if (opcode == SFS_FSCTL_LOCATE)
      {static const int locMask = (SFS_O_FORCE|SFS_O_NOWAIT|SFS_O_RESET|
                                   SFS_O_HNAME|SFS_O_RAWIO |SFS_O_DIRLIST);
       struct stat fstat;
       char pbuff[1024], rType[3];
       const char *Resp[2] = {rType, pbuff};
       const char *locArg, *opq, *Path = Split(args,&opq,pbuff,sizeof(pbuff));
       XrdNetIF::ifType ifType;
       int Resp1Len;
       int find_flag = SFS_O_LOCATE | (cmd & locMask);
       XrdOucEnv loc_Env(opq ? opq+1 : 0,0,client);

       ZTRACE(fsctl, "locate args=" <<(args ? args : "''"));

       if (cmd & SFS_O_TRUNC)           locArg = (char *)"*";
          else {     if (*Path == '*') {locArg = Path; Path++;}
                        else            locArg = Path;
                AUTHORIZE(client,0,AOP_Stat,"locate",Path,einfo);
               }
       if (Finder && Finder->isRemote()
       &&  (retc = Finder->Locate(einfo, locArg, find_flag, &loc_Env)))
          return fsError(einfo, retc);

       if (cmd & SFS_O_TRUNC) {rType[0] = 'S'; rType[1] = ossRW;}
          else {if ((retc = XrdOfsOss->Stat(Path, &fstat, 0, &loc_Env)))
                   return XrdOfsFS->Emsg(epname, einfo, retc, "locate", Path);
                rType[0] = ((fstat.st_mode & S_IFBLK) == S_IFBLK ? 's' : 'S');
                rType[1] =  (fstat.st_mode & S_IWUSR             ? 'w' : 'r');
               }
       rType[2] = '\0';

       ifType = XrdNetIF::GetIFType((einfo.getUCap() & XrdOucEI::uIPv4)  != 0,
                                    (einfo.getUCap() & XrdOucEI::uIPv64) != 0,
                                    (einfo.getUCap() & XrdOucEI::uPrip)  != 0);
       bool retHN = (cmd & SFS_O_HNAME) != 0;
       if ((Resp1Len = myIF->GetDest(pbuff, sizeof(pbuff), ifType, retHN)))
           {einfo.setErrInfo(Resp1Len+3, (const char **)Resp, 2);
            return SFS_DATA;
           }
       return Emsg(epname, einfo, ENETUNREACH, "locate", Path);
      }

// Process the STATFS request
//
   if (opcode == SFS_FSCTL_STATFS)
      {char pbuff[1024];
       const char *opq, *Path = Split(args, &opq, pbuff, sizeof(pbuff));
       XrdOucEnv fs_Env(opq ? opq+1 : 0,0,client);
       ZTRACE(fsctl, "statfs args=" <<(args ? args : "''"));
       AUTHORIZE(client,0,AOP_Stat,"statfs",Path,einfo);
       if (Finder && Finder->isRemote()
       &&  (retc = Finder->Space(einfo, Path, &fs_Env)))
          return fsError(einfo, retc);
       bP = einfo.getMsgBuff(blen);
       if ((retc = XrdOfsOss->StatFS(Path, bP, blen, &fs_Env)))
          return XrdOfsFS->Emsg(epname, einfo, retc, "statfs", args);
       einfo.setErrCode(blen+1);
       return SFS_DATA;
      }

// Process the STATLS request
//
   if (opcode == SFS_FSCTL_STATLS)
      {char pbuff[1024];
       const char *opq, *Path = Split(args, &opq, pbuff, sizeof(pbuff));
       XrdOucEnv statls_Env(opq ? opq+1 : 0,0,client);
       ZTRACE(fsctl, "statls args=" <<(args ? args : "''"));
       AUTHORIZE(client,0,AOP_Stat,"statfs",Path,einfo);
       if (Finder && Finder->isRemote())
          {statls_Env.Put("cms.qvfs", "1");
           if ((retc = Finder->Space(einfo, Path, &statls_Env)))
              {if (retc == SFS_DATA) retc = Reformat(einfo);
               return fsError(einfo, retc);
              }
          }
       bP = einfo.getMsgBuff(blen);
       if ((retc = XrdOfsOss->StatLS(statls_Env, Path, bP, blen)))
          return XrdOfsFS->Emsg(epname, einfo, retc, "statls", Path);
       einfo.setErrCode(blen+1);
       return SFS_DATA;
      }

// Process the STATXA request
//
   if (opcode == SFS_FSCTL_STATXA)
      {char pbuff[1024];
       const char *opq, *Path = Split(args, &opq, pbuff, sizeof(pbuff));
       XrdOucEnv xa_Env(opq ? opq+1 : 0,0,client);
       ZTRACE(fsctl, "statxa args=" <<(args ? args : "''"));
       AUTHORIZE(client,0,AOP_Stat,"statxa",Path,einfo);
       if (Finder && Finder->isRemote()
       && (retc = Finder->Locate(einfo,Path,SFS_O_RDONLY|SFS_O_STAT,&xa_Env)))
          return fsError(einfo, retc);
       bP = einfo.getMsgBuff(blen);
       if ((retc = XrdOfsOss->StatXA(Path, bP, blen, &xa_Env)))
          return XrdOfsFS->Emsg(epname, einfo, retc, "statxa", Path);
       if (!client || !XrdOfsFS->Authorization) privs = XrdAccPriv_All;
          else privs = XrdOfsFS->Authorization->Access(client, Path, AOP_Any);
       cP = bP + blen; strcpy(cP, "&ofs.ap="); cP += 8;
       if (privs == XrdAccPriv_All) *cP++ = 'a';
          else {for (i = 0; i < PrivNum; i++)
                    if (PrivTab[i] & privs) *cP++ = PrivLet[i];
                if (cP == (bP + blen + 1)) *cP++ = '?';
               }
       *cP++ = '\0';
       einfo.setErrCode(cP-bP+1);
       return SFS_DATA;
      }

// Process the STATCC request (this should always succeed)
//
   if (opcode == SFS_FSCTL_STATCC)
      {static const int lcc_flag = SFS_O_LOCATE | SFS_O_LOCAL;
       XrdOucEnv lcc_Env(0,0,client);
       ZTRACE(fsctl, "statcc args=" <<(args ? args : "''"));
            if (Finder)   retc = Finder  ->Locate(einfo,".",lcc_flag,&lcc_Env);
       else if (Balancer) retc = Balancer->Locate(einfo,".",lcc_flag,&lcc_Env);
       else retc = SFS_ERROR;
       if (retc != SFS_DATA) einfo.setErrInfo(5, "none|");
       return fsError(einfo, SFS_DATA);
      }

// Operation is not supported
//
   return XrdOfsFS->Emsg(epname, einfo, ENOTSUP, "fsctl", args);
}
 
/******************************************************************************/
/*                     F S c t l   ( V e r s i o n   2 )                      */
/******************************************************************************/
  
int XrdOfs::FSctl(const int            cmd,
                  XrdSfsFSctl          &args,
                  XrdOucErrInfo        &eInfo,
                  const XrdSecEntity  *client)
{
// If we have a plugin to handle this, use it.
//
   if (FSctl_PI) return FSctl_PI->FSctl(cmd, args, eInfo, client);

// Operation is not supported
//
   return XrdOfsFS->Emsg("FSctl", eInfo, ENOTSUP, "FSctl", "");
}

/******************************************************************************/
/*                            F S c t l   f i l e                             */
/******************************************************************************/

int XrdOfs::FSctl(XrdOfsFile &file, const int cmd, int alen, const char *args,
                  const XrdSecEntity *client)
{
// Supported only if we have a plugin for this
//
   if (FSctl_PI) return FSctl_PI->FSctl(cmd,alen,args,file,file.error,client);

// No Go
//
   file.error.setErrInfo(ENOTSUP, "fctl operation not supported");
   return SFS_ERROR;
}
