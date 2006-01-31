#ifndef __XROOTD_PROTOCOL_H__
#define __XROOTD_PROTOCOL_H__
/******************************************************************************/
/*                                                                            */
/*                  X r d X r o o t d P r o t o c o l . h h                   */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//       $Id$
 
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdSec/XrdSecInterface.hh"

#include "Xrd/XrdObject.hh"
#include "Xrd/XrdProtocol.hh"
#include "XrdXrootd/XrdXrootdResponse.hh"
#include "XProtocol/XProtocol.hh"

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define XROOTD_VERSBIN 0x00000245

#define XROOTD_VERSION "2.4.5"

#define ROOTD_PQ 2012

#define XRD_LOGGEDIN       1
#define XRD_NEED_AUTH      2
#define XRD_ADMINUSER      4

#ifndef __GNUC__
#define __attribute__(x)
#endif

/******************************************************************************/
/*                   x r d _ P r o t o c o l _ X R o o t d                    */
/******************************************************************************/

class XrdNetSocket;
class XrdOucErrInfo;
class XrdOucStream;
class XrdOucTokenizer;
class XrdOucTrace;
class XrdSfsFileSystem;
class XrdSecProtocol;
class XrdBuffer;
class XrdLink;
class XrdXrootdFile;
class XrdXrootdFileLock;
class XrdXrootdFileTable;
class XrdXrootdJob;
class XrdXrootdMonitor;
class XrdXrootdStats;
class XrdXrootdXPath;
class XrdXrootdAioReq;

class XrdXrootdProtocol : public XrdProtocol
{
friend class XrdXrootdAdmin;
friend class XrdXrootdAioReq;
public:

static int           Configure(char *parms, XrdProtocol_Config *pi);

       void          DoIt() {(*this.*Resume)();}

       XrdProtocol  *Match(XrdLink *lp);

       int           Process(XrdLink *lp); //  Sync: Job->Link.DoIt->Process

       void          Recycle(XrdLink *lp, int consec, const char *reason);

       int           Stats(char *buff, int blen, int do_sync=0);

              XrdXrootdProtocol operator =(const XrdXrootdProtocol &rhs);
              XrdXrootdProtocol();
             ~XrdXrootdProtocol() {Cleanup();}

private:

       int   do_Admin();
       int   do_Auth();
       int   do_Chmod();
       int   do_CKsum(int canit);
       int   do_Close();
       int   do_Dirlist();
       int   do_Endsess();
       int   do_Getfile();
       int   do_Login();
       int   do_Mkdir();
       int   do_Mv();
       int   do_Open();
       int   do_Ping();
       int   do_Prepare();
       int   do_Protocol();
       int   do_Putfile();
       int   do_Query();
       int   do_Read();
       int   do_ReadAll();
       int   do_ReadNone(int &retc);
       int   do_Rm();
       int   do_Rmdir();
       int   do_Set();
       int   do_Set_Mon(XrdOucTokenizer &setargs);
       int   do_Stat();
       int   do_StatGen(struct stat &buf, char *xxBuff);
       int   do_Statx();
       int   do_Sync();
       int   do_Write();
       int   do_WriteAll();
       int   do_WriteCont();
       int   do_WriteNone();

       int   aio_Error(const char *op, int ecode);
       int   aio_Read();
       int   aio_Write();
       int   aio_WriteAll();
       int   aio_WriteCont();

static XrdNetSocket *ASocket(char *path, const char *fn, mode_t mode, int isudp=0);
static char *ASPath(char *path, const char *fn, mode_t mode);
       void  Assign(const XrdXrootdProtocol &rhs);
       void  Cleanup();
static int   Config(const char *fn);
       int   fsError(int rc, XrdOucErrInfo &myError);
       int   getBuff(const int isRead, int Quantum);
       int   getData(const char *dtype, char *buff, int blen);
static int   mapError(int rc);
static int   mapMode(int mode);
       int   Process2();
       void  Reset();
static int   rpCheck(char *fn, const char **opaque);
       int   rpEmsg(const char *op, char *fn);
       int   vpEmsg(const char *op, char *fn);
static int   Squash(char *);
static int   xapath(XrdOucStream &Config);
static int   xasync(XrdOucStream &Config);
static int   xcksum(XrdOucStream &Config);
static int   xexp(XrdOucStream &Config);
static int   xexpdo(char *path);
static int   xfsl(XrdOucStream &Config);
static int   xprep(XrdOucStream &Config);
static int   xlog(XrdOucStream &Config);
static int   xmon(XrdOucStream &Config);
static int   xsecl(XrdOucStream &Config);
static int   xtrace(XrdOucStream &Config);

static XrdObjectQ<XrdXrootdProtocol> ProtStack;
XrdObject<XrdXrootdProtocol>         ProtLink;

protected:

static XrdXrootdXPath        XPList;    // Exported paths
static XrdSfsFileSystem     *osFS;      // The filesystem
static XrdSecService        *CIA;       // Authentication Server
static XrdXrootdFileLock    *Locker;    // File lock handler
static XrdScheduler         *Sched;     // System scheduler
static XrdBuffManager       *BPool;     // Buffer manager
static XrdOucError           eDest;     // Error message handler
static const char           *myInst;
static const char           *TraceID;
static int                   myPID;

// Admin control area
//
static XrdNetSocket         *AdminSock;

// Processing configuration values
//
static int                 readWait;
static int                 Port;
static char               *SecLib;
static char               *FSLib;
static char               *Notify;
static char                isRedir;
static char                chkfsV;
static XrdXrootdJob       *JobCKS;
static char               *JobCKT;

// async configuration values
//
static int                 as_maxperlnk; // Max async requests per link
static int                 as_maxperreq; // Max async ops per request
static int                 as_maxpersrv; // Max async ops per server
static int                 as_miniosz;   // Min async request size
static int                 as_segsize;   // Aio quantum (optimal)
static int                 as_maxstalls; // Maximum stalls we will tolerate
static int                 as_force;     // aio to be forced
static int                 as_noaio;     // aio is disabled
static int                 as_syncw;     // writes to be synchronous
static int                 maxBuffsz;    // Maximum buffer size we can have

// Statistical area
//
static XrdXrootdStats     *SI;
int                        numReads;     // Count
int                        numReadP;     // Count
int                        numWrites;    // Count
int                        numFiles;     // Count

int                        cumReads;     // Count less numReads
int                        cumReadP;     // Count less numReadP
int                        cumWrites;    // Count less numWrites
long long                  totReadP;     // Bytes

// Data local to each protocol/link combination
//
XrdLink                   *Link;
XrdBuffer                 *argp;
XrdXrootdFileTable        *FTab;
XrdXrootdMonitor          *Monitor;
kXR_unt32                  monUID;
char                       monFILE;
char                       monIO;
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
long long                  myOffset;
int                        myIOLen;
int                        myStalls;
int                        myLastRC;

// Buffer resize control area
//
static int                 hcMax;
       int                 hcPrev;
       int                 hcNext;
       int                 hcNow;
       int                 halfBSize;

// Buffers to handle client requests
//
ClientRequest              Request;
XrdXrootdResponse          Response;
};
#endif
