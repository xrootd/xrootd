/******************************************************************************/
/*                                                                            */
/*                     x r o o t d _ P r o t o c o l . C                      */
/*                                                                            */
/* (c) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*      All Rights Reserved. See XrdVersion.cc for complete License Terms     */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$

const char *XrdXrootdProtocolCVSID = "$Id$";
 
#include "Experiment/Experiment.hh"

#include "XrdSfs/XrdSfsInterface.hh"
#include "Xrd/XrdBuffer.hh"
#include "Xrd/XrdLink.hh"
#include "XProtocol/XProtocol.hh"
#include "XrdXrootd/XrdXrootdFile.hh"
#include "XrdXrootd/XrdXrootdFileLock.hh"
#include "XrdXrootd/XrdXrootdFileLock1.hh"
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
char                  XrdXrootdProtocol::isProxy = 0;

int                   XrdXrootdProtocol::as_maxaspl =  8;   // Max async ops per link
int                   XrdXrootdProtocol::as_maxasps = 64;   // Max async ops per server
int                   XrdXrootdProtocol::as_maxbfsz;
int                   XrdXrootdProtocol::as_aiosize;

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
  
#define ABANDON(x,y) (x->setEtext(y), x->Close(), (XrdProtocol *)0)

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
   pi->eDest->Say(0,(char *)"(c) 2003 Stanford University/SLAC XRootd "
                    "(eXtended Root Daemon) v " XROOTD_VERSION);

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
                    : ProtLink(this), XrdProtocol("xrootd protocol handler")
{
   Reset();
}

/******************************************************************************/
/*                   A s s i g n m e n t   O p e r a t o r                    */
/******************************************************************************/

XrdXrootdProtocol &XrdXrootdProtocol::operator =(const XrdXrootdProtocol &rhs)
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
/*                                  D o I t                                   */
/******************************************************************************/
  
void XrdXrootdProtocol::DoIt()
{

// Invoke the appropriate routine for this asynchronous entry
//
   (*this.*Resume)();

// Do some statistics (no need to lock these)
//
   if (--SI->AsyncNow < 0) SI->AsyncNow=0;

// Dereference the link
//
   if (Link) {Link->setRef(-1); Link = 0;}

// Recycle ourselves
//
   Recycle();
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
               {kXR_int16 streamid;
                kXR_int16 status;
                kXR_int32 rlen;
                kXR_int32 pval;
                kXR_int32 styp;
               } hsresp={0, 0, htonl((long)8), htonl((long)XROOTD_VERSBIN),
                               htonl((long)kXR_DataServer)};

XrdXrootdProtocol *xp;
int dlen;

// Peek at the first 20 bytes of data
//
   if ((dlen = lp->Peek(hsbuff,sizeof(hsdata),readWait)) != sizeof(hsdata))
       if (dlen <= 0) return ABANDON(lp, "connection timed out");
          else return (XrdProtocol *)0;

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
   if (isProxy) hsresp.styp =  htonl((kXR_int32)kXR_LBalServer);
   if (!lp->Send((char *)&hsresp, sizeof(hsresp)))
      return ABANDON(lp, "handshake failed");

// We can now read all 20 bytes and discard them (no need to wait for it)
//
   if (lp->Recv(hsbuff, sizeof(hsdata)) != sizeof(hsdata))
      return ABANDON(lp, "reread failed");

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
      if (myBlen && (rc = getData("data", myBuff, myBlen) != 0)) return rc;
         else if ((rc = (*this.*Resume)()) != 0) return rc;
                 else {Resume = 0; return 0;}

// Read the next request header
//
   if ((rc=getData("request",(char *)&Request,sizeof(Request))) != 0) return rc;

// Deserialize the data
//
   Request.requestid = ntohs(Request.requestid);
   Request.dlen      = ntohl(Request.dlen);
   Response.Set(Request.streamid);
   TRACEP(REQ, "req=" <<Request.requestid <<" dlen=" <<Request.dlen);

// If the user is not yet logged in, restrict what the user can do
//
   if (!Status)
      switch(Request.requestid)
            {case kXR_login:    return do_Login();
             case kXR_protocol: return do_Protocol();
             default:           Response.Send(kXR_InvalidRequest,
                                "Invalid request; user not logged in");
                                ABANDON(Link,"protocol sequence error");
                                return -1;
            }
      else 
      switch(Request.requestid)   // First, the ones with file handles
            {case kXR_read:     return do_Read();
             case kXR_write:    return do_Write();
             case kXR_sync:     return do_Sync();
             case kXR_close:    return do_Close();
             default:           return Process2();
            }
   return 0; // We should never get here
}

/******************************************************************************/
/*                      p r i v a t e   P r o c e s s 2                       */
/******************************************************************************/
  
int XrdXrootdProtocol::Process2()
{
   int rc;

// First select any protocol that does not need an argument
//
   switch(Request.requestid)
         {case kXR_protocol: return do_Protocol();
          case kXR_ping:     return do_Ping();
          default:           break;
         }

// Get a buffer for this argument
//
   if (!argp || Request.dlen+1 > argp->bsize)
      {if (argp) BPool->Release(argp);
       if (!(argp = BPool->Obtain(Request.dlen+1)))
          {Response.Send(kXR_ArgTooLong, "Request argument is too long");
           return 0;
          }
      }

// Read in the remainder of the request and process it
//
   if (rc = getData("arg", argp->buff, Request.dlen))
      {Resume = &XrdXrootdProtocol::Process3; return rc;}
   argp->buff[Request.dlen] = '\0';
   return Process3();
}

/******************************************************************************/
/*                      p r i v a t e   P r o c e s s 3                       */
/******************************************************************************/
  
int XrdXrootdProtocol::Process3()
{
// Process items that keep own statistics
//
   switch(Request.requestid)
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
   switch(Request.requestid)
         {case kXR_admin:     if (Status & XRD_ADMINUSER) return do_Admin();
                                 else break;
          case kXR_auth:      return do_Auth();
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
  
void XrdXrootdProtocol::Recycle()
{

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
  
int XrdXrootdProtocol::Stats(char *buff, int blen)
{
   return SI->Stats(buff, blen);
}
 
/******************************************************************************/
/*                             s y n c S t a t s                              */
/******************************************************************************/

void XrdXrootdProtocol::syncStats()
{
    SI->statsMutex.Lock();
    SI->readCnt += numReads; 
    numReads = 0;
    SI->prerCnt += numReadP;
    numReadP = 0;
    SI->writeCnt += numWrites; 
    numWrites = 0;
    SI->statsMutex.UnLock();
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
      {ABANDON(Link,"link read error"); return -1;}
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
   myOffset           = 0;
   myIOLen            = 0;
   numReads           = 0;
   numReadP           = 0;
   numWrites          = 0;
}
