/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d P r o t o c o l . c c                   */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

const char *XrdXrootdProtocolCVSID = "$Id$";
 
#include "XrdSfs/XrdSfsInterface.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdOuc/XrdOucTimer.hh"
#include "XrdXrootd/XrdXrootdAio.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdFileLock.hh"
#include "XrdXrootd/XrdXrootdFileLock1.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"
#include "XrdXrootd/XrdXrootdStats.hh"
#include "XrdXrootd/XrdXrootdTrace.hh"
#include "XrdXrootd/XrdXrootdXPath.hh"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

XrdOucTrace          *XrdXrootdTrace;

XrdXrootdXPath        XrdXrootdProtocol::XPList;
XrdXrootdXPath       *XrdXrootdXPath::first = 0;
XrdSfsFileSystem     *XrdXrootdProtocol::osFS;
char                 *XrdXrootdProtocol::FSLib    = 0;
XrdXrootdFileLock    *XrdXrootdProtocol::Locker;
XrdSecProtocol       *XrdXrootdProtocol::CIA      = 0;
char                 *XrdXrootdProtocol::SecLib   = 0;
XrdScheduler         *XrdXrootdProtocol::Sched;
XrdBuffManager       *XrdXrootdProtocol::BPool;
XrdOucError           XrdXrootdProtocol::eDest(0, "Xrootd");
XrdXrootdStats       *XrdXrootdProtocol::SI;
XrdOucProg           *XrdXrootdProtocol::ProgCKS  = 0;
char                 *XrdXrootdProtocol::ProgCKT  = 0;

char                 *XrdXrootdProtocol::Notify = 0;
int                   XrdXrootdProtocol::readWait;
int                   XrdXrootdProtocol::Port;
char                  XrdXrootdProtocol::isRedir = 0;
char                  XrdXrootdProtocol::chkfsV  = 0;

int                   XrdXrootdProtocol::maxBuffsz;
int                   XrdXrootdProtocol::as_maxperlnk = 8;   // Max ops per link
int                   XrdXrootdProtocol::as_maxperreq = 8;   // Max ops per request
int                   XrdXrootdProtocol::as_maxpersrv = 4096;// Max ops per server
int                   XrdXrootdProtocol::as_segsize   = 65536;
int                   XrdXrootdProtocol::as_miniosz   = 16384;
int                   XrdXrootdProtocol::as_maxstalls = 5;
int                   XrdXrootdProtocol::as_force     = 0;
int                   XrdXrootdProtocol::as_noaio     = 0;
int                   XrdXrootdProtocol::as_syncw     = 0;

const char           *XrdXrootdProtocol::TraceID = "Protocol";

/******************************************************************************/
/*            P r o t o c o l   M a n a g e m e n t   S t a c k s             */
/******************************************************************************/
  
XrdObjectQ<XrdXrootdProtocol>
            XrdXrootdProtocol::ProtStack("ProtStack",
                                       "xrootd protocol anchor");

/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/
  
#define UPSTATS(x) SI->statsMutex.Lock(); SI->x++; SI->statsMutex.UnLock()

/******************************************************************************/
/*                       P r o t o c o l   L o a d e r                        */
/******************************************************************************/
  
// This protocol is meant to live in a shared library. The interface below is
// used by the server to obtain a copy of the protocol object that can be used
// to decide whether or not a link is talking a particular protocol.
//

extern "C"
{
XrdProtocol *XrdgetProtocol(const char *pname, char *parms,
                              XrdProtocol_Config *pi)
{

// Put up the banner
//
   pi->eDest->Say(0,(char *)"(c) 2004 Stanford University/SLAC XRootd "
                    "(eXtended Root Daemon).");

// Return the protocol object to be used if static init succeeds
//
   if (XrdXrootdProtocol::Configure(parms, pi))
      return (XrdProtocol *)new XrdXrootdProtocol();
   return (XrdProtocol *)0;
}
}

/******************************************************************************/
/*             x r d _ P r o t o c o l _ X R o o t d   C l a s s              */
/******************************************************************************/
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdXrootdProtocol::XrdXrootdProtocol() 
                    : XrdProtocol("xrootd protocol handler"), ProtLink(this)
{
   Reset();
}

/******************************************************************************/
/*                   A s s i g n m e n t   O p e r a t o r                    */
/******************************************************************************/

XrdXrootdProtocol XrdXrootdProtocol::operator =(const XrdXrootdProtocol &rhs)
{
// Reset all common fields
//
   Reset();

// Now copy the relevant fields only
//
   Link          = rhs.Link;
   Link->setRef(1);      // Keep the link stable until we dereference it
   Status        = rhs.Status;
   myFile        = rhs.myFile;
   myIOLen       = rhs.myIOLen;
   myOffset      = rhs.myOffset;
   Response      = rhs.Response;
   memcpy((void *)&Request,(const void *)&rhs.Request, sizeof(Request));
   memcpy((void *)&Client, (const void *)&rhs.Client,  sizeof(Client));
   return *this;
}
  
/******************************************************************************/
/*                                 M a t c h                                  */
/******************************************************************************/

#define TRACELINK lp
  
XrdProtocol *XrdXrootdProtocol::Match(XrdLink *lp)
{
        struct ClientInitHandShake hsdata;
        char  *hsbuff = (char *)&hsdata;

static  struct hs_response
               {kXR_unt16 streamid;
                kXR_unt16 status;
                kXR_int32 rlen;
                kXR_int32 pval;
                kXR_int32 styp;
               } hsresp={0, 0, htonl(8), htonl(XROOTD_VERSBIN),
                               htonl(kXR_DataServer)};

XrdXrootdProtocol *xp;
int dlen;

// Peek at the first 20 bytes of data
//
   if ((dlen = lp->Peek(hsbuff,sizeof(hsdata),readWait)) != sizeof(hsdata))
      {if (dlen <= 0) lp->setEtext("handshake not received");
       return (XrdProtocol *)0;
      }

// Trace the data
//
// TRACEI(REQ, "received: " <<Trace->bin2hex(hsbuff,dlen));

// Verify that this is our protocol
//
   hsdata.fourth  = ntohl(hsdata.fourth);
   hsdata.fifth   = ntohl(hsdata.fifth);
   if (dlen != sizeof(hsdata) ||  hsdata.first || hsdata.second
   || hsdata.third || hsdata.fourth != 4 || hsdata.fifth != ROOTD_PQ) return 0;

// Respond to this request with the handshake response
//
   if (isRedir) hsresp.styp =  static_cast<kXR_int32>(htonl(kXR_LBalServer));
   if (!lp->Send((char *)&hsresp, sizeof(hsresp)))
      {lp->setEtext("handshake failed");
       return (XrdProtocol *)0;
      }

// We can now read all 20 bytes and discard them (no need to wait for it)
//
   if (lp->Recv(hsbuff, sizeof(hsdata)) != sizeof(hsdata))
      {lp->setEtext("reread failed");
       return (XrdProtocol *)0;
      }

// Get a protocol object off the stack (if none, allocate a new one)
//
   if (!(xp = ProtStack.Pop())) xp = new XrdXrootdProtocol();

// Bind the protocol to the link and return the protocol
//
   UPSTATS(Count);
   xp->Link = lp;
   xp->Client.prot[0] = 'h'; xp->Client.prot[1] = 's';
   xp->Client.prot[2] = 's'; xp->Client.prot[3] = 't';
   xp->Client.name[0] = '\0';
   strlcpy(xp->Client.host,lp->Name(&(xp->Client.hostaddr)),sizeof(Client.host));
   Client.tident = (char *)xp->Client.host;
   xp->Response.Set(lp);
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

// Check if we are servicing a slow link
//
   if (Resume)
      if (myBlen && (rc = getData("data", myBuff, myBlen)) != 0) 
         {if (rc < 0 && myAioReq) myAioReq->Recycle(-1);
          return rc;
         }
         else if ((rc = (*this.*Resume)()) != 0) return rc;
                 else {Resume = 0; return 0;}

// Read the next request header
//
   if ((rc=getData("request",(char *)&Request,sizeof(Request))) != 0) return rc;

// Deserialize the data
//
   Request.header.requestid = ntohs(Request.header.requestid);
   Request.header.dlen      = ntohl(Request.header.dlen);
   Response.Set(Request.header.streamid);
   TRACEP(REQ, "req=" <<Request.header.requestid <<" dlen=" <<Request.header.dlen);

// Every request has an associated data length. It better be >= 0 or we won't
// be able to know how much data to read.
//
   if (Request.header.dlen < 0)
      {Response.Send(kXR_ArgInvalid, "Invalid request data length");
       return Link->setEtext("protocol data length error");
      }

// Read any argument data at this point, except when the request is a write.
// The argument may have to be segmented and we're not prepared to do that here.
//
   if (Request.header.requestid != kXR_write && Request.header.dlen)
      {if (!argp || Request.header.dlen+1 > argp->bsize)
          {if (argp) BPool->Release(argp);
           if (!(argp = BPool->Obtain(Request.header.dlen+1)))
              {Response.Send(kXR_ArgTooLong, "Request argument is too long");
               return 0;
              }
          }
       if ((rc = getData("arg", argp->buff, Request.header.dlen)))
          {Resume = &XrdXrootdProtocol::Process2; return rc;}
       argp->buff[Request.header.dlen] = '\0';
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

// If the user is not yet logged in, restrict what the user can do
//
   if (!Status)
      switch(Request.header.requestid)
            {case kXR_login:    return do_Login();
             case kXR_protocol: return do_Protocol();
             default:           Response.Send(kXR_InvalidRequest,
                                "Invalid request; user not logged in");
                                return Link->setEtext("protocol sequence error 1");
            }

// Help the compiler, select the the high activity requests (the ones with
// file handles) in a separate switch statement
//
   switch(Request.header.requestid)   // First, the ones with file handles
         {case kXR_read:     return do_Read();
          case kXR_write:    return do_Write();
          case kXR_sync:     return do_Sync();
          case kXR_close:    return do_Close();
          default:           break;
         }

// Now select the requests that do not need authentication
//
   switch(Request.header.requestid)
         {case kXR_protocol: return do_Protocol();   // dlen ignored
          case kXR_ping:     return do_Ping();       // dlen ignored
          default:           break;
         }

// All remaining requests require an argument. Make sure we have one
//
   if (!argp || !Request.header.dlen)
      {Response.Send(kXR_ArgMissing, "Required argument not present");
       return 0;
      }

// Force authentication at this point, if need be
//
   if (Status & XRD_NEED_AUTH)
      if (Request.header.requestid == kXR_auth) return do_Auth();
         else {Response.Send(kXR_InvalidRequest,
                             "Invalid request; user not logged in");
               return -1;
              }

// Process items that keep own statistics
//
   switch(Request.header.requestid)
         {case kXR_open:      return do_Open();
          case kXR_getfile:   return do_Getfile();
          case kXR_putfile:   return do_Putfile();
          default:            break;
         }

// Update misc stats count
//
   UPSTATS(miscCnt);

// Now process whatever we have
//
   switch(Request.header.requestid)
         {case kXR_admin:     if (Status & XRD_ADMINUSER) return do_Admin();
                                 else break;
          case kXR_chmod:     return do_Chmod();
          case kXR_dirlist:   return do_Dirlist();
          case kXR_mkdir:     return do_Mkdir();
          case kXR_mv:        return do_Mv();
          case kXR_query:     return do_Query();
          case kXR_prepare:   return do_Prepare();
          case kXR_rm:        return do_Rm();
          case kXR_rmdir:     return do_Rmdir();
          case kXR_set:       return do_Set();
          case kXR_stat:      return do_Stat();
          case kXR_statx:     return do_Statx();
          default:            break;
         }

// Whatever we have, it's not valid
//
   Response.Send(kXR_InvalidRequest, "Invalid request code");
   return 0;
}

/******************************************************************************/
/*                               R e c y c l e                                */
/******************************************************************************/
  
void XrdXrootdProtocol::Recycle(XrdLink *lp, int csec, char *reason)
{
   char *sfxp, ctbuff[24], buff[128];

// Document the disconnect
//
   if (lp)
      {XrdOucTimer::s2hms(csec, ctbuff, sizeof(ctbuff));
       if (reason) {snprintf(buff, sizeof(buff), "%s (%s)", ctbuff, reason);
                    sfxp = buff;
                   } else sfxp = ctbuff;

       eDest.Log(OUC_LOG_02,"Xeq",(const char *)lp->ID,(char *)"disc", sfxp);
      }

// Check if we should monitor disconnects
//
   if (XrdXrootdMonitor::monUSER && Monitor) Monitor->Disc(monUID, csec);

// Release all appendages
//
   Cleanup();

// Set fields to starting point (debugging mostly)
//
   Reset();

// Push ourselves on the stack
//
   ProtStack.Push(&ProtLink);
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
       numReads = 0;
       SI->prerCnt += numReadP;
       numReadP = 0;
       SI->writeCnt += numWrites;
       numWrites = 0;
       SI->statsMutex.UnLock();
      }

// Now return the statistics
//
   return SI->Stats(buff, blen, do_sync);
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                               C l e a n u p                                */
/******************************************************************************/
  
void XrdXrootdProtocol::Cleanup()
{

// If we have a buffer, release it
//
   if (argp) {BPool->Release(argp); argp = 0;}

// Delete the FTab if we have it
//
   if (FTab) {delete FTab; FTab = 0;}

// Handle statistics
//
   SI->statsMutex.Lock();
   SI->readCnt += numReads; SI->writeCnt += numWrites;
   SI->statsMutex.UnLock();

// Handle Monitor
//
   if (Monitor) {Monitor->unAlloc(Monitor); Monitor = 0;}
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
      if (rlen != -ENOMSG) return Link->setEtext("link read error");
         else return -1;
   if (rlen < blen)
      {myBuff = buff+rlen; myBlen = blen-rlen;
       TRACEP(REQ, dtype <<" timeout; read " <<rlen <<" of " <<blen <<" bytes");
       return 1;
      }
   return 0;
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
   Resume             = 0;
   myBuff             = (char *)&Request;
   myBlen             = sizeof(Request);
   myBlast            = 0;
   myOffset           = 0;
   myIOLen            = 0;
   myStalls           = 0;
   numReads           = 0;
   numReadP           = 0;
   numWrites          = 0;
   Monitor            = 0;
   monUID             = 0;
   monFILE            = 0;
   monIO              = 0;
}
