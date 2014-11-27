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
 
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSfs/XrdSfsDio.hh"

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
#define XRD_ADMINUSER      4
#define XRD_BOUNDPATH      8

#ifndef __GNUC__
#define __attribute__(x)
#endif

/******************************************************************************/
/*                   x r d _ P r o t o c o l _ X R o o t d                    */
/******************************************************************************/

class XrdNetSocket;
class XrdOucErrInfo;
class XrdOucReqID;
class XrdOucStream;
class XrdOucTList;
class XrdOucTokenizer;
class XrdOucTrace;
class XrdSfsDirectory;
class XrdSfsFileSystem;
class XrdSecProtocol;
class XrdBuffer;
class XrdLink;
class XrdXrootdAioReq;
class XrdXrootdFile;
class XrdXrootdFileLock;
class XrdXrootdFileTable;
class XrdXrootdJob;
class XrdXrootdMonitor;
class XrdXrootdPio;
class XrdXrootdStats;
class XrdXrootdXPath;

class XrdXrootdProtocol : public XrdProtocol, public XrdSfsDio
{
friend class XrdXrootdAdmin;
friend class XrdXrootdAioReq;
public:

static int           Configure(char *parms, XrdProtocol_Config *pi);

       void          DoIt() {(*this.*Resume)();}

       int           do_WriteSpan();

       XrdProtocol  *Match(XrdLink *lp);

       int           Process(XrdLink *lp); //  Sync: Job->Link.DoIt->Process

       int           Process2();

       void          Recycle(XrdLink *lp, int consec, const char *reason);

       int           SendFile(int fildes);

       int           SendFile(XrdOucSFVec *sfvec, int sfvnum);

       void          SetFD(int fildes);

       int           Stats(char *buff, int blen, int do_sync=0);

static int           StatGen(struct stat &buf, char *xxBuff);

              XrdXrootdProtocol operator =(const XrdXrootdProtocol &rhs);
              XrdXrootdProtocol();
             ~XrdXrootdProtocol() {Cleanup();}

private:

// Note that Route[] structure (below) must have RD_Num elements!
//
enum RD_func {RD_chmod = 0, RD_chksum,  RD_dirlist, RD_locate, RD_mkdir,
              RD_mv,        RD_prepare, RD_prepstg, RD_rm,     RD_rmdir,
              RD_stat,      RD_trunc,
              RD_open1,     RD_open2,   RD_open3,   RD_open4,  RD_Num};

       int   do_Admin();
       int   do_Auth();
       int   do_Bind();
       int   do_Chmod();
       int   do_CKsum(int canit);
       int   do_CKsum(char *algT, const char *Path, const char *Opaque);
       int   do_Close();
       int   do_Dirlist();
       int   do_DirStat(XrdSfsDirectory *dp, char *pbuff, const char *opaque);
       int   do_Endsess();
       int   do_Getfile();
       int   do_Login();
       int   do_Locate();
       int   do_Mkdir();
       int   do_Mv();
       int   do_Offload(int pathID, int isRead);
       int   do_OffloadIO();
       int   do_Open();
       int   do_Ping();
       int   do_Prepare();
       int   do_Protocol(int retRole=0);
       int   do_Putfile();
       int   do_Qconf();
       int   do_Qfh();
       int   do_Qopaque(short);
       int   do_Qspace();
       int   do_Query();
       int   do_Qxattr();
       int   do_Read();
       int   do_ReadV();
       int   do_ReadAll(int asyncOK=1);
       int   do_ReadNone(int &retc, int &pathID);
       int   do_Rm();
       int   do_Rmdir();
       int   do_Set();
       int   do_Set_Mon(XrdOucTokenizer &setargs);
       int   do_Stat();
       int   do_Statx();
       int   do_Sync();
       int   do_Truncate();
       int   do_Write();
       int   do_WriteAll();
       int   do_WriteCont();
       int   do_WriteNone();

       int   aio_Error(const char *op, int ecode);
       int   aio_Read();
       int   aio_Write();
       int   aio_WriteAll();
       int   aio_WriteCont();

       void  Assign(const XrdXrootdProtocol &rhs);
static int   CheckSum(XrdOucStream *, char **, int);
       void  Cleanup();
static int   Config(const char *fn);
       int   fsError(int rc, char opc, XrdOucErrInfo &myError, const char *Path);
       int   getBuff(const int isRead, int Quantum);
       int   getData(const char *dtype, char *buff, int blen);
       void  logLogin(bool xauth=false);
static int   mapMode(int mode);
static void  PidFile();
       void  Reset();
static int   rpCheck(char *fn, const char **opaque);
       int   rpEmsg(const char *op, char *fn);
       int   vpEmsg(const char *op, char *fn);
static int   Squash(char *);
static int   xapath(XrdOucStream &Config);
static int   xasync(XrdOucStream &Config);
static int   xcksum(XrdOucStream &Config);
static int   xdig(XrdOucStream &Config);
static int   xexp(XrdOucStream &Config);
static int   xexpdo(char *path, int popt=0);
static int   xfsl(XrdOucStream &Config);
static int   xfsL(XrdOucStream &Config, char *val, int lix);
static int   xpidf(XrdOucStream &Config);
static int   xprep(XrdOucStream &Config);
static int   xlog(XrdOucStream &Config);
static int   xmon(XrdOucStream &Config);
static int   xred(XrdOucStream &Config);
static void  xred_set(RD_func func, char *rHost[2], int rPort[2]);
static bool  xred_xok(int     func, char *rHost[2], int rPort[2]);
static int   xsecl(XrdOucStream &Config);
static int   xtrace(XrdOucStream &Config);

static XrdObjectQ<XrdXrootdProtocol> ProtStack;
XrdObject<XrdXrootdProtocol>         ProtLink;

protected:

       void  MonAuth();
       int   SetSF(kXR_char *fhandle, bool seton=false);

static XrdXrootdXPath        RPList;    // Redirected paths
static XrdXrootdXPath        RQList;    // Redirected paths for ENOENT
static XrdXrootdXPath        XPList;    // Exported   paths
static XrdSfsFileSystem     *osFS;      // The filesystem
static XrdSfsFileSystem     *digFS;     // The filesystem (digFS)
static XrdSecService        *CIA;       // Authentication Server
static XrdXrootdFileLock    *Locker;    // File lock handler
static XrdScheduler         *Sched;     // System scheduler
static XrdBuffManager       *BPool;     // Buffer manager
static XrdSysError           eDest;     // Error message handler
static const char           *myInst;
static const char           *TraceID;
static       char           *pidPath;
static int                   RQLxist;   // Something is present in RQList
static int                   myPID;
static int                   myRole;     // Role for kXR_protocol (>= 2.9.7)
static int                   myRolf;     // Role for kXR_protocol (<  2.9.7)

// Admin control area
//
static XrdNetSocket         *AdminSock;

// Processing configuration values
//
static int                 hailWait;
static int                 readWait;
static int                 Port;
static int                 Window;
static int                 WANPort;
static int                 WANWindow;
static char               *SecLib;
static char               *FSLib[2];
static int                 FSLvn[2];
static char               *digLib;    // Normally zero for now
static char               *digParm;
static char               *Notify;
static char                isRedir;
static char                JobLCL;
static char                JobCKCGI;
static XrdXrootdJob       *JobCKS;
static char               *JobCKT;
static XrdOucTList        *JobCKTLST;
static XrdOucReqID        *PrepID;

// Static redirection
//
static struct RD_Table {char *Host[2]; int Port[2];} Route[RD_Num];

// async configuration values
//
static int                 as_maxperlnk; // Max async requests per link
static int                 as_maxperreq; // Max async ops per request
static int                 as_maxpersrv; // Max async ops per server
static int                 as_miniosz;   // Min async request size
static int                 as_minsfsz;   // Min sendf request size
static int                 as_segsize;   // Aio quantum (optimal)
static int                 as_maxstalls; // Maximum stalls we will tolerate
static int                 as_force;     // aio to be forced
static int                 as_noaio;     // aio is disabled
static int                 as_nosf;      // sendfile is disabled
static int                 as_syncw;     // writes to be synchronous
static int                 maxBuffsz;    // Maximum buffer size we can have
static int                 maxTransz;    // Maximum transfer size we can have
static const int           maxRvecsz = 1024;   // Maximum read vector size

// Statistical area
//
static XrdXrootdStats     *SI;
int                        numReads;     // Count for kXR_read
int                        numReadP;     // Count for kXR_read pre-preads
int                        numReadV;     // Count for kR_readv
int                        numSegsV;     // Count for kR_readv segmens
int                        numWrites;    // Count
int                        numFiles;     // Count

int                        cumReads;     // Count less numReads
int                        cumReadP;     // Count less numReadP
int                        cumReadV;     // Count less numReadV
int                        cumSegsV;     // Count less numSegsV
int                        cumWrites;    // Count less numWrites
long long                  totReadP;     // Bytes

// Data local to each protocol/link combination
//
XrdLink                   *Link;
XrdBuffer                 *argp;
XrdXrootdFileTable        *FTab;
XrdXrootdMonitor::User     Monitor;
int                        clientPV;
short                      rdType;
char                       Status;
unsigned char              CapVer;

// Authentication area
//
XrdSecEntity              *Client;
XrdSecProtocol            *AuthProt;
XrdSecEntity               Entity;

// Buffer information, used to drive DoIt(), getData(), and (*Resume)()
//
XrdXrootdAioReq           *myAioReq;
char                      *myBuff;
int                        myBlen;
int                        myBlast;
int                       (XrdXrootdProtocol::*Resume)();
XrdXrootdFile             *myFile;
union {
long long                  myOffset;
int                        myEInfo[2];
      };
int                        myIOLen;
int                        myStalls;

// Buffer resize control area
//
static int                 hcMax;
       int                 hcPrev;
       int                 hcNext;
       int                 hcNow;
       int                 halfBSize;

// This area is used for parallel streams
//
static const int           maxStreams = 16;
XrdSysMutex                streamMutex;
XrdSysSemaphore           *reTry;
XrdXrootdProtocol         *Stream[maxStreams];
unsigned int               mySID;
char                       isActive;
char                       isDead;
char                       isBound;
char                       isNOP;

static const int           maxPio = 4;
XrdXrootdPio              *pioFirst;
XrdXrootdPio              *pioLast;
XrdXrootdPio              *pioFree;

short                      PathID;
char                       doWrite;
char                       doWriteC;
char                       rvSeq;

// Buffers to handle client requests
//
XrdXrootdReqID             ReqID;
ClientRequest              Request;
XrdXrootdResponse          Response;
};
#endif
