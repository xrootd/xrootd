#ifndef __XROOTD_PROTOCOL_H__
#define __XROOTD_PROTOCOL_H__
/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d P r o t o c o l . h h                   */
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
 
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>

#include "XrdNet/XrdNetPMark.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysRAtomic.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSfs/XrdSfsDio.hh"
#include "XrdSfs/XrdSfsXioImpl.hh"

#include "Xrd/XrdObject.hh"
#include "Xrd/XrdProtocol.hh"
#include "XrdXrootd/XrdXrootdMonitor.hh"
#include "XrdXrootd/XrdXrootdReqID.hh"
#include "XrdXrootd/XrdXrootdResponse.hh"
#include "XProtocol/XProtocol.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/

#define ROOTD_PQ 2012

#define XRD_LOGGEDIN       1
#define XRD_NEED_AUTH      2
#define XRD_BOUNDPATH      8

#ifndef __GNUC__
#define __attribute__(x)
#endif

/******************************************************************************/
/*                   x r d _ P r o t o c o l _ X R o o t d                    */
/******************************************************************************/

class XrdNetSocket;
class XrdOucEnv;
class XrdOucErrInfo;
class XrdOucReqID;
class XrdOucStream;
class XrdOucTList;
class XrdOucTokenizer;
class XrdSecProtect;
class XrdSecProtector;
class XrdSfsDirectory;
class XrdSfsFACtl;
class XrdSfsFileSystem;
class XrdSecProtocol;
class XrdBuffer;
class XrdLink;
class XrdTlsContext;
class XrdXrootdFile;
class XrdXrootdFileLock;
class XrdXrootdFileTable;
class XrdXrootdJob;
class XrdXrootdMonitor;
class XrdXrootdPgwCtl;
class XrdXrootdPio;
class XrdXrootdStats;
class XrdXrootdWVInfo;
class XrdXrootdXPath;

/******************************************************************************/
/*                   N a m e s p a c e   X r d X r o o t d                    */
/******************************************************************************/
  
namespace XrdXrootd
{
/******************************************************************************/
/*                            g d C a l l B a c k                             */
/******************************************************************************/
  
class gdCallBack             // Used for new style getData() with callback
{
public:

// Called when getData with a buffer successfully completed with a suspension.
// A direct return is made if there was no suspension. Return values and action:
// >1 If getData with a buffer was called while in the callback, the operation
//    is performed with a subsequent callback. Otherwise, a fatal error results.
// =0 Variable discard holds the number of bytes to be discarded from the
//    from the socket (default 0). Return is made to link-level.
// <0 Considered a fatal link error.
//
virtual int   gdDone() = 0;

// Called when a fatal link error occurs during reading.
//
virtual void  gdFail() {}   // Called when a link failure occurs

              gdCallBack() {}
virtual      ~gdCallBack() {}
};

/******************************************************************************/
/*                               I O P a r m s                                */
/******************************************************************************/
  
struct IOParms
{
XrdXrootdFile    *File;
union {
long long         Offset;
long long         WVBytes;
int               EInfo[2];
      };
int               IOLen;
unsigned short    Flags;
char              reserved;
char              Mode;
static const int  useBasic = 0;
static const int  useMMap  = 1;
static const int  useSF    = 2;
};
}

/******************************************************************************/
/*               C l a s s   X r d X r o o t d P r o t o c o l                */
/******************************************************************************/
  
class XrdXrootdProtocol : public XrdProtocol, public XrdXrootd::gdCallBack,
                          public XrdSfsDio,   public XrdSfsXio
{
friend class XrdXrootdAdmin;
public:

       void          aioUpdate(int val) {srvrAioOps += val;}

       void          aioUpdReq(int val) {linkAioReq += val;}

static char         *Buffer(XrdSfsXioHandle h, int *bsz); // XrdSfsXio

XrdSfsXioHandle      Claim(const char *buff, int datasz, int minasz=0) override;// XrdSfsXio

static int           Configure(char *parms, XrdProtocol_Config *pi);

       void          DoIt() override {(*this.*Resume)();}

       int           do_WriteSpan();

       int           getData(gdCallBack   *gdcbP, const char *dtype,
                             char *buff, int blen);

       int           getData(gdCallBack   *gdcbP, const char *dtype,
                             struct iovec *iov,   int iovn);

       int           getDump(const char *dtype, int dlen);

       int           getPathID() {return PathID;}

       XrdProtocol  *Match(XrdLink *lp) override;

       int           Process(XrdLink *lp) override; //  Sync: Job->Link.DoIt->Process

       int           Process2();

       int           ProcSig();

       void          Recycle(XrdLink *lp, int consec, const char *reason) override;

static void          Reclaim(XrdSfsXioHandle h); // XrdSfsXio

       int           SendFile(int fildes) override; // XrdSfsDio

       int           SendFile(XrdOucSFVec *sfvec, int sfvnum) override; // XrdSfsDio

       void          SetFD(int fildes) override; // XrdSfsDio

       int           Stats(char *buff, int blen, int do_sync=0) override;

       void          StreamNOP();

XrdSfsXioHandle      Swap(const char *buff, XrdSfsXioHandle h=0) override; // XrdSfsXio

XrdXrootdProtocol   *VerifyStream(int &rc, int pID, bool lok=true);

              XrdXrootdProtocol operator =(const XrdXrootdProtocol &rhs) = delete;
              XrdXrootdProtocol();
             ~XrdXrootdProtocol() {Cleanup();}

static const int     maxStreams = 16;

// async configuration values (referenced outside this class)
//
static int           as_maxperlnk; // Max async requests per link
static int           as_maxperreq; // Max async ops per request
static int           as_maxpersrv; // Max async ops per server
static int           as_miniosz;   // Min async request size
static int           as_minsfsz;   // Min sendf request size
static int           as_seghalf;
static int           as_segsize;   // Aio quantum (optimal)
static int           as_maxstalls; // Maximum stalls we will tolerate
static short         as_okstutter; // Allowable stutters per transfer unit
static short         as_timeout;   // request timeout (usually < stream timeout)
static bool          as_force;     // aio to be forced
static bool          as_aioOK;     // aio is enabled
static bool          as_nosf;      // sendfile is disabled
static bool          as_syncw;     // writes to be synchronous

private:

// Note that Route[] structure (below) must have RD_Num elements!
//
enum RD_func {RD_chmod = 0, RD_chksum,  RD_dirlist, RD_locate, RD_mkdir,
              RD_mv,        RD_prepare, RD_prepstg, RD_rm,     RD_rmdir,
              RD_stat,      RD_trunc,   RD_ovld,    RD_client,
              RD_open1,     RD_open2,   RD_open3,   RD_open4,  RD_Num};

       int   do_Auth();
       int   do_Bind();
       int   do_ChkPnt();
       int   do_ChkPntXeq();
       int   do_Chmod();
       int   do_CKsum(int canit);
       int   do_CKsum(char *algT, const char *Path, char *Opaque);
       int   do_Close();
       int   do_Dirlist();
       int   do_DirStat(XrdSfsDirectory *dp, char *pbuff, char *opaque);
       int   do_Endsess();
       int   do_FAttr();
       int   do_gpFile();
       int   do_Login();
       int   do_Locate();
       int   do_Mkdir();
       int   do_Mv();
       int   do_Offload(int (XrdXrootdProtocol::*Invoke)(), int pathID);
       int   do_OffloadIO();
       int   do_Open();
       bool  do_PgClose(XrdXrootdFile *fP, int &rc);
       int   do_PgRead();
       int   do_PgRIO();
       int   do_PgWrite();
       bool  do_PgWAIO(int &rc);
       int   do_PgWIO();
       int   do_PgWIO(bool isFresh);
       bool  do_PgWIORetry(int &rc);
       bool  do_PgWIOSetup(XrdXrootdPgwCtl *pgwCtl);
       int   do_Ping();
       int   do_Prepare(bool isQuery=false);
       int   do_Protocol();
       int   do_Qconf();
       int   do_QconfCX(XrdOucTokenizer &qcargs, char *val);
       int   do_Qfh();
       int   do_Qopaque(short);
       int   do_Qspace();
       int   do_Query();
       int   do_Qxattr();
       int   do_Read();
       int   do_ReadV();
       int   do_ReadAll();
       int   do_ReadNone(int &retc, int &pathID);
       int   do_Rm();
       int   do_Rmdir();
       int   do_Set();
       int   do_Set_Cache(XrdOucTokenizer &setargs);
       int   do_Set_Mon(XrdOucTokenizer &setargs);
       int   do_Stat();
       int   do_Statx();
       int   do_Sync();
       int   do_Truncate();
       int   do_Write();
       int   do_WriteAio();
       int   do_WriteAll();
       int   do_WriteCont();
       int   do_WriteNone();
       int   do_WriteNone(int pathid, XErrorCode  ec=kXR_noErrorYet,
                                      const char *emsg=0);
       int   do_WriteNoneMsg();
       int   do_WriteV();
       int   do_WriteVec();

       int   gdDone() override {return do_PgWIO(false);}

       void  Assign(const XrdXrootdProtocol &rhs);
static int   CheckSum(XrdOucStream *, char **, int);
       void  Cleanup();
static int   Config(const char *fn);
static bool  ConfigMon(XrdProtocol_Config *pi, XrdOucEnv &xrootdEnv);
static int   ConfigSecurity(XrdOucEnv &xEnv, const char *cfn);
       int   fsError(int rc, char opc, XrdOucErrInfo &myError,
                     const char *Path, char *Cgi);
       int   fsOvrld(char opc, const char *Path, char *Cgi);
       int   fsRedirNoEnt(const char *eMsg, char *Cgi, int popt);
       int   getBuff(const int isRead, int Quantum);
       char *getCksType(char *opaque, char *cspec=0, int cslen=0);
       int   getData(const char *dtype, char *buff, int blen);
       int   getDataCont();
       int   getDataIovCont();
       int   getDumpCont();
       bool  logLogin(bool xauth=false);
static int   mapMode(int mode);
       void  Reset();
static int   rpCheck(char *fn, char **opaque);
       int   rpEmsg(const char *op, char *fn);
       int   vpEmsg(const char *op, char *fn);
static int   CheckTLS(const char *tlsProt);
static bool  ConfigFS(XrdOucEnv &xEnv, const char *cfn);
static bool  ConfigFS(const char *path, XrdOucEnv &xEnv, const char *cfn);
static bool  ConfigGStream(XrdOucEnv &myEnv, XrdOucEnv *urEnv);
static int   Squash(char *);
       int   StatGen(struct stat &buf, char *xxBuff, int xxLen, bool xa=false);
static int   xapath(XrdOucStream &Config);
static int   xasync(XrdOucStream &Config);
static int   xcksum(XrdOucStream &Config);
static int   xbif(XrdOucStream &Config);
static int   xdig(XrdOucStream &Config);
static int   xexp(XrdOucStream &Config);
static int   xexpdo(char *path, int popt=0);
static int   xfsl(XrdOucStream &Config);
static int   xfsL(XrdOucStream &Config, char *val, int lix);
static int   xfso(XrdOucStream &Config);
static int   xgpf(XrdOucStream &Config);
static int   xprep(XrdOucStream &Config);
static int   xlog(XrdOucStream &Config);
static int   xmon(XrdOucStream &Config);
static char *xmondest(const char *what, char *val);
static int   xmongs(XrdOucStream &Config);
static bool  xmongsend(XrdOucStream &Config, char *val, char *&dest,
                       int &opt, int &fmt, int &hdr);
static int   xred(XrdOucStream &Config);
static int   xred_clnt(XrdOucStream &Config, char *hP[2], int rPort[2]);
static bool  xred_php(char *val, char *hP[2], int rPort[2], const char *what,
                      bool optport=false);
static void  xred_set(RD_func func, char *rHost[2], int rPort[2]);
static bool  xred_xok(int     func, char *rHost[2], int rPort[2]);
static int   xsecl(XrdOucStream &Config);
static int   xtls(XrdOucStream &Config);
static int   xtlsr(XrdOucStream &Config);
static int   xtrace(XrdOucStream &Config);
static int   xlimit(XrdOucStream &Config);

       int   ProcFAttr(char *faPath, char *faCgi,  char *faArgs,
                       int   faALen, int   faCode, bool  doAChk);
       int   XeqFADel(XrdSfsFACtl &ctl, char *faVars, int faVLen);
       int   XeqFAGet(XrdSfsFACtl &ctl, char *faVars, int faVLen);
       int   XeqFALsd(XrdSfsFACtl &ctl);
       int   XeqFALst(XrdSfsFACtl &ctl);
       int   XeqFASet(XrdSfsFACtl &ctl, char *faVars, int faVLen);

static XrdObjectQ<XrdXrootdProtocol> ProtStack;
XrdObject<XrdXrootdProtocol>         ProtLink;

protected:

static unsigned int getSID();

       void  MonAuth();
       int   SetSF(kXR_char *fhandle, bool seton=false);

static XrdXrootdXPath        RPList;    // Redirected paths
static XrdXrootdXPath        RQList;    // Redirected paths for ENOENT
static XrdXrootdXPath        XPList;    // Exported   paths
static XrdSfsFileSystem     *osFS;      // The filesystem
static XrdSfsFileSystem     *digFS;     // The filesystem (digFS)
static XrdSecService        *CIA;       // Authentication Server
static XrdSecProtector      *DHS;       // Protection     Server
static XrdTlsContext        *tlsCtx;    // Protection     Server TLS available
static XrdXrootdFileLock    *Locker;    // File lock handler
static XrdScheduler         *Sched;     // System scheduler
static XrdBuffManager       *BPool;     // Buffer manager
static XrdSysError          &eDest;     // Error message handler
static XrdNetPMark          *PMark;     // Packet marking API
static const char           *myInst;
static const char           *TraceID;
static int                   RQLxist;   // Something is present in RQList
static int                   myPID;
static int                   myRole;     // Role for kXR_protocol (>= 2.9.7)
static int                   myRolf;     // Role for kXR_protocol (<  2.9.7)

static gid_t                 myGID;
static uid_t                 myUID;
static int                   myGNLen;
static int                   myUNLen;
static const char           *myGName;
static const char           *myUName;
static time_t                keepT;

// Admin control area
//
static XrdNetSocket         *AdminSock;

// Processing configuration values
//
static int                 hailWait;
static int                 readWait;
static int                 Port;
static int                 Window;
static int                 tlsPort;
static char               *Notify;
static const char         *myCName;
static int                 myCNlen;
static char                isRedir;
static char                JobLCL;
static char                JobCKCGI;
static XrdXrootdJob       *JobCKS;
static char               *JobCKT;
static XrdOucTList        *JobCKTLST;
static XrdOucReqID        *PrepID;
static uint64_t            fsFeatures;

// Static redirection
//
static struct RD_Table {char          *Host[2];
                        unsigned short Port[2];
                                 short RDSz[2];} Route[RD_Num];

static struct RC_Table {char          *Domain[4];
                        short          DomCnt;
                        bool           pvtIP;
                        bool           lclDom;}  RouteClient;

static int    OD_Stall;
static bool   OD_Bypass;
static bool   OD_Redir;

static bool   CL_Redir;

static bool   isProxy;

// Extended attributes
//
static int    usxMaxNsz;
static int    usxMaxVsz;
static char  *usxParms;

// TLS configuration
//
static const char Req_TLSData  = 0x01;
static const char Req_TLSGPFile= 0x02;
static const char Req_TLSLogin = 0x04;
static const char Req_TLSSess  = 0x08;
static const char Req_TLSTPC   = 0x10;

static char   tlsCap;    // TLS requirements for capable clients
static char   tlsNot;    // TLS requirements for incapable clients

// Buffer configuration
//
static int                 maxBuffsz;    // Maximum buffer size we can have
static int                 maxTransz;    // Maximum transfer size we can have
static int                 maxReadv_ior; // Maximum readv element length

// Statistical area
//
static XrdXrootdStats     *SI;
int                        numReads;     // Count for kXR_read
int                        numReadP;     // Count for kXR_read pre-preads
int                        numReadV;     // Count for kkR_readv
int                        numSegsV;     // Count for kkR_readv  segmens
int                        numWritV;     // Count for kkR_write
int                        numSegsW;     // Count for kkR_writev segmens
int                        numWrites;    // Count
int                        numFiles;     // Count

int                        cumReads;     // Count less numReads
int                        cumReadP;     // Count less numReadP
int                        cumReadV;     // Count less numReadV
int                        cumSegsV;     // Count less numSegsV
int                        cumWritV;     // Count less numWritV
int                        cumSegsW;     // Count less numSegsW
int                        cumWrites;    // Count less numWrites
int                        myStalls;     // Number of stalls
long long                  totReadP;     // Bytes

// Data local to each protocol/link combination
//
XrdLink                   *Link;
XrdBuffer                 *argp;
XrdXrootdFileTable        *FTab;
XrdXrootdMonitor::User     Monitor;
XrdNetPMark::Handle       *pmHandle;
int                        clientPV; // Protocol version + capabilities
int                        clientRN; // Release as maj.min.patch (1 byte each).
bool                       pmDone;   // Packet marking has been enabled
char                       reserved[3];
short                      rdType;
char                       Status;
unsigned char              CapVer;

// Authentication area
//
XrdSecEntity              *Client;
XrdSecProtocol            *AuthProt;
XrdSecEntity               Entity;
XrdSecProtect             *Protect;
char                      *AppName;

// Request signing area
//
ClientRequest              sigReq2Ver;   // Request to verify
SecurityRequest            sigReq;       // Signature request
char                       sigBuff[64];  // Signature payload SHA256 + blowfish
bool                       sigNeed;      // Signature target  present
bool                       sigHere;      // Signature request present
bool                       sigRead;      // Signature being read
bool                       sigWarn;      // Once for unneeded signature

// Async I/O area, these need to be atomic
//
RAtomic_int                linkAioReq;   // Aio requests   inflight for link
static RAtomic_int         srvrAioOps;   // Aio operations inflight for server

// Buffer information, used to drive getData(), and (*Resume)()
//
XrdXrootdPgwCtl           *pgwCtl;
char                      *myBuff;
int                        myBlen;
int                        myBlast;

struct GetDataCtl
{
int                        iovNum;
int                        iovNow;
union {int                 iovAdj;
       int                 BuffLen;
       int                 DumpLen;
      };
bool                       useCB;
char                       Status;
unsigned char              stalls;
RAtomic_uchar              linkWait;
union {struct iovec       *iovVec;
       char               *Buffer;
      };
const  char               *ioDType;
XrdXrootd::gdCallBack     *CallBack;

static const int inNone    = 0;
static const int inCallBk  = 1;
static const int inData    = 2;
static const int inDataIov = 3;
static const int inDump    = 4;

static const int Active    = 1;  // linkWait: thread is waiting for link
static const int Terminate = 3;  // linkWait: thread should immediately exit

}                          gdCtl;

XrdXrootdWVInfo           *wvInfo;
int                       (XrdXrootdProtocol::*ResumePio)(); //Used by Offload
int                       (XrdXrootdProtocol::*Resume)();
XrdXrootd::IOParms         IO;

// Buffer resize control area
//
static int                 hcMax;
       int                 hcPrev;
       int                 hcNext;
       int                 hcNow;
       int                 halfBSize;

// This area is used for parallel streams
//
XrdSysMutex                unbindMutex;   // If locked always before streamMutex
XrdSysMutex                streamMutex;
XrdSysSemaphore           *reTry;
XrdSysCondVar2            *endNote;
XrdXrootdProtocol         *Stream[maxStreams];
unsigned int               mySID;
bool                       isActive;
bool                       isLinkWT;
bool                       isNOP;
bool                       isDead;

static const int           maxPio = 4;
XrdXrootdPio              *pioFirst;
XrdXrootdPio              *pioLast;
XrdXrootdPio              *pioFree;

short                      PathID;       // Path for this protocol object
bool                       newPio;       // True when initially scheduled
unsigned char              rvSeq;
unsigned char              wvSeq;

char                       doTLS;       // TLS requirements for client
bool                       ableTLS;     // T->Client is able to use TLS
bool                       isTLS;       // T->Client using TLS on control stream

// Track usage limts.
//
static bool                PrepareAlt;  // Use alternate prepare handling
static bool                LimitError;  // Indicates that hitting a limit should result in an error response.
                                        // If false, when possible, silently ignore errors.
int                        PrepareCount;
static int                 PrepareLimit;

// Buffers to handle client requests
//
XrdXrootdReqID             ReqID;
ClientRequest              Request;
XrdXrootdResponse          Response;
};
#endif
