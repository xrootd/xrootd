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
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include "XrdNet/XrdNetAddrInfo.hh"

#include "XrdOuc/XrdOucBuffer.hh"
#include "XrdOuc/XrdOucERoute.hh"
#include "XrdOuc/XrdOucPList.hh"

#include "XrdSec/XrdSecEntity.hh"

#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSfs/XrdSfsXio.hh"

#include "XrdSsi/XrdSsiEntity.hh"
#include "XrdSsi/XrdSsiFile.hh"
#include "XrdSsi/XrdSsiService.hh"
#include "XrdSsi/XrdSsiSfs.hh"
#include "XrdSsi/XrdSsiStream.hh"
#include "XrdSsi/XrdSsiTrace.hh"
#include "XrdSsi/XrdSsiUtils.hh"

#include "XrdSys/XrdSysError.hh"
  
/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

namespace XrdSsi
{
class FileResource : public XrdSsiService::Resource
{
public:

void           ProvisionDone(XrdSsiSession *sessP) {mySess=sessP; mySem.Post();}

void           ProvisionWait() {mySem.Wait();}

XrdSsiSession *Session() {return mySess;}

      FileResource(const char *path, const XrdSecEntity *entP, int atype)
                  : XrdSsiService::Resource(path), mySem(0), mySess(0)
                  {if (atype && entP)
                      {strncpy(mySec.prot, entP->prot, XrdSsiPROTOIDSIZE);
                       mySec.name = entP->name;
                       mySec.host = (atype <= 1 ? entP->host
                                    : entP->addrInfo->Name(entP->host));
                       mySec.role = entP->vorg;
                       mySec.role = entP->role;
                       mySec.grps = entP->grps;
                       mySec.endorsements = entP->endorsements;
                       mySec.creds = entP->creds;
                       mySec.credslen = entP->credslen;
                       mySec.tident = entP->tident;
                      }
                  }
     ~FileResource() {}

private:
XrdSysSemaphore mySem;
XrdSsiSession  *mySess;
XrdSsiEntity    mySec;
};
}
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdSsi
{
       XrdOucBuffPool    EmsgPool;
extern XrdOucBuffPool   *BuffPool;
extern XrdSsiService    *Service;
extern XrdSfsFileSystem *theFS;
extern XrdOucPListAnchor FSPath;
extern bool              fsChk;
extern XrdSysError       Log;
extern XrdOucTrace       Trace;
extern int               respWT;
};

using namespace XrdSsi;

namespace
{
class nullCallBack : public XrdOucEICB
{
public:

void     Done(int &Result, XrdOucErrInfo *eInfo, const char *Path=0) {}

int      Same(unsigned long long arg1, unsigned long long arg2) {return 0;}

         nullCallBack() {}
virtual ~nullCallBack() {}
};

nullCallBack nullCB;
};
  
/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

int           XrdSsiFile::authXQ = 0;

int           XrdSsiFile::maxRSZ = 0;

/******************************************************************************/
/*                X r d S s i F i l e   C o n s t r u c t o r                 */
/******************************************************************************/

XrdSsiFile::XrdSsiFile(const char *user, int monid) : XrdSfsFile(user, monid)
{
   tident     = (user ? user : "");
   gigID      = 0;
   fsFile     = 0;
   fsUser     = 0;
   xioP       = 0;
   oucBuff    = 0;
   reqSize    = 0;
   reqLeft    = 0;
   sessP      = 0;
   isOpen     = false;
   inProg     = false;
   viaDel     = false;
}

/******************************************************************************/
/*                 X r d S s i F i l e   D e s t r u c t o r                  */
/******************************************************************************/
  
XrdSsiFile::~XrdSsiFile()
{

// If we have a file object then delete it -- it needs to close. Else do it.
//
   if (fsFile) delete fsFile;
      else {viaDel = true; close();}

// Release other buffers
//
   if (fsUser) free(fsUser);
}
/******************************************************************************/
/*                              A t t n I n f o                               */
/******************************************************************************/
  
bool XrdSsiFile::AttnInfo(XrdOucErrInfo &eInfo, const XrdSsiRespInfo *respP,
                          int reqID) // Called with the request mutex locked!
{
   struct AttnResp {struct iovec ioV[4]; XrdSsiRRInfoAttn aHdr;};

   AttnResp *attnResp;
   char *mBuff;
   int n, ioN = 2;
   bool doFin;

// If there is no data we can send back to the client in the attn response,
// then simply reply with a short message to make the client come back.
//
   if (!respP->mdlen)
      {if (respP->rType != XrdSsiRespInfo::isData
       ||  respP->blen > XrdSsiResponder::MaxDirectXfr)
          {eInfo.setErrInfo(0, "");
           return false;
          }
      }

// We will be constructing the response in the message buffer. This is
// gauranteed to be big enough for our purposes so no need to check the size.
//
   mBuff = eInfo.getMsgBuff(n);

// Initialize the response
//
   attnResp = (AttnResp *)mBuff;
   memset(attnResp, 0, sizeof(AttnResp));
   attnResp->aHdr.pfxLen = htons(sizeof(XrdSsiRRInfoAttn));

// Fill out iovec to point to our header
//
   attnResp->ioV[0].iov_len  = sizeof(XrdSsiRRInfoAttn) + respP->mdlen;
   attnResp->ioV[1].iov_base = mBuff+offsetof(struct AttnResp, aHdr);
   attnResp->ioV[1].iov_len  = sizeof(XrdSsiRRInfoAttn);

// Fill out the iovec for the metadata if we have some
//
   if (respP->mdlen)
      {attnResp->ioV[2].iov_base = (void *)respP->mdata;
       attnResp->ioV[2].iov_len  =         respP->mdlen; ioN = 3;
       attnResp->aHdr.mdLen = htonl(respP->mdlen);
      }

// Check if we have actual data here as well and can send it along
//
   if (respP->rType == XrdSsiRespInfo::isData
   &&  respP->blen+respP->mdlen <= XrdSsiResponder::MaxDirectXfr)
      {if (respP->blen)
          {attnResp->ioV[ioN].iov_base = (void *)respP->buff;
           attnResp->ioV[ioN].iov_len  =         respP->blen; ioN++;
          }
         attnResp->aHdr.tag = XrdSsiRRInfoAttn::fullResp; doFin = true;
      }
   else {attnResp->aHdr.tag = XrdSsiRRInfoAttn::pendResp; doFin = false;}

// If we sent the full response we must remove the request from the request
// table as it will get finished off when the response is actually sent.
//
   if (doFin) rTab.Del(reqID, false);

// Setup to have metadata actually sent to the requestor
//
   eInfo.setErrCode(ioN);
   return doFin;
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
   const char *epname = "close";

// Route this request as needed (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->close();
       return (rc ? CopyErr(epname, rc) : rc);
      }

// Do some debugging
//
   DEBUG((gigID ? gigID : "???") <<" del=" <<viaDel);

// Run through all outstanding requests and comlete them
//
   rTab.Reset();

// Stop the session
//
   if (sessP) {sessP->Unprovision(viaDel); sessP = 0;}

// Free any in-progress buffers
//
   if (inProg)
      {if (oucBuff) {oucBuff->Recycle(); oucBuff = 0;}
       inProg = false;
      }

// Clean up storage
//
   if (viaDel && gigID) {free(gigID); gigID = 0;}
   isOpen = false;
   return SFS_OK;
}
  
/******************************************************************************/
/* Private:                      C o p y E C B                                */
/******************************************************************************/
  
void XrdSsiFile::CopyECB()
{
   unsigned long long cbArg;
   XrdOucEICB        *cbVal = error.getErrCB(cbArg);

// We only need to copy some information
//
   if (!isOpen) fsFile->error.setUCap(error.getUCap());
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
   return XrdSsiUtils::Emsg("fctl", ENOTSUP, "fctl", gigID, out_error);
}

/******************************************************************************/

int      XrdSsiFile::fctl(const int           cmd,
                                int           alen,
                          const char         *args,
                          const XrdSecEntity *client)
{
   static const char *epname = "fctl";
   XrdSsiRRInfo      *rInfo;
   XrdSsiFileReq     *rqstP;
   int reqID;

// Route this request as needed (callback possible)
//
   if (fsFile)
      {CopyECB();
       int rc = fsFile->fctl(cmd, alen, args, client);
       return (rc ? CopyErr(epname, rc) : rc);
      }

// If this isn't the special query, then return an error
//
   if (cmd != SFS_FCTL_SPEC1)
      return XrdSsiUtils::Emsg(epname, ENOTSUP, "fctl", gigID, error);

// Caller wishes to find out if a request is ready and wait if it is not
//
   if (!args || alen < (int)sizeof(XrdSsiRRInfo))
      return XrdSsiUtils::Emsg(epname, EINVAL, "fctl", gigID, error);

// Grab the request identifier
//
   rInfo = (XrdSsiRRInfo *)args;
   reqID = rInfo->Id();

// Do some debugging
//
   DEBUG(reqID <<':' <<gigID <<" query resp status");

// Find the request
//
   if (!(rqstP = rTab.LookUp(reqID)))
      return XrdSsiUtils::Emsg(epname, ESRCH, "fctl", gigID, error);

// Check if a response is waiting for the caller
//
   if (rqstP->WantResponse(error))
      {DEBUG(reqID <<':' <<gigID <<" resp ready");
       return SFS_DATAVEC;
      }

// Put this client into callback state
//
   DEBUG(reqID <<':' <<gigID <<" resp not ready");
   error.setErrCB((XrdOucEICB *)rqstP);
   error.setErrInfo(respWT, "");
   return SFS_STARTED;
}

/******************************************************************************/
/*                                 F N a m e                                  */
/******************************************************************************/
  
const char *XrdSsiFile::FName()
{

// Route to filesystem if need be
//
   if (fsFile) return fsFile->FName();

// Return out name
//
   return gigID;
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
/* Private:                      G e t U s e r                                */
/******************************************************************************/
  
char *XrdSsiFile::GetUser(const char *incgi)
{
   const char *usr, *amp;
   char *ubuff;
   int n;

// Find the user identification element
//
   if (!incgi || !(usr = strstr(incgi, "ssi.user="))) return 0;
   usr += 9;

// Extract out user
//
   n = ((amp = index(usr, '&')) ? amp-usr : strlen(usr));

// Allocate a buffer for the user name if we have anything at all
//
   if (!n) return 0;
   ubuff = (char *)malloc(n+1);

// Copy username into buffer and return it
//
   strncpy(ubuff, usr, n); *(ubuff+n) = 0;
   return ubuff;
}

/******************************************************************************/
/* Private:                   N e w R e q u e s t                             */
/******************************************************************************/
  
bool XrdSsiFile::NewRequest(int              reqid,
                            XrdOucBuffer    *oP,
                            XrdSfsXioHandle *bR,
                            int              rSz)
{
   XrdSsiFileReq *reqP;

// Allocate a new request object
//
   if ((reqid > XrdSsiRRInfo::maxID)
   || !(reqP = XrdSsiFileReq::Alloc(&error, this, sessP, gigID, tident, reqid)))
      return false;

// Add it to the table
//
   rTab.Add(reqP, reqid);

// Activate the request
//
   inProg = false;
   reqP->Activate(oP, bR, rSz);
   return true;
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
   FileResource fileResource(path, client, authXQ);
   const char *eText;
   int eNum;

// Verify that this object is not already associated with an open file
//
   if (isOpen)
      return XrdSsiUtils::Emsg(epname, EADDRINUSE, "open session", path, error);

// Open a regular file if this is wanted
//
   if (fsChk && FSPath.Find(path))
      {if (!(fsFile = theFS->newFile((char *)tident, error.getErrMid())))
          return XrdSsiUtils::Emsg(epname, ENOMEM, "open file", path, error);
       CopyECB();
       if ((eNum = fsFile->open(path, open_mode, Mode, client, info)))
          {eNum = CopyErr(epname, eNum);
           delete fsFile; fsFile = 0;
          } else isOpen = true;
       return eNum;
      }

// Make sure the open flag is correct
//
// if (open_mode != SFS_O_RDWR)
//    return XrdSsiUtils::Emsg(epname, EPROTOTYPE, "open session", path, error);

// Handle the cgi information
//
   fileResource.rUser = fsUser = GetUser(info);
   fileResource.rInfo = info;

// Obtain a session
//
   Service->Provision(&fileResource);
   fileResource.ProvisionWait();
   if ((sessP = fileResource.Session()))
      {if (!fsUser) gigID = strdup(path);
          else {char gBuff[2048];
                snprintf(gBuff, sizeof(gBuff), "%s:%s", fsUser, path);
                gigID = strdup(gBuff);
               }
       DEBUG(gigID);
       isOpen = true;
       return SFS_OK;
      }

// Get error information
//
   eText = fileResource.eInfo.Get(eNum);
   if (!eNum)
      {eNum = ENOMSG; eText = "Service returned invalid session response.";}

// Decode the error
//
   switch(eNum)
         {case EAGAIN:
               if (!eText || !(*eText)) break;
               eNum = fileResource.eInfo.GetArg();
               DEBUG(path <<" --> " <<eText <<':' <<eNum);
               error.setErrInfo(eNum, eText);
               return SFS_REDIRECT;
               break;
          case EBUSY:
               eNum = fileResource.eInfo.GetArg();
               if (!eText || !(*eText)) eText = "Service is busy.";
               DEBUG(path <<" dly " <<eNum <<' ' <<eText);
               if (eNum <= 0) eNum = 1;
               error.setErrInfo(eNum, eText);
               return eNum;
               break;
          default:
               if (!eText || !(*eText)) eText = strerror(eNum);
               DEBUG(path <<" err " <<eNum <<' ' <<eText);
               error.setErrInfo(eNum, eText);
               return SFS_ERROR;
               break;
         };

// Something is quite wrong here
//
   Log.Emsg(epname, "Service redirect returned no target host name!");
   error.setErrInfo(ENOMSG, "Server logic error");
   return SFS_ERROR;
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
   static const char *epname = "read";
   XrdSsiRRInfo   rInfo(offset);
   XrdSsiFileReq *rqstP;
   XrdSfsXferSize retval;
   int            reqID = rInfo.Id();
   bool           noMore = false;

// Route to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->read(offset, buff, blen);
       return (rc ? CopyErr(epname, rc) : rc);
      }

// Find the request object. If not there we may have encountered an eof
//
   if (!(rqstP = rTab.LookUp(reqID)))
      {if (eofVec.IsSet(reqID))
          {eofVec.UnSet(reqID);
           return 0;
          }
        return XrdSsiUtils::Emsg(epname, ESRCH, "read", gigID, error);
       }

// Simply effect the read via the request object
//
   retval = rqstP->Read(noMore, buff, blen);

// See if we just completed this request
//
   if (noMore)
      {rqstP->Finalize();
       rTab.Del(reqID);
       eofVec.Set(reqID);
      }

// All done
//
   return retval;
}

/******************************************************************************/
/*                              r e a d   A I O                               */
/******************************************************************************/
  
int XrdSsiFile::read(XrdSfsAio *aiop)
{
   EPNAME("readaio");

// Do some debugging
//
   DEBUG("");

// Execute this request in a synchronous fashion
//
   aiop->Result = this->read((XrdSfsFileOffset)aiop->sfsAio.aio_offset,
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
   static const char *epname = "readv";

// Route this request to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->readv(readV, readCount);
       return (rc ? CopyErr(epname, rc) : rc);
      }

   return XrdSsiUtils::Emsg(epname, ENOSYS, "readv", gigID, error);
}
  
/******************************************************************************/
/*                              S e n d D a t a                               */
/******************************************************************************/
  
int XrdSsiFile::SendData(XrdSfsDio         *sfDio,
                         XrdSfsFileOffset   offset,
                         XrdSfsXferSize     size)
{
   static const char *epname = "SendData";
   XrdSsiRRInfo   rInfo(offset);
   XrdSsiFileReq *rqstP;
   int rc, reqID = rInfo.Id();

// Route this request to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->SendData(sfDio, offset, size);
       return (rc ? CopyErr(epname, rc) : rc);
      }

// Find the request object
//
   if (!(rqstP = rTab.LookUp(reqID)))
      return XrdSsiUtils::Emsg(epname, ESRCH, "send", gigID, error);

// Simply effect the send via the request object
//
   rc = rqstP->Send(sfDio, size);

// Determine how this ended
//
   if (rc > 0) rc = SFS_OK;
      else {rqstP->Finalize();
            rTab.Del(reqID);
           }
   return rc;
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

// Route this request to file system if need be (callback possible)
//
   if (fsFile)
      {CopyECB();
       int rc = fsFile->stat(buf);
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
   static const char *epname = "sync";

// Route this request to file system if need be (callback possible)
//
   if (fsFile)
      {CopyECB();
       int rc = fsFile->sync();
       return (rc ? CopyErr(epname, rc) : rc);
      }

// We don't support this
//
   return XrdSsiUtils::Emsg(epname, ENOSYS, "sync", gigID, error);
}

/******************************************************************************/
/*                              s y n c   A I O                               */
/******************************************************************************/
  
int XrdSsiFile::sync(XrdSfsAio *aiop)
{

// Execute this request in a synchronous fashion
//
   aiop->Result = this->sync();
   aiop->doneWrite();
   return 0;
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
   static const char *epname = "trunc";
   XrdSsiFileReq     *rqstP;
   XrdSsiRRInfo       rInfo(flen);
   XrdSsiRRInfo::Opc  reqXQ = rInfo.Cmd();
   int reqID = rInfo.Id();

// Route this request to file system if need be (callback possible)
//
   if (fsFile)
      {CopyECB();
       int rc = fsFile->truncate(flen);
       return (rc ? CopyErr(epname, rc) : rc);
      }

// Find the request object. If not there we may have encountered an eof
//
   if (!(rqstP = rTab.LookUp(reqID)))
      {if (eofVec.IsSet(reqID))
          {eofVec.UnSet(reqID);
           return 0;
          }
        return XrdSsiUtils::Emsg(epname, ESRCH, "cancel", gigID, error);
       }

// Process request (this can only be a cancel request)
//
   if (reqXQ != XrdSsiRRInfo::Can)
      return XrdSsiUtils::Emsg(epname, ENOSYS, "trunc", gigID, error);

// Perform the cancellation
//
   DEBUG(reqID <<':' <<gigID <<" cancelled");
   rqstP->Finalize();
   rTab.Del(reqID);
   return SFS_OK;
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
   static const char *epname = "write";
   XrdSsiRRInfo   rInfo(offset);
   int reqID = rInfo.Id();

// Route this request to file system if need be (no callback possible)
//
   if (fsFile)
      {int rc = fsFile->write(offset, buff, blen);
       return (rc ? CopyErr(epname, rc) : rc);
      }

// Check if we are reading a request segment and handle that. This assumes that
// writes to different requests cannot be interleaved (which they can't be).
//
   if (inProg) return writeAdd(buff, blen, reqID);

// Make sure this request does not refer to an active request
//
   if (rTab.LookUp(reqID))
      return XrdSsiUtils::Emsg(epname, EADDRINUSE, "write", gigID, error);

// The offset contains the actual size of the request, make sure it's OK
//
   reqSize = rInfo.Size();
   if (reqSize <= 0 || reqSize > maxRSZ || reqSize < blen)
      return XrdSsiUtils::Emsg(epname, EFBIG, "write", gigID, error);

// Indicate we are in the progress of collecting the request arguments
//
   inProg = true;
   eofVec.UnSet(reqID);

// Do some debugging
//
   DEBUG(reqID <<':' <<gigID <<" rsz=" <<reqSize <<" wsz=" <<blen);

// If the complete request is here then grab the buffer, transfer ownership to
// the request object, and then activate it for processing.
//
   if (reqSize == blen && xioP)
      {XrdSfsXioHandle *bRef;
       XrdSfsXio::XioStatus xStat = xioP->Swap(buff, bRef);
       if (xStat != XrdSfsXio::allOK)
          {char etxt[16];
           sprintf(etxt, "%d", xStat);
           Log.Emsg(epname, "Xio.Swap() return error status of ", etxt);
           return XrdSsiUtils::Emsg(epname, ENOMEM, "write", gigID, error);
          }
       if (!NewRequest(reqID, 0, bRef, blen))
          return XrdSsiUtils::Emsg(epname, ENOMEM, "write", gigID, error);
       return blen;
      }

// The full request is not present, so get a buffer to piece it together
//
   if (!(oucBuff = BuffPool->Alloc(reqSize)))
      return XrdSsiUtils::Emsg(epname, ENOMEM, "write", gigID, error);

// Setup to buffer this
//
   reqLeft = reqSize - blen;
   memcpy(oucBuff->Data(), buff, blen);
   if (!reqLeft)
      {oucBuff->SetLen(reqSize);

       if (!NewRequest(reqID, oucBuff, 0, reqSize))
          return XrdSsiUtils::Emsg(epname, ENOMEM, "write", gigID, error);
       oucBuff = 0;
      } else oucBuff->SetLen(blen, blen);
   return blen;
}

/******************************************************************************/
/* Private:                     w r i t e A d d                               */
/******************************************************************************/

XrdSfsXferSize XrdSsiFile::writeAdd(const char     *buff,      // In
                                    XrdSfsXferSize  blen,      // In
                                    int             rid)
/*
  Function: Add `blen' bytes from 'buff' to request and return the actual
            number of bytes added.

  Input:    buff      - Address of the buffer from which to get the data.
            blen      - The size of the buffer. This is the maximum number
                        of bytes that will be added.

  Output:   Returns the number of bytes added upon success and SFS_ERROR o/w.

  Notes:    An error return may be delayed until the next write(), close(), or
            sync() call.
*/
{
   static const char *epname = "writeAdd";
   int dlen;

// Make sure the caller is not exceeding the size stated on the first write
//
   if (blen > reqLeft)
      return XrdSsiUtils::Emsg(epname, EFBIG, "writeAdd", gigID, error);

// Append the bytes
//
   memcpy(oucBuff->Data(dlen), buff, blen);

// Adjust how much we have left
//
   reqLeft -= blen;
   DEBUG(rid <<':' <<gigID <<" rsz=" <<reqLeft <<" wsz=" <<blen);

// If we have a complete request. Transfer the buffer ownership to the request
// object and activate processing.
//
   if (!reqLeft)
      {oucBuff->SetLen(reqSize);
       if (!NewRequest(rid, oucBuff, 0, reqSize))
          return XrdSsiUtils::Emsg(epname, ENOMEM, "write", gigID, error);
       oucBuff = 0;
      }

// Return how much we appended
//
   dlen += blen;
   oucBuff->SetLen(dlen, dlen);
   return blen;
}

/******************************************************************************/
/*                             w r i t e   A I O                              */
/******************************************************************************/
  
int XrdSsiFile::write(XrdSfsAio *aiop)
{
   EPNAME("writeaio");

// Do some debugging
//
   DEBUG("");

// Execute this request in a synchronous fashion
//
   aiop->Result = this->write((XrdSfsFileOffset)aiop->sfsAio.aio_offset,
                                        (char *)aiop->sfsAio.aio_buf,
                                (XrdSfsXferSize)aiop->sfsAio.aio_nbytes);
   aiop->doneWrite();
   return 0;
}
