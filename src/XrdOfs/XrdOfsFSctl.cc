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
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdNet/XrdNetIF.hh"

#include "XrdOfs/XrdOfs.hh"
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
#include "XrdOuc/XrdOucTrace.hh"

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

extern XrdOucTrace      OfsTrace;

/******************************************************************************/
/*                    F i l e   S y s t e m   O b j e c t                     */
/******************************************************************************/
  
extern XrdOfs* XrdOfsFS;

/******************************************************************************/
/*                 S t o r a g e   S y s t e m   O b j e c t                  */
/******************************************************************************/
  
extern XrdOss *XrdOfsOss;

/******************************************************************************/
/*           L o c a l   F u n c t i o n s   a n d   O b j e c t s            */
/******************************************************************************/

namespace
{
XrdSysMutex faMutex;

static const int faSize = 8192-sizeof(XrdSfsFABuff);
}

/******************************************************************************/
/*                             G e t F A B u f f                              */
/******************************************************************************/
  
namespace
{
bool GetFABuff(XrdSfsFACtl &faCtl, int sz=0)
{
   XrdSfsFABuff *fabP = (XrdSfsFABuff *)malloc(sz + sizeof(XrdSfsFABuff));

// Check if we allocate a buffer
//
   if (!fabP) return false;

// Setup the buffer
//
   fabP->next = faCtl.fabP;
   faCtl.fabP = fabP;
   fabP->dlen = sz;
   return true;
}
}

/******************************************************************************/
/*                              G e t F A V a l                               */
/******************************************************************************/
  
namespace
{
bool GetFAVal(XrdSfsFACtl &faCtl, char *&bP, int &bL, unsigned int k)
{
   int rc;

// Get the attribute value
//
   rc = XrdSysFAttr::Xat->Get(faCtl.info[k].Name, bP, bL, faCtl.path);

// If all went well, record the value and update incomming information
//
   if (rc >= 0)
      {faCtl.info[k].faRC  = 0;
       faCtl.info[k].Value = bP;
       faCtl.info[k].VLen  = rc;
       bP += rc; bL -= rc;
       return true;
      }

// Check for any error other than buffer too small
//
   if (rc != -ERANGE)
      {faCtl.info[k].faRC = -rc;
       return true;
      }

// Buffer is too small, tell the caller to recover
//
   return false;
}
}

/******************************************************************************/
/*                             G u l p F A V a l                              */
/******************************************************************************/

namespace
{
bool GulpFAVal(XrdSfsFACtl &faCtl, char *&bP, int &bL, unsigned int k)
{
   XrdSysMutexHelper mHelper(faMutex);
   char *bzP = 0;
   int n = 0;

// Get the size of the attribute value
//
   if (!GetFAVal(faCtl, bzP, n, k))
      {faCtl.info[k].faRC = ERANGE;
       faCtl.info[k].VLen = 0;
       return true;
      }

// Allocate a new buffer to hold this and possible some more values
//
   n = faCtl.info[k].VLen;
   faCtl.info[k].VLen = 0;
   if (n < faSize/2) n = faSize;
   if (!GetFABuff(faCtl, n)) return false;

// Now fetch the variable in a right sized buffer
//
   bP = faCtl.fabP->data;
   bL = faCtl.fabP->dlen;
   if (!GetFAVal(faCtl, bP, bL, k)) faCtl.info[k].faRC = ERANGE;
      else {bP += faCtl.info[k].VLen;
            bL -= faCtl.info[k].VLen;
           }
   return true;
}
}

/******************************************************************************/
/*                              S e t N o M e m                               */
/******************************************************************************/
  
namespace
{
int SetNoMem(XrdSfsFACtl &faCtl, unsigned int iX)
{

// Set no memory error for remaining attributes
//
   for (unsigned int i = iX; i < faCtl.iNum; i++) faCtl.info[i].faRC = ENOMEM;
   return SFS_OK;
}
}
  
/******************************************************************************/
/*                     f s c t l   ( V e r s i o n   1 )                      */
/******************************************************************************/

int XrdOfs::fsctl(const int               cmd,
                  const char             *args,
                  XrdOucErrInfo          &einfo,
                  const XrdSecEntity     *client)
/*
  Function: Perform filesystem operations:

  Input:    cmd       - Operation command (currently supported):
                        SFS_FSCTL_FATTR  - Manipulate extended attributes
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
   XTRACE(fsctl, args, "");

// Process the LOCATE request
//
   if (opcode == SFS_FSCTL_LOCATE)
      {static const int locMask = (SFS_O_FORCE|SFS_O_NOWAIT|SFS_O_RESET|
                                   SFS_O_HNAME|SFS_O_RAWIO);
       struct stat fstat;
       char pbuff[1024], rType[3];
       const char *Resp[2] = {rType, pbuff};
       const char *locArg, *opq, *Path = Split(args,&opq,pbuff,sizeof(pbuff));
       XrdNetIF::ifType ifType;
       int Resp1Len;
       int find_flag = SFS_O_LOCATE | (cmd & locMask);
       XrdOucEnv loc_Env(opq ? opq+1 : 0,0,client);

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
            if (Finder)   retc = Finder  ->Locate(einfo,".",lcc_flag,&lcc_Env);
       else if (Balancer) retc = Balancer->Locate(einfo,".",lcc_flag,&lcc_Env);
       else retc = SFS_ERROR;
       if (retc != SFS_DATA) einfo.setErrInfo(5, "none|");
       return fsError(einfo, SFS_DATA);
      }

// Process the FATTR request.
//
   if (opcode == SFS_FSCTL_FATTR)
      {if (args) return ctlFAttr((XrdSfsFACtl &)*args, einfo, client);
          else {XrdOucEnv *envP = einfo.getEnv();
                if (!envP || !usxMaxNsz) return SFS_ERROR;
                envP->PutInt("usxMaxNsz", usxMaxNsz);
                envP->PutInt("usxMaxVsz", usxMaxVsz);
                return SFS_OK;
               }
      }

// Operation is not supported
//
   return XrdOfsFS->Emsg(epname, einfo, ENOTSUP, "fsctl", args);
}

/******************************************************************************/
/*                              c t l F A t t r                               */
/******************************************************************************/
  
int XrdOfs::ctlFAttr(XrdSfsFACtl        &faCtl,
                     XrdOucErrInfo      &einfo,
                     const XrdSecEntity *client)
{
   EPNAME("ctlFAttr");
   XrdOucEnv FAttr_Env(faCtl.pcgi,0,client);
   const char *accType;
   long long xOpts;

   struct faArgs {const char *name; int fArg; Access_Operation aop;};

   static faArgs faTab[] = {{ "del fattr", SFS_O_RDWR, AOP_Update}, // del
                            { "get fattr", 0,          AOP_Read},   // get
                            {"list fattr", 0,          AOP_Read},   // list
                            { "set fattr", SFS_O_RDWR, AOP_Update}  // set
                           };
   static const int faNum = sizeof(faTab)/sizeof(struct faArgs);

   int rc;

// Make sure request code is valid (we also set some options)
//
   if (faCtl.rqst > faNum)
      return Emsg(epname, einfo, EINVAL, "process fattrs", faCtl.path);
   accType  = faTab[faCtl.rqst].name;

// Extract the export options if we can
//
   xOpts = (ossRPList ? ossRPList->Find(faCtl.path) : 0);

// Perform authrorization and redirection if required
//
   if (faCtl.opts & XrdSfsFACtl::accChk)
      {int luFlag           = faTab[faCtl.rqst].fArg;
       Access_Operation aOP = faTab[faCtl.rqst].aop;

       AUTHORIZE(client, 0, aOP, accType ,faCtl.path, einfo);

       if (Finder && Finder->isRemote()
       &&  (rc = Finder->Locate(einfo, faCtl.path, luFlag, &FAttr_Env)))
          return fsError(einfo, rc);

       if (aOP == AOP_Update && xOpts & XRDEXP_NOTRW)
          return Emsg(epname, einfo, EROFS, accType, faCtl.path);
      }

// If this is a proxy server then hand this request to the storage system
// as it will need to be executed elsewhere.
//
   if (OssIsProxy)
      {faCtl.envP = &FAttr_Env;
       rc = XrdOfsOss->FSctl(XRDOSS_FSCTLFA, 0, (const char *)&faCtl);
       if (rc) return XrdOfsFS->Emsg(epname, einfo, rc, accType, faCtl.path);
       return SFS_OK;
      }

// Make sure we can use xattrs on the path
//
   if (xOpts & XRDEXP_NOXATTR)
      return XrdOfsFS->Emsg(epname, einfo, EPERM, accType, faCtl.path);

// Fan out for processing this on the local file system
//
   switch(faCtl.rqst)
         {case XrdSfsFACtl::faDel:
               return ctlFADel(faCtl, FAttr_Env, einfo);
               break;
          case XrdSfsFACtl::faGet:
               return ctlFAGet(faCtl, FAttr_Env, einfo);
               break;
          case XrdSfsFACtl::faLst:
               return ctlFALst(faCtl, FAttr_Env, einfo);
               break;
          case XrdSfsFACtl::faSet:
               return ctlFASet(faCtl, FAttr_Env, einfo);
               break;
          default: break;
         }

// The request code is not one we understand
//
   return XrdOfsFS->Emsg(epname, einfo, EINVAL, "process fattrs", faCtl.path);
}
  
/******************************************************************************/
/*                              c t l F A D e l                               */
/******************************************************************************/
  
int XrdOfs::ctlFADel(XrdSfsFACtl &faCtl, XrdOucEnv &faEnv, XrdOucErrInfo &einfo)
{

// Delete each variable
//
   for (unsigned int i = 0; i < faCtl.iNum; i++)
       faCtl.info[i].faRC = XrdSysFAttr::Xat->Del(faCtl.info[i].Name,faCtl.path);

// All done
//
   return SFS_OK;
}
  
/******************************************************************************/
/*                              c t l F A L s t                               */
/******************************************************************************/
  
int XrdOfs::ctlFALst(XrdSfsFACtl &faCtl, XrdOucEnv &faEnv, XrdOucErrInfo &einfo)
{
   EPNAME("ctlFALst");
   XrdSysXAttr::AList *alP, *aEnt;
   char *nP;
   int rc, pfLen, iX = 0, faSize = 0;
   int  getMsz = (faCtl.opts & XrdSfsFACtl::retvsz) != 0;
   bool xPlode = (faCtl.opts & XrdSfsFACtl::xplode) != 0;

// Get all of the attribute names
//
   rc = XrdSysFAttr::Xat->List(&alP, faCtl.path, -1, getMsz);
   if (rc < 0) return Emsg(epname, einfo, -rc, "list fattrs", faCtl.path);

// Count up attributes
//
   faCtl.info = 0;
   faCtl.iNum = 0;
   pfLen = (*faCtl.nPfx ? sizeof(faCtl.nPfx) : 0);
   aEnt = alP;
   while(aEnt)
        {if (aEnt->Nlen)
            {if (!pfLen || !strncmp(faCtl.nPfx, aEnt->Name, pfLen))
                {faCtl.iNum++;
                 faSize += aEnt->Nlen - pfLen + 1;
                } else aEnt->Nlen = 0;
            }
          aEnt = aEnt->Next;
        }

// If there are no attributes of interest, we are done.
//
   if (!faCtl.iNum) return SFS_OK;

// Allocate sufficient memory to hold the complete list
//
   if (!GetFABuff(faCtl, faSize))
      {XrdSysFAttr::Xat->Free(alP);
       return Emsg(epname, einfo, ENOMEM, "list fattrs", faCtl.path);
      }

// Allocate an info vector if caller wants this exploded
//
   if (xPlode) faCtl.info = new XrdSfsFAInfo[faCtl.iNum];
      else faCtl.info = 0;

// Copy over the names
//
   nP = faCtl.fabP->data;
   aEnt = alP;
   while(aEnt)
        {if (aEnt->Nlen)
            {strcpy(nP, aEnt->Name+pfLen);
             if (xPlode)
                {faCtl.info[iX].Name = nP;
                 faCtl.info[iX].NLen = aEnt->Nlen - pfLen;
                 faCtl.info[iX].VLen = aEnt->Vlen;
                 iX++;
                }
             nP += aEnt->Nlen-pfLen+1;
            }
         aEnt = aEnt->Next;
        }

      {faCtl.info = 0;
       faCtl.iNum = 0;
       return SFS_OK;
      }
   faCtl.fabP->dlen = nP - faCtl.fabP->data;

// Finish up
//
   XrdSysFAttr::Xat->Free(alP);
   return SFS_OK;
}

/******************************************************************************/
/*                              c t l F A G e t                               */
/******************************************************************************/

int XrdOfs::ctlFAGet(XrdSfsFACtl &faCtl, XrdOucEnv &faEnv, XrdOucErrInfo &einfo)
{
   char *bP;
   int   bL;

// Allocate the initial buffer. We make it big enough to, hopefully get all
// of the attributes, though we may have to reallocate.
//
   if (!GetFABuff(faCtl, faSize)) return SetNoMem(faCtl, 0);

// Setup to retrieve attributes
//
   bP = faCtl.fabP->data;
   bL = faCtl.fabP->dlen;

// Get each variable. Unfortunately, we need to allocate a buffer for each
// one as we don't know the size.
//
   for (unsigned int i = 0; i < faCtl.iNum; i++)
       {if (bL < 8)
           {if (!GetFABuff(faCtl, faSize)) return SetNoMem(faCtl, i);
            bP = faCtl.fabP->data;
            bL = faCtl.fabP->dlen;
           }

        if (!GetFAVal(faCtl, bP, bL, i) && !GulpFAVal(faCtl, bP, bL, i))
           return SetNoMem(faCtl, i);
       }

   return SFS_OK;
}
  
/******************************************************************************/
/*                              c t l F A S e t                               */
/******************************************************************************/

int XrdOfs::ctlFASet(XrdSfsFACtl &faCtl, XrdOucEnv &faEnv, XrdOucErrInfo &einfo)
{
   int isNew = (faCtl.opts & XrdSfsFACtl::newAtr) != 0;

// Lock this code if we are replacing variables
//
   if (!isNew) faMutex.Lock();

// Set each variable
//
   for (unsigned int i = 0; i < faCtl.iNum; i++)
       faCtl.info[i].faRC = XrdSysFAttr::Xat->Set(faCtl.info[i].Name,
                                                  faCtl.info[i].Value,
                                                  faCtl.info[i].VLen,
                                                  faCtl.path, -1, isNew);

// Unlock the mutex if we locked it
//
   if (!isNew) faMutex.UnLock();

// All done
   return SFS_OK;
}
