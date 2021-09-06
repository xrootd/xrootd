/******************************************************************************/
/*                                                                            */
/*                        X r d O f s F A t t r . c c                         */
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

#include "XrdOfs/XrdOfs.hh"
#include "XrdOfs/XrdOfsTrace.hh"
#include "XrdOfs/XrdOfsSecurity.hh"

#include "XrdOss/XrdOss.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucExport.hh"

#include "XrdSec/XrdSecEntity.hh"

#include "XrdSfs/XrdSfsFAttr.hh"
#include "XrdSfs/XrdSfsFlags.hh"
#include "XrdSfs/XrdSfsInterface.hh"

#include "XrdSys/XrdSysFAttr.hh"

/******************************************************************************/
/*                  E r r o r   R o u t i n g   O b j e c t                   */
/******************************************************************************/

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
   rc = XrdSysFAttr::Xat->Get(faCtl.info[k].Name, bP, bL, faCtl.pfnP);

// If all went well, record the value and update incoming information
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
/*                                 F A t t r                                  */
/******************************************************************************/
  
int XrdOfs::FAttr(XrdSfsFACtl        *faReq,
                  XrdOucErrInfo      &einfo,
                  const XrdSecEntity *client)
{
   EPNAME("FAttr");
   const char *tident = einfo.getErrUser();
   char pfnbuff[MAXPATHLEN+8];
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

// Check if we need only return support information
//
   if (!faReq)
      {XrdOucEnv *envP = einfo.getEnv();
       ZTRACE(fsctl, "FAttr req=info");
       if (!envP || !usxMaxNsz)
          {einfo.setErrInfo(ENOTSUP, "Not supported.");
           return SFS_ERROR;
          }
       envP->PutInt("usxMaxNsz", usxMaxNsz);
       envP->PutInt("usxMaxVsz", usxMaxVsz);
       return SFS_OK;
      }

// Setup for to perform attribute functions
//
   XrdSfsFACtl &faCtl = *faReq;
   XrdOucEnv FAttr_Env(faCtl.pcgi,0,client);

// Make sure request code is valid (we also set some options)
//
   if (faCtl.rqst > faNum)
      return Emsg(epname, einfo, EINVAL, "process fattrs", faCtl.path);
   accType  = faTab[faCtl.rqst].name;

// Trace this call
//
   ZTRACE(fsctl, "FAttr " <<accType <<' ' <<faCtl.path);

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

// Convert the lfn to a pfn for actual calls to the attribute processor
//
   faCtl.pfnP = XrdOfsOss->Lfn2Pfn(faCtl.path, pfnbuff, sizeof(pfnbuff), rc);
   if (!faCtl.pfnP) return XrdOfsFS->Emsg(epname,einfo,rc,accType,faCtl.path);

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
       faCtl.info[i].faRC = XrdSysFAttr::Xat->Del(faCtl.info[i].Name,faCtl.pfnP);

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
   char *nP, *bP;
   int bL, rc, pfLen, iX = 0, faSize = 0, fvSize = 0;
   bool getMsz = (faCtl.opts & XrdSfsFACtl::retvsz) == XrdSfsFACtl::retvsz;
   bool getVal = (faCtl.opts & XrdSfsFACtl::retval) == XrdSfsFACtl::retval;
   bool xPlode = (faCtl.opts & XrdSfsFACtl::xplode) != 0;

// Get all of the attribute names
//
   rc = XrdSysFAttr::Xat->List(&alP, faCtl.pfnP, -1, getMsz);
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
                 if (getVal && aEnt->Vlen) faCtl.info[iX].Value = aEnt->Name;
                 fvSize += aEnt->Vlen;
                 iX++;
                }
             nP += aEnt->Nlen-pfLen+1;
            }
         aEnt = aEnt->Next;
        }

// If we don't need to return values, we are done
//
   if (!getVal)
      {XrdSysFAttr::Xat->Free(alP);
       return SFS_OK;
      }

// Allocate a buffer to hold all of the values
//
   if (!GetFABuff(faCtl, fvSize))
      {XrdSysFAttr::Xat->Free(alP);
       return SetNoMem(faCtl, 0);
      }

// Setup to retrieve attributes
//
   bP = faCtl.fabP->data;
   bL = faCtl.fabP->dlen;

// Retrieve the attribute values
//
   for (unsigned int i = 0; i < faCtl.iNum; i++)
       {if (faCtl.info[i].VLen)
           {nP = faCtl.info[i].Name;
            faCtl.info[i].Name  = faCtl.info[i].Value;
            faCtl.info[i].Value = 0;
            if (!GetFAVal(faCtl, bP, bL, i) && !GulpFAVal(faCtl, bP, bL, i))
               {XrdSysFAttr::Xat->Free(alP);
                return SetNoMem(faCtl, i);
               }
            faCtl.info[i].Name = nP;
           }
       }

// Free up the buffer list and return success
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
                                                  faCtl.pfnP, -1, isNew);

// Unlock the mutex if we locked it
//
   if (!isNew) faMutex.UnLock();

// All done
   return SFS_OK;
}
