/******************************************************************************/
/*                                                                            */
/*                       X r d X r o o t d X e q . c c                        */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cctype>
#include <cstdio>
#include <string>
#include <sys/time.h>

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSfs/XrdSfsFlags.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdCks/XrdCksData.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucReqID.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSec/XrdSecProtector.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdInet.hh"
#include "Xrd/XrdLinkCtl.hh"
#include "XrdXrootd/XrdXrootdAioFob.hh"
#include "XrdXrootd/XrdXrootdCallBack.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdFileLock.hh"
#include "XrdXrootd/XrdXrootdJob.hh"
#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdNormAio.hh"
#include "XrdXrootd/XrdXrootdPio.hh"
#include "XrdXrootd/XrdXrootdPrepare.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdWVInfo.hh"
#include "XrdXrootd/XrdXrootdXeq.hh"
#include "XrdXrootd/XrdXrootdXPath.hh"

#include "XrdVersion.hh"

#ifndef ENODATA
#define ENODATA ENOATTR
#endif

#ifndef ETIME
#define ETIME ETIMEDOUT
#endif
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdSysTrace  XrdXrootdTrace;

/******************************************************************************/
/*                      L o c a l   S t r u c t u r e s                       */
/******************************************************************************/
  
struct XrdXrootdSessID
       {unsigned int       Sid;
                 int       Pid;
                 int       FD;
        unsigned int       Inst;

        XrdXrootdSessID() {}
       ~XrdXrootdSessID() {}
       };

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/

namespace
{
static const int op_isOpen    = 0x00010000;
static const int op_isRead    = 0x00020000;

const char *getTime()
{
static char buff[16];
char tuff[8];
struct timeval tv;
struct tm *tmp;

   if (gettimeofday(&tv, 0))
      {perror("gettimeofday");
       exit(255);
      }
   tmp = localtime(&tv.tv_sec);
   if (!tmp)
      {perror("localtime");
       exit(255);
      }
                                   //012345678901234
   if (strftime(buff, sizeof(buff), "%y%m%d:%H%M%S. ", tmp) <= 0)
      {errno = EINVAL;
       perror("strftime");
       exit(255);
      }

    snprintf(tuff, sizeof(tuff), "%d", static_cast<int>(tv.tv_usec/100000));
    buff[14] = tuff[0];
    return buff;
}

// comment out genUEID as it is not used
//

//int genUEID()
//{
//    static XrdSysMutex ueidMutex;
//    static int ueidVal = 1;
//    AtomicBeg(ueidMutex);
//    int n = AtomicInc(ueidVal);
//    AtomicEnd(ueidMutex);
//    return n;
//}

// Startup time
//          012345670123456
//          yymmdd:hhmmss.t
static const char *startUP = getTime();
}
 
/******************************************************************************/
/*                               d o _ A u t h                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Auth()
{
    XrdSecCredentials cred;
    XrdSecParameters *parm = 0;
    XrdOucErrInfo     eMsg;
    const char *eText;
    int rc, n;

// Ignore authenticate requests if security turned off
//
   if (!CIA) return Response.Send();
   cred.size   = Request.header.dlen;
   cred.buffer = argp->buff;

// If we have no auth protocol or the current protocol is being changed by the
// client (the client can do so at any time), try to get it. Track number of
// times we got a protocol object as the read count (we will zero it out later).
// The credtype change check is always done. While the credtype is consistent,
// not all protocols provided this information in the past. So, old clients will
// not necessarily be able to switch protocols mid-stream.
//
   if (!AuthProt
   ||  strncmp(Entity.prot, (const char *)Request.auth.credtype,
                                   sizeof(Request.auth.credtype)))
      {if (AuthProt) AuthProt->Delete();
       size_t size = sizeof(Request.auth.credtype);
       strncpy(Entity.prot, (const char *)Request.auth.credtype, size);
       if (!(AuthProt = CIA->getProtocol(Link->Host(), *(Link->AddrInfo()),
                                         &cred, eMsg)))
          {eText = eMsg.getErrText(rc);
           eDest.Emsg("Xeq", "User authentication failed;", eText);
           return Response.Send(kXR_AuthFailed, eText);
          }
       AuthProt->Entity.tident = AuthProt->Entity.pident = Link->ID;
       numReads++;
      }

// Now try to authenticate the client using the current protocol
//
   if (!(rc = AuthProt->Authenticate(&cred, &parm, &eMsg))
   &&  CIA->PostProcess(AuthProt->Entity, eMsg))
      {rc = Response.Send(); Status &= ~XRD_NEED_AUTH; SI->Bump(SI->LoginAU);
       AuthProt->Entity.ueid = mySID;
       Client = &AuthProt->Entity; numReads = 0; strcpy(Entity.prot, "host");
       if (TRACING(TRACE_AUTH)) Client->Display(eDest);
       if (DHS) Protect = DHS->New4Server(*AuthProt,clientPV&XrdOucEI::uVMask);
       if (Monitor.Logins() && Monitor.Auths()) MonAuth();
       if (!logLogin(true)) return -1;
       return rc;
      }

// If we need to continue authentication, tell the client as much
//
   if (rc > 0)
      {TRACEP(LOGIN, "more auth requested; sz=" <<(parm ? parm->size : 0));
       if (parm) {rc = Response.Send(kXR_authmore, parm->buffer, parm->size);
                  delete parm;
                  return rc;
                 }
       eDest.Emsg("Xeq", "Security requested additional auth w/o parms!");
       return Response.Send(kXR_ServerError,"invalid authentication exchange");
      }

// Authentication failed. We will delete the authentication object and zero
// out the pointer. We can do this without any locks because this section is
// single threaded relative to a connection. To prevent guessing attacks, we
// wait a variable amount of time if there have been 3 or more tries.
//
   if (AuthProt) {AuthProt->Delete(); AuthProt = 0;}
   if ((n = numReads - 2) > 0) XrdSysTimer::Snooze(n > 5 ? 5 : n);

// We got an error, bail out.
//
   SI->Bump(SI->AuthBad);
   eText = eMsg.getErrText(rc);
   eDest.Emsg("Xeq", "User authentication failed;", eText);
   return Response.Send(kXR_AuthFailed, eText);
}

/******************************************************************************/
/*                               d o _ B i n d                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Bind()
{
   XrdXrootdSessID *sp = (XrdXrootdSessID *)Request.bind.sessid;
   XrdXrootdProtocol *pp;
   XrdLink *lp;
   int i, pPid, rc;
   char buff[64], *cp, *dp;

// Update misc stats count
//
   SI->Bump(SI->miscCnt);

// Check if binds need to occur on a TLS connection.
//
   if ((doTLS & Req_TLSData) && !isTLS && !Link->hasBridge())
      return Response.Send(kXR_TLSRequired, "bind requires TLS");

// Find the link we are to bind to
//
   if (sp->FD <= 0 || !(lp = XrdLinkCtl::fd2link(sp->FD, sp->Inst)))
      return Response.Send(kXR_NotFound, "session not found");

// The link may have escaped so we need to hold this link and try again
//
   lp->Hold(1);
   if (lp != XrdLinkCtl::fd2link(sp->FD, sp->Inst))
      {lp->Hold(0);
       return Response.Send(kXR_NotFound, "session just closed");
      }

// Get the protocol associated with the link
//
   if (!(pp=dynamic_cast<XrdXrootdProtocol *>(lp->getProtocol()))||lp != pp->Link)
      {lp->Hold(0);
       return Response.Send(kXR_ArgInvalid, "session protocol not xroot");
      }

// Verify that the parent protocol is fully logged in
//
   if (!(pp->Status & XRD_LOGGEDIN) || (pp->Status & XRD_NEED_AUTH))
      {lp->Hold(0);
       return Response.Send(kXR_ArgInvalid, "session not logged in");
      }

// Verify that the bind is valid for the requestor
//
   if (sp->Pid != myPID || sp->Sid != pp->mySID)
      {lp->Hold(0);
       return Response.Send(kXR_ArgInvalid, "invalid session ID");
      }

// For now, verify that the request is comming from the same host
//
   if (strcmp(Link->Host(), lp->Host()))
      {lp->Hold(0);
       return Response.Send(kXR_NotAuthorized, "cross-host bind not allowed");
      }

// We need to hold the parent's stream mutex to prevent inspection or
// modification of other parallel binds that may occur
//
   XrdSysMutexHelper smHelper(pp->streamMutex);

// Find a slot for this path in parent protocol
//
   for (i = 1; i < maxStreams && pp->Stream[i]; i++) {}
   if (i >= maxStreams)
      {lp->Hold(0);
       return Response.Send(kXR_NoMemory, "bind limit exceeded");
      }

// Link this protocol to the parent
//
   pp->Stream[i] = this;
   Stream[0]     = pp;
   PathID        = i;

// Construct a login name for this bind session
//
   cp = strdup(lp->ID);
   if ( (dp = rindex(cp, '@'))) *dp = '\0';
   if (!(dp = rindex(cp, '.'))) pPid = 0;
      else {*dp++ = '\0'; pPid = strtol(dp, (char **)NULL, 10);}
   Link->setID(cp, pPid);
   free(cp);
   CapVer = pp->CapVer;
   Status = XRD_BOUNDPATH;
   clientPV = pp->clientPV;

// Document the bind
//
   smHelper.UnLock();
   sprintf(buff, "FD %d#%d bound", Link->FDnum(), i);
   eDest.Log(SYS_LOG_01, "Xeq", buff, lp->ID);

// Get the required number of parallel I/O objects
//
   pioFree = XrdXrootdPio::Alloc(maxPio);

// There are no errors possible at this point unless the response fails
//
   buff[0] = static_cast<char>(i);
   if (!(rc = Response.Send(kXR_ok, buff, 1))) rc = -EINPROGRESS;

// Return but keep the link disabled
//
   lp->Hold(0);
   return rc;
}

/******************************************************************************/
/*                             d o _ C h k P n t                              */
/*                                                                            */
/* Resides in XrdXrootdXeqChkPnt.cc                                           */
/******************************************************************************/
  
/******************************************************************************/
/*                              d o _ c h m o d                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Chmod()
{
   int mode, rc;
   char *opaque;
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);

// Check for static routing
//
   STATIC_REDIRECT(RD_chmod);

// Unmarshall the data
//
   mode = mapMode((int)ntohs(Request.chmod.mode));
   if (rpCheck(argp->buff, &opaque)) return rpEmsg("Modifying", argp->buff);
   if (!Squash(argp->buff))          return vpEmsg("Modifying", argp->buff);

// Preform the actual function
//
   rc = osFS->chmod(argp->buff, (XrdSfsMode)mode, myError, CRED, opaque);
   TRACEP(FS, "chmod rc=" <<rc <<" mode=" <<Xrd::oct1 <<mode <<' ' <<argp->buff);
   if (SFS_OK == rc) return Response.Send();

// An error occurred
//
   return fsError(rc, XROOTD_MON_CHMOD, myError, argp->buff, opaque);
}

/******************************************************************************/
/*                              d o _ C K s u m                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_CKsum(int canit)
{
   char *opaque;
   char *algT = JobCKT, *args[6];
   int rc;

// Check for static routing
//
   STATIC_REDIRECT(RD_chksum);

// Check if we support this operation
//
   if (!JobCKT || (!JobLCL && !JobCKS))
      return Response.Send(kXR_Unsupported, "query chksum is not supported");

// Prescreen the path
//
   if (rpCheck(argp->buff, &opaque)) return rpEmsg("Check summing", argp->buff);
   if (!Squash(argp->buff))          return vpEmsg("Check summing", argp->buff);

// If this is a cancel request, do it now
//
   if (canit)
      {if (JobCKS) JobCKS->Cancel(argp->buff, &Response);
       return Response.Send();
      }

// Check if multiple checksums are supported and if so, pre-process
//
   if (JobCKCGI && opaque && *opaque)
      {char cksT[64];
       algT = getCksType(opaque, cksT, sizeof(cksT));
       if (!algT)
          {char ebuf[1024];
           snprintf(ebuf, sizeof(ebuf), "%s checksum not supported.", cksT);
           return Response.Send(kXR_ServerError, ebuf);
          }
      }

// If we are allowed to locally query the checksum to avoid computation, do it
//
   if (JobLCL && (rc = do_CKsum(algT, argp->buff, opaque)) <= 0) return rc;

// Just make absolutely sure we can continue with a calculation
//
   if (!JobCKS)
      return Response.Send(kXR_ServerError, "Logic error computing checksum.");

// Check if multiple checksums are supported and construct right argument list
// We make a concession to a wrongly placed setfsuid/gid plugin. Fortunately,
// it only needs to know user's name but that can come from another plugin.
//
   std::string keyval; // Contents will be copied prior to return!
   if (JobCKCGI > 1 || JobLCL)
      {args[0] = algT;
       args[1] = algT;
       args[2] = argp->buff;
       args[3] = const_cast<char *>(Client->tident);
       if (Client->eaAPI->Get(std::string("request.name"), keyval))
          args[4] = const_cast<char *>(keyval.c_str());
          else if (Client->name) args[4] = Client->name;
                  else args[4] = 0;
       args[5] = 0;
      } else {
       args[0] = algT;
       args[1] = argp->buff;
       args[2] = 0;
      }

// Preform the actual function
//
   return JobCKS->Schedule(argp->buff, (const char **)args, &Response,
                  ((CapVer & kXR_vermask) >= kXR_ver002 ? 0 : JOB_Sync));
}

/******************************************************************************/
  
int XrdXrootdProtocol::do_CKsum(char *algT, const char *Path, char *Opaque)
{
   static char Space = ' ';
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
   int  CKTLen = strlen(algT);
   int ec, rc = osFS->chksum(XrdSfsFileSystem::csGet, algT, Path,
                             myError, CRED, Opaque);
   const char *csData = myError.getErrText(ec);

// Diagnose any hard errors
//
   if (rc) return fsError(rc, 0, myError, Path, Opaque);

// Return result if it is actually available
//
   if (*csData)
      {if (*csData == '!') return Response.Send(csData+1);
       struct iovec iov[4] = {{0,0}, {algT, (size_t)CKTLen}, {&Space, 1},
                              {(char *)csData, strlen(csData)+1}};
       return Response.Send(iov, 4);
      }

// Diagnose soft errors
//
   if (!JobCKS)
      {const char *eTxt[2] = {JobCKT, " checksum not available."};
       myError.setErrInfo(0, eTxt, 2);
       return Response.Send(kXR_ChkSumErr, myError.getErrText());
      }

// Return indicating that we should try calculating the checksum
//
   return 1;
}

/******************************************************************************/
/*                              d o _ C l o s e                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Close()
{
   static XrdXrootdCallBack closeCB("close", XROOTD_MON_CLOSE);
   XrdXrootdFile *fp;
   XrdXrootdFHandle fh(Request.close.fhandle);
   int rc;
   bool doDel = true;

// Keep statistics
//
   SI->Bump(SI->miscCnt);

// Find the file object
//
   if (!FTab || !(fp = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen, 
                          "close does not refer to an open file");

// Serialize the file to make sure all references due to async I/O and parallel
// stream operations have completed.
//
   fp->Serialize();

// If the file has a fob then it was subject to pgwrite and if uncorrected
// checksum errors exist do a forced close. This will trigger POSC or a restore.
//
   if (fp->pgwFob && !do_PgClose(fp, rc))
      {FTab->Del((Monitor.Files() ? Monitor.Agent : 0), fh.handle, true);
       numFiles--;
       return rc;
      }

// Setup the callback to allow close() to return SFS_STARTED so we can defer
// the response to the close request as it may be a lengthy operation. In
// this case the argument is the actual file pointer and the link reference
// is recorded in the file object.
//
   fp->cbArg = ReqID.getID();
   fp->XrdSfsp->error.setErrCB(&closeCB, (unsigned long long)fp);

// Do an explicit close of the file here; check for exceptions. Stall requests
// leave the file open as there will be a retry. Otherwise, we remove the
// file from our open table but a "started" return defers the the delete.
//
   rc = fp->XrdSfsp->close();
   TRACEP(FS, " fh=" <<fh.handle <<" close rc=" <<rc);
   if (rc >= SFS_STALL) return fsError(rc, 0, fp->XrdSfsp->error, 0, 0);
   if (rc == SFS_STARTED) doDel = false;

// Before we potentially delete the file handle in FTab->Del, generate the
// appropriate error code (if necessary).  Note that we delay the call
// to Response.Send() in the successful case to avoid holding on to the lock
// while the response is sent.
//
   int retval = 0;
   if (SFS_OK != rc) retval = fsError(rc, 0, fp->XrdSfsp->error, 0, 0);

// Delete the file from the file table. If the file object is deleted then it
// will unlock the file In all cases, final monitoring records will be produced.
//
   FTab->Del((Monitor.Files() ? Monitor.Agent : 0), fh.handle, doDel);
   numFiles--;

// Send back the right response
//
   if (SFS_OK == rc) return Response.Send();
   return retval;
}

/******************************************************************************/
/*                            d o _ D i r l i s t                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Dirlist()
{
   int bleft, rc = 0, dlen, cnt = 0;
   char *opaque, *buff, ebuff[4096];
   const char *dname;
   XrdSfsDirectory *dp;
   bool doDig;

// Check if we are digging for data
//
   doDig = (digFS && SFS_LCLROOT(argp->buff));

// Check for static routing
//
   if (!doDig) {STATIC_REDIRECT(RD_dirlist);}

// Prescreen the path
//
   if (rpCheck(argp->buff, &opaque)) return rpEmsg("Listing", argp->buff);
   if (!doDig && !Squash(argp->buff))return vpEmsg("Listing", argp->buff);

// Get a directory object
//
   if (doDig) dp = digFS->newDir(Link->ID, Monitor.Did);
      else    dp =  osFS->newDir(Link->ID, Monitor.Did);

// Make sure we have the object
//
   if (!dp)
      {snprintf(ebuff,sizeof(ebuff)-1,"Insufficient memory to open %s",argp->buff);
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_NoMemory, ebuff);
      }

// First open the directory
//
   dp->error.setUCap(clientPV);
   if ((rc = dp->open(argp->buff, CRED, opaque)))
      {rc = fsError(rc, XROOTD_MON_OPENDIR, dp->error, argp->buff, opaque);
       delete dp;
       return rc;
      }

// Check if the caller wants stat information as well
//
   if (Request.dirlist.options[0] & (kXR_dstat | kXR_dcksm))
      return do_DirStat(dp, ebuff, opaque);

// Start retreiving each entry and place in a local buffer with a trailing new
// line character (the last entry will have a null byte). If we cannot fit a
// full entry in the buffer, send what we have with an OKSOFAR and continue.
// This code depends on the fact that a directory entry will never be longer
// than sizeof( ebuff)-1; otherwise, an infinite loop will result. No errors
// are allowed to be reflected at this point.
//
  dname = 0;
  do {buff = ebuff; bleft = sizeof(ebuff);
      while(dname || (dname = dp->nextEntry()))
           {dlen = strlen(dname);
            if (dlen > 2 || dname[0] != '.' || (dlen == 2 && dname[1] != '.'))
               {if ((bleft -= (dlen+1)) < 0) break;
                strcpy(buff, dname); buff += dlen; *buff = '\n'; buff++; cnt++;
               }
            dname = 0;
           }
       if (dname) rc = Response.Send(kXR_oksofar, ebuff, buff-ebuff);
     } while(!rc && dname);

// Send the ending packet if we actually have one to send
//
   if (!rc) 
      {if (ebuff == buff) rc = Response.Send();
          else {*(buff-1) = '\0';
                rc = Response.Send((void *)ebuff, buff-ebuff);
               }
      }

// Close the directory
//
   dp->close();
   delete dp;
   if (!rc) {TRACEP(FS, "dirlist entries=" <<cnt <<" path=" <<argp->buff);}
   return rc;
}

/******************************************************************************/
/*                            d o _ D i r S t a t                             */
/******************************************************************************/

int XrdXrootdProtocol::do_DirStat(XrdSfsDirectory *dp, char *pbuff,
                                                       char *opaque)
{
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
   struct stat Stat;
   char *buff, *dLoc, *algT = 0;
   const char *csData, *dname;
   int bleft, rc = 0, dlen, cnt = 0, statSz = 160;
   bool manStat;
   struct {char ebuff[8192]; char epad[512];} XB;

// Preprocess checksum request. If we don't support checksums or if the
// requested checksum type is not supported, ignore it.
//
   if ((Request.dirlist.options[0] & kXR_dcksm) && JobLCL)
      {char cksT[64];
       algT = getCksType(opaque, cksT, sizeof(cksT));
       if (!algT)
          {char ebuf[1024];
           snprintf(ebuf, sizeof(ebuf), "%s checksum not supported.", cksT);
           return Response.Send(kXR_ServerError, ebuf);
          }
       statSz += XrdCksData::NameSize + (XrdCksData::ValuSize*2) + 8;
      }

// We always return stat information, see if we can use autostat
//
   manStat = (dp->autoStat(&Stat) != SFS_OK);

// Construct the path to the directory as we will be asking for stat calls
// if the interface does not support autostat or returning checksums.
//
   if (manStat || algT)
      {strcpy(pbuff, argp->buff);
       dlen = strlen(pbuff);
       if (pbuff[dlen-1] != '/') {pbuff[dlen] = '/'; dlen++;}
       dLoc = pbuff+dlen;
      } else dLoc = 0;

// The initial leadin is a "dot" entry to indicate to the client that we
// support the dstat option (older servers will not do that). It's up to the
// client to issue individual stat requests in that case.
//
   memset(&Stat, 0, sizeof(Stat));
   strcpy(XB.ebuff, ".\n0 0 0 0\n");
   buff = XB.ebuff+10; bleft = sizeof(XB.ebuff)-10;

// Start retreiving each entry and place in a local buffer with a trailing new
// line character (the last entry will have a null byte). If we cannot fit a
// full entry in the buffer, send what we have with an OKSOFAR and continue.
// This code depends on the fact that a directory entry will never be longer
// than sizeof( ebuff)-1; otherwise, an infinite loop will result. No errors
// are allowed to be reflected at this point.
//
  dname = 0;
  do {while(dname || (dname = dp->nextEntry()))
           {dlen = strlen(dname);
            if (dlen > 2 || dname[0] != '.' || (dlen == 2 && dname[1] != '.'))
               {if ((bleft -= (dlen+1)) < 0 || bleft < statSz) break;
                if (dLoc) strcpy(dLoc, dname);
                if (manStat)
                   {rc = osFS->stat(pbuff, &Stat, myError, CRED, opaque);
                    if (rc == SFS_ERROR && myError.getErrInfo() == ENOENT)
                       {dname = 0; continue;}
                    if (rc != SFS_OK)
                       return fsError(rc, XROOTD_MON_STAT, myError,
                                          argp->buff, opaque);
                   }
                strcpy(buff, dname); buff += dlen; *buff = '\n'; buff++; cnt++;
                dlen = StatGen(Stat, buff, sizeof(XB.epad));
                bleft -= dlen; buff += (dlen-1);
                if (algT)
                   {int ec = osFS->chksum(XrdSfsFileSystem::csGet, algT,
                                          pbuff, myError, CRED, opaque);
                    csData = myError.getErrText();
                    if (ec != SFS_OK || !(*csData) || *csData == '!')
                       csData = "none";
                    int n = snprintf(buff,sizeof(XB.epad)," [ %s:%s ]",
                                     algT, csData);
                    buff += n; bleft -= n;
                   }
                *buff = '\n'; buff++;
               }
            dname = 0;
           }
       if (dname)
          {rc = Response.Send(kXR_oksofar, XB.ebuff, buff-XB.ebuff);
           buff = XB.ebuff; bleft = sizeof(XB.ebuff);
           TRACEP(FS, "dirstat sofar n=" <<cnt <<" path=" <<argp->buff);
          }
     } while(!rc && dname);

// Send the ending packet if we actually have one to send
//
   if (!rc) 
      {if (XB.ebuff == buff) rc = Response.Send();
          else {*(buff-1) = '\0';
                rc = Response.Send((void *)XB.ebuff, buff-XB.ebuff);
               }
      }

// Close the directory
//
   dp->close();
   delete dp;
   if (!rc) {TRACEP(FS, "dirstat entries=" <<cnt <<" path=" <<argp->buff);}
   return rc;
}

/******************************************************************************/
/*                            d o _ E n d s e s s                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Endsess()
{
   XrdXrootdSessID *sp, sessID;
   int rc;

// Update misc stats count
//
   SI->Bump(SI->miscCnt);

// Extract out the FD and Instance from the session ID
//
   sp = (XrdXrootdSessID *)Request.endsess.sessid;
   memcpy((void *)&sessID.Pid,  &sp->Pid,  sizeof(sessID.Pid));
   memcpy((void *)&sessID.FD,   &sp->FD,   sizeof(sessID.FD));
   memcpy((void *)&sessID.Inst, &sp->Inst, sizeof(sessID.Inst));

// Trace this request
//
   TRACEP(LOGIN, "endsess " <<sessID.Pid <<':' <<sessID.FD <<'.' <<sessID.Inst);

// If this session id does not refer to us, ignore the request
//
   if (sessID.Pid != myPID) return Response.Send();

// Terminate the indicated session, if possible. This could also be a self-termination.
//
   if ((sessID.FD == 0 && sessID.Inst == 0) 
   ||  !(rc = Link->Terminate(0, sessID.FD, sessID.Inst))) return -1;

// Trace this request
//
   TRACEP(LOGIN, "endsess " <<sessID.Pid <<':' <<sessID.FD <<'.' <<sessID.Inst
          <<" rc=" <<rc <<" (" <<XrdSysE2T(rc < 0 ? -rc : EAGAIN) <<")");

// Return result. We only return obvious problems (exclude ESRCH and EPIPE).
//
   if (rc >  0)
      return (rc = Response.Send(kXR_wait, rc, "session still active")) ? rc:1;

   if (rc == -EACCES)return Response.Send(kXR_NotAuthorized, "not session owner");
   if (rc == -ETIME) return Response.Send(kXR_Cancelled,"session not ended");

   return Response.Send();
}

/******************************************************************************/
/*                              d o _ F A t t r                               */
/*                                                                            */
/* Resides in XrdXrootdXeqFAttr.cc                                            */
/******************************************************************************/

/******************************************************************************/
/*                             d o _ g p F i l e                              */
/******************************************************************************/
  
int XrdXrootdProtocol::do_gpFile()
{
// int gopts, buffsz;

// Keep Statistics (TO DO: differentiate get vs put)
//
   SI->Bump(SI->getfCnt);
// SI->Bump(SI->putfCnt);

// Check if gpfile need to occur on a TLS connection
//
   if ((doTLS & Req_TLSGPFile) && !isTLS && !Link->hasBridge())
      return Response.Send(kXR_TLSRequired, "gpfile requires TLS");

   return Response.Send(kXR_Unsupported, "gpfile request is not supported");
}

/******************************************************************************/
/*                             d o _ L o c a t e                              */
/******************************************************************************/

int XrdXrootdProtocol::do_Locate()
{
   static XrdXrootdCallBack locCB("locate", XROOTD_MON_LOCATE);
   int rc, opts, fsctl_cmd = SFS_FSCTL_LOCATE;
   char *opaque = 0, *Path, *fn = argp->buff, opt[8], *op=opt;
   XrdOucErrInfo myError(Link->ID,&locCB,ReqID.getID(),Monitor.Did,clientPV);
   bool doDig = false;

// Unmarshall the data
//
   opts = (int)ntohs(Request.locate.options);

// Map the options
//
   if (opts & kXR_nowait)  {fsctl_cmd |= SFS_O_NOWAIT; *op++ = 'i';}
   if (opts & kXR_refresh) {fsctl_cmd |= SFS_O_RESET;  *op++ = 's';}
   if (opts & kXR_force  ) {fsctl_cmd |= SFS_O_FORCE;  *op++ = 'f';}
   if (opts & kXR_prefname){fsctl_cmd |= SFS_O_HNAME;  *op++ = 'n';}
   if (opts & kXR_compress){fsctl_cmd |= SFS_O_RAWIO;  *op++ = 'u';}
   if (opts & kXR_4dirlist){fsctl_cmd |= SFS_O_DIRLIST;*op++ = 'D';}
   *op = '\0';
   TRACEP(FS, "locate " <<opt <<' ' <<fn);

// Check if this is a non-specific locate
//
        if (*fn != '*'){Path = fn;
                        doDig = (digFS && SFS_LCLROOT(Path));
                       }
   else if (*(fn+1))   {Path = fn+1;
                        doDig = (digFS && SFS_LCLROOT(Path));
                       }
   else                {Path = 0; 
                        fn = XPList.Next()->Path();
                        fsctl_cmd |= SFS_O_TRUNC;
                       }

// Check for static routing
//
   if (!doDig) {STATIC_REDIRECT(RD_locate);}

// Prescreen the path
//
   if (Path)
      {if (rpCheck(Path, &opaque)) return rpEmsg("Locating", Path);
       if (!doDig && !Squash(Path))return vpEmsg("Locating", Path);
      }

// Preform the actual function
//
   if (doDig) rc = digFS->fsctl(fsctl_cmd, fn, myError, CRED);
      else    rc =  osFS->fsctl(fsctl_cmd, fn, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<" locate " <<fn);
   return fsError(rc, (doDig ? 0 : XROOTD_MON_LOCATE), myError, Path, opaque);
}
  
/******************************************************************************/
/*                              d o _ L o g i n                               */
/*.x***************************************************************************/
  
int XrdXrootdProtocol::do_Login()
{
   XrdXrootdSessID sessID;
   XrdNetAddrInfo *addrP;
   int i, pid, rc, sendSID = 0;
   char uname[sizeof(Request.login.username)+1];

// Keep Statistics
//
   SI->Bump(SI->LoginAT);

// Check if login need to occur on a TLS connection
//
   if ((doTLS & Req_TLSLogin) && !isTLS && !Link->hasBridge())
      {const char *emsg = "login requires TLS be enabled";
       if (!ableTLS)
          {emsg = "login requires TLS support";
           eDest.Emsg("Xeq","login requires TLS but",Link->ID,"is incapable.");
          }
       return Response.Send(kXR_TLSRequired, emsg);
      }

// Unmarshall the pid and construct username using the POSIX.1-2008 standard
//
   pid = (int)ntohl(Request.login.pid);
   strncpy(uname, (const char *)Request.login.username, sizeof(uname)-1);
   uname[sizeof(uname)-1] = 0;
   XrdOucUtils::Sanitize(uname);

// Make sure the user is not already logged in
//
   if (Status) return Response.Send(kXR_InvalidRequest,
                                    "duplicate login; already logged in");

// Establish the ID for this link
//
   Link->setID(uname, pid);
   CapVer = Request.login.capver[0];

// Establish the session ID if the client can handle it (protocol version > 0)
//
   if ((i = (CapVer & kXR_vermask)))
      {sessID.FD   = Link->FDnum();
       sessID.Inst = Link->Inst();
       sessID.Pid  = myPID;
       mySID = getSID();
       sessID.Sid  = mySID;
       sendSID = 1;
       if (!clientPV)
          {        if (i >= kXR_ver004) clientPV = (int)0x0310;
              else if (i == kXR_ver003) clientPV = (int)0x0300;
              else if (i == kXR_ver002) clientPV = (int)0x0290;
              else if (i == kXR_ver001) clientPV = (int)0x0200;
              else                      clientPV = (int)0x0100;
          }
       if (CapVer & kXR_asyncap) clientPV |= XrdOucEI::uAsync;
       if (Request.login.ability & kXR_fullurl)
          clientPV |= XrdOucEI::uUrlOK;
       if (Request.login.ability & kXR_lclfile)
          clientPV |= XrdOucEI::uLclF;
       if (Request.login.ability & kXR_multipr)
          clientPV |= (XrdOucEI::uMProt | XrdOucEI::uUrlOK);
       if (Request.login.ability & kXR_readrdok)
          clientPV |= XrdOucEI::uReadR;
       if (Request.login.ability & kXR_hasipv64)
          clientPV |= XrdOucEI::uIPv64;
       if (Request.login.ability & kXR_redirflags)
          clientPV |= XrdOucEI::uRedirFlgs;
       if (Request.login.ability2 & kXR_ecredir )
          clientPV |= XrdOucEI::uEcRedir;
      }

// Mark the client as IPv4 if they came in as IPv4 or mapped IPv4 we can only
// return IPv4 addresses. Of course, if the client is dual-stacked then we
// simply indicate the client can accept either (the client better be honest).
//
   addrP = Link->AddrInfo();
   if (addrP->isIPType(XrdNetAddrInfo::IPv4) || addrP->isMapped())
      clientPV |= XrdOucEI::uIPv4;
// WORKAROUND: XrdCl 4.0.x often identifies worker nodes as being IPv6-only.
// Rather than breaking a significant number of our dual-stack workers, we
// automatically denote IPv6 connections as also supporting IPv4 - regardless
// of what the remote client claims. This was fixed in 4.3.x but we can't
// tell release differences until 4.5 when we can safely ignore this as we
// also don't want to misidentify IPv6-only clients either.
   else if (i < kXR_ver004 && XrdInet::GetAssumeV4())
           clientPV |= XrdOucEI::uIPv64;

// Mark the client as being on a private net if the address is private
//
   if (addrP->isPrivate()) {clientPV |= XrdOucEI::uPrip; rdType = 1;}
      else rdType = 0;

// Get the security token for this link. We will either get a token, a null
// string indicating host-only authentication, or a null indicating no
// authentication. We can then optimize of each case.
//
   if (CIA)
      {const char *pp=CIA->getParms(i, Link->AddrInfo());
       if (pp && i ) {if (!sendSID) rc = Response.Send((void *)pp, i);
                         else {struct iovec iov[3];
                               iov[1].iov_base = (char *)&sessID;
                               iov[1].iov_len  = sizeof(sessID);
                               iov[2].iov_base = (char *)pp;
                               iov[2].iov_len  = i;
                               rc = Response.Send(iov,3,int(i+sizeof(sessID)));
                              }
                      Status = (XRD_LOGGEDIN | XRD_NEED_AUTH);
                     }
          else {rc = (sendSID ? Response.Send((void *)&sessID, sizeof(sessID))
                              : Response.Send());
                Status = XRD_LOGGEDIN; SI->Bump(SI->LoginUA);
               }
      }
      else {rc = (sendSID ? Response.Send((void *)&sessID, sizeof(sessID))
                          : Response.Send());
            Status = XRD_LOGGEDIN; SI->Bump(SI->LoginUA);
           }

// We always allow at least host-based authentication. This may be over-ridden
// should strong authentication be enabled. Allocation of the protocol object
// already supplied the protocol name and the host name. We supply the tident
// and the connection details in addrInfo.
//
   Entity.tident = Entity.pident = Link->ID;
   Entity.addrInfo = Link->AddrInfo();
   Client = &Entity;

// Check if we need to process a login environment
//
   if (Request.login.dlen > 8)
      {XrdOucEnv loginEnv(argp->buff+1, Request.login.dlen-1);
       char *rnumb = loginEnv.Get("xrd.rn");
       char *cCode = loginEnv.Get("xrd.cc");
       char *tzVal = loginEnv.Get("xrd.tz");
       char *appXQ = loginEnv.Get("xrd.appname");
       char *aInfo = loginEnv.Get("xrd.info");
       int   tzNum = (tzVal ? atoi(tzVal) : 0);
       if (cCode && *cCode && tzNum >= -12 && tzNum <= 14)
          {XrdNetAddrInfo::LocInfo locInfo;
           locInfo.Country[0] = cCode[0]; locInfo.Country[1] = cCode[1];
           locInfo.TimeZone = tzNum & 0xff;
           Link->setLocation(locInfo);
          }
       if (Monitor.Ready() && (appXQ || aInfo))
          {char apBuff[1024];
           snprintf(apBuff, sizeof(apBuff), "&R=%s&x=%s&y=%s&I=%c",
                    (rnumb ? rnumb : ""),
                    (appXQ ? appXQ : ""), (aInfo ? aInfo : ""),
                    (clientPV & XrdOucEI::uIPv4 ? '4' : '6'));
           Entity.moninfo = strdup(apBuff);
          }

       if (rnumb)
          {int majr, minr, pchr;
           if (sscanf(rnumb, "v%d.%d.%d", &majr, &minr, &pchr) == 3)
              clientRN = (majr<<16) | ((minr<<8) | pchr);
              else if (sscanf(rnumb, "v%d-%*x", &majr) == 1) clientRN = -1;
          }
       if (appXQ) AppName = strdup(appXQ);
      }

// Allocate a monitoring object, if needed for this connection
//
   if (Monitor.Ready())
      {Monitor.Register(Link->ID, Link->Host(), "xroot");
       if (Monitor.Logins() && (!Monitor.Auths() || !(Status & XRD_NEED_AUTH)))
          {Monitor.Report(Entity.moninfo);
           if (Entity.moninfo) {free(Entity.moninfo); Entity.moninfo = 0;}
          }
      }

// Complete the rquestID object
//
   ReqID.setID(Request.header.streamid, Link->FDnum(), Link->Inst());

// Document this login
//
   if (!(Status & XRD_NEED_AUTH) && !logLogin()) return -1;
   return rc;
}

/******************************************************************************/
/*                              d o _ M k d i r                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Mkdir()
{
   int mode, rc;
   char *opaque;
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);

// Check for static routing
//
   STATIC_REDIRECT(RD_mkdir);

// Unmarshall the data
//
   mode = mapMode((int)ntohs(Request.mkdir.mode)) | S_IRWXU;
   if (Request.mkdir.options[0] & static_cast<unsigned char>(kXR_mkdirpath))
      mode |= SFS_O_MKPTH;
   if (rpCheck(argp->buff, &opaque)) return rpEmsg("Creating", argp->buff);
   if (!Squash(argp->buff))          return vpEmsg("Creating", argp->buff);

// Preform the actual function
//
   rc = osFS->mkdir(argp->buff, (XrdSfsMode)mode, myError, CRED, opaque);
   TRACEP(FS, "rc=" <<rc <<" mkdir " <<Xrd::oct1 <<mode <<' ' <<argp->buff);
   if (SFS_OK == rc) return Response.Send();

// An error occurred
//
   return fsError(rc, XROOTD_MON_MKDIR, myError, argp->buff, opaque);
}

/******************************************************************************/
/*                                 d o _ M v                                  */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Mv()
{
   int rc;
   char *oldp, *newp, *Opaque, *Npaque;
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);

// Check for static routing
//
   STATIC_REDIRECT(RD_mv);

// Find the space separator between the old and new paths
//
   oldp = newp = argp->buff;
   if (Request.mv.arg1len)
      {int n = ntohs(Request.mv.arg1len);
       if (n < 0 || n >= Request.mv.dlen || *(argp->buff+n) != ' ')
          return Response.Send(kXR_ArgInvalid, "invalid path specification");
       *(oldp+n) = 0;
       newp += n+1;
      } else {
       while(*newp && *newp != ' ') newp++;
       if (*newp) {*newp = '\0'; newp++;
                   while(*newp && *newp == ' ') newp++;
                  }
      }

// Get rid of relative paths and multiple slashes
//
   if (rpCheck(oldp, &Opaque)) return rpEmsg("Renaming",    oldp);
   if (rpCheck(newp, &Npaque)) return rpEmsg("Renaming to", newp);
   if (!Squash(oldp))          return vpEmsg("Renaming",    oldp);
   if (!Squash(newp))          return vpEmsg("Renaming to", newp);

// Check if new path actually specified here
//
   if (*newp == '\0')
      Response.Send(kXR_ArgMissing, "new path specified for mv");

// Preform the actual function
//
   rc = osFS->rename(oldp, newp, myError, CRED, Opaque, Npaque);
   TRACEP(FS, "rc=" <<rc <<" mv " <<oldp <<' ' <<newp);
   if (SFS_OK == rc) return Response.Send();

// An error occurred
//
   return fsError(rc, XROOTD_MON_MV, myError, oldp, Opaque);
}

/******************************************************************************/
/*                            d o _ O f f l o a d                             */
/******************************************************************************/

int XrdXrootdProtocol::do_Offload(int (XrdXrootdProtocol::*Invoke)(),int pathID)
{
   XrdSysSemaphore isAvail(0);
   XrdXrootdProtocol *pp;
   XrdXrootdPio      *pioP;
   int rc;
   kXR_char streamID[2];

// Verify that the path actually exists (note we will have the stream lock)
//
   if (!(pp = VerifyStream(rc, pathID))) return rc;

// Grab the stream ID
//
   Response.StreamID(streamID);

// Try to schedule this operation. In order to maximize the I/O overlap, we
// will wait until the stream gets control and will have a chance to start
// reading from the network. We handle refs for consistency.
//
   do{if (!pp->isActive)
         {pp->IO       = IO;
          pp->myBlen   = 0;
          pp->Resume   = &XrdXrootdProtocol::do_OffloadIO;
          pp->ResumePio= Invoke;
          pp->isActive = true;
          pp->newPio   = true;
          pp->reTry    = &isAvail;
          pp->Response.Set(streamID);
          pp->streamMutex.UnLock();
          Link->setRef(1);
          IO.File->Ref(1);
          Sched->Schedule((XrdJob *)(pp->Link));
          isAvail.Wait();
          return 0;
         }

      if ((pioP = pp->pioFree)) break;
      pp->reTry = &isAvail;
      pp->streamMutex.UnLock();
      TRACEP(FSZIO, "busy  path " <<pathID <<" offs=" <<IO.Offset);
      isAvail.Wait();
      TRACEP(FSZIO, "retry path " <<pathID <<" offs=" <<IO.Offset);
      pp->streamMutex.Lock();
      if (pp->isNOP)
         {pp->streamMutex.UnLock();
          return Response.Send(kXR_ArgInvalid, "path ID is not connected");
         }
      } while(1);

// Fill out the queue entry and add it to the queue
//
   pp->pioFree = pioP->Next; pioP->Next = 0;
   pioP->Set(Invoke, IO, streamID);
   IO.File->Ref(1);
   if (pp->pioLast) pp->pioLast->Next = pioP;
      else          pp->pioFirst      = pioP;
   pp->pioLast = pioP;
   pp->streamMutex.UnLock();
   return 0;
}

/******************************************************************************/
/*                          d o _ O f f l o a d I O                           */
/******************************************************************************/

int XrdXrootdProtocol::do_OffloadIO()
{
   XrdXrootdPio *pioP;
   int rc;

// Entry implies that we just got scheduled and are marked as active. Hence
// we need to post the session thread so that it can pick up the next request.
//
   streamMutex.Lock();
   isLinkWT = false;
   if (newPio)
      {newPio = false;
       if (reTry) {reTry->Post(); reTry = 0;}
       TRACEP(FSZIO, "dispatch new I/O path " <<PathID <<" offs=" <<IO.Offset);
      }
  
// Perform all I/O operations on a parallel stream
//
   if (!isNOP)
      do {streamMutex.UnLock();
          rc = (*this.*ResumePio)();
          streamMutex.Lock();

          if (rc > 0 && !isNOP)
             {ResumePio = Resume;
              Resume = &XrdXrootdProtocol::do_OffloadIO;
              isLinkWT = true;
              streamMutex.UnLock();
              return rc;
             }

          IO.File->Ref(-1);  // Note: File was ref'd when request was queued
          if (rc || isNOP || !(pioP = pioFirst)) break;
          if (!(pioFirst = pioP->Next)) pioLast = 0;

          IO = pioP->IO;
          ResumePio = pioP->ResumePio;
          Response.Set(pioP->StreamID);
          pioP->Next = pioFree; pioFree = pioP;
          if (reTry) {reTry->Post(); reTry = 0;}
         } while(1);
      else {rc = -1; IO.File->Ref(-1);}

// There are no pending operations or the link died
//
   if (rc) isNOP = true;
   isActive = false;
   Stream[0]->Link->setRef(-1);
   if (reTry) {reTry->Post(); reTry = 0;}
   if (endNote) endNote->Signal();
   streamMutex.UnLock();
   TRACEP(FSZIO, "offload complete path "<<PathID<<" virt rc=" <<rc);
   return (rc ? rc : -EINPROGRESS);
}

/******************************************************************************/
/*                               d o _ O p e n                                */
/******************************************************************************/

namespace
{
struct OpenHelper
      {XrdSfsFile        *fp;
       XrdXrootdFile     *xp;
       XrdXrootdFileLock *Locker;
       const char        *path;
       char               mode;
       bool               isOK;

                          OpenHelper(XrdXrootdFileLock *lkP, const char *fn)
                          : fp(0), xp(0), Locker(lkP), path(fn), mode(0),
                            isOK(false) {}

                         ~OpenHelper()
                              {if (!isOK)
                                  {if (xp) delete xp; // Deletes fp & unlocks
                                      else {if (fp) delete fp;
                                            if (mode) Locker->Unlock(path,mode);
                                           }
                                  }
                              }
      };
}
  
int XrdXrootdProtocol::do_Open()
{
   static XrdXrootdCallBack openCB("open file", XROOTD_MON_OPENR);
   int fhandle;
   int rc, mode, opts, openopts, compchk = 0;
   int popt, retStat = 0;
   char *opaque, usage, ebuff[2048], opC;
   bool doDig, doforce = false, isAsync = false;
   char *fn = argp->buff, opt[16], *op=opt;
   XrdSfsFile *fp;
   XrdXrootdFile *xp;
   struct stat statbuf;
   struct ServerResponseBody_Open myResp;
   int resplen = sizeof(myResp.fhandle);
   struct iovec IOResp[3];  // Note that IOResp[0] is completed by Response

// Keep Statistics
//
   SI->Bump(SI->openCnt);

// Unmarshall the data
//
   mode = (int)ntohs(Request.open.mode);
   opts = (int)ntohs(Request.open.options);

// Map the mode and options
//
   mode = mapMode(mode) | S_IRUSR | S_IWUSR; usage = 'r';
        if (opts & kXR_open_read)  
           {openopts  = SFS_O_RDONLY;  *op++ = 'r'; opC = XROOTD_MON_OPENR;}
   else if (opts & kXR_open_updt)   
           {openopts  = SFS_O_RDWR;    *op++ = 'u'; usage = 'w';
                                                    opC = XROOTD_MON_OPENW;}
   else if (opts & kXR_open_wrto)
           {openopts  = SFS_O_WRONLY;  *op++ = 'o'; usage = 'w';
                                                    opC = XROOTD_MON_OPENW;}
   else    {openopts  = SFS_O_RDONLY;  *op++ = 'r'; opC = XROOTD_MON_OPENR;}

        if (opts & kXR_new)
           {openopts |= SFS_O_CREAT;   *op++ = 'n'; opC = XROOTD_MON_OPENC;
            if (opts & kXR_replica)   {*op++ = '+';
                                       openopts |= SFS_O_REPLICA;
                                      }
            // Up until 3/28/19 we mistakenly used kXR_mkdir instead of
            // kXR_mkpath to allow path creation. That meant, path creation was
            // allowed if _mkpath|_async|_refresh|_open_apnd|_replica were set.
            // Since the client has always turned on _async that meant that
            // path creation was always enabled. We continue this boondogle
            // using the correct flag for backward compatibility reasons, sigh.
            //
            if (opts & (kXR_mkpath | kXR_async))
                                      {*op++ = 'm';
                                       mode |= SFS_O_MKPTH;
                                      }
           }
   else if (opts & kXR_delete)
           {openopts  = SFS_O_TRUNC;   *op++ = 'd'; opC = XROOTD_MON_OPENW;
            if (opts & kXR_mkdir)     {*op++ = 'm';
                                       mode |= SFS_O_MKPTH;
                                      }
           }
   if (opts & kXR_compress)        
           {openopts |= SFS_O_RAWIO;   *op++ = 'c'; compchk = 1;}
   if (opts & kXR_force)              {*op++ = 'f'; doforce = true;}
   if ((opts & kXR_async || as_force) && as_aioOK)
                                      {*op++ = 'a'; isAsync = true;}
   if (opts & kXR_refresh)            {*op++ = 's'; openopts |= SFS_O_RESET;
                                       SI->Bump(SI->Refresh);
                                      }
   if (opts & kXR_retstat)            {*op++ = 't'; retStat = 1;}
   if (opts & kXR_posc)               {*op++ = 'p'; openopts |= SFS_O_POSC;}
   *op = '\0';
   TRACEP(FS, "open " <<opt <<' ' <<fn);

// Check if opaque data has been provided
//
   if (rpCheck(fn, &opaque)) return rpEmsg("Opening", fn);

// Check if this is a local dig type file
//
   doDig = (digFS && SFS_LCLPATH(fn));

// Validate the path and then check if static redirection applies
//
   if (doDig) {popt = XROOTDXP_NOLK; opC = 0;}
      else {int ropt;
            if (!(popt = Squash(fn))) return vpEmsg("Opening", fn);
            if (Route[RD_open1].Host[rdType] && (ropt = RPList.Validate(fn)))
               return Response.Send(kXR_redirect, Route[ropt].Port[rdType],
                                                  Route[ropt].Host[rdType]);
           }

// Add the multi-write option if this path supports it
//
   if (popt & XROOTDXP_NOMWCHK) openopts |= SFS_O_MULTIW;

// Construct an open helper to release resources should we exit due to an error.
//
   OpenHelper oHelp(Locker, fn);

// Lock this file
//
   if (!(popt & XROOTDXP_NOLK))
      {if ((rc = Locker->Lock(fn, usage, doforce)))
          {const char *who;
           if (rc > 0) who = (rc > 1 ? "readers" : "reader");
              else {   rc = -rc;
                       who = (rc > 1 ? "writers" : "writer");
                   }
           snprintf(ebuff, sizeof(ebuff)-1,
                    "%s file %s is already opened by %d %s; open denied.",
                    ('r' == usage ? "Input" : "Output"), fn, rc, who);
           eDest.Emsg("Xeq", ebuff);
           return Response.Send(kXR_FileLocked, ebuff);
          } else oHelp.mode = usage;
      }

// Get a file object
//
   if (doDig) fp = digFS->newFile(Link->ID, Monitor.Did);
      else    fp =  osFS->newFile(Link->ID, Monitor.Did);

// Make sure we got one
//
   if (!fp)
      {snprintf(ebuff, sizeof(ebuff)-1,"Insufficient memory to open %s",fn);
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_NoMemory, ebuff);
      }
   oHelp.fp = fp;

// The open is elegible for a deferred response, indicate we're ok with that
//
   fp->error.setErrCB(&openCB, ReqID.getID());
   fp->error.setUCap(clientPV);

// If TPC opens require TLS but this is not a TLS connection, prohibit TPC
//
   if ((doTLS & Req_TLSTPC) && !isTLS && !Link->hasBridge())
      openopts|= SFS_O_NOTPC;

// Open the file
//
   if ((rc = fp->open(fn, (XrdSfsFileOpenMode)openopts,
                     (mode_t)mode, CRED, opaque)))
      {rc = fsError(rc, opC, fp->error, fn, opaque); return rc;}

// Obtain a hyper file object
//
   xp = new XrdXrootdFile(Link->ID, fn, fp, usage, isAsync, &statbuf);
   if (!xp)
      {snprintf(ebuff, sizeof(ebuff)-1, "Insufficient memory to open %s", fn);
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_NoMemory, ebuff);
      }
   oHelp.xp = xp;

// Serialize the link
//
   Link->Serialize();
   *ebuff = '\0';

// Create a file table for this link if it does not have one
//
   if (!FTab) FTab = new XrdXrootdFileTable(Monitor.Did);

// Insert this file into the link's file table
//
   if (!FTab || (fhandle = FTab->Add(xp)) < 0)
      {snprintf(ebuff, sizeof(ebuff)-1, "Insufficient memory to open %s", fn);
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_NoMemory, ebuff);
      }

// If the file supports exchange buffering, supply it with the object
//
   if (fsFeatures & XrdSfs::hasSXIO) fp->setXio(this);

// Document forced opens
//
   if (doforce)
      {int rdrs, wrtrs;
       Locker->numLocks(fn, rdrs, wrtrs);
       if (('r' == usage && wrtrs) || ('w' == usage && rdrs) || wrtrs > 1)
          {snprintf(ebuff, sizeof(ebuff)-1,
             "%s file %s forced opened with %d reader(s) and %d writer(s).",
             ('r' == usage ? "Input" : "Output"), fn, rdrs, wrtrs);
           eDest.Emsg("Xeq", ebuff);
          }
      }

// Determine if file is compressed
//
   memset(&myResp, 0, sizeof(myResp));
   if (!compchk) resplen = sizeof(myResp.fhandle);
      else {int cpsize;
            fp->getCXinfo((char *)myResp.cptype, cpsize);
            myResp.cpsize = static_cast<kXR_int32>(htonl(cpsize));
            resplen = sizeof(myResp);
           }

// If client wants a stat in open, return the stat information
//
   if (retStat)
      {retStat = StatGen(statbuf, ebuff, sizeof(ebuff));
       IOResp[1].iov_base = (char *)&myResp; IOResp[1].iov_len = sizeof(myResp);
       IOResp[2].iov_base =         ebuff;   IOResp[2].iov_len = retStat;
       resplen = sizeof(myResp) + retStat;
      }

// If we are monitoring, send off a path to dictionary mapping (must try 1st!)
//
   if (Monitor.Files())
      {xp->Stats.FileID = Monitor.MapPath(fn);
       if (!(xp->Stats.monLvl)) xp->Stats.monLvl = XrdXrootdFileStats::monOn;
       Monitor.Agent->Open(xp->Stats.FileID, statbuf.st_size);
      }

// Since file monitoring is deprecated, a dictid may not have been assigned.
// But if fstream monitoring is enabled it will assign the dictid.
//
   if (Monitor.Fstat())
      XrdXrootdMonFile::Open(&(xp->Stats), fn, Monitor.Did, usage == 'w');

// Insert the file handle
//
   memcpy((void *)myResp.fhandle,(const void *)&fhandle,sizeof(myResp.fhandle));
   numFiles++;

// Respond (failure is not an option now)
//
   oHelp.isOK = true;
   if (retStat)  return Response.Send(IOResp, 3, resplen);
      else       return Response.Send((void *)&myResp, resplen);
}

/******************************************************************************/
/*                               d o _ P i n g                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Ping()
{

// Keep Statistics
//
   SI->Bump(SI->miscCnt);

// This is a basic nop
//
   return Response.Send();
}

/******************************************************************************/
/*                            d o _ P r e p a r e                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Prepare(bool isQuery)
{
   static XrdXrootdCallBack prpCB("query", XROOTD_MON_QUERY);

   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);

   XrdOucTokenizer pathlist(argp->buff);
   XrdOucTList *pFirst=0, *pP, *pLast = 0;
   XrdOucTList *oFirst=0, *oP, *oLast = 0;
   XrdOucTListHelper pHelp(&pFirst), oHelp(&oFirst);
   XrdXrootdPrepArgs pargs(0, 0);
   XrdSfsPrep fsprep;

   int rc, pathnum = 0;
   char reqid[128], nidbuff[512], *path, *opaque, *prpid = 0;
   unsigned short optX = ntohs(Request.prepare.optionX);
   char opts;
   bool isCancel, isEvict, isPrepare;

// Check if this is an evict request (similar to stage)
//
   isEvict = (optX & kXR_evict) != 0;

// Establish what we are really doing here
//
   if (isQuery)
      {opts = 0;
       isCancel = false;
      } else {
       if (Request.prepare.options & kXR_cancel)
          {opts = 0;
           isCancel = true;
          } else {
           opts = (isEvict ? 0 : Request.prepare.options);
           isCancel = false;
          }
      }
   isPrepare = !(isCancel || isQuery);

// Apply prepare limits, as necessary.
//
   if (isPrepare && (PrepareLimit >= 0) && (++PrepareCount > PrepareLimit)) {
      if (LimitError) {
         return Response.Send( kXR_overQuota,
                              "Surpassed this connection's prepare limit.");
      } else {
         return Response.Send();
      }
   }

// Check for static routing
//
   if ((opts & kXR_stage) || isCancel) {STATIC_REDIRECT(RD_prepstg);}
   STATIC_REDIRECT(RD_prepare);

// Prehandle requests that must have a requestID. Otherwise, generate one.
// Note that prepare request id's have two formats. The external format is
// is qualifiaed by this host while the internal one removes the qualification.
// The internal one is only used for the native prepare implementation.
// To wit: prpid is the unqualified ID while reqid is the qualified one for
// generated id's while prpid is always the specified request id.
//
   if (isCancel || isQuery)
      {if (!(prpid = pathlist.GetLine()))
          return Response.Send(kXR_ArgMissing, "Prepare requestid not specified");
       fsprep.reqid = prpid;
       fsprep.opts = (isCancel ? Prep_CANCEL : Prep_QUERY);
       if (!PrepareAlt)
          {char hname[256];
           int  hport;
           prpid = PrepID->isMine(prpid, hport, hname, sizeof(hname));
           if (!prpid)
              {if (!hport) return Response.Send(kXR_ArgInvalid,
                           "Prepare requestid owned by an unknown server");
               TRACEI(REDIR, Response.ID() <<" redirecting prepare to "
                             << hname <<':' <<hport);
               return Response.Send(kXR_redirect, hport, hname);
              }
          }
      } else {
       if (opts & kXR_stage)
          {prpid = PrepID->ID(reqid, sizeof(reqid));
           fsprep.reqid = reqid;
           fsprep.opts  = Prep_STAGE | (opts & kXR_coloc ? Prep_COLOC : 0);
          } else {
            reqid[0]='*'; reqid[1]='\0';
            fsprep.reqid = prpid = reqid;
            fsprep.opts = (isEvict ? Prep_EVICT : 0);
          }
      }

// Initialize the file system prepare arg list
//
   fsprep.paths   = 0;
   fsprep.oinfo   = 0;
   fsprep.notify  = 0;

// Cycle through all of the paths in the list
//
   while((path = pathlist.GetLine()))
        {if (rpCheck(path, &opaque)) return rpEmsg("Preparing", path);
         if (!Squash(path))          return vpEmsg("Preparing", path);
         pP = new XrdOucTList(path, pathnum);
         (pLast ? (pLast->next = pP) : (pFirst = pP)); pLast = pP;
         oP = new XrdOucTList(opaque, 0);
         (oLast ? (oLast->next = oP) : (oFirst = oP)); oLast = oP;
         pathnum++;
        }
   fsprep.paths = pFirst;
   fsprep.oinfo = oFirst;

// We support callbacks but only for alternate prepare processing
//
   if (PrepareAlt) myError.setErrCB(&prpCB, ReqID.getID());

// Process cancel requests here; they are simple at this point.
//
   if (isCancel)
      {if (SFS_OK != (rc = osFS->prepare(fsprep, myError, CRED)))
          return fsError(rc, XROOTD_MON_PREP, myError, path, 0);
       rc = Response.Send();
       if (!PrepareAlt) XrdXrootdPrepare::Logdel(prpid);
       return rc;
      }

// Process query requests here; they are simple at this point.
//
   if (isQuery)
      {if (PrepareAlt)
          {if (SFS_OK != (rc = osFS->prepare(fsprep, myError, CRED)))
              return fsError(rc, XROOTD_MON_PREP, myError, path, 0);
           rc = Response.Send();
          } else {
           char *mBuff = myError.getMsgBuff(rc);
           pargs.reqid = prpid;
           pargs.user  = Link->ID;
           pargs.paths = pFirst;
           rc = XrdXrootdPrepare::List(pargs, mBuff, rc);
           if (rc < 0) rc = Response.Send("No information found.");
              else rc = Response.Send(mBuff);
          }
       return rc;
      }

// Make sure we have at least one path
//
   if (!pFirst)
      return Response.Send(kXR_ArgMissing, "No prepare paths specified");

// Handle evict. We only support the evicts for alternate prepare handlers.
//
   if (isEvict)
      {if (PrepareAlt
       && (SFS_OK != (rc = osFS->prepare(fsprep, myError, CRED))))
          return fsError(rc, XROOTD_MON_PREP, myError, path, 0);
       return Response.Send();
      }

// Handle notification parameter. The notification depends on whether or not
// we have a custom prepare handler.
//
   if (opts & kXR_notify)
      {const char *nprot = (opts & kXR_usetcp ? "tcp" : "udp");
       fsprep.notify  = nidbuff;
       if (PrepareAlt)
          {if (Request.prepare.port == 0) fsprep.notify = 0;
              else snprintf(nidbuff, sizeof(nidbuff), "%s://%s:%d/",
                            nprot, Link->Host(), ntohs(Request.prepare.port));
          } else sprintf(nidbuff, Notify, nprot, Link->FDnum(), Link->ID);
       if (fsprep.notify)
          fsprep.opts = (opts & kXR_noerrs ? Prep_SENDAOK : Prep_SENDACK);
      }

// Complete prepare options
//
   fsprep.opts |= (opts & kXR_fresh ? Prep_FRESH : 0);
   if (opts & kXR_wmode) fsprep.opts |= Prep_WMODE;
   if (PrepareAlt)
      {switch(Request.prepare.prty)
             {case 0:  fsprep.opts |= Prep_PRTY0; break;
              case 1:  fsprep.opts |= Prep_PRTY1; break;
              case 2:  fsprep.opts |= Prep_PRTY2; break;
              case 3:  fsprep.opts |= Prep_PRTY3; break;
              default: break;
             }
      } else {
       if (Request.prepare.prty == 0) fsprep.opts |= Prep_PRTY0;
          else fsprep.opts |= Prep_PRTY1;
      }

// Issue the prepare request
//
   if (SFS_OK != (rc = osFS->prepare(fsprep, myError, CRED)))
      return fsError(rc, XROOTD_MON_PREP, myError, pFirst->text, oFirst->text);

// Perform final processing
//
   if (!(opts & kXR_stage)) rc = Response.Send();
      else {rc = Response.Send(reqid, strlen(reqid));
            if (!PrepareAlt)
               {pargs.reqid = prpid;
                pargs.user  = Link->ID;
                pargs.paths = pFirst;
                XrdXrootdPrepare::Log(pargs);
               }
           }
   return rc;
}
  
/******************************************************************************/
/*                           d o _ P r o t o c o l                            */
/******************************************************************************/

namespace XrdXrootd
{
extern char *bifResp[2];
extern int   bifRLen[2];
}
  
int XrdXrootdProtocol::do_Protocol()
{
   static kXR_int32 verNum = static_cast<kXR_int32>(htonl(kXR_PROTOCOLVERSION));
   static kXR_int32 theRle = static_cast<kXR_int32>(htonl(myRole));
   static kXR_int32 theRlf = static_cast<kXR_int32>(htonl(myRolf));
   static kXR_int32 theRlt = static_cast<kXR_int32>(htonl(myRole|kXR_gotoTLS));

   ServerResponseBody_Protocol theResp;
   struct iovec ioVec[4] = {{0,0},{&theResp,kXR_ShortProtRespLen},{0,0},{0,0}};

   int rc, iovN = 2, RespLen = kXR_ShortProtRespLen;
   bool wantTLS = false;

// Keep Statistics
//
   SI->Bump(SI->miscCnt);

// Determine which response to provide
//
   if (Request.protocol.clientpv)
      {int cvn = XrdOucEI::uVMask & ntohl(Request.protocol.clientpv);
       if (!Status || !(clientPV & XrdOucEI::uVMask))
          clientPV = (clientPV & ~XrdOucEI::uVMask) | cvn;
          else cvn = (clientPV &  XrdOucEI::uVMask);

       if (Request.protocol.flags & ClientProtocolRequest::kXR_bifreqs
       &&  XrdXrootd::bifResp[0])
          {int k =( Link->AddrInfo()->isPrivate() ? 1 : 0);
           ioVec[iovN  ].iov_base = XrdXrootd::bifResp[k];
           ioVec[iovN++].iov_len  = XrdXrootd::bifRLen[k];
           RespLen += XrdXrootd::bifRLen[k];
          }

       if (DHS && cvn >= kXR_PROTSIGNVERSION
       &&  Request.protocol.flags & ClientProtocolRequest::kXR_secreqs)
          {int n = DHS->ProtResp(theResp.secreq, *(Link->AddrInfo()), cvn);
           ioVec[iovN  ].iov_base = (void *)&theResp.secreq;
           ioVec[iovN++].iov_len  = n;
           RespLen += n;
          }

       if ((myRole & kXR_haveTLS) != 0 && !(Link->hasTLS()))
          {wantTLS = (Request.protocol.flags &
                      ClientProtocolRequest::kXR_wantTLS) != 0;
           ableTLS = wantTLS || (Request.protocol.flags &
                                 ClientProtocolRequest::kXR_ableTLS) != 0;
           if (ableTLS) doTLS = tlsCap;
              else      doTLS = tlsNot;
           if (ableTLS && !wantTLS)
              switch(Request.protocol.expect & ClientProtocolRequest::kXR_ExpMask)
                    {case ClientProtocolRequest::kXR_ExpBind:
                          wantTLS = (doTLS & Req_TLSData)  != 0;
                          break;
                     case ClientProtocolRequest::kXR_ExpLogin:
                          wantTLS = (doTLS & Req_TLSLogin) != 0;
                          break;
                     case ClientProtocolRequest::kXR_ExpTPC:
                          wantTLS = (doTLS & Req_TLSTPC)   != 0 ||
                                    (doTLS & Req_TLSLogin) != 0;
                          break;
                     default: break;
                    }
          }
       theResp.flags = (wantTLS ? theRlt : theRle);
      } else {
       theResp.flags = theRlf;
       doTLS = tlsNot;
      }

// Send the response
//
   theResp.pval = verNum;
   rc = Response.Send(ioVec, iovN, RespLen);

// If the client wants to start using TLS, enable it now. If we fail then we
// have no choice but to terminate the connection. Note that incapable clients
// don't want TLS but if we require TLS anyway, they will get an error either
// pre-login or post-login or on a bind later on.
//
   if (rc == 0 && wantTLS)
      {if (Link->setTLS(true, tlsCtx))
          {Link->setProtName("xroots");
           isTLS = true;
          } else {
           eDest.Emsg("Xeq", "Unable to enable TLS for", Link->ID);
           rc = -1;
          }
      }
   return rc;
}

/******************************************************************************/
/*                              d o _ Q c o n f                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Qconf()
{
   static const int fsctl_cmd = SFS_FSCTL_STATCC|SFS_O_LOCAL;
   XrdOucTokenizer qcargs(argp->buff);
   char *val, buff[4096], *bp=buff;
   int n, bleft = sizeof(buff);

// Get the first argument
//
   if (!qcargs.GetLine() || !(val = qcargs.GetToken()))
      return Response.Send(kXR_ArgMissing, "query config argument not specified.");

// The first item can be xrootd or cmsd to display the config file
//
   if (!strcmp(val, "cmsd") || !strcmp(val, "xrootd"))
      return do_QconfCX(qcargs, val);

// Trace this query variable
//
   do {TRACEP(DEBUG, "query config " <<val);

   // Now determine what the user wants to query
   //
        if (!strcmp("bind_max", val))
           {n = snprintf(bp, bleft, "%d\n", maxStreams-1);
            bp += n; bleft -= n;
           }
   else if (!strcmp("chksum", val))
           {if (!JobCKT)
               {n = snprintf(bp, bleft, "chksum\n");
                bp += n; bleft -= n;
                continue;
               }
            XrdOucTList *tP = JobCKTLST;
            char sep;
            do {sep = (tP->next ? ',' :'\n');
                n = snprintf(bp, bleft, "%d:%s%c", tP->ival[0], tP->text, sep);
                bp += n; bleft -= n;
                tP = tP->next;
               } while(tP && bleft > 0);
           }
   else if (!strcmp("cid", val))
           {const char *cidval = getenv("XRDCMSCLUSTERID");
            if (!cidval || !(*cidval)) cidval = "cid";
            n = snprintf(bp, bleft, "%s\n", cidval);
            bp += n; bleft -= n;
           }
   else if (!strcmp("cms", val))
           {XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
            if (osFS->fsctl(fsctl_cmd, ".", myError, CRED) == SFS_DATA)
                    n = snprintf(bp, bleft, "%s\n", myError.getErrText());
               else n = snprintf(bp, bleft, "%s\n", "cms");
            bp += n; bleft -= n;
           }
   else if (!strcmp("pio_max", val))
           {n = snprintf(bp, bleft, "%d\n", maxPio+1);
            bp += n; bleft -= n;
           }
   else if (!strcmp("readv_ior_max", val))
           {n = snprintf(bp,bleft,"%d\n",maxTransz-(int)sizeof(readahead_list));
            bp += n; bleft -= n;
           }
   else if (!strcmp("readv_iov_max", val)) 
           {n = snprintf(bp, bleft, "%d\n", XrdProto::maxRvecsz);
            bp += n; bleft -= n;
           }
   else if (!strcmp("role", val))
           {const char *theRole = getenv("XRDROLE");
            n = snprintf(bp, bleft, "%s\n", (theRole ? theRole : "none"));
            bp += n; bleft -= n;
           }
   else if (!strcmp("sitename", val))
           {const char *siteName = getenv("XRDSITE");
            n = snprintf(bp, bleft, "%s\n", (siteName ? siteName : "sitename"));
            bp += n; bleft -= n;
           }
   else if (!strcmp("start", val))
           {n = snprintf(bp, bleft, "%s\n", startUP);
            bp += n; bleft -= n;
           }
   else if (!strcmp("sysid", val))
           {const char *cidval = getenv("XRDCMSCLUSTERID");
            const char *nidval = getenv("XRDCMSVNID");
            if (!cidval || !(*cidval) || !nidval || !(*nidval))
               {cidval = "sysid"; nidval = "";}
            n = snprintf(bp, bleft, "%s %s\n", nidval, cidval);
            bp += n; bleft -= n;
           }
   else if (!strcmp("tpc", val))
           {char *tpcval = getenv("XRDTPC");
            n = snprintf(bp, bleft, "%s\n", (tpcval ? tpcval : "tpc"));
            bp += n; bleft -= n;
           }
   else if (!strcmp("tpcdlg", val))
           {char *tpcval = getenv("XRDTPCDLG");
            n = snprintf(bp, bleft, "%s\n", (tpcval ? tpcval : "tpcdlg"));
            bp += n; bleft -= n;
           }
   else if (!strcmp("tls_port", val) && tlsPort)
           {n = snprintf(bp, bleft, "%d\n", tlsPort);
            bp += n; bleft -= n;
           }
   else if (!strcmp("window", val) && Window)
           {n = snprintf(bp, bleft, "%d\n", Window);
            bp += n; bleft -= n;
           }
   else if (!strcmp("version", val))
           {n = snprintf(bp, bleft, "%s\n", XrdVSTRING);
            bp += n; bleft -= n;
           }
   else if (!strcmp("vnid", val))
           {const char *nidval = getenv("XRDCMSVNID");
            if (!nidval || !(*nidval)) nidval = "vnid";
            n = snprintf(bp, bleft, "%s\n", nidval);
           }
   else if (!strcmp("fattr", val))
           {n = snprintf(bp, bleft, "%s\n", usxParms);
            bp += n; bleft -= n;
           }
   else {n = strlen(val);
         if (bleft <= n) break;
         strcpy(bp, val); bp +=n; *bp = '\n'; bp++;
         bleft -= (n+1);
        }
   } while(bleft > 0 && (val = qcargs.GetToken()));

// Make sure all ended well
//
   if (val) 
      return Response.Send(kXR_ArgTooLong, "too many query config arguments.");

// All done
//
   return Response.Send(buff, sizeof(buff) - bleft);
}
  
/******************************************************************************/
/*                            d o _ Q c o n f C X                             */
/******************************************************************************/

int XrdXrootdProtocol::do_QconfCX(XrdOucTokenizer &qcargs, char *val)
{
  extern XrdOucString *XrdXrootdCF;
  bool isCMSD = (*val == 'c');

// Make sure there is nothing else following the token
//
   if ((val = qcargs.GetToken()))
      return Response.Send(kXR_ArgInvalid, "too many query config arguments.");

// If this is a cms just return a null for now
//
   if (isCMSD) return Response.Send((void *)"\n", 2);

// Display the xrootd configuration
//
   if (XrdXrootdCF && isTLS && getenv("XROOTD_QCFOK"))
      return Response.Send((void *)XrdXrootdCF->c_str(), XrdXrootdCF->length());

// Respond with a null
//
   return Response.Send((void *)"\n", 2);
}

/******************************************************************************/
/*                                d o _ Q f h                                 */
/******************************************************************************/

int XrdXrootdProtocol::do_Qfh()
{
   static XrdXrootdCallBack qryCB("query", XROOTD_MON_QUERY);
   XrdXrootdFHandle fh(Request.query.fhandle);
   XrdXrootdFile *fp;
   const char *fArg = 0, *qType = "";
   int rc;
   short qopt = (short)ntohs(Request.query.infotype);

// Update misc stats count
//
   SI->Bump(SI->miscCnt);

// Find the file object
//
   if (!FTab || !(fp = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,
                           "query does not refer to an open file");

// The query is elegible for a deferred response, indicate we're ok with that
//
   fp->XrdSfsp->error.setErrCB(&qryCB, ReqID.getID());

// Perform the appropriate query
//
   switch(qopt)
         {case kXR_Qopaqug: qType = "Qopaqug";
                            fArg = (Request.query.dlen ? argp->buff : 0);
                            rc = fp->XrdSfsp->fctl(SFS_FCTL_SPEC1,
                                                   Request.query.dlen, fArg,
                                                   CRED);
                            break;
          case kXR_Qvisa:   qType = "Qvisa";
                            rc = fp->XrdSfsp->fctl(SFS_FCTL_STATV, 0,
                                                   fp->XrdSfsp->error);
                            break;
          default:          return Response.Send(kXR_ArgMissing, 
                                   "Required query argument not present");
         }

// Preform the actual function
//
   TRACEP(FS, "fh=" <<fh.handle <<" query " <<qType <<" rc=" <<rc);

// Return appropriately
//
   if (SFS_OK != rc)
      return fsError(rc, XROOTD_MON_QUERY, fp->XrdSfsp->error, 0, 0);
   return Response.Send();
}
  
/******************************************************************************/
/*                            d o _ Q o p a q u e                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Qopaque(short qopt)
{
   static XrdXrootdCallBack qpqCB("query", XROOTD_MON_QUERY);
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
   XrdSfsFSctl myData;
   const char *Act, *AData;
   char *opaque;
   int fsctl_cmd, rc, dlen = Request.query.dlen;

// Process unstructured as well as structured (path/opaque) requests
//
   if (qopt == kXR_Qopaque)
      {myData.Arg1 = argp->buff; myData.Arg1Len = dlen;
       myData.Arg2 = 0;          myData.Arg2Len = 0;
       fsctl_cmd = SFS_FSCTL_PLUGIO;
       Act = " qopaque '"; AData = "...";
      } else {
       // Check for static routing (this falls under stat)
       //
       STATIC_REDIRECT(RD_stat);

       // Prescreen the path
       //
       if (rpCheck(argp->buff, &opaque)) return rpEmsg("Querying", argp->buff);
       if (!Squash(argp->buff))          return vpEmsg("Querying", argp->buff);

       // Setup arguments
       //
       myData.Arg1    = argp->buff;
       myData.Arg1Len = (opaque ? opaque - argp->buff - 1    : dlen);
       myData.Arg2    = opaque;
       myData.Arg2Len = (opaque ? argp->buff + dlen - opaque : 0);
       fsctl_cmd = SFS_FSCTL_PLUGIN;
       Act = " qopaquf '"; AData = argp->buff;
      }
// The query is elegible for a deferred response, indicate we're ok with that
//
   myError.setErrCB(&qpqCB, ReqID.getID());

// Preform the actual function using the supplied arguments
//
   rc = osFS->FSctl(fsctl_cmd, myData, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<Act <<AData <<"'");
   if (rc == SFS_OK) return Response.Send("");
   return fsError(rc, 0, myError, 0, 0);
}

/******************************************************************************/
/*                             d o _ Q s p a c e                              */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Qspace()
{
   static const int fsctl_cmd = SFS_FSCTL_STATLS;
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
   char *opaque;
   int n, rc;

// Check for static routing
//
   STATIC_REDIRECT(RD_stat);

// Prescreen the path
//
   if (rpCheck(argp->buff, &opaque)) return rpEmsg("Stating", argp->buff);
   if (!Squash(argp->buff))          return vpEmsg("Stating", argp->buff);

// Add back the opaque info
//
   if (opaque)
      {n = strlen(argp->buff); argp->buff[n] = '?';
       if ((argp->buff)+n != opaque-1)
          memmove(&argp->buff[n+1], opaque, strlen(opaque)+1);
      }

// Preform the actual function using the supplied logical FS name
//
   rc = osFS->fsctl(fsctl_cmd, argp->buff, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<" qspace '" <<argp->buff <<"'");
   if (rc == SFS_OK) return Response.Send("");
   return fsError(rc, XROOTD_MON_QUERY, myError, argp->buff, opaque);
}

/******************************************************************************/
/*                              d o _ Q u e r y                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Query()
{
    short qopt = (short)ntohs(Request.query.infotype);

// Perform the appropriate query
//
   switch(qopt)
         {case kXR_QStats: return SI->Stats(Response,
                              (Request.header.dlen ? argp->buff : "a"));
          case kXR_Qcksum:  return do_CKsum(0);
          case kXR_Qckscan: return do_CKsum(1);
          case kXR_Qconfig: return do_Qconf();
          case kXR_Qspace:  return do_Qspace();
          case kXR_Qxattr:  return do_Qxattr();
          case kXR_Qopaque:
          case kXR_Qopaquf: return do_Qopaque(qopt);
          case kXR_Qopaqug: return do_Qfh();
          case kXR_QPrep:   return do_Prepare(true);
          default:          break;
         }

// Whatever we have, it's not valid
//
   return Response.Send(kXR_ArgInvalid, 
                        "Invalid information query type code");
}

/******************************************************************************/
/*                             d o _ Q x a t t r                              */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Qxattr()
{
   static XrdXrootdCallBack statCB("stat", XROOTD_MON_QUERY);
   static const int fsctl_cmd = SFS_FSCTL_STATXA;
   int rc;
   char *opaque;
   XrdOucErrInfo myError(Link->ID,&statCB,ReqID.getID(),Monitor.Did,clientPV);

// Check for static routing
//
   STATIC_REDIRECT(RD_stat);

// Prescreen the path
//
   if (rpCheck(argp->buff, &opaque)) return rpEmsg("Stating", argp->buff);
   if (!Squash(argp->buff))          return vpEmsg("Stating", argp->buff);

// Preform the actual function
//
   rc = osFS->fsctl(fsctl_cmd, argp->buff, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<" qxattr " <<argp->buff);
   return fsError(rc, XROOTD_MON_QUERY, myError, argp->buff, opaque);
}
  
/******************************************************************************/
/*                               d o _ R e a d                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Read()
{
   int pathID, retc;
   XrdXrootdFHandle fh(Request.read.fhandle);
   numReads++;

// We first handle the pre-read list, if any. We do it this way because of
// a historical glitch in the protocol. One should really not piggy back a
// pre-read on top of a read, though it is allowed.
//
   if (!Request.header.dlen) pathID = 0;
      else if (do_ReadNone(retc, pathID)) return retc;

// Unmarshall the data
//
   IO.IOLen  = ntohl(Request.read.rlen);
               n2hll(Request.read.offset, IO.Offset);

// Find the file object
//
   if (!FTab || !(IO.File = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,
                           "read does not refer to an open file");

// Trace and verify read length is not negative
//
   TRACEP(FSIO, pathID <<" fh=" <<fh.handle <<" read " <<IO.IOLen
                       <<'@' <<IO.Offset);
   if ( IO.IOLen < 0) return Response.Send(kXR_ArgInvalid,
                                          "Read length is negative");

// If we are monitoring, insert a read entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_rd(IO.File->Stats.FileID, Request.read.rlen,
                                                  Request.read.offset);

// Short circuit processing if read length is zero
//
   if (!IO.IOLen) return Response.Send();

// There are many competing ways to accomplish a read. Pick the one we
// will use and if possible, do a fast dispatch.
//
        if (IO.File->isMMapped) IO.Mode = XrdXrootd::IOParms::useMMap;
   else if (IO.File->sfEnabled && !isTLS && IO.IOLen >= as_minsfsz
        &&  IO.Offset+IO.IOLen <= IO.File->Stats.fSize)
           IO.Mode = XrdXrootd::IOParms::useSF;
   else if (IO.File->AsyncMode && IO.IOLen >= as_miniosz
        &&  IO.Offset+IO.IOLen <= IO.File->Stats.fSize+as_seghalf
        &&  linkAioReq < as_maxperlnk && srvrAioOps < as_maxpersrv)
           {XrdXrootdProtocol *pP;
            XrdXrootdNormAio  *aioP;

            if (!pathID) pP = this;
               else {if (!(pP = VerifyStream(retc, pathID, false))) return retc;
                     if (pP->linkAioReq >= as_maxperlnk) pP = 0;
                    }
            if (pP && (aioP = XrdXrootdNormAio::Alloc(pP,pP->Response,IO.File)))
               {if (!IO.File->aioFob) IO.File->aioFob = new XrdXrootdAioFob;
                aioP->Read(IO.Offset, IO.IOLen);
                return 0;
               }
            SI->AsyncRej++;
            IO.Mode = XrdXrootd::IOParms::useBasic;
           }
   else IO.Mode = XrdXrootd::IOParms::useBasic;

// See if an alternate path is required, offload the read
//
   if (pathID) return do_Offload(&XrdXrootdProtocol::do_ReadAll, pathID);

// Now read all of the data (do pre-reads first)
//
   return do_ReadAll();
}

/******************************************************************************/
/*                            d o _ R e a d A l l                             */
/******************************************************************************/

// IO.File   = file to be read
// IO.Offset = Offset at which to read
// IO.IOLen  = Number of bytes to read from file and write to socket
  
int XrdXrootdProtocol::do_ReadAll()
{
   int rc, xframt, Quantum = (IO.IOLen > maxBuffsz ? maxBuffsz : IO.IOLen);
   char *buff;

// If this file is memory mapped, short ciruit all the logic and immediately
// transfer the requested data to minimize latency.
//
   if (IO.Mode == XrdXrootd::IOParms::useMMap)
      {if (IO.Offset >= IO.File->Stats.fSize) return Response.Send();
       if (IO.Offset+IO.IOLen <= IO.File->Stats.fSize)
          {IO.File->Stats.rdOps(IO.IOLen);
           return Response.Send(IO.File->mmAddr+IO.Offset, IO.IOLen);
          }
       xframt = IO.File->Stats.fSize -IO.Offset;
       IO.File->Stats.rdOps(xframt);
       return Response.Send(IO.File->mmAddr+IO.Offset, xframt);
      }

// If we are sendfile enabled, then just send the file if possible
//
   if (IO.Mode == XrdXrootd::IOParms::useSF)
      {IO.File->Stats.rdOps(IO.IOLen);
       if (IO.File->fdNum >= 0)
          return Response.Send(IO.File->fdNum, IO.Offset, IO.IOLen);
       rc = IO.File->XrdSfsp->SendData((XrdSfsDio *)this, IO.Offset, IO.IOLen);
       if (rc == SFS_OK)
          {if (!IO.IOLen)    return 0;
           if (IO.IOLen < 0) return -1;  // Otherwise retry using read()
          } else return fsError(rc, 0, IO.File->XrdSfsp->error, 0, 0);
      }

// Make sure we have a large enough buffer
//
   if (!argp || Quantum < halfBSize || Quantum > argp->bsize)
      {if ((rc = getBuff(1, Quantum)) <= 0) return rc;}
      else if (hcNow < hcNext) hcNow++;
   buff = argp->buff;

// Now read all of the data. For statistics, we need to record the orignal
// amount of the request even if we really do not get to read that much!
//
   IO.File->Stats.rdOps(IO.IOLen);
   do {if ((xframt = IO.File->XrdSfsp->read(IO.Offset, buff, Quantum)) <= 0) break;
       if (xframt >= IO.IOLen) return Response.Send(buff, xframt);
       if (Response.Send(kXR_oksofar, buff, xframt) < 0) return -1;
       IO.Offset += xframt; IO.IOLen -= xframt;
       if (IO.IOLen < Quantum) Quantum = IO.IOLen;
      } while(IO.IOLen);

// Determine why we ended here
//
   if (xframt == 0) return Response.Send();
   return fsError(xframt, 0, IO.File->XrdSfsp->error, 0, 0);
}

/******************************************************************************/
/*                           d o _ R e a d N o n e                            */
/******************************************************************************/
  
int XrdXrootdProtocol::do_ReadNone(int &retc, int &pathID)
{
   XrdXrootdFHandle fh;
   int ralsz = Request.header.dlen;
   struct read_args *rargs=(struct read_args *)(argp->buff);
   struct readahead_list *ralsp = (readahead_list *)(rargs+sizeof(read_args));

// Return the pathid
//
   pathID = static_cast<int>(rargs->pathid);
   if ((ralsz -= sizeof(read_args)) <= 0) return 0;

// Make sure that we have a proper pre-read list
//
   if (ralsz%sizeof(readahead_list))
      {Response.Send(kXR_ArgInvalid, "Invalid length for read ahead list");
       return 1;
      }

// Run down the pre-read list
//
   while(ralsz > 0)
        {IO.IOLen  = ntohl(ralsp->rlen);
                    n2hll(ralsp->offset, IO.Offset);
         memcpy((void *)&fh.handle, (const void *)ralsp->fhandle,
                  sizeof(fh.handle));
         TRACEP(FSIO, "fh="<<fh.handle<<" read "<<IO.IOLen<<'@'<<IO.Offset);
         if (!FTab || !(IO.File = FTab->Get(fh.handle)))
            {retc = Response.Send(kXR_FileNotOpen,
                             "preread does not refer to an open file");
             return 1;
            }
         IO.File->XrdSfsp->read(IO.Offset, IO.IOLen);
         ralsz -= sizeof(struct readahead_list);
         ralsp++;
         numReads++;
        };

// All done
//
   return 0;
}

/******************************************************************************/
/*                               d o _ R e a d V                              */
/******************************************************************************/
  
int XrdXrootdProtocol::do_ReadV()
{
// This will read multiple buffers at the same time in an attempt to avoid
// the latency in a network. The information with the offsets and lengths
// of the information to read is passed as a data buffer... then we decode
// it and put all the individual buffers in a single one it's up to the
// client to interpret it. Code originally developed by Leandro Franco, CERN.
// The readv file system code originally added by Brian Bockelman, UNL.
//
   const int hdrSZ = sizeof(readahead_list);
   struct XrdOucIOVec     rdVec[XrdProto::maxRvecsz+1];
   struct readahead_list *raVec, respHdr;
   long long totSZ;
   XrdSfsXferSize rdVAmt, rdVXfr, xfrSZ = 0;
   int rdVBeg, rdVBreak, rdVNow, rdVNum, rdVecNum;
   int currFH, i, k, Quantum, Qleft, rdVecLen = Request.header.dlen;
   int rvMon = Monitor.InOut();
   int ioMon = (rvMon > 1);
   char *buffp, vType = (ioMon ? XROOTD_MON_READU : XROOTD_MON_READV);

// Compute number of elements in the read vector and make sure we have no
// partial elements.
//
   rdVecNum = rdVecLen / sizeof(readahead_list);
   if ( (rdVecLen <= 0) || (rdVecNum*hdrSZ != rdVecLen) )
      return Response.Send(kXR_ArgInvalid, "Read vector is invalid");

// Make sure that we can copy the read vector to our local stack. We must impose 
// a limit on it's size. We do this to be able to reuse the data buffer to 
// prevent cross-cpu memory cache synchronization.
//
   if (rdVecNum > XrdProto::maxRvecsz)
      return Response.Send(kXR_ArgTooLong, "Read vector is too long");

// So, now we account for the number of readv requests and total segments
//
   numReadV++; numSegsV += rdVecNum;

// Run down the list and compute the total size of the read. No individual
// read may be greater than the maximum transfer size. We also use this loop
// to copy the read ahead list to our readv vector for later processing.
//
   raVec = (readahead_list *)argp->buff;
   totSZ = rdVecLen; Quantum = maxTransz - hdrSZ;
   for (i = 0; i < rdVecNum; i++) 
       {totSZ += (rdVec[i].size = ntohl(raVec[i].rlen));
        if (rdVec[i].size < 0)       return Response.Send(kXR_ArgInvalid,
                                           "Readv length is negative");
        if (rdVec[i].size > Quantum) return Response.Send(kXR_NoMemory,
                                           "Single readv transfer is too large");
        rdVec[i].offset = ntohll(raVec[i].offset);
        memcpy(&rdVec[i].info, raVec[i].fhandle, sizeof(int));
       }

// Now add an extra dummy element to force flushing of the read vector.
//
   rdVec[i].offset = -1;
   rdVec[i].size   =  0;
   rdVec[i].info   = -1;
   rdVBreak = rdVecNum;
   rdVecNum++;

// We limit the total size of the read to be 2GB for convenience
//
   if (totSZ > 0x7fffffffLL)
      return Response.Send(kXR_NoMemory, "Total readv transfer is too large");

// Calculate the transfer unit which will be the smaller of the maximum
// transfer unit and the actual amount we need to transfer.
//
   if ((Quantum = static_cast<int>(totSZ)) > maxTransz) Quantum = maxTransz;
   
// Now obtain the right size buffer
//
   if ((Quantum < halfBSize && Quantum > 1024) || Quantum > argp->bsize)
      {if ((k = getBuff(1, Quantum)) <= 0) return k;}
      else if (hcNow < hcNext) hcNow++;

// Check that we really have at least one file open. This needs to be done 
// only once as this code runs in the control thread.
//
   if (!FTab) return Response.Send(kXR_FileNotOpen,
                              "readv does not refer to an open file");

// Preset the previous and current file handle to be the handle of the first
// element and make sure the file is actually open.
//
   currFH = rdVec[0].info;
   memcpy(respHdr.fhandle, &currFH, sizeof(respHdr.fhandle));
   if (!(IO.File = FTab->Get(currFH))) return Response.Send(kXR_FileNotOpen,
                                      "readv does not refer to an open file");

// Setup variables for running through the list.
//
   Qleft = Quantum; buffp = argp->buff; rvSeq++;
   rdVBeg = rdVNow = 0; rdVXfr = rdVAmt = 0;

// Now run through the elements
//
   for (i = 0; i < rdVecNum; i++)
       {if (rdVec[i].info != currFH)
           {xfrSZ = IO.File->XrdSfsp->readv(&rdVec[rdVNow], i-rdVNow);
            if (xfrSZ != rdVAmt) break;
            rdVNum = i - rdVBeg; rdVXfr += rdVAmt;
            IO.File->Stats.rvOps(rdVXfr, rdVNum);
            if (rvMon)
               {Monitor.Agent->Add_rv(IO.File->Stats.FileID, htonl(rdVXfr),
                                              htons(rdVNum), rvSeq, vType);
                if (ioMon) for (k = rdVBeg; k < i; k++)
                    Monitor.Agent->Add_rd(IO.File->Stats.FileID,
                            htonl(rdVec[k].size), htonll(rdVec[k].offset));
               }
            rdVXfr = rdVAmt = 0;
            if (i == rdVBreak) break;
            rdVBeg = rdVNow = i; currFH = rdVec[i].info;
            memcpy(respHdr.fhandle, &currFH, sizeof(respHdr.fhandle));
            if (!(IO.File = FTab->Get(currFH)))
               return Response.Send(kXR_FileNotOpen,
                                    "readv does not refer to an open file");
            }

        if (Qleft < (rdVec[i].size + hdrSZ))
           {if (rdVAmt)
               {xfrSZ = IO.File->XrdSfsp->readv(&rdVec[rdVNow], i-rdVNow);
                if (xfrSZ != rdVAmt) break;
               }
            if (Response.Send(kXR_oksofar,argp->buff,Quantum-Qleft) < 0)
               return -1;
            Qleft = Quantum;
            buffp = argp->buff;
            rdVNow = i; rdVXfr += rdVAmt; rdVAmt = 0;
           }

        xfrSZ = rdVec[i].size; rdVAmt += xfrSZ;
        respHdr.rlen   = htonl(xfrSZ);
        respHdr.offset = htonll(rdVec[i].offset);
        memcpy(buffp, &respHdr, hdrSZ);
        rdVec[i].data = buffp + hdrSZ;
        buffp += (xfrSZ+hdrSZ); Qleft -= (xfrSZ+hdrSZ);
        TRACEP(FSIO,"fh=" <<currFH<<" readV "<< xfrSZ <<'@'<<rdVec[i].offset);
       }

// Check if we have an error here. This is indicated when rdVAmt is not zero.
//
   if (rdVAmt)
      {if (xfrSZ >= 0)
          {xfrSZ = SFS_ERROR;
           IO.File->XrdSfsp->error.setErrInfo(-ENODATA,"readv past EOF");
          }
       return fsError(xfrSZ, 0, IO.File->XrdSfsp->error, 0, 0);
      }

// All done, return result of the last segment or just zero
//
   return (Quantum != Qleft ? Response.Send(argp->buff, Quantum-Qleft) : 0);
}

/******************************************************************************/
/*                                 d o _ R m                                  */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Rm()
{
   int rc;
   char *opaque;
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);

// Check for static routing
//
   STATIC_REDIRECT(RD_rm);

// Prescreen the path
//
   if (rpCheck(argp->buff, &opaque)) return rpEmsg("Removing", argp->buff);
   if (!Squash(argp->buff))          return vpEmsg("Removing", argp->buff);

// Preform the actual function
//
   rc = osFS->rem(argp->buff, myError, CRED, opaque);
   TRACEP(FS, "rc=" <<rc <<" rm " <<argp->buff);
   if (SFS_OK == rc) return Response.Send();

// An error occurred
//
   return fsError(rc, XROOTD_MON_RM, myError, argp->buff, opaque);
}

/******************************************************************************/
/*                              d o _ R m d i r                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Rmdir()
{
   int rc;
   char *opaque;
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);

// Check for static routing
//
   STATIC_REDIRECT(RD_rmdir);

// Prescreen the path
//
   if (rpCheck(argp->buff, &opaque)) return rpEmsg("Removing", argp->buff);
   if (!Squash(argp->buff))          return vpEmsg("Removing", argp->buff);

// Preform the actual function
//
   rc = osFS->remdir(argp->buff, myError, CRED, opaque);
   TRACEP(FS, "rc=" <<rc <<" rmdir " <<argp->buff);
   if (SFS_OK == rc) return Response.Send();

// An error occurred
//
   return fsError(rc, XROOTD_MON_RMDIR, myError, argp->buff, opaque);
}

/******************************************************************************/
/*                                d o _ S e t                                 */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Set()
{
   XrdOucTokenizer setargs(argp->buff);
   char *val, *rest;

// Get the first argument
//
   if (!setargs.GetLine() || !(val = setargs.GetToken(&rest)))
      return Response.Send(kXR_ArgMissing, "set argument not specified.");

// Trace this set
//
   TRACEP(DEBUG, "set " <<val <<' ' <<rest);

// Now determine what the user wants to set
//
        if (!strcmp("appid", val))
           {while(*rest && *rest == ' ') rest++;
            eDest.Emsg("Xeq", Link->ID, "appid", rest);
            return Response.Send();
           }
   else if (!strcmp("monitor", val)) return do_Set_Mon(setargs);

// All done
//
   return Response.Send(kXR_ArgInvalid, "invalid set parameter");
}

/******************************************************************************/
/*                            d o _ S e t _ M o n                             */
/******************************************************************************/

// Process: set monitor {off | on} {[appid] | info [info]}

int XrdXrootdProtocol::do_Set_Mon(XrdOucTokenizer &setargs)
{
  char *val, *appid;
  kXR_unt32 myseq = 0;

// Get the first argument
//
   if (!(val = setargs.GetToken(&appid)))
      return Response.Send(kXR_ArgMissing,"set monitor argument not specified.");

// For info requests, nothing changes. However, info events must have been
// enabled for us to record them. Route the information via the static
// monitor entry, since it knows how to forward the information.
//
   if (!strcmp(val, "info"))
      {if (appid && Monitor.Info())
          {while(*appid && *appid == ' ') appid++;
           if (strlen(appid) > 1024) appid[1024] = '\0';
           if (*appid) myseq = Monitor.MapInfo(appid);
          }
       return Response.Send((void *)&myseq, sizeof(myseq));
      }

// Determine if on do appropriate processing
//
   if (!strcmp(val, "on"))
      {Monitor.Enable();
       if (appid && Monitor.InOut())
          {while(*appid && *appid == ' ') appid++;
           if (*appid) Monitor.Agent->appID(appid);
          }
       if (!Monitor.Did && Monitor.Logins()) MonAuth();
       return Response.Send();
      }

// Determine if off and do appropriate processing
//
   if (!strcmp(val, "off"))
      {if (appid && Monitor.InOut())
          {while(*appid && *appid == ' ') appid++;
           if (*appid) Monitor.Agent->appID(appid);
          }
       Monitor.Disable();
       return Response.Send();
      }

// Improper request
//
   return Response.Send(kXR_ArgInvalid, "invalid set monitor argument");
}
  
/******************************************************************************/
/*                               d o _ S t a t                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Stat()
{
   static XrdXrootdCallBack statCB("stat", XROOTD_MON_STAT);
   static const int fsctl_cmd = SFS_FSCTL_STATFS;
   bool doDig;
   int rc;
   char *opaque, xxBuff[1024];
   struct stat buf;
   XrdOucErrInfo myError(Link->ID,&statCB,ReqID.getID(),Monitor.Did,clientPV);

// Update misc stats count
//
   SI->Bump(SI->miscCnt);

// The stat request may refer to an open file handle. So, screen this out.
//
   if (!argp || !Request.header.dlen)
      {XrdXrootdFile *fp;
       XrdXrootdFHandle fh(Request.stat.fhandle);
       if (Request.stat.options & kXR_vfs)
          {Response.Send(kXR_ArgMissing, "Required argument not present");
           return 0;
          }
       if (!FTab || !(fp = FTab->Get(fh.handle)))
          return Response.Send(kXR_FileNotOpen,
                              "stat does not refer to an open file");
       rc = fp->XrdSfsp->stat(&buf);
       TRACEP(FS, "fh=" <<fh.handle <<" stat rc=" <<rc);
       if (SFS_OK == rc) return Response.Send(xxBuff,
                                StatGen(buf,xxBuff,sizeof(xxBuff)));
       return fsError(rc, 0, fp->XrdSfsp->error, 0, 0);
      }

// Check if we are handling a dig type path
//
   doDig = (digFS && SFS_LCLROOT(argp->buff));

// Check for static routing
//
   if (!doDig) {STATIC_REDIRECT(RD_stat);}

// Prescreen the path
//
   if (rpCheck(argp->buff, &opaque)) return rpEmsg("Stating", argp->buff);
   if (!doDig && !Squash(argp->buff))return vpEmsg("Stating", argp->buff);

// Preform the actual function
//
   if (Request.stat.options & kXR_vfs)
      {rc = osFS->fsctl(fsctl_cmd, argp->buff, myError, CRED);
       TRACEP(FS, "rc=" <<rc <<" statfs " <<argp->buff);
       if (rc == SFS_OK) Response.Send("");
      } else {
       if (doDig) rc = digFS->stat(argp->buff, &buf, myError, CRED, opaque);
          else    rc =  osFS->stat(argp->buff, &buf, myError, CRED, opaque);
       TRACEP(FS, "rc=" <<rc <<" stat " <<argp->buff);
       if (rc == SFS_OK) return Response.Send(xxBuff,
                                StatGen(buf,xxBuff,sizeof(xxBuff)));
      }
   return fsError(rc, (doDig ? 0 : XROOTD_MON_STAT),myError,argp->buff,opaque);
}

/******************************************************************************/
/*                              d o _ S t a t x                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Statx()
{
   static XrdXrootdCallBack statxCB("xstat", XROOTD_MON_STAT);
   int rc;
   char *path, *opaque, *respinfo = argp->buff;
   mode_t mode;
   XrdOucErrInfo myError(Link->ID,&statxCB,ReqID.getID(),Monitor.Did,clientPV);
   XrdOucTokenizer pathlist(argp->buff);

// Check for static routing
//
   STATIC_REDIRECT(RD_stat);

// Cycle through all of the paths in the list
//
   while((path = pathlist.GetLine()))
        {if (rpCheck(path, &opaque)) return rpEmsg("Stating", path);
         if (!Squash(path))          return vpEmsg("Stating", path);
         rc = osFS->stat(path, mode, myError, CRED, opaque);
         TRACEP(FS, "rc=" <<rc <<" stat " <<path);
         if (rc != SFS_OK)
            return fsError(rc, XROOTD_MON_STAT, myError, path, opaque);
            else {if (mode == (mode_t)-1)    *respinfo = (char)kXR_offline;
                     else if (S_ISDIR(mode)) *respinfo = (char)kXR_isDir;
                             else            *respinfo = (char)kXR_file;
                 }
         respinfo++;
        }

// Return result
//
   return Response.Send(argp->buff, respinfo-argp->buff);
}

/******************************************************************************/
/*                               d o _ S y n c                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Sync()
{
   static XrdXrootdCallBack syncCB("sync", 0);
   int rc;
   XrdXrootdFile *fp;
   XrdXrootdFHandle fh(Request.sync.fhandle);

// Keep Statistics
//
   SI->Bump(SI->syncCnt);

// Find the file object
//
   if (!FTab || !(fp = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,"sync does not refer to an open file");

// The sync is elegible for a deferred response, indicate we're ok with that
//
   fp->XrdSfsp->error.setErrCB(&syncCB, ReqID.getID());

// Sync the file
//
   rc = fp->XrdSfsp->sync();
   TRACEP(FS, "fh=" <<fh.handle <<" sync rc=" <<rc);
   if (SFS_OK != rc) return fsError(rc, 0, fp->XrdSfsp->error, 0, 0);

// Respond that all went well
//
   return Response.Send();
}

/******************************************************************************/
/*                           d o _ T r u n c a t e                            */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Truncate()
{
   static XrdXrootdCallBack truncCB("trunc", 0);
   XrdXrootdFile *fp;
   XrdXrootdFHandle fh(Request.truncate.fhandle);
   long long theOffset;
   int rc;

// Unmarshall the data
//
   n2hll(Request.truncate.offset, theOffset);

// Check if this is a truncate for an open file (no path given)
//
   if (!Request.header.dlen)
      {
       // Update misc stats count
       //
          SI->Bump(SI->miscCnt);

      // Find the file object
      //
         if (!FTab || !(fp = FTab->Get(fh.handle)))
            return Response.Send(kXR_FileNotOpen,
                                     "trunc does not refer to an open file");

     // Truncate the file (it is eligible for async callbacks)
     //
        fp->XrdSfsp->error.setErrCB(&truncCB, ReqID.getID());
        rc = fp->XrdSfsp->truncate(theOffset);
        TRACEP(FS, "fh=" <<fh.handle <<" trunc rc=" <<rc <<" sz=" <<theOffset);
        if (SFS_OK != rc) return fsError(rc, 0, fp->XrdSfsp->error, 0, 0);

   } else {

       XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
       char *opaque;

    // Check for static routing
    //
       STATIC_REDIRECT(RD_trunc);

    // Verify the path and extract out the opaque information
    //
       if (rpCheck(argp->buff,&opaque)) return rpEmsg("Truncating",argp->buff);
       if (!Squash(argp->buff))         return vpEmsg("Truncating",argp->buff);

    // Preform the actual function
    //
       rc = osFS->truncate(argp->buff, (XrdSfsFileOffset)theOffset, myError,
                           CRED, opaque);
       TRACEP(FS, "rc=" <<rc <<" trunc " <<theOffset <<' ' <<argp->buff);
       if (SFS_OK != rc)
          return fsError(rc, XROOTD_MON_TRUNC, myError, argp->buff, opaque);
   }

// Respond that all went well
//
   return Response.Send();
}
  
/******************************************************************************/
/*                              d o _ W r i t e                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Write()
{
   int pathID;
   XrdXrootdFHandle fh(Request.write.fhandle);
   numWrites++;

// Unmarshall the data
//
   IO.IOLen  = Request.header.dlen;
              n2hll(Request.write.offset, IO.Offset);
   pathID   = static_cast<int>(Request.write.pathid);

// Find the file object. We will drain socket data on the control path only!
//                                                                             .
   if (!FTab || !(IO.File = FTab->Get(fh.handle)))
      {IO.File = 0;
       return do_WriteNone(pathID);
      }

// Trace and verify that length is not negative
//
   TRACEP(FSIO, pathID<<" fh="<<fh.handle<<" write "<<IO.IOLen<<'@'<<IO.Offset);
   if ( IO.IOLen < 0) return Response.Send(kXR_ArgInvalid,
                                          "Write length is negative");

// If we are monitoring, insert a write entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_wr(IO.File->Stats.FileID, Request.write.dlen,
                                                  Request.write.offset);

// If zero length write, simply return
//
   if (!IO.IOLen) return Response.Send();
   IO.File->Stats.wrOps(IO.IOLen); // Optimistically correct

// If async write allowed and it is a true write request (e.g. not chkpoint) and
// current conditions permit async; schedule the write to occur asynchronously
//
   if (IO.File->AsyncMode && Request.header.requestid == kXR_write
   &&  !as_syncw && IO.IOLen >= as_miniosz && srvrAioOps < as_maxpersrv)
      {if (myStalls < as_maxstalls)
          {if (pathID) return do_Offload(&XrdXrootdProtocol::do_WriteAio,pathID);
           return do_WriteAio();
          }
       SI->AsyncRej++;
       myStalls--;
      }

// See if an alternate path is required
//
   if (pathID) return do_Offload(&XrdXrootdProtocol::do_WriteAll, pathID);

// Just to the i/o now
//
   return do_WriteAll();
}
  
/******************************************************************************/
/*                           d o _ W r i t e A i o                            */
/******************************************************************************/

// IO.File   = file to be written
// IO.Offset = Offset at which to write
// IO.IOLen  = Number of bytes to read from socket and write to file
  
int XrdXrootdProtocol::do_WriteAio()
{
   XrdXrootdNormAio *aioP;

// Allocate an aio request object if client hasn't exceeded the link limit
//
   if (linkAioReq >= as_maxperlnk
   ||  !(aioP = XrdXrootdNormAio::Alloc(this, Response, IO.File)))
      {SI->AsyncRej++;
       if (myStalls > 0) myStalls--;
       return do_WriteAll();
      }

// Issue the write request
//
   return aioP->Write(IO.Offset, IO.IOLen);
}

/******************************************************************************/
/*                           d o _ W r i t e A l l                            */
/******************************************************************************/

// IO.File   = file to be written
// IO.Offset = Offset at which to write
// IO.IOLen  = Number of bytes to read from socket and write to file
  
int XrdXrootdProtocol::do_WriteAll()
{
   int rc, Quantum = (IO.IOLen > maxBuffsz ? maxBuffsz : IO.IOLen);

// Make sure we have a large enough buffer
//
   if (!argp || Quantum < halfBSize || Quantum > argp->bsize)
      {if ((rc = getBuff(0, Quantum)) <= 0) return rc;}
      else if (hcNow < hcNext) hcNow++;

// Now write all of the data (XrdXrootdProtocol.C defines getData())
//
   while(IO.IOLen > 0)
        {if ((rc = getData("data", argp->buff, Quantum)))
            {if (rc > 0) 
                {Resume = &XrdXrootdProtocol::do_WriteCont;
                 myBlast = Quantum;
                }
             return rc;
            }
         if ((rc = IO.File->XrdSfsp->write(IO.Offset, argp->buff, Quantum)) < 0)
            {IO.IOLen  = IO.IOLen-Quantum; IO.EInfo[0] = rc; IO.EInfo[1] = 0;
             return do_WriteNone();
            }
         IO.Offset += Quantum; IO.IOLen -= Quantum;
         if (IO.IOLen < Quantum) Quantum = IO.IOLen;
        }

// All done
//
   return Response.Send();
}

/******************************************************************************/
/*                          d o _ W r i t e C o n t                           */
/******************************************************************************/

// IO.File   = file to be written
// IO.Offset = Offset at which to write
// IO.IOLen  = Number of bytes to read from socket and write to file
// myBlast  = Number of bytes already read from the socket
  
int XrdXrootdProtocol::do_WriteCont()
{
   int rc;

// Write data that was finaly finished comming in
//
   if ((rc = IO.File->XrdSfsp->write(IO.Offset, argp->buff, myBlast)) < 0)
      {IO.IOLen  = IO.IOLen-myBlast; IO.EInfo[0] = rc; IO.EInfo[1] = 0;
       return do_WriteNone();
      }
    IO.Offset += myBlast; IO.IOLen -= myBlast;

// See if we need to finish this request in the normal way
//
   if (IO.IOLen > 0) return do_WriteAll();
   return Response.Send();
}
  
/******************************************************************************/
/*                          d o _ W r i t e N o n e                           */
/******************************************************************************/
  
int XrdXrootdProtocol::do_WriteNone()
{
   char *buff, dbuff[4096];
   int rlen, blen;

// Determine which buffer we will use
//
   if (argp && argp->bsize > (int)sizeof(dbuff))
      {buff = argp->buff;
       blen = argp->bsize;
      } else {
       buff = dbuff;
       blen = sizeof(dbuff);
      }
    if (IO.IOLen < blen) blen = IO.IOLen;

// Discard any data being transmitted
//
   TRACEP(REQ, "discarding " <<IO.IOLen <<" bytes");
   while(IO.IOLen > 0)
        {rlen = Link->Recv(buff, blen, readWait);
         if (rlen  < 0) return Link->setEtext("link read error");
         IO.IOLen -= rlen;
         if (rlen < blen) 
            {myBlen   = 0;
             Resume   = &XrdXrootdProtocol::do_WriteNone;
             return 1;
            }
         if (IO.IOLen < blen) blen = IO.IOLen;
        }

// Send final message
//
   return do_WriteNoneMsg();
}

/******************************************************************************/

int XrdXrootdProtocol::do_WriteNone(int pathID, XErrorCode ec,
                                    const char *emsg)
{
// We can't recover when the data is arriving on a foriegn bound path as there
// no way to properly drain the socket. So, we terminate the connection.
//
   if (pathID != PathID)
      {if (ec && emsg) Response.Send(ec, emsg);
          else do_WriteNoneMsg();
       return  Link->setEtext("write protocol violation");
      }

// Set error code if present
//
   if (ec != kXR_noErrorYet)
      {IO.EInfo[1] = ec;
       if (IO.File)
          {if (!emsg) emsg = XProtocol::errName(ec);
           IO.File->XrdSfsp->error.setErrInfo(0, emsg);
          }
      }

// Otherwise, continue to darin the socket
//
   return do_WriteNone();
}

/******************************************************************************/
/*                       d o _ W r i t e N o n e M s g                        */
/******************************************************************************/
  
int XrdXrootdProtocol::do_WriteNoneMsg()
{
// Send our the error message and return
//
   if (!IO.File) return
      Response.Send(kXR_FileNotOpen,"write does not refer to an open file");

   if (IO.EInfo[1])
      return Response.Send((XErrorCode)IO.EInfo[1],
                           IO.File->XrdSfsp->error.getErrText());

   if (IO.EInfo[0]) return fsError(IO.EInfo[0], 0, IO.File->XrdSfsp->error, 0, 0);

   return Response.Send(kXR_FSError, IO.File->XrdSfsp->error.getErrText());
}
  
/******************************************************************************/
/*                          d o _ W r i t e S p a n                           */
/******************************************************************************/
  
int XrdXrootdProtocol::do_WriteSpan()
{
   int rc;
   XrdXrootdFHandle fh(Request.write.fhandle);
   numWrites++;

// Unmarshall the data
//
   IO.IOLen  = Request.header.dlen;
              n2hll(Request.write.offset, IO.Offset);

// Find the file object. We will only drain socket data on the control path.
//                                                                             .
   if (!FTab || !(IO.File = FTab->Get(fh.handle)))
      {IO.IOLen -= myBlast;
       IO.File = 0;
       return do_WriteNone(Request.write.pathid);
      }

// If we are monitoring, insert a write entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_wr(IO.File->Stats.FileID, Request.write.dlen,
                                                  Request.write.offset);

// Trace this entry
//
   TRACEP(FSIO, "fh=" <<fh.handle <<" write " <<IO.IOLen <<'@' <<IO.Offset);

// Write data that was already read
//
   if ((rc = IO.File->XrdSfsp->write(IO.Offset, myBuff, myBlast)) < 0)
      {IO.IOLen  = IO.IOLen-myBlast; IO.EInfo[0] = rc; IO.EInfo[1] = 0;
       return do_WriteNone();
      }
    IO.Offset += myBlast; IO.IOLen -= myBlast;

// See if we need to finish this request in the normal way
//
   if (IO.IOLen > 0) return do_WriteAll();
   return Response.Send();
}
  
/******************************************************************************/
/*                             d o _ W r i t e V                              */
/******************************************************************************/
  
int XrdXrootdProtocol::do_WriteV()
{
// This will write multiple buffers at the same time in an attempt to avoid
// the disk latency. The information with the offsets and lengths of teh data
// to write is passed as a data buffer. We attempt to optimize as best as
// possible, though certain combinations may result in multiple writes. Since
// socket flushing is nearly impossible when an error occurs, most errors
// simply terminate the connection.
//
   const int wveSZ = sizeof(XrdProto::write_list);
   struct trackInfo
         {XrdXrootdWVInfo **wvInfo; bool doit;
          trackInfo(XrdXrootdWVInfo **wvP) : wvInfo(wvP), doit(true) {}
         ~trackInfo() {if (doit && *wvInfo) {free(*wvInfo); *wvInfo = 0;}}
         } freeInfo(&wvInfo);

   struct XrdProto::write_list *wrLst;
   XrdOucIOVec *wrVec;
   long long totSZ, maxSZ;
   int curFH, k, Quantum, wrVecNum, wrVecLen = Request.header.dlen;

// Compute number of elements in the write vector and make sure we have no
// partial elements.
//
   wrVecNum = wrVecLen / wveSZ;
   if ( (wrVecLen <= 0) || (wrVecNum*wveSZ != wrVecLen) )
      {Response.Send(kXR_ArgInvalid, "Write vector is invalid");
       return -1;
      }

// Make sure that we can make a copy of the read vector. So, we impose a limit
// on it's size.
//
   if (wrVecNum > XrdProto::maxWvecsz)
      {Response.Send(kXR_ArgTooLong, "Write vector is too long");
       return -1;
      }

// Create the verctor write information structure sized as needed.
//
   if (wvInfo) free(wvInfo);
   wvInfo = (XrdXrootdWVInfo *)malloc(sizeof(XrdXrootdWVInfo) +
                                      sizeof(XrdOucIOVec)*(wrVecNum-1));
   memset(wvInfo, 0, sizeof(XrdXrootdWVInfo) - sizeof(XrdOucIOVec));
   wvInfo->wrVec = wrVec = wvInfo->ioVec;

// Run down the list and compute the total size of the write. No individual
// write may be greater than the maximum transfer size. We also use this loop
// to copy the write list to our writev vector for later processing.
//
   wrLst = (XrdProto::write_list *)argp->buff;
   totSZ = 0; maxSZ = 0; k = 0; Quantum = maxTransz; curFH = 0;
   for (int i = 0; i < wrVecNum; i++)
       {if (wrLst[i].wlen == 0) continue;
        memcpy(&wrVec[k].info, wrLst[i].fhandle, sizeof(int));
        wrVec[k].size = ntohl(wrLst[i].wlen);
        if (wrVec[k].size < 0)
           {Response.Send(kXR_ArgInvalid, "Writev length is negtive");
            return -1;
           }
        if (wrVec[k].size > Quantum)
           {Response.Send(kXR_NoMemory,"Single writev transfer is too large");
            return -1;
           }
        wrVec[k].offset = ntohll(wrLst[i].offset);
        if (wrVec[k].info == curFH) totSZ += wrVec[k].size;
           else {if (maxSZ < totSZ) maxSZ = totSZ;
                 totSZ = wrVec[k].size;
                }
        k++;
       }

// Check if we are not actually writing anything, simply return success
//
   if (maxSZ < totSZ) maxSZ = totSZ;
   if (maxSZ == 0) return Response.Send();

// So, now we account for the number of writev requests and total segments
//
   numWritV++; numSegsW += k; wrVecNum = k;

// Calculate the transfer unit which will be the smaller of the maximum
// transfer unit and the actual amount we need to transfer.
//
   if (maxSZ > maxTransz) Quantum = maxTransz;
      else Quantum = static_cast<int>(maxSZ);
   
// Now obtain the right size buffer
//
   if ((Quantum < halfBSize && Quantum > 1024) || Quantum > argp->bsize)
      {if (getBuff(0, Quantum) <= 0) return -1;}
      else if (hcNow < hcNext) hcNow++;

// Check that we really have at least the first file open (part of setup)
//
   if (!FTab || !(IO.File = FTab->Get(wrVec[0].info)))
      {Response.Send(kXR_FileNotOpen, "writev does not refer to an open file");
       return -1;
      }

// Setup to do the complete transfer
//
   wvInfo->curFH = wrVec[0].info;
   wvInfo->vBeg  = 0;
   wvInfo->vPos  = 0;
   wvInfo->vEnd  = wrVecNum;
   wvInfo->vMon  = 0;
   wvInfo->doSync= (Request.writev.options & ClientWriteVRequest::doSync) != 0;
   wvInfo->wvMon = Monitor.InOut();
   wvInfo->ioMon = (wvInfo->vMon > 1);
// wvInfo->vType = (wvInfo->ioMon ? XROOTD_MON_WRITEU : XROOTD_MON_WRITEV);
   IO.WVBytes     = 0;
   IO.IOLen       = wrVec[0].size;
   myBuff        = argp->buff;
   myBlast       = 0;

// Now we simply start the write operations if this is a true writev request.
// Otherwise return to the caller for additional processing.
//
   freeInfo.doit = false;
   if (Request.header.requestid == kXR_writev) return do_WriteVec();
   return 0;
}

/******************************************************************************/
/*                           d o _ W r i t e V e c                            */
/******************************************************************************/

int XrdXrootdProtocol::do_WriteVec()
{
   XrdSfsXferSize xfrSZ;
   int rc, wrVNum, vNow = wvInfo->vPos;
   bool done, newfile;

// Read the complete data from the socket for the current element. Note that
// should we enter a resume state; upon re-entry all of the data will be read.
//
do{if (IO.IOLen > 0)
      {wvInfo->wrVec[vNow].data = argp->buff + myBlast;
       myBlast += IO.IOLen;
       if ((rc = getData("data", myBuff, IO.IOLen)))
          {if (rc < 0) return rc;
           IO.IOLen  = 0;
           Resume = &XrdXrootdProtocol::do_WriteVec;
           return rc;
          }
      }

// Establish the state at this point as this will tell us what to do next.
//
   vNow++;
   done = newfile = false;
        if (vNow >= wvInfo->vEnd) done = true;
   else if (wvInfo->wrVec[vNow].info != wvInfo->curFH) newfile = true;
   else if (myBlast + wvInfo->wrVec[vNow].size <= argp->bsize)
           {IO.IOLen = wvInfo->wrVec[vNow].size;
            myBuff  = argp->buff + myBlast;
            wvInfo->vPos = vNow;
            continue;
           }

// We need to write out what we have.
//
   wrVNum = vNow - wvInfo->vBeg;
   xfrSZ = IO.File->XrdSfsp->writev(&(wvInfo->wrVec[wvInfo->vBeg]), wrVNum);
   TRACEP(FSIO,"fh=" <<wvInfo->curFH <<" writeV " << xfrSZ <<':' <<wrVNum);
   if (xfrSZ != myBlast) break;

// Check if we need to do monitoring or a sync with no deferal. Note that
// we currently do not support detailed monitoring for vector writes!
//
   if (done || newfile)
      {int monVnum = vNow - wvInfo->vMon;
       IO.File->Stats.wvOps(IO.WVBytes, monVnum);
/*!!   if (wvMon)
          {Monitor.Agent->Add_wv(IO.File->Stats.FileID, htonl(IO.WVBytes),
                                 htons(monVNum), wvSeq++, wvInfo->vType);
           if (ioMon) for (int k = wvInfo->vMon; k < vNow; k++)
              Monitor.Agent->Add_wr(IO.File->Stats.FileID,
                                    htonl(wvInfo->wrVec[k].size),
                                    htonll(wvInfo->wrVec[k].offset));
          }
*/
       wvInfo->vMon = vNow;
       IO.WVBytes = 0;
       if (wvInfo->doSync)
          {IO.File->XrdSfsp->error.setErrCB(0,0);
           xfrSZ = IO.File->XrdSfsp->sync();
           if (xfrSZ< 0) break;
          }
      }

// If we are done, the finish up
//
   if (done)
      {if (wvInfo) {free(wvInfo); wvInfo = 0;}
       return Response.Send();
      }

// Sequence to a new file if we need to do so
//
   if (newfile)
      {if (!FTab || !(IO.File = FTab->Get(wvInfo->wrVec[vNow].info)))
          {Response.Send(kXR_FileNotOpen,"writev does not refer to an open file");
           return -1;
          }
       wvInfo->curFH = wvInfo->wrVec[vNow].info;
      }

// Setup to resume transfer
//
   myBlast = 0;
   myBuff  = argp->buff;
   IO.IOLen = wvInfo->wrVec[vNow].size;
   wvInfo->vBeg = vNow;
   wvInfo->vPos = vNow;

} while(true);

// If we got here then there was a write error (file pointer is valid).
//
   if (wvInfo) {free(wvInfo); wvInfo = 0;}
   return fsError((int)xfrSZ, 0, IO.File->XrdSfsp->error, 0, 0);
}
  
/******************************************************************************/
/*                              S e n d F i l e                               */
/******************************************************************************/

int XrdXrootdProtocol::SendFile(int fildes)
{

// Make sure we have some data to send
//
   if (!IO.IOLen) return 1;

// Send off the data
//
   IO.IOLen = Response.Send(fildes, IO.Offset, IO.IOLen);
   return IO.IOLen;
}

/******************************************************************************/

int XrdXrootdProtocol::SendFile(XrdOucSFVec *sfvec, int sfvnum)
{
   int i, xframt = 0;

// Make sure we have some data to send
//
   if (!IO.IOLen) return 1;

// Verify the length, it can't be greater than what the client wants
//
   for (i = 1; i < sfvnum; i++) xframt += sfvec[i].sendsz;
   if (xframt > IO.IOLen) return 1;

// Send off the data
//
   if (xframt) IO.IOLen = Response.Send(sfvec, sfvnum, xframt);
      else {IO.IOLen = 0; Response.Send();}
   return IO.IOLen;
}

/******************************************************************************/
/*                                 S e t F D                                  */
/******************************************************************************/
  
void XrdXrootdProtocol::SetFD(int fildes)
{
   if (fildes < 0) IO.File->sfEnabled = 0;
      else IO.File->fdNum = fildes;
}

/******************************************************************************/
/*                       U t i l i t y   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                               f s E r r o r                                */
/******************************************************************************/
  
int XrdXrootdProtocol::fsError(int rc, char opC, XrdOucErrInfo &myError,
                               const char *Path, char *Cgi)
{
   int ecode, popt, rs;
   const char *eMsg = myError.getErrText(ecode);

// Process standard errors
//
   if (rc == SFS_ERROR)
      {SI->errorCnt++;
       rc = XProtocol::mapError(ecode);

       if (Path && (rc == kXR_Overloaded) && (opC == XROOTD_MON_OPENR
                || opC == XROOTD_MON_OPENW || opC == XROOTD_MON_OPENC))
          {if (myError.extData()) myError.Reset();
           return fsOvrld(opC, Path, Cgi);
          }

       if (Path && (rc == kXR_NotFound) && RQLxist && opC
       &&  (popt = RQList.Validate(Path)))
          {if (XrdXrootdMonitor::Redirect())
               XrdXrootdMonitor::Redirect(Monitor.Did,
                                          Route[popt].Host[rdType],
                                          Route[popt].Port[rdType],
                                          opC|XROOTD_MON_REDLOCAL, Path);
           if (Cgi) rs = fsRedirNoEnt(eMsg, Cgi, popt);
              else  rs = Response.Send(kXR_redirect,
                                       Route[popt].Port[rdType],
                                       Route[popt].Host[rdType]);
          } else rs = Response.Send((XErrorCode)rc, eMsg);
       if (myError.extData()) myError.Reset();
       return rs;
      }

// Process the redirection (error msg is host:port)
//
   if (rc == SFS_REDIRECT)
      {SI->redirCnt++;
       // if the plugin set some redirect flags but the client does not
       // support them, clear the flags (set -1)
       if( ecode < -1 && !( clientPV & XrdOucEI::uRedirFlgs ) )
           ecode = -1;
       if (XrdXrootdMonitor::Redirect() && Path && opC)
           XrdXrootdMonitor::Redirect(Monitor.Did, eMsg, Port, opC, Path);
       if (TRACING(TRACE_REDIR))
          {if (ecode < 0)
              {TRACEI(REDIR, Response.ID() <<"redirecting to " << eMsg);}
              else {TRACEI(REDIR, Response.ID() <<"redirecting to "
                                  << eMsg <<':' <<ecode);
                   }
          }
       rs = Response.Send(kXR_redirect, ecode, eMsg, myError.getErrTextLen());
       if (myError.extData()) myError.Reset();
       return rs;
      }

// Process the deferal. We also synchronize sending the deferal response with
// sending the actual deferred response by calling Done() in the callback object.
// This allows the requestor of he callback know that we actually send the
// kXR_waitresp to the end client and avoid violating time causality.
//
   if (rc == SFS_STARTED)
      {SI->stallCnt++;
       if (ecode <= 0) ecode = 1800;
       TRACEI(STALL, Response.ID() <<"delaying client up to " <<ecode <<" sec");
       rc = Response.Send(kXR_waitresp, ecode, eMsg);
       if (myError.getErrCB()) myError.getErrCB()->Done(ecode, &myError);
       if (myError.extData()) myError.Reset();
       return (rc ? rc : 1);
      }

// Process the data response
//
   if (rc == SFS_DATA)
      {if (ecode) rs = Response.Send((void *)eMsg, ecode);
          else    rs = Response.Send();
       if (myError.extData()) myError.Reset();
       return rs;
      }

// Process the data response via an iovec
//
   if (rc == SFS_DATAVEC)
      {if (ecode < 2) rs = Response.Send();
          else        rs = Response.Send((struct iovec *)eMsg, ecode);
       if (myError.getErrCB()) myError.getErrCB()->Done(ecode, &myError);
       if (myError.extData())  myError.Reset();
       return rs;
      }

// Process the deferal
//
   if (rc >= SFS_STALL)
      {SI->stallCnt++;
       TRACEI(STALL, Response.ID() <<"stalling client for " <<rc <<" sec");
       rs = Response.Send(kXR_wait, rc, eMsg);
       if (myError.extData()) myError.Reset();
       return rs;
      }

// Unknown conditions, report it
//
   {char buff[32];
    SI->errorCnt++;
    sprintf(buff, "%d", rc);
    eDest.Emsg("Xeq", "Unknown error code", buff, eMsg);
    rs = Response.Send(kXR_ServerError, eMsg);
    if (myError.extData()) myError.Reset();
    return rs;
   }
}

/******************************************************************************/
/*                               f s O v r l d                                */
/******************************************************************************/
  
int XrdXrootdProtocol::fsOvrld(char opC, const char *Path, char *Cgi)
{
   static const char *prot = "root://";
   static int negOne = -1;
   static char quest = '?', slash = '/';

   struct iovec rdrResp[8];
   char *destP=0, dest[512];
   int iovNum=0, pOff, port;

// If this is a forwarded path and the client can handle full url's then
// redirect the client to the destination in the path. Otherwise, if there is
// an alternate destination, send client there. Otherwise, stall the client.
//
   if (OD_Bypass && clientPV & XrdOucEI::uUrlOK
   &&  (pOff = XrdOucUtils::isFWD(Path, &port, dest, sizeof(dest))))
      {    rdrResp[1].iov_base = (char *)&negOne;
           rdrResp[1].iov_len  = sizeof(negOne);
           rdrResp[2].iov_base = (char *)prot;
           rdrResp[2].iov_len  = 7;                        // root://
           rdrResp[3].iov_base = (char *)dest;
           rdrResp[3].iov_len  = strlen(dest);             // host:port
           rdrResp[4].iov_base = (char *)&slash;
           rdrResp[4].iov_len  = (*Path == '/' ? 1 : 0);   // / or nil for objid
           rdrResp[5].iov_base = (char *)(Path+pOff);
           rdrResp[5].iov_len  = strlen(Path+pOff);        // path
       if (Cgi && *Cgi)
          {rdrResp[6].iov_base = (char *)&quest;
           rdrResp[6].iov_len  = sizeof(quest);            // ?
           rdrResp[7].iov_base = (char *)Cgi;
           rdrResp[7].iov_len  = strlen(Cgi);              // cgi
           iovNum = 8;
          } else iovNum = 6;
       destP = dest;
      } else if ((destP = Route[RD_ovld].Host[rdType]))
                 port   = Route[RD_ovld].Port[rdType];

// If a redirect happened, then trace it.
//
   if (destP)
      {SI->redirCnt++;
       if (XrdXrootdMonitor::Redirect())
           XrdXrootdMonitor::Redirect(Monitor.Did, destP, port,
                                      opC|XROOTD_MON_REDLOCAL, Path);
       if (iovNum)
          {TRACEI(REDIR, Response.ID() <<"redirecting to "<<dest);
           return Response.Send(kXR_redirect, rdrResp, iovNum);
          } else {
           TRACEI(REDIR, Response.ID() <<"redirecting to "<<destP<<':'<<port);
           return Response.Send(kXR_redirect, port, destP);
          }
      }

// If there is a stall value, then delay the client
//
   if (OD_Stall)
      {TRACEI(STALL, Response.ID()<<"stalling client for "<<OD_Stall<<" sec");
       SI->stallCnt++;
       return Response.Send(kXR_wait, OD_Stall, "server is overloaded");
      }

// We were unsuccessful, return overload as an error
//
   return Response.Send(kXR_Overloaded, "server is overloaded");
}
  
/******************************************************************************/
/*                          f s R e d i r N o E n t                           */
/******************************************************************************/

int XrdXrootdProtocol::fsRedirNoEnt(const char *eMsg, char *Cgi, int popt)
{
   struct iovec ioV[4];
   char *tried, *trend, *ptried = 0;
   kXR_int32 pnum = htonl(static_cast<kXR_int32>(Route[popt].Port[rdType]));
   int tlen;

// Try to find the last tried token in the cgi
//
   if ((trend = Cgi))
      {do {if (!(tried = strstr(Cgi, "tried=")))    break;
           if (tried == trend || *(tried-1) == '&')
              {if (!ptried || (*(tried+6) && *(tried+6) != '&')) ptried=tried;}
           Cgi = index(tried+6, '&');
          } while(Cgi);
      }

// If we did find a tried, bracket it out with a leading comma (we can modify
// the passed cgi string here because this is the last time it will be used.
//
   if ((tried = ptried))
      {tried += 5;
       while(*(tried+1) && *(tried+1) == ',') tried++;
       trend  = index(tried, '&');
       if (trend) {tlen = trend - tried; *trend = 0;}
           else    tlen = strlen(tried);
       *tried = ',';
      } else tlen = 0;

// Check if we are in a redirect loop (i.e. we are listed in the client's cgi).
// If so, then treat this and file not found as we've been here before.
//
   if ((trend = tried) && eMsg)
      do {if ((trend = strstr(trend, myCName)))
             {if (*(trend+myCNlen) == '\0' || *(trend+myCNlen) == ',')
                 return Response.Send(kXR_NotFound, eMsg);
              trend = index(trend+myCNlen, ',');
             }
         } while(trend);


// If we have not found a tried token or that token far too large to propogate
// (i.e. it's likely we have an undetected loop), then do a simple redirect.
//
   if (!tried || !tlen || tlen > 16384)
      return Response.Send(kXR_redirect,
                           Route[popt].Port[rdType],
                           Route[popt].Host[rdType]);

// We need to append the client's tried list to the one we have to avoid loops
//

   ioV[1].iov_base = (char *)&pnum;
   ioV[1].iov_len  = sizeof(pnum);
   ioV[2].iov_base = Route[popt].Host[rdType];
   ioV[2].iov_len  = Route[popt].RDSz[rdType];
   ioV[3].iov_base = tried;
   ioV[3].iov_len  = tlen;

// Compute total length
//
   tlen += sizeof(pnum) + Route[popt].RDSz[rdType];

// Send off the redirect
//
   return Response.Send(kXR_redirect, ioV, 4, tlen);
}

/******************************************************************************/
/*                               g e t B u f f                                */
/******************************************************************************/
  
int XrdXrootdProtocol::getBuff(const int isRead, int Quantum)
{

// Check if we need to really get a new buffer
//
   if (!argp || Quantum > argp->bsize) hcNow = hcPrev;
      else if (Quantum >= halfBSize || hcNow-- > 0) return 1;
              else if (hcNext >= hcMax) hcNow = hcMax;
                      else {int tmp = hcPrev;
                            hcNow   = hcNext;
                            hcPrev  = hcNext;
                            hcNext  = tmp+hcNext;
                           }

// Get a new buffer
//
   if (argp) BPool->Release(argp);
   if ((argp = BPool->Obtain(Quantum))) halfBSize = argp->bsize >> 1;
      else return Response.Send(kXR_NoMemory, (isRead ?
                                "insufficient memory to read file" :
                                "insufficient memory to write file"));

// Success
//
   return 1;
}

/******************************************************************************/
/* Private:                   g e t C k s T y p e                             */
/******************************************************************************/

char *XrdXrootdProtocol::getCksType(char *opaque, char *cspec, int cslen)
{
   char *cksT;

// Get match for user specified checksum type, if any. Otherwise return default.
//
   if (opaque && *opaque)
      {XrdOucEnv jobEnv(opaque);
       if ((cksT = jobEnv.Get("cks.type")))
          {XrdOucTList *tP = JobCKTLST;
           while(tP && strcasecmp(tP->text, cksT)) tP = tP->next;
           if (!tP && cspec) snprintf(cspec, cslen, "%s", cksT);
           return (tP ? tP->text : 0);
          }
      }

// Return default
//
   return JobCKT;
}
  
/******************************************************************************/
/* Private:                     l o g L o g i n                               */
/******************************************************************************/
  
bool XrdXrootdProtocol::logLogin(bool xauth)
{
   const char *uName, *ipName, *tMsg, *zMsg = "";
   char lBuff[512], pBuff[512];

// Determine ip type
//
   if (clientPV & XrdOucEI::uIPv4)
           ipName = (clientPV & XrdOucEI::uIPv64 ? "IP46"   : "IPv4");
      else ipName = (clientPV & XrdOucEI::uIPv64 ? "IP64"   : "IPv6");

// Determine client name
//
   if (xauth) uName = (Client->name ? Client->name : "nobody");
      else    uName = 0;

// Check if TLS was or will be used
//
   tMsg = Link->verTLS();
   if (*tMsg) zMsg = " ";

// Format the line
//
   snprintf(lBuff, sizeof(lBuff), "%s %s %s%slogin%s%s",
                  (clientPV & XrdOucEI::uPrip ? "pvt"    : "pub"), ipName,
                  tMsg, zMsg,
                  (xauth                      ? " as "   : ""),
                  (uName                      ? uName    : ""));

// Document the login
//
   if (Client->tident != Client->pident)
      {snprintf(pBuff, sizeof(pBuff), "via %s auth for %s",
                       Client->prot, Client->pident);
      } else *pBuff = 0;
   eDest.Log(SYS_LOG_01, "Xeq", Link->ID, lBuff, (*pBuff ? pBuff : 0));

// Enable TLS if we need to (note sess setting is off if login setting is on).
// If we need to but the client is not TLS capable, send an error and terminate.
//
   if ((doTLS & Req_TLSSess) && !Link->hasBridge())
      {if (ableTLS)
          {if (Link->setTLS(true, tlsCtx))
              {Link->setProtName("xroots");
               isTLS = true;
              } else {
               eDest.Emsg("Xeq", "Unable to require TLS for", Link->ID);
               return false;
              }
          } else {
           eDest.Emsg("Xeq","session requires TLS but",Link->ID,"is incapable.");
           Response.Send(kXR_TLSRequired, "session requires TLS support");
           return false;
          }
      }

// Record the appname in the final SecEntity object
//
   if (AppName) Client->eaAPI->Add("xrd.appname", (std::string)AppName);

// Assign unique identifier to the final SecEntity object
//
   Client->ueid = mySID;

// Propogate a connect through the whole system
//
   osFS->Connect(Client);
   return true;
}

/******************************************************************************/
/*                               m a p M o d e                                */
/******************************************************************************/

#define Map_Mode(x,y) if (Mode & kXR_ ## x) newmode |= S_I ## y

int XrdXrootdProtocol::mapMode(int Mode)
{
   int newmode = 0;

// Map the mode in the obvious way
//
   Map_Mode(ur, RUSR); Map_Mode(uw, WUSR);  Map_Mode(ux, XUSR);
   Map_Mode(gr, RGRP); Map_Mode(gw, WGRP);  Map_Mode(gx, XGRP);
   Map_Mode(or, ROTH);                      Map_Mode(ox, XOTH);

// All done
//
   return newmode;
}

/******************************************************************************/
/*                               M o n A u t h                                */
/******************************************************************************/

void XrdXrootdProtocol::MonAuth()
{
         char Buff[4096];
   const char *bP = Buff;

   if (Client == &Entity) bP = Entity.moninfo;
      else snprintf(Buff,sizeof(Buff),
                    "&p=%s&n=%s&h=%s&o=%s&r=%s&g=%s&m=%s%s&I=%c",
                     Client->prot,
                    (Client->name ? Client->name : ""),
                    (Client->host ? Client->host : ""),
                    (Client->vorg ? Client->vorg : ""),
                    (Client->role ? Client->role : ""),
                    (Client->grps ? Client->grps : ""),
                    (Client->moninfo ? Client->moninfo : ""),
                    (Entity.moninfo  ? Entity.moninfo  : ""),
                    (clientPV & XrdOucEI::uIPv4 ? '4' : '6')
                   );

   Monitor.Report(bP);
   if (Entity.moninfo) {free(Entity.moninfo); Entity.moninfo = 0;}
}
  
/******************************************************************************/
/*                               r p C h e c k                                */
/******************************************************************************/
  
int XrdXrootdProtocol::rpCheck(char *fn, char **opaque)
{
   char *cp;

   if (*fn != '/')
      {if (!(XPList.Opts() & XROOTDXP_NOSLASH)) return 1;
       if (  XPList.Opts() & XROOTDXP_NOCGI) {*opaque = 0; return 0;}
      }

   if (!(cp = index(fn, '?'))) *opaque = 0;
      else {*cp = '\0'; *opaque = cp+1;
            if (!**opaque) *opaque = 0;
           }

   if (*fn != '/') return 0;

   while ((cp = index(fn, '/')))
         {fn = cp+1;
          if (fn[0] == '.' && fn[1] == '.' && fn[2] == '/') return 1;
         }
   return 0;
}
  
/******************************************************************************/
/*                                r p E m s g                                 */
/******************************************************************************/
  
int XrdXrootdProtocol::rpEmsg(const char *op, char *fn)
{
   char buff[2048];
   snprintf(buff,sizeof(buff)-1,"%s relative path '%s' is disallowed.",op,fn);
   buff[sizeof(buff)-1] = '\0';
   return Response.Send(kXR_NotAuthorized, buff);
}
 
/******************************************************************************/
/*                                 S e t S F                                  */
/******************************************************************************/

int XrdXrootdProtocol::SetSF(kXR_char *fhandle, bool seton)
{
   XrdXrootdFHandle fh(fhandle);
   XrdXrootdFile   *theFile;

   if (!FTab || !(theFile = FTab->Get(fh.handle))) return -EBADF;

// Turn it off or on if so wanted
//
   if (!seton) theFile->sfEnabled = 0;
      else if (theFile->fdNum >= 0) theFile->sfEnabled = 1;

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                                S q u a s h                                 */
/******************************************************************************/
  
int XrdXrootdProtocol::Squash(char *fn)
{
   char *ofn, *ifn = fn;

   if (*fn != '/') return XPList.Opts();

   while(*ifn)
        {if (*ifn == '/')
            if (*(ifn+1) == '/'
            || (*(ifn+1) == '.' && *(ifn+1) && *(ifn+2) == '/')) break;
         ifn++;
        }

   if (!*ifn) return XPList.Validate(fn, ifn-fn);

   ofn = ifn;
   while(*ifn) {*ofn = *ifn++;
                while(*ofn == '/')
                   {while(*ifn == '/') ifn++;
                    if (ifn[0] == '.' && ifn[1] == '/') ifn += 2;
                       else break;
                   }
                ofn++;
               }
   *ofn = '\0';

   return XPList.Validate(fn, ofn-fn);
}

/******************************************************************************/
/*                                v p E m s g                                 */
/******************************************************************************/
  
int XrdXrootdProtocol::vpEmsg(const char *op, char *fn)
{
   char buff[2048];
   snprintf(buff,sizeof(buff)-1,"%s path '%s' is disallowed.",op,fn);
   buff[sizeof(buff)-1] = '\0';
   return Response.Send(kXR_NotAuthorized, buff);
}
