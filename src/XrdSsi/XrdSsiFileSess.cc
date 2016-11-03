/******************************************************************************/
/*                                                                            */
/*                     X r d S s i F i l e S e s s . c c                      */
/*                                                                            */
/* (c) 2016 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucERoute.hh"
#include "XrdOuc/XrdOucPList.hh"

#include "XrdSec/XrdSecEntity.hh"

#include "XrdSfs/XrdSfsAio.hh"
#include "XrdSfs/XrdSfsXio.hh"

#include "XrdSsi/XrdSsiEntity.hh"
#include "XrdSsi/XrdSsiFileSess.hh"
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
extern XrdOucBuffPool   *BuffPool;
extern XrdSsiService    *Service;
extern XrdSysError       Log;
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

XrdSysMutex     XrdSsiFileSess::arMutex;
XrdSsiFileSess *XrdSsiFileSess::freeList = 0;
int             XrdSsiFileSess::freeNum  = 0;
int             XrdSsiFileSess::freeNew  = 0;
int             XrdSsiFileSess::freeMax  = 100;
int             XrdSsiFileSess::freeAbs  = 200;

int             XrdSsiFileSess::authXQ   = 0;

int             XrdSsiFileSess::maxRSZ   = 0;

/******************************************************************************/
/*                                 A l l o c                                  */
/******************************************************************************/
  
XrdSsiFileSess *XrdSsiFileSess::Alloc(XrdOucErrInfo &einfo, const char *user)
{
   XrdSsiFileSess *fsP;

// Get a lock
//
   arMutex.Lock();

// Get either a reuseable object or a new one
//
   if ((fsP = freeList))
      {freeNum--;
       freeList = fsP->nextFree;
       arMutex.UnLock();
       fsP->Init(einfo, user, true);
      } else {
       freeNew++;
       if (freeMax <= freeAbs && freeNew >= freeMax/2)
          {freeMax += freeMax/2;
           freeNew = 0;
          }
       arMutex.UnLock();
       fsP = new XrdSsiFileSess(einfo, user);
      }

// Return the object
//
   return fsP;
}

/******************************************************************************/
/*                              A t t n I n f o                               */
/******************************************************************************/
  
bool XrdSsiFileSess::AttnInfo(XrdOucErrInfo &eInfo, const XrdSsiRespInfo *respP,
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

int XrdSsiFileSess::close(bool viaDel)
/*
  Function: Close the file object.

  Input:    None

  Output:   Always returns SFS_OK
*/
{
   const char *epname = "close";

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
   isOpen = false;
   return SFS_OK;
}
  
/******************************************************************************/
/*                                  f c t l                                   */
/******************************************************************************/

int XrdSsiFileSess::fctl(const int           cmd,
                               int           alen,
                         const char         *args,
                         const XrdSecEntity *client)
{
   static const char *epname = "fctl";
   XrdSsiRRInfo      *rInfo;
   XrdSsiFileReq     *rqstP;
   int reqID;

// If this isn't the special query, then return an error
//
   if (cmd != SFS_FCTL_SPEC1)
      return XrdSsiUtils::Emsg(epname, ENOTSUP, "fctl", gigID, *eInfo);

// Caller wishes to find out if a request is ready and wait if it is not
//
   if (!args || alen < (int)sizeof(XrdSsiRRInfo))
      return XrdSsiUtils::Emsg(epname, EINVAL, "fctl", gigID, *eInfo);

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
      return XrdSsiUtils::Emsg(epname, ESRCH, "fctl", gigID, *eInfo);

// Check if a response is waiting for the caller
//
   if (rqstP->WantResponse(*eInfo))
      {DEBUG(reqID <<':' <<gigID <<" resp ready");
       return SFS_DATAVEC;
      }

// Put this client into callback state
//
   DEBUG(reqID <<':' <<gigID <<" resp not ready");
   eInfo->setErrCB((XrdOucEICB *)rqstP);
   eInfo->setErrInfo(respWT, "");
   return SFS_STARTED;
}

/******************************************************************************/
/* Private:                         I n i t                                   */
/******************************************************************************/

void XrdSsiFileSess::Init(XrdOucErrInfo &einfo, const char *user, bool forReuse)
{
   tident     = (user ? strdup(user) : strdup(""));
   eInfo      = &einfo;
   gigID      = 0;
   fsUser     = 0;
   xioP       = 0;
   oucBuff    = 0;
   sessP      = 0;
   reqSize    = 0;
   reqLeft    = 0;
   isOpen     = false;
   inProg     = false;
   if (forReuse)
      {eofVec.Reset();
       rTab.Clear();
      }
}
  
/******************************************************************************/
/* Private:                   N e w R e q u e s t                             */
/******************************************************************************/
  
bool XrdSsiFileSess::NewRequest(int              reqid,
                                XrdOucBuffer    *oP,
                                XrdSfsXioHandle *bR,
                                int              rSz)
{
   XrdSsiFileReq *reqP;

// Allocate a new request object
//
   if ((reqid > XrdSsiRRInfo::maxID)
   || !(reqP = XrdSsiFileReq::Alloc(eInfo, this, sessP, gigID, tident, reqid)))
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

int XrdSsiFileSess::open(const char         *path,      // In
                         XrdOucEnv          &theEnv,    // In
                         XrdSfsFileOpenMode  open_mode) // In
/*
  Function: Open the file `path' in the mode indicated by `open_mode'.  

  Input:    path      - The fully qualified name of the resource.
            theEnv    - Environmental information.
            open_mode - It must contain only SFS_O_RDWR.

  Output:   Returns SFS_OK upon success, otherwise SFS_ERROR is returned.
*/
{
   static const char *epname = "open";
   FileResource fileResource(path, theEnv.secEnv(), authXQ);
   const char *eText, *usr;
   int eNum, n;

// Verify that this object is not already associated with an open file
//
   if (isOpen)
      return XrdSsiUtils::Emsg(epname, EADDRINUSE, "open session", path, *eInfo);

// Make sure the open flag is correct (we now open this R/O so don't check)
//
// if (open_mode != SFS_O_RDWR)
//    return XrdSsiUtils::Emsg(epname, EPROTOTYPE, "open session", path, *eInfo);

// Handle the cgi information
//
   fileResource.rDesc.rUser = fsUser = ((usr = theEnv.Get("ssi.user"))
                                     ? strdup(usr) : 0);
   fileResource.rDesc.rInfo = theEnv.Env(n);

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
       DEBUG(gigID <<" provisioned.");
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
               eInfo->setErrInfo(eNum, eText);
               return SFS_REDIRECT;
               break;
          case EBUSY:
               eNum = fileResource.eInfo.GetArg();
               if (!eText || !(*eText)) eText = "Service is busy.";
               DEBUG(path <<" dly " <<eNum <<' ' <<eText);
               if (eNum <= 0) eNum = 1;
               eInfo->setErrInfo(eNum, eText);
               return eNum;
               break;
          default:
               if (!eText || !(*eText)) eText = strerror(eNum);
               DEBUG(path <<" err " <<eNum <<' ' <<eText);
               eInfo->setErrInfo(eNum, eText);
               return SFS_ERROR;
               break;
         };

// Something is quite wrong here
//
   Log.Emsg(epname, "Service redirect returned no target host name!");
   eInfo->setErrInfo(ENOMSG, "Server logic error");
   return SFS_ERROR;
}
  
/******************************************************************************/
/*                                  r e a d                                   */
/******************************************************************************/

XrdSfsXferSize XrdSsiFileSess::read(XrdSfsFileOffset  offset,    // In
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

// Find the request object. If not there we may have encountered an eof
//
   if (!(rqstP = rTab.LookUp(reqID)))
      {if (eofVec.IsSet(reqID))
          {eofVec.UnSet(reqID);
           return 0;
          }
        return XrdSsiUtils::Emsg(epname, ESRCH, "read", gigID, *eInfo);
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
/*                               R e c y c l e                                */
/******************************************************************************/

void XrdSsiFileSess::Recycle()
{

// Do an immediate reset on ourselves to avoid getting too many locks
//
   Reset();

// Get a lock
//
   arMutex.Lock();

// Check if we should place this on the free list or simply delete it
//
   if (freeNum < freeMax)
      {nextFree = freeList;
       freeList = this;
       freeNum++;
       arMutex.UnLock();
      } else {
       arMutex.UnLock();
       delete this;
      }
}

/******************************************************************************/
/* Private:                        R e s e t                                  */
/******************************************************************************/
  
void XrdSsiFileSess::Reset()
{

// Close this session
//
   if (isOpen) close(true);

// Release other buffers
//
   if (tident) free(tident);
   if (fsUser) free(fsUser);
   if (gigID)  free(gigID);
}
  
/******************************************************************************/
/*                              S e n d D a t a                               */
/******************************************************************************/
  
int XrdSsiFileSess::SendData(XrdSfsDio         *sfDio,
                             XrdSfsFileOffset   offset,
                             XrdSfsXferSize     size)
{
   static const char *epname = "SendData";
   XrdSsiRRInfo   rInfo(offset);
   XrdSsiFileReq *rqstP;
   int rc, reqID = rInfo.Id();

// Find the request object
//
   if (!(rqstP = rTab.LookUp(reqID)))
      return XrdSsiUtils::Emsg(epname, ESRCH, "send", gigID, *eInfo);

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
/*                              t r u n c a t e                               */
/******************************************************************************/

int XrdSsiFileSess::truncate(XrdSfsFileOffset  flen)  // In
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

// Find the request object. If not there we may have encountered an eof
//
   if (!(rqstP = rTab.LookUp(reqID)))
      {if (eofVec.IsSet(reqID))
          {eofVec.UnSet(reqID);
           return 0;
          }
        return XrdSsiUtils::Emsg(epname, ESRCH, "cancel", gigID, *eInfo);
       }

// Process request (this can only be a cancel request)
//
   if (reqXQ != XrdSsiRRInfo::Can)
      return XrdSsiUtils::Emsg(epname, ENOSYS, "trunc", gigID, *eInfo);

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

XrdSfsXferSize XrdSsiFileSess::write(XrdSfsFileOffset  offset,    // In
                                     const char       *buff,      // In
                                     XrdSfsXferSize    blen)      // In
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

// Check if we are reading a request segment and handle that. This assumes that
// writes to different requests cannot be interleaved (which they can't be).
//
   if (inProg) return writeAdd(buff, blen, reqID);

// Make sure this request does not refer to an active request
//
   if (rTab.LookUp(reqID))
      return XrdSsiUtils::Emsg(epname, EADDRINUSE, "write", gigID, *eInfo);

// The offset contains the actual size of the request, make sure it's OK
//
   reqSize = rInfo.Size();
   if (reqSize <= 0 || reqSize > maxRSZ || reqSize < blen)
      return XrdSsiUtils::Emsg(epname, EFBIG, "write", gigID, *eInfo);

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
           return XrdSsiUtils::Emsg(epname, ENOMEM, "write", gigID, *eInfo);
          }
       if (!NewRequest(reqID, 0, bRef, blen))
          return XrdSsiUtils::Emsg(epname, ENOMEM, "write", gigID, *eInfo);
       return blen;
      }

// The full request is not present, so get a buffer to piece it together
//
   if (!(oucBuff = BuffPool->Alloc(reqSize)))
      return XrdSsiUtils::Emsg(epname, ENOMEM, "write", gigID, *eInfo);

// Setup to buffer this
//
   reqLeft = reqSize - blen;
   memcpy(oucBuff->Data(), buff, blen);
   if (!reqLeft)
      {oucBuff->SetLen(reqSize);

       if (!NewRequest(reqID, oucBuff, 0, reqSize))
          return XrdSsiUtils::Emsg(epname, ENOMEM, "write", gigID, *eInfo);
       oucBuff = 0;
      } else oucBuff->SetLen(blen, blen);
   return blen;
}

/******************************************************************************/
/* Private:                     w r i t e A d d                               */
/******************************************************************************/

XrdSfsXferSize XrdSsiFileSess::writeAdd(const char     *buff,      // In
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
      return XrdSsiUtils::Emsg(epname, EFBIG, "writeAdd", gigID, *eInfo);

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
          return XrdSsiUtils::Emsg(epname, ENOMEM, "write", gigID, *eInfo);
       oucBuff = 0;
      }

// Return how much we appended
//
   dlen += blen;
   oucBuff->SetLen(dlen, dlen);
   return blen;
}
