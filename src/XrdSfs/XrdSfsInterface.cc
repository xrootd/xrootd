/******************************************************************************/
/*                                                                            */
/*                    X r d S f s I n t e r f a c e . h h                     */
/*                                                                            */
/* (c) 2019 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSfs/XrdSfsInterface.hh"

/******************************************************************************/
/*                                p g R e a d                                 */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgRead(XrdSfsFileOffset   offset,
                                  char              *buffer,
                                  XrdSfsXferSize     rdlen,
                                  uint32_t          *csvec,
                                  bool               verify)
{(void)offset; (void)buffer; (void)rdlen;
 (void)csvec;  (void)verify;
 error.setErrInfo(ENOTSUP, "Not supported.");
 return SFS_ERROR;
}

/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgRead(XrdSfsAio *aioparm, bool verify)
{
   aioparm->Result = this->pgRead((XrdSfsFileOffset)aioparm->sfsAio.aio_offset,
                                            (char *)aioparm->sfsAio.aio_buf,
                                    (XrdSfsXferSize)aioparm->sfsAio.aio_nbytes,
                                                    aioparm->cksVec, verify);
   aioparm->doneRead();
   return SFS_OK;
}

/******************************************************************************/
/*                               p g W r i t e                                */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgWrite(XrdSfsFileOffset   offset,
                                   char              *buffer,
                                   XrdSfsXferSize     wrlen,
                                   uint32_t          *csvec,
                                   bool               verify)
{(void)offset; (void)buffer; (void)wrlen;
 (void)csvec;  (void)verify;
 error.setErrInfo(ENOTSUP, "Not supported.");
 return SFS_ERROR;
}

/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::pgWrite(XrdSfsAio *aioparm, bool verify)
{
   aioparm->Result = this->pgWrite((XrdSfsFileOffset)aioparm->sfsAio.aio_offset,
                                             (char *)aioparm->sfsAio.aio_buf,
                                     (XrdSfsXferSize)aioparm->sfsAio.aio_nbytes,
                                                     aioparm->cksVec, verify);
   aioparm->doneWrite();
   return SFS_OK;
}

/******************************************************************************/
/*                                 r e a d v                                  */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::readv(XrdOucIOVec      *readV,
                                 int               rdvCnt)
{
   XrdSfsXferSize rdsz, totbytes = 0;

   for (int i = 0; i < rdvCnt; i++)
       {rdsz = read(readV[i].offset,
                    readV[i].data, readV[i].size);
        if (rdsz != readV[i].size)
           {if (rdsz < 0) return rdsz;
            error.setErrInfo(ESPIPE,"read past eof");
            return SFS_ERROR;
           }
        totbytes += rdsz;
       }
   return totbytes;
}

/******************************************************************************/
/*                                w r i t e v                                 */
/******************************************************************************/
  
XrdSfsXferSize XrdSfsFile::writev(XrdOucIOVec      *writeV,
                                  int               wdvCnt)
{
    XrdSfsXferSize wrsz, totbytes = 0;

    for (int i = 0; i < wdvCnt; i++)
        {wrsz = write(writeV[i].offset,
                      writeV[i].data, writeV[i].size);
         if (wrsz != writeV[i].size)
            {if (wrsz < 0) return wrsz;
            error.setErrInfo(ESPIPE,"write past eof");
            return SFS_ERROR;
           }
        totbytes += wrsz;
       }
   return totbytes;
}
