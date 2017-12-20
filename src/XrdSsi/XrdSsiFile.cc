/******************************************************************************/
/*                                                                            */
/*                         X r d S s i F i l e . c c                          */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucPList.hh"

#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSfs/XrdSfsXio.hh"

#include "XrdSsi/XrdSsiFile.hh"
#include "XrdSsi/XrdSsiFileSess.hh"
#include "XrdSsi/XrdSsiSfs.hh"
#include "XrdSsi/XrdSsiUtils.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdSsi
{
       XrdOucBuffPool    EmsgPool;
extern XrdSfsFileSystem *theFS;
extern XrdOucPListAnchor FSPath;
extern bool              fsChk;
};

using namespace XrdSsi;

/******************************************************************************/
/*                X r d S s i F i l e   C o n s t r u c t o r                 */
/******************************************************************************/

XrdSsiFile::XrdSsiFile(const char *user, int monid)
          : XrdSfsFile(user, monid), fsFile(0), fSessP(0), xioP(0) {}

/******************************************************************************/
/*                 X r d S s i F i l e   D e s t r u c t o r                  */
/******************************************************************************/
  
XrdSsiFile::~XrdSsiFile()
{

// If we have a file object then delete it -- it needs to close. Else do it.
//
   if (fsFile) delete fsFile;
   if (fSessP)        fSessP->Recycle();
}
  
/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/

int XrdSsiFile::close()
/*
  Function: Close the file object.

  Input:    None

  Output:   Always returns SFS_OK
*/
{

// Route this request as needed (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->close();
       return (rc ? CopyErr("close", rc) : rc);
      }

// Forward this to the file session object
//
   return fSessP->close();
}
  
/******************************************************************************/
/* Private:                      C o p y E C B                                */
/******************************************************************************/
  
void XrdSsiFile::CopyECB(bool forOpen)
{
   unsigned long long cbArg;
   XrdOucEICB        *cbVal = error.getErrCB(cbArg);

// We only need to copy some information
//
   if (forOpen) fsFile->error.setUCap(error.getUCap());
   fsFile->error.setErrCB(cbVal, cbArg);
}

/******************************************************************************/
/* Private:                      C o p y E r r                                */
/******************************************************************************/
  
int XrdSsiFile::CopyErr(const char *op, int rc)
{
   XrdOucBuffer *buffP;
   const char   *eText;
   int           eTLen, eCode;

// Get the error information
//
   eText = fsFile->error.getErrText(eCode);

// Handle callbacks
//
   if (rc == SFS_STARTED || rc == SFS_DATAVEC)
      {unsigned long long cbArg;
       XrdOucEICB        *cbVal = fsFile->error.getErrCB(cbArg);
       error.setErrCB(cbVal, cbArg);
       if (rc == SFS_DATAVEC)
          {struct iovec *iovP = (struct iovec *)eText;
           char *mBuff = error.getMsgBuff(eTLen);
           eTLen = iovP->iov_len;
           memcpy(mBuff, eText, eTLen);
           error.setErrCode(eCode);
           return SFS_DATAVEC;
          }
      }

// Check if we need to copy an external buffer. If this fails then if there is
// an ofs callback pending, we must tell the ofs plugin we failed.
//
   if (!(fsFile->error.extData())) error.setErrInfo(eCode, eText);
      else {eTLen = fsFile->error.getErrTextLen();
            buffP = EmsgPool.Alloc(eTLen);
            if (buffP)
               {memcpy(buffP->Buffer(), eText, eTLen);
                error.setErrInfo(eCode, buffP);
               } else {
                XrdSsiUtils::Emsg("CopyErr",ENOMEM,op,fsFile->FName(),error);
                if (rc == SFS_STARTED && fsFile->error.getErrCB())
                   {rc = eCode = SFS_ERROR;
                    fsFile->error.getErrCB()->Done(eCode, &error);
                   }
              }
           }

// All done
//
   return rc;
}
  
/******************************************************************************/
/*                                  f c t l                                   */
/******************************************************************************/

int      XrdSsiFile::fctl(const int         cmd,
                          const char       *args,
                          XrdOucErrInfo    &out_error)
{

// Route this request as needed
//
   if (fsFile) return fsFile->fctl(cmd, args, out_error);

// Indicate we would like to use SendData()
//
   if (cmd == SFS_FCTL_GETFD)
      {out_error.setErrCode(SFS_SFIO_FDVAL);
       return SFS_OK;
      }

// We don't support any other kind of command
//
   return XrdSsiUtils::Emsg("fctl",ENOTSUP,"fctl",fSessP->FName(),out_error);
}

/******************************************************************************/

int      XrdSsiFile::fctl(const int           cmd,
                                int           alen,
                          const char         *args,
                          const XrdSecEntity *client)
{

// Route this request as needed (callback possible)
//
   if (fsFile)
      {CopyECB();
       int rc = fsFile->fctl(cmd, alen, args, client);
       return (rc ? CopyErr("fctl", rc) : rc);
      }

// Forward this to the session object
//
   return fSessP->fctl(cmd, alen, args, client);
}

/******************************************************************************/
/*                                 F N a m e                                  */
/******************************************************************************/
  
const char *XrdSsiFile::FName()
{

// Route to filesystem if need be
//
   if (fsFile) return fsFile->FName();

// Return our name
//
   return fSessP->FName();
}

/******************************************************************************/
/*                             g e t C X i n f o                              */
/******************************************************************************/
  
int XrdSsiFile::getCXinfo(char cxtype[4], int &cxrsz)
/*
  Function: Set the length of the file object to 'flen' bytes.

  Input:    n/a

  Output:   cxtype - Compression algorithm code
            cxrsz  - Compression region size

            Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
// Route this request as needed (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->getCXinfo(cxtype, cxrsz);
       return (rc ? CopyErr("getcx", rc) : rc);
      }

// Indicate we don't support compression
//
   cxrsz = 0;
   return SFS_OK;
}

/******************************************************************************/
/*                               g e t M m a p                                */
/******************************************************************************/

int XrdSsiFile::getMmap(void **Addr, off_t &Size)         // Out
/*
  Function: Return memory mapping for file, if any.

  Output:   Addr        - Address of memory location
            Size        - Size of the file or zero if not memory mapped.
            Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
// Route this request as needed (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->getMmap(Addr, Size);
       return (rc ? CopyErr("getmmap", rc) : rc);
      }

// Indicate we don't support memory mapping
//
   if (Addr) *Addr = 0;
   Size = 0;
   return SFS_OK;
}
  
/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/

int XrdSsiFile::open(const char          *path,      // In
                     XrdSfsFileOpenMode   open_mode, // In
                     mode_t               Mode,      // In
               const XrdSecEntity        *client,    // In
               const char                *info)      // In
/*
  Function: Open the file `path' in the mode indicated by `open_mode'.  

  Input:    path      - The fully qualified name of the resource.
            open_mode - It must contain only SFS_O_RDWR.
            Mode      - Ignored.
            client    - Authentication credentials, if any.
            info      - Opaque information to be used as seen fit.

  Output:   Returns OOSS_OK upon success, otherwise SFS_ERROR is returned.
*/
{
   static const char *epname = "open";
   int eNum;

// Verify that this object is not already associated with an open file
//
   if (fsFile || fSessP)
      return XrdSsiUtils::Emsg(epname, EADDRINUSE, "open session", path, error);

// Open a regular file if this is wanted
//
   if (fsChk && FSPath.Find(path))
      {if (!(fsFile = theFS->newFile((char *)error.getErrUser(),
                                             error.getErrMid())))
          return XrdSsiUtils::Emsg(epname, ENOMEM, "open file", path, error);
       CopyECB(true);
       if ((eNum = fsFile->open(path, open_mode, Mode, client, info)))
          {eNum = CopyErr(epname, eNum);
           delete fsFile; fsFile = 0;
          }
       return eNum;
      }

// Convert opaque and security into an environment
//
   XrdOucEnv Open_Env(info, 0, client);

// Allocate file session and issue open
//
    fSessP = XrdSsiFileSess::Alloc(error, error.getErrUser());
    eNum   = fSessP->open(path, Open_Env, open_mode);
    if (eNum) {fSessP->Recycle(); fSessP = 0;}
    return eNum;
}

/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

int            XrdSsiFile::read(XrdSfsFileOffset  offset,    // In
                                XrdSfsXferSize    blen)      // In
/*
  Function: Preread `blen' bytes at `offset'

  Input:    offset    - The absolute byte offset at which to start the read.
            blen      - The amount to preread.

  Output:   Returns SFS_OK upon success and SFS_ERROR o/w.
*/
{

// Route to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->read(offset, blen);
       return (rc ? CopyErr("read", rc) : rc);
      }

// We ignore these
//
   return SFS_OK;
}
  
/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

XrdSfsXferSize XrdSsiFile::read(XrdSfsFileOffset  offset,    // In
                                char             *buff,      // Out
                                XrdSfsXferSize    blen)      // In
/*
  Function: Read `blen' bytes at `offset' into 'buff' and return the actual
            number of bytes read.

  Input:    offset    - Contains request information.
            buff      - Address of the buffer in which to place the data.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be returned.

  Output:   Returns the number of bytes read upon success and SFS_ERROR o/w.
*/
{

// Route to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->read(offset, buff, blen);
       return (rc ? CopyErr("read", rc) : rc);
      }

// Forward this to the file session
//
   return fSessP->read(offset, buff, blen);
}

/******************************************************************************/
/*                              r e a d   A I O                               */
/******************************************************************************/
  
int XrdSsiFile::read(XrdSfsAio *aiop)
{

// Route to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->read(aiop);
       return (rc ? CopyErr("readaio", rc) : rc);
      }

// Execute this request in a synchronous fashion
//
   aiop->Result = fSessP->read((XrdSfsFileOffset)aiop->sfsAio.aio_offset,
                                         (char *)aiop->sfsAio.aio_buf,
                                 (XrdSfsXferSize)aiop->sfsAio.aio_nbytes);
   aiop->doneRead();
   return 0;
}

/******************************************************************************/
/*                                  r e a d v                                 */
/******************************************************************************/

XrdSfsXferSize XrdSsiFile::readv(XrdOucIOVec     *readV,     // In
                                       int        readCount) // In
/*
  Function: Perform all the reads specified in the readV vector.

  Input:    readV     - A description of the reads to perform; includes the
                        absolute offset, the size of the read, and the buffer
                        to place the data into.
            readCount - The size of the readV vector.

  Output:   Returns an error as this is not supported.
*/
{

// Route this request to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->readv(readV, readCount);
       return (rc ? CopyErr("readv", rc) : rc);
      }

   return XrdSsiUtils::Emsg("readv", ENOSYS, "readv", fSessP->FName(), error);
}
  
/******************************************************************************/
/*                              S e n d D a t a                               */
/******************************************************************************/
  
int XrdSsiFile::SendData(XrdSfsDio         *sfDio,
                         XrdSfsFileOffset   offset,
                         XrdSfsXferSize     size)
{

// Route this request to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->SendData(sfDio, offset, size);
       return (rc ? CopyErr("SendData", rc) : rc);
      }

// Forward this to the file session object
//
   return fSessP->SendData(sfDio, offset, size);
}

/******************************************************************************/
/*                                  s t a t                                   */
/******************************************************************************/

int XrdSsiFile::stat(struct stat     *buf)         // Out
/*
  Function: Return file status information

  Input:    buf         - The stat structure to hold the results

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{

// Route this request to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->stat(buf);
       return (rc ? CopyErr("stat", rc) : rc);
      }

// Otherwise there is no stat information
//
   memset(buf, 0 , sizeof(struct stat));
   return SFS_OK;
}

/******************************************************************************/
/*                                  s y n c                                   */
/******************************************************************************/

int XrdSsiFile::sync()
/*
  Function: Commit all unwritten bytes to physical media.

  Input:    None

  Output:   Returns SFS_OK if a response is ready or SFS_STARTED otherwise.
*/
{

// Route this request to file system if need be (callback possible)
//
   if (fsFile)
      {CopyECB();
       int rc = fsFile->sync();
       return (rc ? CopyErr("sync", rc) : rc);
      }

// We don't support this
//
   return XrdSsiUtils::Emsg("sync", ENOSYS, "sync", fSessP->FName(), error);
}

/******************************************************************************/
/*                              s y n c   A I O                               */
/******************************************************************************/
  
int XrdSsiFile::sync(XrdSfsAio *aiop)
{

// Route this request to file system if need be (callback possible)
//
   if (fsFile)
      {CopyECB();
       int rc = fsFile->sync(aiop);
       return (rc ? CopyErr("syncaio", rc) : rc);
      }

// We don't support this
//
   return XrdSsiUtils::Emsg("syncaio", ENOSYS, "sync", fSessP->FName(), error);
}

/******************************************************************************/
/*                              t r u n c a t e                               */
/******************************************************************************/

int XrdSsiFile::truncate(XrdSfsFileOffset  flen)  // In
/*
  Function: Set the length of the file object to 'flen' bytes.

  Input:    flen      - The new size of the file.

  Output:   Returns SFS_ERROR a this function is not supported.
*/
{

// Route this request to file system if need be (callback possible)
//
   if (fsFile)
      {CopyECB();
       int rc = fsFile->truncate(flen);
       return (rc ? CopyErr("trunc", rc) : rc);
      }

// Route this to the file session object
//
   return fSessP->truncate(flen);
}

/******************************************************************************/
/*                                 w r i t e                                  */
/******************************************************************************/

XrdSfsXferSize XrdSsiFile::write(XrdSfsFileOffset      offset,    // In
                                       const char     *buff,      // In
                                       XrdSfsXferSize  blen)      // In
/*
  Function: Write `blen' bytes at `offset' from 'buff' and return the actual
            number of bytes written.

  Input:    offset    - The absolute byte offset at which to start the write.
            buff      - Address of the buffer from which to get the data.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be written to 'fd'.

  Output:   Returns the number of bytes written upon success and SFS_ERROR o/w.

  Notes:    An error return may be delayed until the next write(), close(), or
            sync() call.
*/
{

// Route this request to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->write(offset, buff, blen);
       return (rc ? CopyErr("write", rc) : rc);
      }

// Route this to the file session object
//
   return fSessP->write(offset, buff, blen);
}

/******************************************************************************/
/*                             w r i t e   A I O                              */
/******************************************************************************/
  
int XrdSsiFile::write(XrdSfsAio *aiop)
{

// Route to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->write(aiop);
       return (rc ? CopyErr("writeaio", rc) : rc);
      }

// Execute this request in a synchronous fashion
//
   aiop->Result = fSessP->write((XrdSfsFileOffset)aiop->sfsAio.aio_offset,
                                          (char *)aiop->sfsAio.aio_buf,
                                  (XrdSfsXferSize)aiop->sfsAio.aio_nbytes);
   aiop->doneWrite();
   return 0;
}
