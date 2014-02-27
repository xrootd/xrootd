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

#include <stdio.h>

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdOuc/XrdOucReqID.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XrdXrootd/XrdXrootdAio.hh"
#include "XrdXrootd/XrdXrootdCallBack.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdFileLock.hh"
#include "XrdXrootd/XrdXrootdJob.hh"
#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdPio.hh"
#include "XrdXrootd/XrdXrootdPrepare.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdXPath.hh"

#include "XrdVersion.hh"
  
/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdOucTrace *XrdXrootdTrace;

/******************************************************************************/
/*                      L o c a l   S t r u c t u r e s                       */
/******************************************************************************/
  
struct XrdXrootdFHandle
       {kXR_int32 handle;

        void Set(kXR_char *ch)
            {memcpy((void *)&handle, (const void *)ch, sizeof(handle));}
        XrdXrootdFHandle() {}
        XrdXrootdFHandle(kXR_char *ch) {Set(ch);}
       ~XrdXrootdFHandle() {}
       };

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

#define CRED (const XrdSecEntity *)Client

#define TRACELINK Link

#define STATIC_REDIRECT(xfnc) \
        if (Route[xfnc].Port[rdType]) \
           return Response.Send(kXR_redirect,Route[xfnc].Port[rdType],\
                                             Route[xfnc].Host[rdType])
 
/******************************************************************************/
/*                              d o _ A d m i n                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Admin()
{
   return Response.Send(kXR_Unsupported, "admin request is not supported");
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
       strncpy(Entity.prot, (const char *)Request.auth.credtype,
                                   sizeof(Request.auth.credtype));
       if (!(AuthProt = CIA->getProtocol(Link->Host(), *(Link->AddrInfo()),
                                         &cred, &eMsg)))
          {eText = eMsg.getErrText(rc);
           eDest.Emsg("Xeq", "User authentication failed;", eText);
           return Response.Send(kXR_NotAuthorized, eText);
          }
       AuthProt->Entity.tident = Link->ID;
       numReads++;
      }

// Now try to authenticate the client using the current protocol
//
   if (!(rc = AuthProt->Authenticate(&cred, &parm, &eMsg)))
      {rc = Response.Send(); Status &= ~XRD_NEED_AUTH; SI->Bump(SI->LoginAU);
       Client = &AuthProt->Entity; numReads = 0; strcpy(Entity.prot, "host");
       if (Monitor.Logins() && Monitor.Auths()) MonAuth();
       logLogin(true);
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
   return Response.Send(kXR_NotAuthorized, eText);
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

// Find the link we are to bind to
//
   if (sp->FD <= 0 || !(lp = XrdLink::fd2link(sp->FD, sp->Inst)))
      return Response.Send(kXR_NotFound, "session not found");

// The link may have escaped so we need to hold this link and try again
//
   lp->Hold(1);
   if (lp != XrdLink::fd2link(sp->FD, sp->Inst))
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
   pp->isBound   = 1;
   PathID        = i;
   sprintf(buff, "FD %d#%d bound", Link->FDnum(), i);
   eDest.Log(SYS_LOG_01, "Xeq", buff, lp->ID);

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
/*                              d o _ c h m o d                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Chmod()
{
   int mode, rc;
   const char *opaque;
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
   TRACEP(FS, "chmod rc=" <<rc <<" mode=" <<std::oct <<mode <<std::dec <<' ' <<argp->buff);
   if (SFS_OK == rc) return Response.Send();

// An error occured
//
   return fsError(rc, XROOTD_MON_CHMOD, myError, argp->buff);
}

/******************************************************************************/
/*                              d o _ C K s u m                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_CKsum(int canit)
{
   const char *opaque;
   char *args[3];
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

// If we are allowed to locally query the checksum to avoid computation, do it
//
   if (JobLCL && (rc = do_CKsum(argp->buff, opaque)) <= 0) return rc;

// Just make absolutely sure we can continue with a calculation
//
   if (!JobCKS)
      return Response.Send(kXR_ServerError, "Logic error computing checksum.");

// Construct the argument list
//
   args[0] = JobCKT;
   args[1] = argp->buff;
   args[2] = 0;

// Preform the actual function
//
   return JobCKS->Schedule(argp->buff, (const char **)args, &Response,
                  ((CapVer & kXR_vermask) >= kXR_ver002 ? 0 : JOB_Sync));
}

/******************************************************************************/
  
int XrdXrootdProtocol::do_CKsum(const char *Path, const char *Opaque)
{
   static char Space = ' ';
   static int  CKTLen = strlen(JobCKT);
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
   int ec, rc = osFS->chksum(XrdSfsFileSystem::csGet, JobCKT, Path,
                             myError, CRED, Opaque);
   const char *csData = myError.getErrText(ec);

// Diagnose any hard errors
//
   if (rc) return fsError(rc, 0, myError, Path);

// Return result if it is actually available
//
   if (*csData)
      {struct iovec iov[4] = {{0,0}, {JobCKT, (size_t)CKTLen}, {&Space, 1},
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
   XrdXrootdFile *fp;
   XrdXrootdFHandle fh(Request.close.fhandle);
   int rc;

// Keep statistics
//
   SI->Bump(SI->miscCnt);

// Find the file object
//
   if (!FTab || !(fp = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen, 
                          "close does not refer to an open file");

// Serialize the link to make sure that any in-flight operations on this handle
// have completed (async mode or parallel streams)
//
   Link->Serialize();

// If we are monitoring, insert a close entry
//
   if (Monitor.Files())
      Monitor.Agent->Close(fp->Stats.FileID,
                           fp->Stats.xfr.read + fp->Stats.xfr.readv,
                           fp->Stats.xfr.write);

// If fstream monitoring enabled, log it out there
//
   if (Monitor.Fstat()) XrdXrootdMonFile::Close(&(fp->Stats));

// Do an explicit close of the file here; reflecting any errors
//
   rc = fp->XrdSfsp->close();
   TRACEP(FS, "close rc=" <<rc <<" fh=" <<fh.handle);
   if (SFS_OK != rc)
      {if (rc == SFS_ERROR || rc == SFS_STALL)
          return fsError(rc, 0, fp->XrdSfsp->error, 0);
       return Response.Send(kXR_FSError, fp->XrdSfsp->error.getErrText());
      }

// Delete the file from the file table; this will unlock/close the file
//
   FTab->Del(fh.handle);
   numFiles--;
   return Response.Send();
}

/******************************************************************************/
/*                            d o _ D i r l i s t                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Dirlist()
{
   int bleft, rc = 0, dlen, cnt = 0;
   char *buff, ebuff[4096];
   const char *opaque, *dname;
   XrdSfsDirectory *dp;
   bool doDig;

// Check if we are digging for data
//
   doDig = (digFS && SFS_LCLPATH(argp->buff));

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
      {rc = fsError(rc, XROOTD_MON_OPENDIR, dp->error, argp->buff);
       delete dp;
       return rc;
      }

// Check if the caller wants stat information as well
//
   if (Request.dirlist.options[0] & kXR_dstat)
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
                                                 const char *opaque)
{
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
   struct stat Stat;
   static const int statSz = 80;
   int bleft, rc = 0, dlen, cnt = 0;
   char *buff, *dLoc, ebuff[8192];
   const char *dname;

// Construct the path to the directory as we will be asking for stat calls
// if the interface does not support autostat.
//
   if (dp->autoStat(&Stat) == SFS_OK) dLoc = 0;
      else {strcpy(pbuff, argp->buff);
            dlen = strlen(pbuff);
            if (pbuff[dlen-1] != '/') {pbuff[dlen] = '/'; dlen++;}
            dLoc = pbuff+dlen;
           }

// The initial leadin is a "dot" entry to indicate to the client that we
// support the dstat option (older servers will not do that). It's up to the
// client to issue individual stat requests in that case.
//
   memset(&Stat, 0, sizeof(Stat));
   strcpy(ebuff, ".\n");
   buff = ebuff+2; bleft = sizeof(ebuff)-2;
   dlen = StatGen(Stat, buff);
   bleft -= (dlen+1); buff += dlen; *buff = '\n'; buff++;

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
                strcpy(buff, dname); buff += dlen; *buff = '\n'; buff++; cnt++;
                if (dLoc)
                   {strcpy(dLoc, dname);
                    rc = osFS->stat(pbuff, &Stat, myError, CRED, opaque);
                    if (rc != SFS_OK)
                       return fsError(rc, XROOTD_MON_STAT, myError, argp->buff);
                   }
                dlen = StatGen(Stat, buff);
                bleft -= (dlen+1); buff += dlen; *buff = '\n'; buff++;
               }
            dname = 0;
           }
       if (dname)
          {rc = Response.Send(kXR_oksofar, ebuff, buff-ebuff);
           buff = ebuff; bleft = sizeof(ebuff);
          }
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
   ||  !(rc = Link->Terminate(Link, sessID.FD, sessID.Inst))) return -1;

// Trace this request
//
   TRACEP(LOGIN, "endsess " <<sessID.Pid <<':' <<sessID.FD <<'.' <<sessID.Inst
          <<" rc=" <<rc <<" (" <<strerror(rc < 0 ? -rc : EAGAIN) <<")");

// Return result
//
   if (rc >  0)
      return (rc = Response.Send(kXR_wait, rc, "session still active")) ? rc:1;

   if (rc == -EACCES)return Response.Send(kXR_NotAuthorized, "not session owner");
   if (rc == -ESRCH) return Response.Send(kXR_NotFound, "session not found");
   if (rc == -ETIME) return Response.Send(kXR_Cancelled,"session not ended");

   return Response.Send();
}

/******************************************************************************/
/*                            d o   G e t f i l e                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Getfile()
{
// int gopts, buffsz;

// Keep Statistics
//
   SI->Bump(SI->getfCnt);

// Unmarshall the data
//
// gopts  = int(ntohl(Request.getfile.options));
// buffsz = int(ntohl(Request.getfile.buffsz));

   return Response.Send(kXR_Unsupported, "getfile request is not supported");
}

/******************************************************************************/
/*                             d o _ L o c a t e                              */
/******************************************************************************/

int XrdXrootdProtocol::do_Locate()
{
   static XrdXrootdCallBack locCB("locate", XROOTD_MON_LOCATE);
   int rc, opts, fsctl_cmd = SFS_FSCTL_LOCATE;
   const char *opaque;
   char *Path, *fn = argp->buff, opt[8], *op=opt;
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
   *op = '\0';
   TRACEP(FS, "locate " <<opt <<' ' <<fn);

// Check if this is a non-specific locate
//
        if (*fn != '*'){Path = fn;
                        doDig = (digFS && SFS_LCLPATH(Path));
                       }
   else if (*(fn+1))   {Path = fn+1;
                        doDig = (digFS && SFS_LCLPATH(Path));
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
   return fsError(rc, (doDig ? 0 : XROOTD_MON_LOCATE), myError, Path);
}
  
/******************************************************************************/
/*                              d o _ L o g i n                               */
/*.x***************************************************************************/
  
int XrdXrootdProtocol::do_Login()
{
   static XrdSysMutex sessMutex;
   static unsigned int Sid = 0;
   XrdXrootdSessID sessID;
   XrdNetAddrInfo *addrP;
   int i, pid, rc, sendSID = 0;
   char uname[sizeof(Request.login.username)+1];

// Keep Statistics
//
   SI->Bump(SI->LoginAT);

// Unmarshall the data
//
   pid = (int)ntohl(Request.login.pid);
   for (i = 0; i < (int)sizeof(Request.login.username); i++)
      {if (Request.login.username[i] == '\0' ||
           Request.login.username[i] == ' ') break;
       uname[i] = Request.login.username[i];
      }
   uname[i] = '\0';

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
       sessMutex.Lock(); mySID = ++Sid; sessMutex.UnLock();
       sessID.Sid  = mySID;
       sendSID = 1;
       if (!clientPV)
          {        if (i >  kXR_ver003) clientPV = (int)0x0300;
              else if (i == kXR_ver003) clientPV = (int)0x0299;
              else if (i == kXR_ver002) clientPV = (int)0x0290;
              else if (i == kXR_ver001) clientPV = (int)0x0200;
              else                      clientPV = (int)0x0100;
          }
       if (CapVer & kXR_asyncap) clientPV |= XrdOucEI::uAsync;
       if (Request.login.ability & kXR_fullurl)
          clientPV |= XrdOucEI::uUrlOK;
       if (Request.login.ability & kXR_multipr)
          clientPV |= (XrdOucEI::uMProt | XrdOucEI::uUrlOK);
       if (Request.login.ability & kXR_readrdok)
          clientPV |= XrdOucEI::uReadR;
      }

// Mark the client as IPv4 if they came in as IPv4 or mapped IPv4
//
   addrP = Link->AddrInfo();
   if (addrP->isIPType(XrdNetAddrInfo::IPv4) || addrP->isMapped())
      clientPV |= XrdOucEI::uIPv4;

// Mark the client as being on a private net if the address is private
//
   if (addrP->isPrivate()) {clientPV |= XrdOucEI::uPrip; rdType = 1;}
      else rdType = 0;

// Check if this is an admin login
//
   if (*(Request.login.role) & (kXR_char)kXR_useradmin)
      Status = XRD_ADMINUSER;

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
   Entity.tident = Link->ID;
   Entity.addrInfo = Link->AddrInfo();
   Client = &Entity;

// Allocate a monitoring object, if needed for this connection
//
   if (Monitor.Ready())
      {Monitor.Register(Link->ID, Link->Host(), "xrootd");
       if (Monitor.Logins() && (!Monitor.Auths() || !(Status & XRD_NEED_AUTH)))
          Monitor.Report(Monitor.Auths() ? "" : 0);
      }

// Complete the rquestID object
//
   ReqID.setID(Request.header.streamid, Link->FDnum(), Link->Inst());

// Document this login
//
   if (!(Status & XRD_NEED_AUTH)) logLogin();
   return rc;
}

/******************************************************************************/
/*                              d o _ M k d i r                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Mkdir()
{
   int mode, rc;
   const char *opaque;
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
   TRACEP(FS, "rc=" <<rc <<" mkdir " <<std::oct <<mode <<std::dec <<' ' <<argp->buff);
   if (SFS_OK == rc) return Response.Send();

// An error occured
//
   return fsError(rc, XROOTD_MON_MKDIR, myError, argp->buff);
}

/******************************************************************************/
/*                                 d o _ M v                                  */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Mv()
{
   int rc;
   const char *Opaque, *Npaque;
   char *oldp, *newp;
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);

// Check for static routing
//
   STATIC_REDIRECT(RD_mv);

// Find the space separator between the old and new paths
//
   oldp = newp = argp->buff;
   while(*newp && *newp != ' ') newp++;
   if (*newp) {*newp = '\0'; newp++;
               while(*newp && *newp == ' ') newp++;
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
      Response.Send(kXR_ArgMissing, "new path specfied for mv");

// Preform the actual function
//
   rc = osFS->rename(oldp, newp, myError, CRED, Opaque, Npaque);
   TRACEP(FS, "rc=" <<rc <<" mv " <<oldp <<' ' <<newp);
   if (SFS_OK == rc) return Response.Send();

// An error occured
//
   return fsError(rc, XROOTD_MON_MV, myError, oldp);
}

/******************************************************************************/
/*                            d o _ O f f l o a d                             */
/******************************************************************************/

int XrdXrootdProtocol::do_Offload(int pathID, int isWrite)
{
   XrdSysSemaphore isAvail(0);
   XrdXrootdProtocol *pp;
   XrdXrootdPio      *pioP;
   kXR_char streamID[2];

// Verify that the path actually exists
//
   if (pathID >= maxStreams || !(pp = Stream[pathID]))
      return Response.Send(kXR_ArgInvalid, "invalid path ID");

// Verify that this path is still functional
//
   pp->streamMutex.Lock();
   if (pp->isDead || pp->isNOP)
      {pp->streamMutex.UnLock();
       return Response.Send(kXR_ArgInvalid, 
       (pp->isDead ? "path ID is not functional"
                   : "path ID is not connected"));
      }

// Grab the stream ID
//
   Response.StreamID(streamID);

// Try to schedule this operation. In order to maximize the I/O overlap, we
// will wait until the stream gets control and will have a chance to start
// reading from the device or from the network.
//
   do{if (!pp->isActive)
         {pp->myFile   = myFile;
          pp->myOffset = myOffset;
          pp->myIOLen  = myIOLen;
          pp->myBlen   = 0;
          pp->doWrite  = static_cast<char>(isWrite);
          pp->doWriteC = 0;
          pp->Resume   = &XrdXrootdProtocol::do_OffloadIO;
          pp->isActive = 1;
          pp->reTry    = &isAvail;
          pp->Response.Set(streamID);
          pp->streamMutex.UnLock();
          Link->setRef(1);
          Sched->Schedule((XrdJob *)(pp->Link));
          isAvail.Wait();
          return 0;
         }

      if ((pioP = pp->pioFree)) break;
      pp->reTry = &isAvail;
      pp->streamMutex.UnLock();
      TRACEP(FS, (isWrite ? 'w' : 'r') <<" busy path " <<pathID <<" offs=" <<myOffset);
      isAvail.Wait();
      TRACEP(FS, (isWrite ? 'w' : 'r') <<" free path " <<pathID <<" offs=" <<myOffset);
      pp->streamMutex.Lock();
      if (pp->isNOP)
         {pp->streamMutex.UnLock();
          return Response.Send(kXR_ArgInvalid, "path ID is not connected");
         }
      } while(1);

// Fill out the queue entry and add it to the queue
//
   pp->pioFree = pioP->Next; pioP->Next = 0;
   pioP->Set(myFile, myOffset, myIOLen, streamID, static_cast<char>(isWrite));
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
   XrdSysSemaphore *sesSem;
   XrdXrootdPio    *pioP;
   int rc;

// Entry implies that we just got scheduled and are marked as active. Hence
// we need to post the session thread so that it can pick up the next request.
// We can manipulate the semaphore pointer without a lock as the only other
// thread that can manipulate the pointer is the waiting session thread.
//
   if (!doWriteC && (sesSem = reTry)) {reTry = 0; sesSem->Post();}
  
// Perform all I/O operations on a parallel stream (suppress async I/O).
//
   do {if (!doWrite) rc = do_ReadAll(0);
          else if ( (rc = (doWriteC ? do_WriteCont() : do_WriteAll()) ) > 0)
                  {Resume = &XrdXrootdProtocol::do_OffloadIO;
                   doWriteC = 1;
                   return rc;
                  }
       streamMutex.Lock();
       if (rc || !(pioP = pioFirst)) break;
       if (!(pioFirst = pioP->Next)) pioLast = 0;
       myFile   = pioP->myFile;
       myOffset = pioP->myOffset;
       myIOLen  = pioP->myIOLen;
       doWrite  = pioP->isWrite;
       doWriteC = 0;
       Response.Set(pioP->StreamID);
       pioP->Next = pioFree; pioFree = pioP;
       if (reTry) {reTry->Post(); reTry = 0;}
       streamMutex.UnLock();
      } while(1);

// There are no pending operations or the link died
//
   if (rc) isNOP = 1;
   isActive = 0;
   Stream[0]->Link->setRef(-1);
   if (reTry) {reTry->Post(); reTry = 0;}
   streamMutex.UnLock();
   return -EINPROGRESS;
}

/******************************************************************************/
/*                               d o _ O p e n                                */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Open()
{
   static XrdXrootdCallBack openCB("open file", XROOTD_MON_OPENR);
   int fhandle;
   int rc, mode, opts, openopts, doforce = 0, compchk = 0;
   int popt, retStat = 0;
   const char *opaque;
   char usage, ebuff[2048], opC;
   bool doDig;
   char *fn = argp->buff, opt[16], *op=opt, isAsync = '\0';
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
            if (opts & kXR_mkdir)     {*op++ = 'm';
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
   if (opts & kXR_force)              {*op++ = 'f'; doforce = 1;}
   if ((opts & kXR_async || as_force) && !as_noaio)
                                      {*op++ = 'a'; isAsync = '1';}
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

// Check if static redirection applies
//
   if (!doDig && Route[RD_open1].Host[rdType] && (popt = RPList.Validate(fn)))
      return Response.Send(kXR_redirect, Route[popt].Port[rdType],
                                         Route[popt].Host[rdType]);

// Validate the path
//
   if (doDig) {popt = XROOTDXP_NOLK; opC = 0;}
      else if (!(popt = Squash(fn))) return vpEmsg("Opening", fn);

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

// The open is elegible for a defered response, indicate we're ok with that
//
   fp->error.setErrCB(&openCB, ReqID.getID());
   fp->error.setUCap(clientPV);

// Open the file
//
   if ((rc = fp->open(fn, (XrdSfsFileOpenMode)openopts,
                     (mode_t)mode, CRED, opaque)))
      {rc = fsError(rc, opC, fp->error, fn); delete fp; return rc;}

// Obtain a hyper file object
//
   if (!(xp=new XrdXrootdFile(Link->ID,fp,usage,isAsync,Link->sfOK,&statbuf)))
      {delete fp;
       snprintf(ebuff, sizeof(ebuff)-1, "Insufficient memory to open %s", fn);
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_NoMemory, ebuff);
      }

// Serialize the link
//
   Link->Serialize();
   *ebuff = '\0';

// Lock this file
//
   if (!(popt & XROOTDXP_NOLK) && (rc = Locker->Lock(xp, doforce)))
      {const char *who;
       if (rc > 0) who = (rc > 1 ? "readers" : "reader");
          else {   rc = -rc;
                   who = (rc > 1 ? "writers" : "writer");
               }
       snprintf(ebuff, sizeof(ebuff)-1,
                "%s file %s is already opened by %d %s; open denied.",
                ('r' == usage ? "Input" : "Output"), fn, rc, who);
       delete fp; xp->XrdSfsp = 0; delete xp;
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_FileLocked, ebuff);
      }

// Create a file table for this link if it does not have one
//
   if (!FTab) FTab = new XrdXrootdFileTable(Monitor.Did);

// Insert this file into the link's file table
//
   if (!FTab || (fhandle = FTab->Add(xp)) < 0)
      {delete xp;
       snprintf(ebuff, sizeof(ebuff)-1, "Insufficient memory to open %s", fn);
       eDest.Emsg("Xeq", ebuff);
       return Response.Send(kXR_NoMemory, ebuff);
      }

// Document forced opens
//
   if (doforce)
      {int rdrs, wrtrs;
       Locker->numLocks(xp, rdrs, wrtrs);
       if (('r' == usage && wrtrs) || ('w' == usage && rdrs) || wrtrs > 1)
          {snprintf(ebuff, sizeof(ebuff)-1,
             "%s file %s forced opened with %d reader(s) and %d writer(s).",
             ('r' == usage ? "Input" : "Output"), fn, rdrs, wrtrs);
           eDest.Emsg("Xeq", ebuff);
          }
      }

// Determine if file is compressed
//
   if (!compchk) 
      {resplen = sizeof(myResp.fhandle);
       memset(&myResp, 0, sizeof(myResp));
      }
      else {int cpsize;
            fp->getCXinfo((char *)myResp.cptype, cpsize);
            if (cpsize) {myResp.cpsize = static_cast<kXR_int32>(htonl(cpsize));
                         resplen = sizeof(myResp);
                        } else myResp.cpsize = 0;
           }

// If client wants a stat in open, return the stat information
//
   if (retStat)
      {retStat = StatGen(statbuf, ebuff);
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

// Respond
//
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
  
int XrdXrootdProtocol::do_Prepare()
{
   int rc, hport, pathnum = 0;
   const char *opaque;
   char opts, hname[256], reqid[128], nidbuff[512], *path;
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
   XrdOucTokenizer pathlist(argp->buff);
   XrdOucTList *pFirst=0, *pP, *pLast = 0;
   XrdOucTList *oFirst=0, *oP, *oLast = 0;
   XrdOucTListHelper pHelp(&pFirst), oHelp(&oFirst);
   XrdXrootdPrepArgs pargs(0, 0);
   XrdSfsPrep fsprep;

// Grab the options
//
   opts = Request.prepare.options;

// Check for static routing
//
   if ((opts & kXR_stage) || (opts & kXR_cancel)) {STATIC_REDIRECT(RD_prepstg);}
   STATIC_REDIRECT(RD_prepare);

// Get a request ID for this prepare and check for static routine
//
   if (opts & kXR_stage && !(opts & kXR_cancel)) 
      {fsprep.reqid = PrepID->ID(reqid, sizeof(reqid));
       fsprep.opts  = Prep_STAGE | (opts & kXR_coloc ? Prep_COLOC : 0);
      }
      else {reqid[0]='*'; reqid[1]='\0'; fsprep.reqid = reqid; fsprep.opts = 0;}

// Initialize the fsile system prepare arg list
//
   fsprep.paths   = 0;
   fsprep.oinfo   = 0;
   fsprep.opts   |= Prep_PRTY0 | (opts & kXR_fresh ? Prep_FRESH : 0);
   fsprep.notify  = 0;

// Check if this is a cancel request
//
   if (opts & kXR_cancel)
      {if (!(path = pathlist.GetLine()))
          return Response.Send(kXR_ArgMissing, "Prepare requestid not specified");
       fsprep.reqid = PrepID->isMine(path, hport, hname, sizeof(hname));
       if (!fsprep.reqid)
          {if (!hport) return Response.Send(kXR_ArgInvalid,
                             "Prepare requestid owned by an unknown server");
           TRACEI(REDIR, Response.ID() <<"redirecting to " << hname <<':' <<hport);
           return Response.Send(kXR_redirect, hport, hname);
          }
       if (SFS_OK != (rc = osFS->prepare(fsprep, myError, CRED)))
          return fsError(rc, XROOTD_MON_PREP, myError, path);
       rc = Response.Send();
       XrdXrootdPrepare::Logdel(path);
       return rc;
      }

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

// Make sure we have at least one path
//
   if (!pFirst)
      return Response.Send(kXR_ArgMissing, "No prepare paths specified");

// Issue the prepare
//
   if (opts & kXR_notify)
      {fsprep.notify  = nidbuff;
       sprintf(nidbuff, Notify, Link->FDnum(), Link->ID);
       fsprep.opts = (opts & kXR_noerrs ? Prep_SENDAOK : Prep_SENDACK);
      }
   if (opts & kXR_wmode) fsprep.opts |= Prep_WMODE;
   fsprep.paths = pFirst;
   fsprep.oinfo = oFirst;
   if (SFS_OK != (rc = osFS->prepare(fsprep, myError, CRED)))
      return fsError(rc, XROOTD_MON_PREP, myError, pFirst->text);

// Perform final processing
//
   if (!(opts & kXR_stage)) rc = Response.Send();
      else {rc = Response.Send(reqid, strlen(reqid));
            pargs.reqid=reqid;
            pargs.user=Link->ID;
            pargs.paths=pFirst;
            XrdXrootdPrepare::Log(pargs);
            pargs.reqid = 0;
           }
   return rc;
}
  
/******************************************************************************/
/*                           d o _ P r o t o c o l                            */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Protocol(int retRole)
{
   static ServerResponseBody_Protocol RespNew
                = {static_cast<kXR_int32>(htonl(kXR_PROTOCOLVERSION)), myRole};

   static ServerResponseBody_Protocol RespOld
                = {static_cast<kXR_int32>(htonl(kXR_PROTOCOLVERSION)),
                   static_cast<kXR_int32>(isRedir ? htonl(kXR_LBalServer)
                                                  : htonl(kXR_DataServer))
                  };

          ServerResponseBody_Protocol *Resp = &RespOld;
          int RespLen = sizeof(RespOld);

// Keep Statistics
//
   SI->Bump(SI->miscCnt);

// Determine which response to provide
//
   if (Request.protocol.clientpv)
      {Resp = &RespNew; RespLen = sizeof(RespNew);
       if (!Status || !(clientPV & XrdOucEI::uVMask))
          clientPV = (clientPV & ~XrdOucEI::uVMask)
                   | (XrdOucEI::uVMask & ntohl(Request.protocol.clientpv));
      }

// Return info
//
    return (retRole ? Resp->flags : Response.Send((void *)Resp, RespLen));
}

/******************************************************************************/
/*                            d o _ P u t f i l e                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Putfile()
{
// int popts, buffsz;

// Keep Statistics
//
   SI->Bump(SI->putfCnt);

// Unmarshall the data
//
// popts  = int(ntohl(Request.putfile.options));
// buffsz = int(ntohl(Request.putfile.buffsz));

   return Response.Send(kXR_Unsupported, "putfile request is not supported");
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
           {n = (JobCKT ? snprintf(bp, bleft, "0:%s\n", JobCKT)
                        : snprintf(bp, bleft, "chksum\n"));
            bp += n; bleft -= n;
           }
   else if (!strcmp("cms", val))
           {XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
            if (osFS->fsctl(fsctl_cmd, ".", myError, CRED) == SFS_DATA)
                    n = snprintf(bp, bleft, "%s\n", myError.getErrText());
               else n = snprintf(bp, bleft, "%s\n", "cms\n");
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
           {n = snprintf(bp, bleft, "%d\n", maxRvecsz);
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
   else if (!strcmp("tpc", val))
           {char *tpcval = getenv("XRDTPC");
            n = snprintf(bp, bleft, "%s\n", (tpcval ? tpcval : "tpc"));
            bp += n; bleft -= n;
           }
   else if (!strcmp("wan_port", val) && WANPort)
           {n = snprintf(bp, bleft, "%d\n", WANPort);
            bp += n; bleft -= n;
           }
   else if (!strcmp("wan_window", val) && WANPort)
           {n = snprintf(bp, bleft, "%d\n", WANWindow);
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

// The query is elegible for a defered response, indicate we're ok with that
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
   TRACEP(FS, "query " <<qType <<" rc=" <<rc <<" fh=" <<fh.handle);

// Return appropriately
//
   if (SFS_OK != rc) return fsError(rc, XROOTD_MON_QUERY,fp->XrdSfsp->error,0);
   return Response.Send();
}
  
/******************************************************************************/
/*                            d o _ Q o p a q u e                             */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Qopaque(short qopt)
{
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
   XrdSfsFSctl myData;
   const char *opaque, *Act, *AData;
   int fsctl_cmd, rc, dlen = Request.query.dlen;

// Process unstructured as well as structured (path/opaque) requests
//
   if (qopt == kXR_Qopaque)
      {myData.Arg1 = argp->buff; myData.Arg1Len = dlen;
       myData.Arg2 = 0;          myData.Arg1Len = 0;
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

// Preform the actual function using the supplied arguments
//
   rc = osFS->FSctl(fsctl_cmd, myData, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<Act <<AData <<"'");
   if (rc == SFS_OK) Response.Send("");
   return fsError(rc, 0, myError, 0);
}

/******************************************************************************/
/*                             d o _ Q s p a c e                              */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Qspace()
{
   static const int fsctl_cmd = SFS_FSCTL_STATLS;
   XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
   const char *opaque;
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
       if ((argp->buff)+n != opaque-1) strcpy(&argp->buff[n+1], opaque);
      }

// Preform the actual function using the supplied logical FS name
//
   rc = osFS->fsctl(fsctl_cmd, argp->buff, myError, CRED);
   TRACEP(FS, "rc=" <<rc <<" qspace '" <<argp->buff <<"'");
   if (rc == SFS_OK) Response.Send("");
   return fsError(rc, XROOTD_MON_QUERY, myError, argp->buff);
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
   const char *opaque;
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
   return fsError(rc, XROOTD_MON_QUERY, myError, argp->buff);
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
   myIOLen  = ntohl(Request.read.rlen);
              n2hll(Request.read.offset, myOffset);

// Find the file object
//
   if (!FTab || !(myFile = FTab->Get(fh.handle)))
      return Response.Send(kXR_FileNotOpen,
                           "read does not refer to an open file");

// Short circuit processing is read length is zero
//
   TRACEP(FS, pathID <<" fh=" <<fh.handle <<" read " <<myIOLen <<'@' <<myOffset);
   if (!myIOLen) return Response.Send();

// If we are monitoring, insert a read entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_rd(myFile->Stats.FileID, Request.read.rlen,
                                                  Request.read.offset);

// See if an alternate path is required, offload the read
//
   if (pathID) return do_Offload(pathID, 0);

// Now read all of the data (do pre-reads first)
//
   return do_ReadAll();
}

/******************************************************************************/
/*                            d o _ R e a d A l l                             */
/******************************************************************************/

// myFile   = file to be read
// myOffset = Offset at which to read
// myIOLen  = Number of bytes to read from file and write to socket
  
int XrdXrootdProtocol::do_ReadAll(int asyncOK)
{
   int rc, xframt, Quantum = (myIOLen > maxBuffsz ? maxBuffsz : myIOLen);
   char *buff;

// If this file is memory mapped, short ciruit all the logic and immediately
// transfer the requested data to minimize latency.
//
   if (myFile->isMMapped)
      {if (myOffset >= myFile->Stats.fSize) return Response.Send();
       if (myOffset+myIOLen <= myFile->Stats.fSize)
          {myFile->Stats.rdOps(myIOLen);
           return Response.Send(myFile->mmAddr+myOffset, myIOLen);
          }
       xframt = myFile->Stats.fSize -myOffset;
       myFile->Stats.rdOps(xframt);
       return Response.Send(myFile->mmAddr+myOffset, xframt);
      }

// If we are sendfile enabled, then just send the file if possible
//
   if (myFile->sfEnabled && myIOLen >= as_minsfsz
   &&  myOffset+myIOLen <= myFile->Stats.fSize)
      {myFile->Stats.rdOps(myIOLen);
       if (myFile->fdNum >= 0)
          return Response.Send(myFile->fdNum, myOffset, myIOLen);
       rc = myFile->XrdSfsp->SendData((XrdSfsDio *)this, myOffset, myIOLen);
       if (rc == SFS_OK)
          {if (!myIOLen)    return 0;
           if (myIOLen < 0) return -1;  // Otherwise retry using read()
          } else return fsError(rc, 0, myFile->XrdSfsp->error, 0);
      }

// If we are in async mode, schedule the read to ocur asynchronously
//
   if (asyncOK && myFile->AsyncMode)
      {if (myIOLen >= as_miniosz && Link->UseCnt() < as_maxperlnk)
          if ((rc = aio_Read()) != -EAGAIN) return rc;
       SI->AsyncRej++;
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
   myFile->Stats.rdOps(myIOLen);
   do {if ((xframt = myFile->XrdSfsp->read(myOffset, buff, Quantum)) <= 0) break;
       if (xframt >= myIOLen) return Response.Send(buff, xframt);
       if (Response.Send(kXR_oksofar, buff, xframt) < 0) return -1;
       myOffset += xframt; myIOLen -= xframt;
       if (myIOLen < Quantum) Quantum = myIOLen;
      } while(myIOLen);

// Determine why we ended here
//
   if (xframt == 0) return Response.Send();
   return fsError(xframt, 0, myFile->XrdSfsp->error, 0);
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
        {myIOLen  = ntohl(ralsp->rlen);
                    n2hll(ralsp->offset, myOffset);
         memcpy((void *)&fh.handle, (const void *)ralsp->fhandle,
                  sizeof(fh.handle));
         TRACEP(FS, "fh=" <<fh.handle <<" read " <<myIOLen <<'@' <<myOffset);
         if (!FTab || !(myFile = FTab->Get(fh.handle)))
            {retc = Response.Send(kXR_FileNotOpen,
                             "preread does not refer to an open file");
             return 1;
            }
         myFile->XrdSfsp->read(myOffset, myIOLen);
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
   struct XrdOucIOVec     rdVec[maxRvecsz+1];
   struct readahead_list *raVec, respHdr;
   long long totSZ;
   XrdSfsXferSize rdVAmt, rdVXfr, xfrSZ;
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
   if (rdVecLen > static_cast<int>(sizeof(rdVec)))
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
   if (!(myFile = FTab->Get(currFH))) return Response.Send(kXR_FileNotOpen,
                                      "readv does not refer to an open file");

// Setup variables for running through the list.
//
   Qleft = Quantum; buffp = argp->buff; rvSeq++;
   rdVBeg = rdVNow = 0; rdVXfr = rdVAmt = 0;

// Now run through the elements
//
   for (i = 0; i < rdVecNum; i++)
       {if (rdVec[i].info != currFH)
           {xfrSZ = myFile->XrdSfsp->readv(&rdVec[rdVNow], i-rdVNow);
            if (xfrSZ != rdVAmt) break;
            rdVNum = i - rdVBeg; rdVXfr += rdVAmt;
            myFile->Stats.rvOps(rdVXfr, rdVNum);
            if (rvMon)
               {Monitor.Agent->Add_rv(myFile->Stats.FileID, htonl(rdVXfr),
                                              htons(rdVNum), rvSeq, vType);
                if (ioMon) for (k = rdVBeg; k < i; k++)
                    Monitor.Agent->Add_rd(myFile->Stats.FileID,
                            htonl(rdVec[k].size), htonll(rdVec[k].offset));
               }
            rdVXfr = rdVAmt = 0;
            if (i == rdVBreak) break;
            rdVBeg = rdVNow = i; currFH = rdVec[i].info;
            memcpy(respHdr.fhandle, &currFH, sizeof(respHdr.fhandle));
            if (!(myFile = FTab->Get(currFH)))
               return Response.Send(kXR_FileNotOpen,
                                    "readv does not refer to an open file");
            }

        if (Qleft < (rdVec[i].size + hdrSZ))
           {if (rdVAmt)
               {xfrSZ = myFile->XrdSfsp->readv(&rdVec[rdVNow], i-rdVNow);
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
        TRACEP(FS,"fh=" <<currFH <<" readV " << xfrSZ <<'@' <<rdVec[i].offset);
       }

// Check if we have an error here. This is indicated when rdVAmt is not zero.
//
   if (rdVAmt)
      {if (xfrSZ >= 0)
          {xfrSZ = SFS_ERROR;
           myFile->XrdSfsp->error.setErrInfo(-ENODATA,"readv past EOF");
          }
       return fsError(xfrSZ, 0, myFile->XrdSfsp->error, 0);
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
   const char *opaque;
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

// An error occured
//
   return fsError(rc, XROOTD_MON_RM, myError, argp->buff);
}

/******************************************************************************/
/*                              d o _ R m d i r                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Rmdir()
{
   int rc;
   const char *opaque;
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

// An error occured
//
   return fsError(rc, XROOTD_MON_RMDIR, myError, argp->buff);
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

// Process: set monitor {off | on} [appid] | info [info]}

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
   const char *opaque;
   char xxBuff[256];
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
       TRACEP(FS, "stat rc=" <<rc <<" fh=" <<fh.handle);
       if (SFS_OK == rc) return Response.Send(xxBuff, StatGen(buf, xxBuff));
       return fsError(rc, 0, fp->XrdSfsp->error, 0);
      }

// Check if we are handling a dig type path
//
   doDig = (digFS && SFS_LCLPATH(argp->buff));

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
       if (rc == SFS_OK) return Response.Send(xxBuff, StatGen(buf, xxBuff));
      }
   return fsError(rc, (doDig ? 0 : XROOTD_MON_STAT), myError, argp->buff);
}

/******************************************************************************/
/*                              d o _ S t a t x                               */
/******************************************************************************/
  
int XrdXrootdProtocol::do_Statx()
{
   static XrdXrootdCallBack statxCB("xstat", XROOTD_MON_STAT);
   int rc;
   const char *opaque;
   char *path, *respinfo = argp->buff;
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
         if (rc != SFS_OK) return fsError(rc, XROOTD_MON_STAT, myError, path);
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

// The sync is elegible for a defered response, indicate we're ok with that
//
   fp->XrdSfsp->error.setErrCB(&syncCB, ReqID.getID());

// Sync the file
//
   rc = fp->XrdSfsp->sync();
   TRACEP(FS, "sync rc=" <<rc <<" fh=" <<fh.handle);
   if (SFS_OK != rc) return fsError(rc, 0, fp->XrdSfsp->error, 0);

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
        TRACEP(FS, "trunc rc=" <<rc <<" sz=" <<theOffset <<" fh=" <<fh.handle);
        if (SFS_OK != rc) return fsError(rc, 0, fp->XrdSfsp->error, 0);

   } else {

       XrdOucErrInfo myError(Link->ID, Monitor.Did, clientPV);
       const char *opaque;

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
       if (SFS_OK != rc) return fsError(rc,XROOTD_MON_TRUNC,myError,argp->buff);
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
   int retc, pathID;
   XrdXrootdFHandle fh(Request.write.fhandle);
   numWrites++;

// Unmarshall the data
//
   myIOLen  = Request.header.dlen;
              n2hll(Request.write.offset, myOffset);
   pathID   = static_cast<int>(Request.write.pathid);

// Find the file object
//                                                                             .
   if (!FTab || !(myFile = FTab->Get(fh.handle)))
      {if (argp && !pathID) return do_WriteNone();
       Response.Send(kXR_FileNotOpen,"write does not refer to an open file");
       return Link->setEtext("write protcol violation");
      }

// If we are monitoring, insert a write entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_wr(myFile->Stats.FileID, Request.write.dlen,
                                                  Request.write.offset);

// If zero length write, simply return
//
   TRACEP(FS, "fh=" <<fh.handle <<" write " <<myIOLen <<'@' <<myOffset);
   if (myIOLen <= 0) return Response.Send();

// See if an alternate path is required
//
   if (pathID) return do_Offload(pathID, 1);

// If we are in async mode, schedule the write to occur asynchronously
//
   if (myFile->AsyncMode && !as_syncw)
      {if (myStalls > as_maxstalls) myStalls--;
          else if (myIOLen >= as_miniosz && Link->UseCnt() < as_maxperlnk)
                  {if ((retc = aio_Write()) != -EAGAIN)
                      {if (retc != -EIO) return retc;
                       myEInfo[0] = SFS_ERROR;
                       myFile->XrdSfsp->error.setErrInfo(retc, "I/O error");
                       return do_WriteNone();
                      }
                  }
       SI->AsyncRej++;
      }

// Just to the i/o now
//
   myFile->Stats.wrOps(myIOLen); // Optimistically correct
   return do_WriteAll();
}
  
/******************************************************************************/
/*                           d o _ W r i t e A l l                            */
/******************************************************************************/

// myFile   = file to be written
// myOffset = Offset at which to write
// myIOLen  = Number of bytes to read from socket and write to file
  
int XrdXrootdProtocol::do_WriteAll()
{
   int rc, Quantum = (myIOLen > maxBuffsz ? maxBuffsz : myIOLen);

// Make sure we have a large enough buffer
//
   if (!argp || Quantum < halfBSize || Quantum > argp->bsize)
      {if ((rc = getBuff(0, Quantum)) <= 0) return rc;}
      else if (hcNow < hcNext) hcNow++;

// Now write all of the data (XrdXrootdProtocol.C defines getData())
//
   while(myIOLen > 0)
        {if ((rc = getData("data", argp->buff, Quantum)))
            {if (rc > 0) 
                {Resume = &XrdXrootdProtocol::do_WriteCont;
                 myBlast = Quantum;
                 myStalls++;
                }
             return rc;
            }
         if ((rc = myFile->XrdSfsp->write(myOffset, argp->buff, Quantum)) < 0)
            {myIOLen  = myIOLen-Quantum; myEInfo[0] = rc;
             return do_WriteNone();
            }
         myOffset += Quantum; myIOLen -= Quantum;
         if (myIOLen < Quantum) Quantum = myIOLen;
        }

// All done
//
   return Response.Send();
}

/******************************************************************************/
/*                          d o _ W r i t e C o n t                           */
/******************************************************************************/

// myFile   = file to be written
// myOffset = Offset at which to write
// myIOLen  = Number of bytes to read from socket and write to file
// myBlast  = Number of bytes already read from the socket
  
int XrdXrootdProtocol::do_WriteCont()
{
   int rc;

// Write data that was finaly finished comming in
//
   if ((rc = myFile->XrdSfsp->write(myOffset, argp->buff, myBlast)) < 0)
      {myIOLen  = myIOLen-myBlast; myEInfo[0] = rc;
       return do_WriteNone();
      }
    myOffset += myBlast; myIOLen -= myBlast;

// See if we need to finish this request in the normal way
//
   if (myIOLen > 0) return do_WriteAll();
   return Response.Send();
}
  
/******************************************************************************/
/*                          d o _ W r i t e N o n e                           */
/******************************************************************************/
  
int XrdXrootdProtocol::do_WriteNone()
{
   int rlen, blen = (myIOLen > argp->bsize ? argp->bsize : myIOLen);

// Discard any data being transmitted
//
   TRACEP(REQ, "discarding " <<myIOLen <<" bytes");
   while(myIOLen > 0)
        {rlen = Link->Recv(argp->buff, blen, readWait);
         if (rlen  < 0) return Link->setEtext("link read error");
         myIOLen -= rlen;
         if (rlen < blen) 
            {myBlen   = 0;
             Resume   = &XrdXrootdProtocol::do_WriteNone;
             return 1;
            }
         if (myIOLen < blen) blen = myIOLen;
        }

// Send our the error message and return
//
   if (!myFile) return
      Response.Send(kXR_FileNotOpen,"write does not refer to an open file");
   if (myEInfo[0]) return fsError(myEInfo[0], 0, myFile->XrdSfsp->error, 0);
   return Response.Send(kXR_FSError, myFile->XrdSfsp->error.getErrText());
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
   myIOLen  = Request.header.dlen;
              n2hll(Request.write.offset, myOffset);

// Find the file object
//                                                                             .
   if (!FTab || !(myFile = FTab->Get(fh.handle)))
      {if (argp && !Request.write.pathid)
          {myIOLen -= myBlast; return do_WriteNone();}
       Response.Send(kXR_FileNotOpen,"write does not refer to an open file");
       return Link->setEtext("write protcol violation");
      }

// If we are monitoring, insert a write entry
//
   if (Monitor.InOut())
      Monitor.Agent->Add_wr(myFile->Stats.FileID, Request.write.dlen,
                                                  Request.write.offset);

// Trace this entry
//
   TRACEP(FS, "fh=" <<fh.handle <<" write " <<myIOLen <<'@' <<myOffset);

// Write data that was already read
//
   if ((rc = myFile->XrdSfsp->write(myOffset, myBuff, myBlast)) < 0)
      {myIOLen  = myIOLen-myBlast; myEInfo[0] = rc;
       return do_WriteNone();
      }
    myOffset += myBlast; myIOLen -= myBlast;

// See if we need to finish this request in the normal way
//
   if (myIOLen > 0) return do_WriteAll();
   return Response.Send();
}
  
/******************************************************************************/
/*                              S e n d F i l e                               */
/******************************************************************************/

int XrdXrootdProtocol::SendFile(int fildes)
{

// Make sure we have some data to send
//
   if (!myIOLen) return 1;

// Send off the data
//
   myIOLen = Response.Send(fildes, myOffset, myIOLen);
   return myIOLen;
}

/******************************************************************************/

int XrdXrootdProtocol::SendFile(XrdOucSFVec *sfvec, int sfvnum)
{
   int i, xframt = 0;

// Make sure we have some data to send
//
   if (!myIOLen) return 1;

// Verify the length, it can't be greater than what the client wants
//
   for (i = 1; i < sfvnum; i++) xframt += sfvec[i].sendsz;
   if (xframt > myIOLen) return 1;

// Send off the data
//
   if (xframt) myIOLen = Response.Send(sfvec, sfvnum, xframt);
      else {myIOLen = 0; Response.Send();}
   return myIOLen;
}

/******************************************************************************/
/*                                 S e t F D                                  */
/******************************************************************************/
  
void XrdXrootdProtocol::SetFD(int fildes)
{
   if (fildes < 0) myFile->sfEnabled = 0;
      else myFile->fdNum = fildes;
}

/******************************************************************************/
/*                       U t i l i t y   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                               f s E r r o r                                */
/******************************************************************************/
  
int XrdXrootdProtocol::fsError(int rc, char opC, XrdOucErrInfo &myError,
                               const char *Path)
{
   int ecode, popt, rs;
   const char *eMsg = myError.getErrText(ecode);

// Process standard errors
//
   if (rc == SFS_ERROR)
      {SI->errorCnt++;
       rc = XProtocol::mapError(ecode);
       if (Path && (rc == kXR_NotFound) && RQLxist && opC
       &&  (popt = RQList.Validate(Path)))
          {if (XrdXrootdMonitor::Redirect())
               XrdXrootdMonitor::Redirect(Monitor.Did,
                                          Route[popt].Host[rdType],
                                          Route[popt].Port[rdType],
                                          opC|XROOTD_MON_REDLOCAL, Path);
           rs = Response.Send(kXR_redirect,
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
       if (ecode <= 0) ecode = (ecode ? -ecode : Port);
       if (XrdXrootdMonitor::Redirect() && Path && opC)
           XrdXrootdMonitor::Redirect(Monitor.Did, eMsg, Port, opC, Path);
       TRACEI(REDIR, Response.ID() <<"redirecting to " << eMsg <<':' <<ecode);
       rs = Response.Send(kXR_redirect, ecode, eMsg, myError.getErrTextLen());
       if (myError.extData()) myError.Reset();
       return rs;
      }

// Process the deferal. We also synchronize sending the deferal response with
// sending the actual defered response by calling Done() in the callback object.
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
/* Private:                     l o g L o g i n                               */
/******************************************************************************/
  
void XrdXrootdProtocol::logLogin(bool xauth)
{
   const char *uName;
   char lBuff[512];

// Determine client name
//
   if (xauth) uName = (Client->name ? Client->name : "nobody");
      else    uName = 0;

// Format the line
//
   sprintf(lBuff, "%s %s %slogin%s",
                  (clientPV & XrdOucEI::uPrip ? "pvt"    : "pub"),
                  (clientPV & XrdOucEI::uIPv4 ? "IPv4"   : "IPv6"),
                  (Status   & XRD_ADMINUSER   ? "admin " : ""),
                  (xauth                      ? " as"    : ""));

// Document the login
//
   eDest.Log(SYS_LOG_01, "Xeq", Link->ID, lBuff, uName);
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
         char Buff[2048];
   const char *bP = Buff;

   if (Client == &Entity) bP = (Monitor.Auths() ? "" : 0);
      else snprintf(Buff,sizeof(Buff), "&p=%s&n=%s&h=%s&o=%s&r=%s&g=%s&m=%s",
                     Client->prot,
                    (Client->name ? Client->name : ""),
                    (Client->host ? Client->host : ""),
                    (Client->vorg ? Client->vorg : ""),
                    (Client->role ? Client->role : ""),
                    (Client->grps ? Client->grps : ""),
                    (Client->moninfo ? Client->moninfo : "")
                   );

   Monitor.Report(bP);
}
  
/******************************************************************************/
/*                               r p C h e c k                                */
/******************************************************************************/
  
int XrdXrootdProtocol::rpCheck(char *fn, const char **opaque)
{
   char *cp;

   if (*fn != '/') return 1;

   if (!(cp = index(fn, '?'))) *opaque = 0;
      else {*cp = '\0'; *opaque = cp+1;
            if (!**opaque) *opaque = 0;
           }

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
/*                               S t a t G e n                                */
/******************************************************************************/
  
#define XRDXROOTD_STAT_CLASSNAME XrdXrootdProtocol
#include "XrdXrootd/XrdXrootdStat.icc"

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
