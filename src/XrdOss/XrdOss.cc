/******************************************************************************/
/*                                                                            */
/*                             X r d O s s . c c                              */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdSfs/XrdSfsAio.hh"

/******************************************************************************/
/*                          C l a s s   X r d O s s                           */
/******************************************************************************/
/******************************************************************************/
/*                               C o n n e c t                                */
/******************************************************************************/

void XrdOss::Connect(XrdOucEnv &env) {(void)env;}

/******************************************************************************/
/*                                  D i s c                                   */
/******************************************************************************/

void XrdOss::Disc(XrdOucEnv &env) {(void)env;}
  
/******************************************************************************/
/*                               E n v I n f o                                */
/******************************************************************************/
  
void XrdOss::EnvInfo(XrdOucEnv *envP) {(void)envP;}

/******************************************************************************/
/*                              F e a t u r e s                               */
/******************************************************************************/

uint64_t XrdOss::Features() {return 0;}

/******************************************************************************/
/*                                 F S c t l                                  */
/******************************************************************************/
  
int XrdOss::FSctl(int cmd, int alen, const char *args, char **resp)
{
   (void)cmd; (void)alen; (void)args; (void)resp;
   return -ENOTSUP;
}

/******************************************************************************/
/*                                 R e l o c                                  */
/******************************************************************************/

int XrdOss::Reloc(const char *tident, const char *path,
                  const char *cgName, const char *anchor)
{
   (void)tident; (void)path; (void)cgName; (void)anchor;
   return -ENOTSUP;
}
  
/******************************************************************************/
/*                                S t a t F S                                 */
/******************************************************************************/
  
int XrdOss::StatFS(const char *path, char *buff, int &blen, XrdOucEnv *eP)
{ 
   (void)path; (void)buff; (void)blen; (void)eP;
   return -ENOTSUP;
}

/******************************************************************************/
/*                                S t a t L S                                 */
/******************************************************************************/
  
int XrdOss::StatLS(XrdOucEnv &env, const char *cgrp, char *buff, int &blen)
{ 
   (void)env; (void)cgrp; (void)buff; (void)blen;
   return -ENOTSUP;
}

/******************************************************************************/
/*                                S t a t P F                                 */
/******************************************************************************/
  
int XrdOss::StatPF(const char *path, struct stat *buff, int opts)
{
   (void)path; (void)buff;
   return -ENOTSUP;
}

/******************************************************************************/
/*                                S t a t V S                                 */
/******************************************************************************/
  
int XrdOss::StatVS(XrdOssVSInfo *sP, const char *sname, int updt)
{
   (void)sP; (void)sname; (void)updt;
   return -ENOTSUP;
}

/******************************************************************************/
/*                                S t a t X A                                 */
/******************************************************************************/
  
int XrdOss::StatXA(const char *path, char *buff, int &blen, XrdOucEnv *eP)
{
   (void)path; (void)buff; (void)blen; (void)eP;
   return -ENOTSUP;
}

/******************************************************************************/
/*                                S t a t X P                                 */
/******************************************************************************/
  
int XrdOss::StatXP(const char *path, unsigned long long &attr, XrdOucEnv *eP)
{
   (void)path; (void)attr; (void)eP;
   return -ENOTSUP;
}

/******************************************************************************/
/*                        C l a s s   X r d O s s D F                         */
/******************************************************************************/
/******************************************************************************/
/*                                  F c t l                                   */
/******************************************************************************/
  
int XrdOssDF::Fctl(int cmd, int alen, const char *args, char **resp)
{
   (void)cmd; (void)alen; (void)args; (void)resp;
   return -ENOTSUP;
}

/******************************************************************************/
/*                                p g R e a d                                 */
/******************************************************************************/

ssize_t XrdOssDF::pgRead(void     *buffer,
                         off_t     offset,
                         size_t    rdlen,
                         uint32_t *csvec,
                         uint64_t  opts)
{
   ssize_t bytes;

// Read the data into the buffer
//
   bytes = Read(buffer, offset, rdlen);

// Calculate checksums if so wanted
//
   if (bytes > 0 && csvec) 
      XrdOucPgrwUtils::csCalc((const char *)buffer, offset, bytes, csvec);

// All done
//
   return bytes;
}

/******************************************************************************/
  
int XrdOssDF::pgRead(XrdSfsAio *aioparm, uint64_t opts)
{
   aioparm->Result = this->pgRead((void *)aioparm->sfsAio.aio_buf,
                                  (off_t) aioparm->sfsAio.aio_offset,
                                  (size_t)aioparm->sfsAio.aio_nbytes,
                                          aioparm->cksVec, opts);
   aioparm->doneRead();
   return 0;
}

/******************************************************************************/
/*                               p g W r i t e                                */
/******************************************************************************/

ssize_t XrdOssDF::pgWrite(void     *buffer,
                          off_t     offset,
                          size_t    wrlen,
                          uint32_t *csvec,
                          uint64_t  opts)
{

// If we have a checksum vector and verify is on, make sure the data
// in the buffer corresponds to he checksums.
//
   if (csvec && (opts & Verify))
      {XrdOucPgrwUtils::dataInfo dInfo((const char *)buffer,csvec,offset,wrlen);
       off_t bado;
       int   badc;
       if (!XrdOucPgrwUtils::csVer(dInfo, bado, badc)) return -EDOM;
      }

// Now just return the result of a plain write
//
   return Write(buffer, offset, wrlen);
}

/******************************************************************************/
  
int XrdOssDF::pgWrite(XrdSfsAio *aioparm, uint64_t opts)
{
   aioparm->Result = this->pgWrite((void *)aioparm->sfsAio.aio_buf,
                                   (off_t) aioparm->sfsAio.aio_offset,
                                   (size_t)aioparm->sfsAio.aio_nbytes,
                                           aioparm->cksVec, opts);
   aioparm->doneWrite();
   return 0;
}

/******************************************************************************/
/*                                 R e a d V                                  */
/******************************************************************************/
  
ssize_t XrdOssDF::ReadV(XrdOucIOVec *readV,
                        int          n)
{
   ssize_t nbytes = 0, curCount = 0;
   for (int i=0; i<n; i++)
       {curCount = Read((void *)readV[i].data,
                         (off_t)readV[i].offset,
                        (size_t)readV[i].size);
        if (curCount != readV[i].size)
           {if (curCount < 0) return curCount;
            return -ESPIPE;
           }
        nbytes += curCount;
       }
   return nbytes;
}

/******************************************************************************/
/*                                W r i t e V                                 */
/******************************************************************************/
  
ssize_t XrdOssDF::WriteV(XrdOucIOVec *writeV,
                         int          n)
{
   ssize_t nbytes = 0, curCount = 0;

   for (int i=0; i<n; i++)
       {curCount =Write((void *)writeV[i].data,
                         (off_t)writeV[i].offset,
                        (size_t)writeV[i].size);
        if (curCount != writeV[i].size)
           {if (curCount < 0) return curCount;
            return -ESPIPE;
           }
        nbytes += curCount;
       }
   return nbytes;
}
