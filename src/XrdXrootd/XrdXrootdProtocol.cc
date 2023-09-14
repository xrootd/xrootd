/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d P r o t o c o l . c c                   */
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

#include <poll.h>
 
#include "XrdVersion.hh"

#include "XProtocol/XProtocol.hh"

#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XrdNet/XrdNetIF.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSec/XrdSecProtect.hh"
#include "XrdSfs/XrdSfsFlags.hh"
#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdTls/XrdTls.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdFileLock.hh"
#include "XrdXrootd/XrdXrootdFileLock1.hh"
#include "XrdXrootd/XrdXrootdMonFile.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdPio.hh"
#include "XrdXrootd/XrdXrootdPgwCtl.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdXPath.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

namespace XrdXrootd
{
XrdSysError           eLog(0, "Xrootd");
XrdBuffManager       *BPool;
XrdScheduler         *Sched;
XrdXrootdStats       *SI;
}

XrdSysTrace           XrdXrootdTrace("Xrootd");

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
XrdXrootdXPath        XrdXrootdProtocol::RPList;
XrdXrootdXPath        XrdXrootdProtocol::RQList;
XrdXrootdXPath        XrdXrootdProtocol::XPList;
XrdSfsFileSystem     *XrdXrootdProtocol::osFS;
XrdSfsFileSystem     *XrdXrootdProtocol::digFS    = 0;
XrdXrootdFileLock    *XrdXrootdProtocol::Locker;
XrdSecService        *XrdXrootdProtocol::CIA      = 0;
XrdSecProtector      *XrdXrootdProtocol::DHS      = 0;
XrdTlsContext        *XrdXrootdProtocol::tlsCtx   = 0;
XrdScheduler         *XrdXrootdProtocol::Sched;
XrdBuffManager       *XrdXrootdProtocol::BPool;
XrdSysError          &XrdXrootdProtocol::eDest = XrdXrootd::eLog;
XrdNetPMark          *XrdXrootdProtocol::PMark    = 0;
XrdXrootdStats       *XrdXrootdProtocol::SI;
XrdXrootdJob         *XrdXrootdProtocol::JobCKS   = 0;
char                 *XrdXrootdProtocol::JobCKT   = 0;
XrdOucTList          *XrdXrootdProtocol::JobCKTLST= 0;
XrdOucReqID          *XrdXrootdProtocol::PrepID   = 0;
uint64_t              XrdXrootdProtocol::fsFeatures = 0;

char                 *XrdXrootdProtocol::Notify = 0;
const char           *XrdXrootdProtocol::myCName= 0;
int                   XrdXrootdProtocol::myCNlen= 0;
int                   XrdXrootdProtocol::hailWait;
int                   XrdXrootdProtocol::readWait;
int                   XrdXrootdProtocol::Port;
int                   XrdXrootdProtocol::Window;
int                   XrdXrootdProtocol::tlsPort = 0;
char                  XrdXrootdProtocol::isRedir = 0;
char                  XrdXrootdProtocol::JobLCL  = 0;
char                  XrdXrootdProtocol::JobCKCGI=0;
XrdNetSocket         *XrdXrootdProtocol::AdminSock= 0;

int                   XrdXrootdProtocol::hcMax        = 28657; // const for now
int                   XrdXrootdProtocol::maxBuffsz;
int                   XrdXrootdProtocol::maxTransz    = 262144; // 256KB
int                   XrdXrootdProtocol::maxReadv_ior =
                      XrdXrootdProtocol::maxTransz-(int)sizeof(readahead_list);
int                   XrdXrootdProtocol::as_maxperlnk = 8;   // Max ops per link
int                   XrdXrootdProtocol::as_maxperreq = 8;   // Max ops per request
int                   XrdXrootdProtocol::as_maxpersrv = 4096;// Max ops per server
int                   XrdXrootdProtocol::as_seghalf   = 32768;
int                   XrdXrootdProtocol::as_segsize   = 65536;
int                   XrdXrootdProtocol::as_miniosz   = 98304;
#ifdef __solaris__
int                   XrdXrootdProtocol::as_minsfsz   = 1;
#else
int                   XrdXrootdProtocol::as_minsfsz   = 8192;
#endif
int                   XrdXrootdProtocol::as_maxstalls = 4;
short                 XrdXrootdProtocol::as_okstutter = 1; // For 64K unit
short                 XrdXrootdProtocol::as_timeout   = 45;
bool                  XrdXrootdProtocol::as_force     = false;
bool                  XrdXrootdProtocol::as_aioOK     = true;
bool                  XrdXrootdProtocol::as_nosf      = false;
bool                  XrdXrootdProtocol::as_syncw     = false;

const char           *XrdXrootdProtocol::myInst  = 0;
const char           *XrdXrootdProtocol::TraceID = "Protocol";
int                   XrdXrootdProtocol::RQLxist = 0;
int                   XrdXrootdProtocol::myPID = static_cast<int>(getpid());

int                   XrdXrootdProtocol::myRole = 0;
int                   XrdXrootdProtocol::myRolf = 0;

gid_t                 XrdXrootdProtocol::myGID  = 0;
uid_t                 XrdXrootdProtocol::myUID  = 0;
int                   XrdXrootdProtocol::myGNLen= 1;
int                   XrdXrootdProtocol::myUNLen= 1;
const char           *XrdXrootdProtocol::myGName= "?";
const char           *XrdXrootdProtocol::myUName= "?";
time_t                XrdXrootdProtocol::keepT  = 86400; // 24 hours

int                   XrdXrootdProtocol::PrepareLimit = -1;
bool                  XrdXrootdProtocol::PrepareAlt = false;
bool                  XrdXrootdProtocol::LimitError = true;

struct XrdXrootdProtocol::RD_Table XrdXrootdProtocol::Route[RD_Num];
struct XrdXrootdProtocol::RC_Table XrdXrootdProtocol::RouteClient;
int                   XrdXrootdProtocol::OD_Stall = 33;
bool                  XrdXrootdProtocol::OD_Bypass= false;
bool                  XrdXrootdProtocol::OD_Redir = false;

bool                  XrdXrootdProtocol::CL_Redir = false;

bool                  XrdXrootdProtocol::isProxy  = false;

int                   XrdXrootdProtocol::usxMaxNsz= kXR_faMaxNlen;
int                   XrdXrootdProtocol::usxMaxVsz= kXR_faMaxVlen;
char                 *XrdXrootdProtocol::usxParms = 0;

char                  XrdXrootdProtocol::tlsCap   = 0;
char                  XrdXrootdProtocol::tlsNot   = 0;

RAtomic_int           XrdXrootdProtocol::srvrAioOps = {0};

/******************************************************************************/
/*            P r o t o c o l   M a n a g e m e n t   S t a c k s             */
/******************************************************************************/
  
XrdObjectQ<XrdXrootdProtocol>
            XrdXrootdProtocol::ProtStack("ProtStack",
                                       "xroot protocol anchor");
  
/******************************************************************************/
/*                       P r o t o c o l   L o a d e r                        */
/*                        X r d g e t P r o t o c o l                         */
/******************************************************************************/
  
// This protocol can live in a shared library. The interface below is used by
// the protocol driver to obtain a copy of the protocol object that can be used
// to decide whether or not a link is talking a particular protocol.
//
XrdVERSIONINFO(XrdgetProtocol,xrootd);

extern "C"
{
XrdProtocol *XrdgetProtocol(const char *pname, char *parms,
                              XrdProtocol_Config *pi)
{
   XrdProtocol *pp = 0;
   const char *txt = "completed.";

// Put up the banner
//
   pi->eDest->Say("Copr.  2012 Stanford University, xroot protocol "
                   kXR_PROTOCOLVSTRING, " version ", XrdVERSION);
   pi->eDest->Say("++++++ xroot protocol initialization started.");

// Return the protocol object to be used if static init succeeds
//
   if (XrdXrootdProtocol::Configure(parms, pi))
      pp = (XrdProtocol *)new XrdXrootdProtocol();
      else txt = "failed.";
    pi->eDest->Say("------ xroot protocol initialization ", txt);
   return pp;
}
}

/******************************************************************************/
/*           P r o t o c o l   P o r t   D e t e r m i n a t i o n            */
/*                    X r d g e t P r o t o c o l P o r t                     */
/******************************************************************************/

// This function is called early on to determine the port we need to use. The
// default is ostensibly 1094 but can be overridden; which we allow.
//
XrdVERSIONINFO(XrdgetProtocolPort,xrootd);

extern "C"
{
int XrdgetProtocolPort(const char *pname, char *parms, XrdProtocol_Config *pi)
{

// Figure out what port number we should return. In practice only one port
// number is allowed. However, we could potentially have a clustered port
// and several unclustered ports. So, we let this practicality slide.
//
   if (pi->Port < 0) return 1094;
   return pi->Port;
}
}
  
/******************************************************************************/
/*               X r d P r o t o c o l X r o o t d   C l a s s                */
/******************************************************************************/

namespace
{
XrdSfsXioImpl SfsXioImpl(&XrdXrootdProtocol::Buffer,
                         &XrdXrootdProtocol::Reclaim);
}

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdXrootdProtocol::XrdXrootdProtocol() 
                    : XrdProtocol("xroot protocol handler"),
                      XrdSfsXio(SfsXioImpl),
                      ProtLink(this), Entity(0), AppName(0)
{
   Reset();
}
  
/******************************************************************************/
/* protected:                     g e t S I D                                 */
/******************************************************************************/

unsigned int XrdXrootdProtocol::getSID()
{
   static XrdSysMutex  SidMutex;
   static unsigned int Sid = 1;
   unsigned int theSid;

// Generate unqiue number for this server instance
//
   AtomicBeg(SidMutex);
   theSid = AtomicInc(Sid);
   AtomicEnd(SidMutex);
   return theSid;
}
  
/******************************************************************************/
/*                                 M a t c h                                  */
/******************************************************************************/

#define TRACELINK lp
  
XrdProtocol *XrdXrootdProtocol::Match(XrdLink *lp)
{
static  const int hsSZ = sizeof(ClientInitHandShake);
        char hsbuff[hsSZ];
        struct ClientInitHandShake   *hsData = (ClientInitHandShake *)hsbuff;

static  struct hs_response
               {kXR_unt16 streamid;
                kXR_unt16 status;
                kXR_unt32 rlen;   // Specified as kXR_int32 in doc!
                kXR_unt32 pval;   // Specified as kXR_int32 in doc!
                kXR_unt32 styp;   // Specified as kXR_int32 in doc!
               } hsresp={0, 0, htonl(8), htonl(kXR_PROTOCOLVERSION),
                         (isRedir ? htonl((unsigned int)kXR_LBalServer)
                                  : htonl((unsigned int)kXR_DataServer))};
XrdXrootdProtocol *xp;
int dlen, rc;

// Peek at the first 20 bytes of data
//
   if ((dlen = lp->Peek(hsbuff, hsSZ, hailWait)) < hsSZ)
      {if (dlen <= 0) lp->setEtext("handshake not received");
       return (XrdProtocol *)0;
      }

// Trace the data
//
// TRACEI(REQ, "received: " <<Trace->bin2hex(hsbuff,dlen));

// Verify that this is our protocol
//
   hsData->fourth  = ntohl(hsData->fourth);
   hsData->fifth   = ntohl(hsData->fifth);
   if (hsData->first || hsData->second || hsData->third
   ||  hsData->fourth != 4 || hsData->fifth != ROOTD_PQ) return 0;

// Send the handshake response. We used optimize the subsequent protocol
// request sent with handshake but the protocol request is now overloaded.
//
   rc = lp->Send((char *)&hsresp, sizeof(hsresp));

// Verify that our handshake response was actually sent
//
   if (!rc)
      {lp->setEtext("handshake failed");
       return (XrdProtocol *)0;
      }

// We can now read all 20 bytes and discard them (no need to wait for it)
//
   if (lp->Recv(hsbuff, hsSZ) != hsSZ)
      {lp->setEtext("reread failed");
       return (XrdProtocol *)0;
      }

// Get a protocol object off the stack (if none, allocate a new one)
//
   if (!(xp = ProtStack.Pop())) xp = new XrdXrootdProtocol();

// Bind the protocol to the link and return the protocol
//
   SI->Bump(SI->Count);
   xp->Link = lp;
   xp->Response.Set(lp);
   strcpy(xp->Entity.prot, "host");
   xp->Entity.host = (char *)lp->Host();
   xp->Entity.addrInfo = lp->AddrInfo();
   return (XrdProtocol *)xp;
}
 
/******************************************************************************/
/*                               P r o c e s s                                */
/******************************************************************************/

#undef  TRACELINK
#define TRACELINK Link
  
int XrdXrootdProtocol::Process(XrdLink *lp) // We ignore the argument here
{
   int rc;
   kXR_unt16 reqID;

// Check if we are servicing a slow link
//
   if (Resume)
      {if (myBlen && (rc = getData("data", myBuff, myBlen)) != 0) return rc;
          else if ((rc = (*this.*Resume)()) != 0) return rc;
                  else {Resume = 0; return 0;}
      }

// Read the next request header
//
   if ((rc=getData("request",(char *)&Request,sizeof(Request))) != 0) return rc;

// Check if we need to copy the request prior to unmarshalling it
//
   reqID = ntohs(Request.header.requestid);
   if (reqID != kXR_sigver && NEED2SECURE(Protect)(Request))
      {memcpy(&sigReq2Ver, &Request, sizeof(ClientRequest));
       sigNeed = true;
      }

// Deserialize the data
//
   Request.header.requestid = reqID;
   Request.header.dlen      = ntohl(Request.header.dlen);
   Response.Set(Request.header.streamid);
   TRACEP(REQ, "req=" <<XProtocol::reqName(reqID)
               <<" dlen=" <<Request.header.dlen);

// Every request has an associated data length. It better be >= 0 or we won't
// be able to know how much data to read.
//
   if (Request.header.dlen < 0)
      {Response.Send(kXR_ArgInvalid, "Invalid request data length");
       return Link->setEtext("protocol data length error");
      }

// Process sigver requests now as they appear ahead of a request
//
   if (reqID == kXR_sigver) return ProcSig();

// Read any argument data at this point, except when the request is a write.
// The argument may have to be segmented and we're not prepared to do that here.
//
   if (reqID != kXR_write && reqID != kXR_pgwrite && Request.header.dlen)
      {if (!argp || Request.header.dlen+1 > argp->bsize)
          {if (argp) BPool->Release(argp);
           if (!(argp = BPool->Obtain(Request.header.dlen+1)))
              {Response.Send(kXR_ArgTooLong, "Request argument is too long");
               return 0;
              }
           hcNow = hcPrev; halfBSize = argp->bsize >> 1;
          }
       argp->buff[Request.header.dlen] = '\0';
       if ((rc = getData("arg", argp->buff, Request.header.dlen)))
          {Resume = &XrdXrootdProtocol::Process2; return rc;}
      }

// Continue with request processing at the resume point
//
   return Process2();
}

/******************************************************************************/
/*                      p r i v a t e   P r o c e s s 2                       */
/******************************************************************************/
  
int XrdXrootdProtocol::Process2()
{
// If we are verifying requests, see if this request needs to be verified
//
   if (sigNeed)
      {const char *eText = "Request not signed";
       if (!sigHere || (eText = Protect->Verify(sigReq,sigReq2Ver,argp->buff)))
          {Response.Send(kXR_SigVerErr, eText);
           TRACEP(REQ, "req=" <<XProtocol::reqName(Request.header.requestid)
                  <<" verification failed; " <<eText);
           SI->Bump(SI->badSCnt);
           return Link->setEtext(eText);
          } else {
           SI->Bump(SI->aokSCnt);
           sigNeed = sigHere = false;
         }
      } else {
       if (sigHere)
          {TRACEP(REQ, "req=" <<XProtocol::reqName(Request.header.requestid)
                     <<" unneeded signature discarded.");
           if (sigWarn)
              {eDest.Emsg("Protocol","Client is needlessly signing requests.");
               sigWarn = false;
              }
           SI->Bump(SI->ignSCnt);
           sigHere = false;
          }
      }

// If the user is not yet logged in, restrict what the user can do
//
   if (!Status)
      switch(Request.header.requestid)
            {case kXR_login:    return do_Login();
             case kXR_protocol: return do_Protocol();
             case kXR_bind:     return do_Bind();
             default:           Response.Send(kXR_InvalidRequest,
                                "Invalid request; user not logged in");
                                return Link->setEtext("request without login");
            }

// Help the compiler, select the the high activity requests (the ones with
// file handles) in a separate switch statement. A special case exists for
// sync() which return with a callback, so handle it here. Note that stat(fh)
// normally never does a callback but historically we allowed it to do so.
// We maintain that capability even when it's likely never used.
//
   switch(Request.header.requestid)   // First, the ones with file handles
         {case kXR_read:     return do_Read();
          case kXR_readv:    return do_ReadV();
          case kXR_write:    return do_Write();
          case kXR_writev:   return do_WriteV();
          case kXR_pgread:   return do_PgRead();
          case kXR_pgwrite:  return do_PgWrite();
          case kXR_sync:     ReqID.setID(Request.header.streamid);
                             return do_Sync();
          case kXR_close:    ReqID.setID(Request.header.streamid);
                             return do_Close();
          case kXR_stat:     if (!Request.header.dlen)
                                {ReqID.setID(Request.header.streamid);
                                 return do_Stat();
                                }
                             break;
          case kXR_truncate: ReqID.setID(Request.header.streamid);
                             if (!Request.header.dlen) return do_Truncate();
                             break;
          case kXR_query:    if (!Request.header.dlen) return do_Qfh();
                             break;
          case kXR_chkpoint: return do_ChkPnt();
          default:           break;
         }

// Now select the requests that do not need authentication
//
   switch(Request.header.requestid)
         {case kXR_protocol: return do_Protocol();   // dlen ignored
          case kXR_ping:     return do_Ping();       // dlen ignored
          default:           break;
         }

// Force authentication at this point, if need be
//
   if (Status & XRD_NEED_AUTH)
      {int rc;
       if (Request.header.requestid == kXR_auth) rc = do_Auth();
          else {Response.Send(kXR_InvalidRequest,
                              "Invalid request; user not authenticated");
                rc = -1;
               }
       if (tlsCtx) XrdTls::ClearErrorQueue();
       return rc;
      }

// Construct request ID as the following functions are async eligible
//
   ReqID.setID(Request.header.streamid);

// Process items that don't need arguments but may have them
//
   switch(Request.header.requestid)
         {case kXR_endsess:   return do_Endsess();
          default:            break;
         }

// All remaining requests require an argument. Make sure we have one
//
   if (!argp || !Request.header.dlen)
      {Response.Send(kXR_ArgMissing, "Required argument not present");
       return 0;
      }

// All of the subsequent requests can be redirected and are subject to
// prefunctory redirection which we check here.
//
   if (CL_Redir && !Link->hasBridge())
      {bool doRdr = false;
       if (Link->AddrInfo()->isPrivate()) rdType = 1;
            if (RouteClient.pvtIP && rdType) doRdr = true;
       else if (RouteClient.lclDom && XrdNetIF::InDomain( Link->AddrInfo()))
               doRdr = true;
       else if (RouteClient.DomCnt)
               {XrdOucString hName = Link->Host();
                for (int i = 0; i < RouteClient.DomCnt; i++)
                    {if (hName.endswith(RouteClient.Domain[i]))
                        {doRdr = true; break;}
                    }
               }
       if (doRdr)
          {Response.Send(kXR_redirect,Route[RD_client].Port[rdType],
                                      Route[RD_client].Host[rdType]);
           return -1;
          }
      }

// Process items that keep own statistics
//
   switch(Request.header.requestid)
         {case kXR_open:      return do_Open();
          case kXR_gpfile:    return do_gpFile();
          default:            break;
         }

// Update misc stats count
//
   SI->Bump(SI->miscCnt);

// Now process whatever we have
//
   switch(Request.header.requestid)
         {case kXR_chmod:     return do_Chmod();
          case kXR_dirlist:   return do_Dirlist();
          case kXR_fattr:     return do_FAttr();
          case kXR_locate:    return do_Locate();
          case kXR_mkdir:     return do_Mkdir();
          case kXR_mv:        return do_Mv();
          case kXR_query:     return do_Query();
          case kXR_prepare:   return do_Prepare();
          case kXR_rm:        return do_Rm();
          case kXR_rmdir:     return do_Rmdir();
          case kXR_set:       return do_Set();
          case kXR_stat:      return do_Stat();
          case kXR_statx:     return do_Statx();
          case kXR_truncate:  return do_Truncate();
          default:            break;
         }

// Whatever we have, it's not valid
//
   Response.Send(kXR_InvalidRequest, "Invalid request code");
   return 0;
}

/******************************************************************************/
/*                               P r o c S i g                                */
/******************************************************************************/
  
int XrdXrootdProtocol::ProcSig()
{
   int rc;

// Check if we completed reading the signature and if so, we are done
//
   if (sigRead)
      {sigRead = false;
       sigHere = true;
       return 0;
      }

// Verify that the hash is not longer that what we support and is present
//
   if (Request.header.dlen <= 0
   ||  Request.header.dlen > (int)sizeof(sigBuff))
      {Response.Send(kXR_ArgInvalid, "Invalid signature data length");
       return Link->setEtext("signature data length error");
      }

// Save relevant information for the next round
//
   memcpy(&sigReq, &Request, sizeof(ClientSigverRequest));
   sigReq.header.dlen = htonl(Request.header.dlen);

// Now read in the signature
//
   sigRead = true;
   if ((rc = getData("arg", sigBuff, Request.header.dlen)))
      {Resume = &XrdXrootdProtocol::ProcSig; return rc;}
   sigRead = false;

// All done
//
   sigHere = true;
   return 0;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/

#undef  TRACELINK
#define TRACELINK Link
  
void XrdXrootdProtocol::Recycle(XrdLink *lp, int csec, const char *reason)
{
   char *sfxp, ctbuff[24], buff[128], Flags = (reason ? XROOTD_MON_FORCED : 0);
   const char *What;
   XrdSysMutexHelper recycleHelper(unbindMutex);

// Check for disconnect or unbind
//
   if (Status == XRD_BOUNDPATH) {What = "unbind"; Flags |= XROOTD_MON_BOUNDP;}
      else What = "disc";

// Document the disconnect or undind
//
   if (lp)
      {XrdSysTimer::s2hms(csec, ctbuff, sizeof(ctbuff));
       if (reason && strcmp(reason, "hangup"))
          {snprintf(buff, sizeof(buff), "%s (%s)", ctbuff, reason);
           sfxp = buff;
          } else sfxp = ctbuff;

       eDest.Log(SYS_LOG_02, "Xeq", lp->ID, (char *)What, sfxp);
      }

// Handle any waiting read on this link. This is a tricky proposition because
// we don't know if the thread is waiting to run or not. However, we will
// indicate that gdFail was already called and should the thread run, it will
// promptly exit should it ever run again. That way, we handle the cleanup.
//
   if (gdCtl.linkWait.fetch_or(GetDataCtl::Terminate) == GetDataCtl::Active
   && (gdCtl.Status == GetDataCtl::inData
   ||  gdCtl.Status == GetDataCtl::inDataIov)) gdCtl.CallBack->gdFail();

// If this is a bound stream then we cannot release the resources until
// all activity ceases on this stream (i.e., lp == 0). This is only relevant for
// writes that read from the link. if we are still tagged as active and not
// waiting for link activity then ask to be signalled once activity stops.
// Otherwise, redrive the parallel I/O so that it cleans up.
//
   if (lp && Status == XRD_BOUNDPATH)
      {streamMutex.Lock();
       isDead = isNOP = true;
       if (isActive)
          {if (isLinkWT)
              {streamMutex.UnLock();
               do_OffloadIO();
              } else {
               while(isActive)
                    {XrdSysCondVar2 aioDone(streamMutex);
                     endNote = &aioDone;
                     aioDone.Wait();
                     endNote = 0;
                    }
               streamMutex.UnLock();
              }
          } else streamMutex.UnLock();
       if (lp) return;  // Async close
      }

// Release all appendages
//
   Cleanup();

// If we are monitoring logins then we are also monitoring disconnects. We do
// this after cleanup so that close records can be generated before we cut a
// disconnect record. This then requires we clear the monitor object here.
// We and the destrcutor are the only ones who call cleanup and a deletion
// will call the monitor clear method. So, we won't leak memeory.
//
   if (Monitor.Logins()) Monitor.Agent->Disc(Monitor.Did, csec, Flags);
   if (Monitor.Fstat() ) XrdXrootdMonFile::Disc(Monitor.Did);
   Monitor.Clear();

// Set fields to starting point (debugging mostly)
//
   Reset();

// Push ourselves on the stack
//
   if (Response.isOurs()) ProtStack.Push(&ProtLink);
}

/******************************************************************************/
/*                               S t a t G e n                                */
/******************************************************************************/
  
int XrdXrootdProtocol::StatGen(struct stat &buf, char *xxBuff, int xxLen,
                               bool xtnd)
{
   const mode_t isReadable = (S_IRUSR | S_IRGRP | S_IROTH);
   const mode_t isWritable = (S_IWUSR | S_IWGRP | S_IWOTH);
   const mode_t isExecable = (S_IXUSR | S_IXGRP | S_IXOTH);
   uid_t theuid;
   gid_t thegid;
   union {long long uuid; struct {int hi; int lo;} id;} Dev;
   long long fsz;
   int m, n, flags = 0;

// Get the right uid/gid
//
   theuid = (Client && Client->uid ? Client->uid : myUID);
   thegid = (Client && Client->gid ? Client->gid : myGID);

// Compute the unique id
//
   Dev.id.lo = buf.st_ino;
   Dev.id.hi = buf.st_dev;

// Compute correct setting of the readable flag
//
   if (buf.st_mode & isReadable
   &&((buf.st_mode & S_IRUSR && theuid == buf.st_uid)
   || (buf.st_mode & S_IRGRP && thegid == buf.st_gid)
   ||  buf.st_mode & S_IROTH)) flags |= kXR_readable;

// Compute correct setting of the writable flag
//
   if (buf.st_mode & isWritable
   &&((buf.st_mode & S_IWUSR && theuid == buf.st_uid)
   || (buf.st_mode & S_IWGRP && thegid == buf.st_gid)
   ||  buf.st_mode & S_IWOTH)) flags |= kXR_writable;

// Compute correct setting of the execable flag
//
   if (buf.st_mode & isExecable
   &&((buf.st_mode & S_IXUSR && theuid == buf.st_uid)
   || (buf.st_mode & S_IXGRP && thegid == buf.st_gid)
   ||  buf.st_mode & S_IXOTH)) flags |= kXR_xset;

// Compute the other flag settings
//
        if (!Dev.uuid)                         flags |= kXR_offline;
        if (S_ISDIR(buf.st_mode))              flags |= kXR_isDir;
   else if (!S_ISREG(buf.st_mode))             flags |= kXR_other;
   else{if (buf.st_mode & XRDSFS_POSCPEND)     flags |= kXR_poscpend;
        if ((buf.st_rdev & XRDSFS_RDVMASK) == 0)
           {if (buf.st_rdev & XRDSFS_OFFLINE)  flags |= kXR_offline;
            if (buf.st_rdev & XRDSFS_HASBKUP)  flags |= kXR_bkpexist;
           }
       }
   fsz = static_cast<long long>(buf.st_size);

// Format the default response: <devid> <size> <flags> <mtime>
//
   m = snprintf(xxBuff, xxLen, "%lld %lld %d %ld",
                Dev.uuid, fsz, flags, buf.st_mtime);
// if (!xtnd || m >= xxLen) return xxLen;
//

// Format extended response: <ctime> <atime> <mode>
//
   char *origP = xxBuff;
   char *nullP = xxBuff + m++;
   xxBuff += m; xxLen -= m;
   n = snprintf(xxBuff, xxLen, "%ld %ld %04o ",
                buf.st_ctime, buf.st_atime, buf.st_mode&07777);
   if (n >= xxLen) return m;
   xxBuff += n; xxLen -= n;

// Tack on owner
//
   if (buf.st_uid == myUID)
      {if (myUNLen >= xxLen) return m;
       strcpy(xxBuff, myUName);
       n = myUNLen;
      } else {
       if (!(n = XrdOucUtils::UidName(buf.st_uid,xxBuff,xxLen,keepT))) return m;
      }
   xxBuff += n;
   *xxBuff++ = ' ';
   xxLen -= (n+1);

// Tack on group
//
   if (buf.st_gid == myGID)
      {if (myGNLen >= xxLen) return m;
       strcpy(xxBuff, myGName);
       n = myGNLen;
      } else {
       if (!(n = XrdOucUtils::GidName(buf.st_gid,xxBuff,xxLen,keepT))) return m;
      }
   xxBuff += n+1;

// All done, return full response
//
   *nullP = ' ';
   return xxBuff - origP;
}

/******************************************************************************/
/*                                 S t a t s                                  */
/******************************************************************************/
  
int XrdXrootdProtocol::Stats(char *buff, int blen, int do_sync)
{
// Synchronize statistics if need be
//
   if (do_sync)
      {SI->statsMutex.Lock();
       SI->readCnt += numReads;
       cumReads += numReads; numReads  = 0;
       SI->prerCnt += numReadP;
       cumReadP += numReadP; numReadP = 0;

       SI->rvecCnt += numReadV;
       cumReadV += numReadV; numReadV = 0;
       SI->rsegCnt += numSegsV;
       cumSegsV += numSegsV; numSegsV = 0;

       SI->wvecCnt += numWritV;
       cumWritV += numWritV; numWritV = 0;
       SI->wsegCnt += numSegsW;
       cumSegsW += numSegsW, numSegsW = 0;

       SI->writeCnt += numWrites;
       cumWrites+= numWrites;numWrites = 0;
       SI->statsMutex.UnLock();
      }

// Now return the statistics
//
   return SI->Stats(buff, blen, do_sync);
}
  
/******************************************************************************/
/*                     X r d S f s X i o   M e t h o d s                      */
/******************************************************************************/
/******************************************************************************/
/* Static:                        B u f f e r                                 */
/******************************************************************************/
  
char *XrdXrootdProtocol::Buffer(XrdSfsXioHandle h, int *bsz)
{
   XrdBuffer *xbP = (XrdBuffer *)h;

   if (h)
      {if (bsz) *bsz = xbP->bsize;
       return xbP->buff;
      }
   if (bsz) *bsz = 0;
   return 0;
}

/******************************************************************************/
/*                                 C l a i m                                  */
/******************************************************************************/

XrdSfsXioHandle XrdXrootdProtocol::Claim(const char *buff, int datasz,
                                                           int minasz)
{

// Qualify swap choice
//
   if (minasz >= argp->bsize || datasz >= argp->bsize/2) return Swap(buff);
   errno = 0;
   return 0;
}
  
/******************************************************************************/
/* Static:                       R e c l a i m                                */
/******************************************************************************/
  
void XrdXrootdProtocol::Reclaim(XrdSfsXioHandle h)
{

   if (h) BPool->Release((XrdBuffer *)h);
}
  
/******************************************************************************/
/*                             S t r e a m N O P                              */
/******************************************************************************/

void XrdXrootdProtocol::StreamNOP()
{

// Mark this stream as not operation if it is not the control strea,
//
   if (PathID)
      {streamMutex.Lock();
       isNOP = true;
       streamMutex.UnLock();
      }
}
  
/******************************************************************************/
/*                                  S w a p                                   */
/******************************************************************************/
  
XrdSfsXioHandle XrdXrootdProtocol::Swap(const char *buff, XrdSfsXioHandle h)
{
   XrdBuffer *oldBP = argp;

// Verify the context and linkage and if OK, swap buffers
//
   if (Request.header.requestid != kXR_write) errno = ENOTSUP;
      else if (buff != argp->buff) errno = EINVAL;
              else {if (h)
                       {argp = (XrdBuffer *)h;
                        return oldBP;
                       } else {
                        argp = BPool->Obtain(argp->bsize);
                        if (argp) return oldBP;
                        argp = oldBP;
                        errno = ENOBUFS;
                       }
                   }
   return 0;
}
  
/******************************************************************************/
/*                          V e r i f y S t r e a m                           */
/******************************************************************************/

XrdXrootdProtocol *XrdXrootdProtocol::VerifyStream(int &rc, int pID, bool lok)
{
   XrdXrootdProtocol *pp;

// Verify that the path actually exists
//
   if (pID >= maxStreams || !(pp = Stream[pID]))
      {rc = Response.Send(kXR_ArgInvalid, "invalid path ID");
       return 0;
      }

// Verify that this path is still functional
//
   pp->streamMutex.Lock();
   if (pp->isNOP)
      {pp->streamMutex.UnLock();
       rc = Response.Send(kXR_ArgInvalid, "path ID is not operational");
       return 0;
      }

// All done!
//
   if (!lok) pp->streamMutex.UnLock();
   return pp;
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                              C h e c k S u m                               */
/******************************************************************************/
  
int XrdXrootdProtocol::CheckSum(XrdOucStream *Stream, char **argv, int argc)
{
   int rc, ecode;

// The arguments must have <name> <cstype> <path> <tident> [name] (argc >= 4)
//
   if (argc < 4)
      {Stream->PutLine("Internal error; not enough checksum args!");
       return 8;
      }

// Construct the error information
//
   XrdOucEnv     myEnv;
   XrdOucErrInfo myInfo(argv[3], &myEnv);

// Add username, if present
//
   if (argc > 4 && *argv[4]) myEnv.Put("request.name", argv[4]);

// Issue the checksum calculation (that's all we do here).
//
   rc = osFS->chksum(XrdSfsFileSystem::csCalc, argv[1], argv[2], myInfo);

// Return result regardless of what it is
//
   Stream->PutLine(myInfo.getErrText(ecode));
   if (rc) {SI->errorCnt++;
            if (ecode) rc = ecode;
           }
   return rc;
}

/******************************************************************************/
/*                               C l e a n u p                                */
/******************************************************************************/
  
void XrdXrootdProtocol::Cleanup()
{
   XrdXrootdPio *pioP;
   int i;

// Handle parallel stream cleanup. The session stream cannot be closed if
// there is any queued activity on subordinate streams. A subordinate
// can either be closed from the session stream or asynchronously only if
// it is active. Which means they could be running while we are running.
// So, we first call close() which should trigger a recycle quiesce. Upon
// return we can actually recycle the object.
//
   if (Status != XRD_BOUNDPATH)
      {streamMutex.Lock();
       for (i = 1; i < maxStreams; i++)
           if (Stream[i])
              {Stream[i]->Stream[0] = 0;
               if (!Stream[i]->isDead) Stream[i]->Link->Close();
               Stream[i]->Recycle(0, 0, 0);
               Stream[i] = 0;
              }
       streamMutex.UnLock();
      }

// Handle packet parking (needs to be done before deleting other stuff)
//
   if (pmHandle) delete pmHandle;

// Release any internal monitoring information
//
   if (Entity.moninfo) {free(Entity.moninfo); Entity.moninfo = 0;}

// If we have a buffer, release it
//
   if (argp) {BPool->Release(argp); argp = 0;}

// Notify the filesystem of a disconnect prior to deleting file tables
//
   if (Status != XRD_BOUNDPATH) osFS->Disc(Client);

// Handle parallel I/O appendages. We need to do this first as these have
// referenced open files and we need to deref them before we cleanup the ftab.
//
   while((pioP = pioFirst))
        {pioP->IO.File->Ref(-1); pioFirst = pioP->Next; pioP->Recycle();}
   while((pioP = pioFree ))     {pioFree  = pioP->Next; pioP->Recycle();}

// Delete the FTab if we have it
//
   if (FTab)
      {FTab->Recycle(Monitor.Files() ? Monitor.Agent : 0);
       FTab = 0;
      }

// Handle statistics
//
   SI->statsMutex.Lock();
   SI->readCnt += numReads; SI->writeCnt += numWrites;
   SI->statsMutex.UnLock();

// Handle authentication protocol
//
   if (AuthProt) {AuthProt->Delete(); AuthProt = 0;}
   if (Protect)  {Protect->Delete();  Protect  = 0;}

// Handle writev appendage
//
   if (wvInfo) {free(wvInfo); wvInfo = 0;}

// Release aplication name
//
   if (AppName) {free(AppName); AppName = 0;}

// Release the pagewrite control object
//
   if (pgwCtl) delete pgwCtl;
}
  
/******************************************************************************/
/*                               g e t D a t a                                */
/******************************************************************************/
  
int XrdXrootdProtocol::getData(const char *dtype, char *buff, int blen)
{
   int rlen;

// Read the data but reschedule he link if we have not received all of the
// data within the timeout interval.
//
   rlen = Link->Recv(buff, blen, readWait);
   if (rlen  < 0)
      {if (rlen != -ENOMSG) return Link->setEtext("link read error");
          else return -1;
      }
   if (rlen < blen)
      {myBuff = buff+rlen; myBlen = blen-rlen;
       TRACEP(REQ, dtype <<" timeout; read " <<rlen <<" of " <<blen <<" bytes");
       return 1;
      }
   return 0;
}

/******************************************************************************/
  
int XrdXrootdProtocol::getData(XrdXrootd::gdCallBack *cbP,
                               const char *dtype, char *buff, int blen)
{
   bool inCB = (gdCtl.Status == GetDataCtl::inCallBk);

// Setup the control information to direct the vector read
//
   memset((char *)&gdCtl, 0, sizeof(gdCtl));
   gdCtl.BuffLen = blen;     // Buffer length (bytes to read)
   gdCtl.Buffer  = buff;     // The actual buffer
   gdCtl.CallBack= cbP;      // Method to callback upon success
   gdCtl.ioDType = dtype;    // Name of the data being read for tracing
   gdCtl.Status  = GetDataCtl::inData;

// Effect the read. We prevent recursive calls if this was called while
// we were in a callback, which is possible due to I/O continuations.
//
   if (inCB)
      {gdCtl.useCB = true;
       return 1;
      }
   return getDataCont();
}

/******************************************************************************/
namespace {int consumed = 0;}
  
int XrdXrootdProtocol::getData(XrdXrootd::gdCallBack *cbP,
                               const char *dtype, struct iovec *iov, int iovn)
{
   bool inCB = (gdCtl.Status == GetDataCtl::inCallBk);

// Setup the control information to direct the vector read
//
   memset((char *)&gdCtl, 0, sizeof(gdCtl));
   gdCtl.iovNum  = iovn;     // Number of original elements
   gdCtl.iovVec  = iov;      // The actual vector
   gdCtl.CallBack= cbP;      // Method to callback upon success
   gdCtl.ioDType = dtype;    // Name of the data being read for tracing
   gdCtl.Status  = GetDataCtl::inDataIov;

// Effect the read. We prevent recursive calls if this was called while
// we were in a callback, which is possible due to I/O continuations.
//
   if (inCB)
      {gdCtl.useCB = true;
       return 1;
      }
consumed = 0;
   return getDataIovCont();
}

/******************************************************************************/
/*                           g e t D a t a C o n t                            */
/******************************************************************************/

int XrdXrootdProtocol::getDataCont()
{
   int rlen;

// Check if we need to terminate because the link died or we can proceed.
//
   if (gdCtl.linkWait.fetch_and(0) == GetDataCtl::Terminate)
      return -EINPROGRESS;

// I/O continuations may occur either via entry or an attempt to continue a new
// operation via the callback. This takes care of it here.
//
do{if ((rlen = Link->Recv(gdCtl.Buffer, gdCtl.BuffLen, readWait)) < 0) break;
   gdCtl.Buffer  += rlen;
   gdCtl.BuffLen -= rlen;

// If we completed the read then either return or use the callback. Note that
// we convert recursive call for more data into an iterative continuation.
//
   if (!gdCtl.BuffLen)
      {if (gdCtl.useCB)
          {gdCtl.Status = GetDataCtl::inCallBk;
           if (int(gdCtl.stalls) > as_okstutter)
              myStalls += int(gdCtl.stalls)/as_okstutter;
           rlen = gdCtl.CallBack->gdDone();
           if (rlen < 0) break;
           if (gdCtl.Status == GetDataCtl::inData) continue;
           if (gdCtl.Status == GetDataCtl::inDataIov) return getDataIovCont();
           if (gdCtl.Status == GetDataCtl::inDump) return getDumpCont();
           gdCtl.Status = GetDataCtl::inNone;
           return rlen;
          }
       gdCtl.Status = GetDataCtl::inNone;
       return 0;
      }

// Make sure we don't have an over-run
//
   if (gdCtl.BuffLen < 0)
      {rlen = Link->setEtext("link excessive read length error");
       break;
      }

// Record where we stopped and setup to resume here when more data arrives. We
// must set myBlen to zero to avoid calling the other GetData() method. We want
// to resume and perform the GetData() function here.
//
   Resume = &XrdXrootdProtocol::getDataCont;
   myBlen = 0;
   gdCtl.useCB    = true;
   gdCtl.linkWait = GetDataCtl::Active;
   if (gdCtl.stalls < 255) gdCtl.stalls++;

// Return indicating we need more data
//
   TRACEP(REQ, gdCtl.ioDType <<" timeout; read " <<rlen <<" bytes "
                             <<gdCtl.BuffLen <<" remaining");
   return 1;

  } while(true);

// If got here then we had a link failure or some other fatal issue
//
   if (rlen != -ENOMSG) return Link->setEtext("link read error");

// Use callback, if need be.
//
   if (gdCtl.useCB)
      {gdCtl.Status = GetDataCtl::inCallBk;
       gdCtl.CallBack->gdFail();
      }
   gdCtl.Status = GetDataCtl::inNone;
   return -1;
}
  
/******************************************************************************/
/*                         g e t D a t I o v C o n t                          */
/******************************************************************************/

int XrdXrootdProtocol::getDataIovCont()
{
   int rc;

// Check if we need to terminate because the link died or we can proceed.
//
   if (gdCtl.linkWait.fetch_and(0) == GetDataCtl::Terminate)
      return -EINPROGRESS;

// I/O continuations may occur either via entry or an attempt to continue a new
// operation via the callback. This takes care of it here.
//
do{struct iovec *ioV = gdCtl.iovVec;
   int i, rlen, iovN = gdCtl.iovNum, iNow = gdCtl.iovNow;

// Read as much data as we can. Handle any link error. Note that when a link
// error occurs we return failure whether or not the callback wants to do more.
//
   rlen = Link->Recv(&ioV[iNow], iovN - iNow, readWait);
   if (rlen  < 0)
      {if (rlen != -ENOMSG) Link->setEtext("link read error");
       if (gdCtl.iovAdj)
          {ioV[iNow].iov_base = ((char *)ioV[iNow].iov_base) - gdCtl.iovAdj;
           ioV[iNow].iov_len += gdCtl.iovAdj;
           gdCtl.iovAdj = 0;
          }
       rc = -1;
       break;
      }

// Compute where we finished in the iovec.
//
   for (i = iNow; i < iovN && (int)ioV[i].iov_len <= rlen; i++)
       rlen -= ioV[i].iov_len;

// Before proceeding, restore any changes we made to a completed iovec element
//
   if (i != iNow && gdCtl.iovAdj)
      {ioV[iNow].iov_base = ((char *)ioV[iNow].iov_base) - gdCtl.iovAdj;
       ioV[iNow].iov_len += gdCtl.iovAdj;
       gdCtl.iovAdj = 0;
      }

// If the vector is complete then effect the callback unless this was the
// initial call, we simply return to prevent recursive continuations by
// converting a recursive call to an iterative continuation!
//
   if (i >= iovN)
      {if (!rlen) 
          {if (gdCtl.useCB)
              {gdCtl.Status = GetDataCtl::inCallBk;
               if (int(gdCtl.stalls) > as_okstutter)
                  myStalls += int(gdCtl.stalls)/as_okstutter;
               rc = gdCtl.CallBack->gdDone();
               if (rc < 0) break;
               if (gdCtl.Status == GetDataCtl::inDataIov) continue;
               if (gdCtl.Status == GetDataCtl::inDump) return getDumpCont();
               gdCtl.Status = GetDataCtl::inNone;
               return rc;
              }
           gdCtl.Status = GetDataCtl::inNone;
           return 0;
          }
       rc = Link->setEtext("link iov read length error");
       break;
      }

// Record where we stopped and adjust the iovec element address and length if
// needed. Record the change made so that it can be undone as we progress.
//
   gdCtl.iovNow = i;
   if (rlen)
      {if (gdCtl.iovAdj == 0) gdCtl.iovNow = i;
       gdCtl.iovAdj += rlen;
       ioV[i].iov_base = ((char *)ioV[i].iov_base) + rlen;
       ioV[i].iov_len -= rlen;
      }

// Setup to resume here when more data arrives. We must set myBlen to zero to
// avoid calling the other GetData() method as we want to resume here.
//
   Resume = &XrdXrootdProtocol::getDataIovCont;
   myBlen = 0;
   gdCtl.useCB    = true;
   gdCtl.linkWait = GetDataCtl::Active;
   if (gdCtl.stalls < 255) gdCtl.stalls++;

// Return indicating we need more data
//
   TRACEP(REQ, gdCtl.ioDType<<" read timeout; "<<iovN-i<<" of "
             <<gdCtl.iovNum <<" iov elements left");
   return 1;

  } while(true);

// If got here then we had a link failure or some other fatal issue
//
   if (gdCtl.useCB)
      {gdCtl.Status = GetDataCtl::inCallBk;
       gdCtl.CallBack->gdFail();
      }
   gdCtl.Status = GetDataCtl::inNone;
   return rc;
}

/******************************************************************************/
/*                               g e t D u m p                                */
/******************************************************************************/
  
int XrdXrootdProtocol::getDump(const char *dtype, int dlen)
{
   bool inCB = (gdCtl.Status == GetDataCtl::inCallBk);

// Setup the control information to direct the vector read
//
   memset((char *)&gdCtl, 0, sizeof(gdCtl));
   gdCtl.DumpLen = dlen;     // Bytes left to drain
   gdCtl.ioDType = dtype;    // Name of the data being read for tracing
   gdCtl.Status  = GetDataCtl::inDump;

// Effect the read. We prevent recursive calls if this was called while
// we were in a callback, which is possible due to I/O continuations.
//
   return (inCB ? 1 : getDumpCont());
}

/******************************************************************************/
/* Private:                  g e t D u m p C o n t                            */
/******************************************************************************/

int XrdXrootdProtocol::getDumpCont()
{
    int  rlen = 0, rwant;
    char buff[65536];

   TRACEP(REQ, gdCtl.ioDType<<" discarding "<<gdCtl.DumpLen<<" bytes.");

// Read data and discard it
//
   while(gdCtl.DumpLen > 0)
        {if (gdCtl.DumpLen <= (int)sizeof(buff)) rwant = gdCtl.DumpLen;
            else rwant = sizeof(buff);
         if ((rlen = Link->Recv(buff, rwant, readWait)) <= 0) break;
         gdCtl.DumpLen -= rlen;
        }

// Check if we failed
//
   if (rlen < 0 || gdCtl.DumpLen < 0)
      {if (gdCtl.DumpLen < 0) Link->setEtext("link read overrun error");
          else if (rlen != -ENOMSG) Link->setEtext("link read error");
       gdCtl.Status = GetDataCtl::inNone;
       return -1;
      }

// Check if we completed
//
   if (gdCtl.DumpLen == 0)
      {gdCtl.Status = GetDataCtl::inNone;
       return 0;
      }

// Wait until more data arrives. We will now need to use the callback.
//
   Resume = &XrdXrootdProtocol::getDumpCont;
   myBlen = 0;

   TRACEP(REQ, gdCtl.ioDType<<" read timeout; "<<gdCtl.DumpLen
             <<" bytes left to discard");
   return 1;
}
  
/******************************************************************************/
/*                                 R e s e t                                  */
/******************************************************************************/
  
void XrdXrootdProtocol::Reset()
{
   Status             = 0;
   argp               = 0;
   Link               = 0;
   FTab               = 0;
   pmHandle           = 0;
   ResumePio          = 0;
   Resume             = 0;
   myBuff             = (char *)&Request;
   myBlen             = sizeof(Request);
   myBlast            = 0;
   myStalls           = 0;
   pgwCtl             = 0;
   memset(&IO, 0, sizeof(IO));
   wvInfo             = 0;
   numReads           = 0;
   numReadP           = 0;
   numReadV           = 0;
   numSegsV           = 0;
   numWritV           = 0;
   numSegsW           = 0;
   numWrites          = 0;
   numFiles           = 0;
   cumReads           = 0;
   cumReadV           = 0;
   cumSegsV           = 0;
   cumWritV           = 0;
   cumSegsW           = 0;
   cumWrites          = 0;
   totReadP           = 0;
   hcPrev             =13;
   hcNext             =21;
   hcNow              =13;
   Client             = 0;
   AuthProt           = 0;
   Protect            = 0;
   mySID              = 0;
   CapVer             = 0;
   clientPV           = 0;
   clientRN           = 0;
   pmDone             = false;
   reTry              = 0;
   endNote            = 0;
   PathID             = 0;
   newPio             = false;
   rvSeq              = 0;
   wvSeq              = 0;
   doTLS              = tlsNot; // Assume client is not capable. This will be
   ableTLS            = false;  // resolved during the kXR_protocol interchange.
   isTLS              = false;  // Made true when link converted to TLS
   linkAioReq         = 0;
   pioFree = pioFirst = pioLast = 0;
   isActive = isLinkWT= isNOP = isDead = false;
   sigNeed = sigHere = sigRead = false;
   sigWarn = true;
   rdType             = 0;
   Entity.Reset(0);
   memset(Stream,  0, sizeof(Stream));
   memset((char *)&gdCtl,  0, sizeof(gdCtl));
   PrepareCount       = 0;
   if (AppName) {free(AppName); AppName = 0;}
}
