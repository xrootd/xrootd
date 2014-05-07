/******************************************************************************/
/*                                                                            */
/*                             X r d C p y . c c                              */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/* Author: Fabrizio Furano (INFN Padova, 2004)                                */
/*            Modified by Andrew Hanushevsky (2012) under contract            */
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
/* Author: Fabrizio Furano (INFN Padova, 2004)                                */
/*            Modified by Andrew Hanushevsky (2012) under contract            */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
//////////////////////////////////////////////////////////////////////////
//                                                                      //
// A cp-like command line tool for xrootd environments                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientUrlInfo.hh"
#include "XrdClient/XrdClientReadCache.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdCpMthrQueue.hh"
#include "XrdClient/XrdClientConn.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdCpWorkLst.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include "XrdClient/XrdClientAbsMonIntf.hh"
#include "XrdClient/XrdcpXtremeRead.hh"

#include "XrdCks/XrdCks.hh"
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCks/XrdCksData.hh"

#include "XrdApps/XrdCpConfig.hh"
#include "XrdApps/XrdCpFile.hh"

#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucTPC.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>
#ifndef WIN32
#include <sys/time.h>
#include <unistd.h>
#include <dlfcn.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <sstream>

#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

/******************************************************************************/
/*                  G l o b a l   C o n f i g u r a t i o n                   */
/******************************************************************************/
  
namespace XrdCopy
{
XrdCpConfig  Config("xrdcp");
XrdCksData   srcCksum, dstCksum;
XrdCksCalc  *csObj;
XrdClient   *tpcSrc;
char         tpcKey[32];
long long    tpcFileSize;
pthread_t    tpcTID;
int          tpcPB;
int          isSrv;
int          isTPC;
int          getCks;
int          lenCks;
int          prtCks;
int          setCks;
int          verCks;
int          xeqCks;
int          lclCks;
static const int rwMode = kXR_ur | kXR_uw | kXR_gw | kXR_gr | kXR_or;
}

using namespace XrdCopy;

#define EMSG(x) {if (isSrv) cout <<Config.Pgm <<": " <<x <<endl;\
                    else    cerr <<Config.Pgm <<": " <<x <<endl;}

extern "C" {
/////////////////////////////////////////////////////////////////////
// function + macro to allow formatted print via cout,cerr
/////////////////////////////////////////////////////////////////////
 void cout_print(const char *format, ...)
 {
    char cout_buff[4096];
    va_list args;
    va_start(args, format);
    vsprintf(cout_buff, format,  args);
    va_end(args);
    cout << cout_buff;
 }

   void cerr_print(const char *format, ...)
   {
      char cerr_buff[4096];
      va_list args;
      va_start(args, format);
      vsprintf(cerr_buff, format,  args);
      va_end(args);
      cerr <<cerr_buff;
   }

#define COUT(s) do {				\
      cout_print s;				\
   } while (0)

#define CERR(s) do {				\
      cerr_print s;				\
   } while (0)

}
//////////////////////////////////////////////////////////////////////


struct XrdCpInfo {
   XrdClient                    *XrdCli;
   int                          localfile;
   long long                    len, bread, bwritten;
   XrdCpMthrQueue               queue;
   XrdClientAbsMonIntf          *mon;

   XrdCpInfo() : XrdCli(0),localfile(0),len(0),bread(0),bwritten(0),mon(0) {}
} cpnfo;

#define XRDCP_BLOCKSIZE          (8*1024*1024)
#define XRDCP_XRDRASIZE          (30*XRDCP_BLOCKSIZE)
#define XRDCP_VERSION            "(C) 2004-2011 by the XRootD collaboration. Version: " XrdVSTRING

///////////////////////////////////////////////////////////////////////
// Coming from parameters on the cmd line

bool summary=false;            // print summary
bool progbar=true;             // print progbar
bool Verbose=true;             // be verbose

XrdOucString monlibname = "libXrdCpMonitorClient.so"; // Default name for the ext monitoring lib

// Default open flags for opening a file (xrd)
kXR_unt16 xrd_wr_flags=kXR_async | kXR_mkpath | kXR_open_updt | kXR_new;

// Flags for open() to force overwriting or not. Default is not.
#define LOC_WR_FLAGS_FORCE ( O_CREAT | O_WRONLY | O_TRUNC | O_BINARY );
#define LOC_WR_FLAGS       ( O_CREAT | O_WRONLY | O_EXCL | O_BINARY );
int loc_wr_flags = LOC_WR_FLAGS;

bool recurse = false;

bool doXtremeCp = false;
XrdOucString XtremeCpRdr;

///////////////////////

// To compute throughput etc
struct timeval abs_start_time;
struct timeval abs_stop_time;
struct timezone tz;

/******************************************************************************/
/*                         p r i n t _ s u m m a r y                          */
/******************************************************************************/
  
void print_summary(const char* src, const char* dst, unsigned long long bytesread)
{
   gettimeofday (&abs_stop_time, &tz);
   float abs_time=((float)((abs_stop_time.tv_sec- abs_start_time.tv_sec)*1000 +
                           (abs_stop_time.tv_usec-abs_start_time.tv_usec)/1000));


   XrdOucString xsrc(src);
   XrdOucString xdst(dst);
   xsrc.erase(xsrc.rfind('?'));
   xdst.erase(xdst.rfind('?'));

   COUT(("[xrdcp] #################################################################\n"));
   COUT(("[xrdcp] # Source Name              : %s\n",xsrc.c_str()));
   COUT(("[xrdcp] # Destination Name         : %s\n",xdst.c_str()));
   COUT(("[xrdcp] # Data Copied [bytes]      : %lld\n",bytesread));
   COUT(("[xrdcp] # Realtime [s]             : %f\n",abs_time/1000.0));
   if (abs_time > 0) {
      COUT(("[xrdcp] # Eff.Copy. Rate[MB/s]     : %f\n",bytesread/abs_time/1000.0));
   }
   if (xeqCks)
      {static const int Bsz = 64;
       char Buff[Bsz];
       dstCksum.Get(Buff,Bsz);
       COUT(("[xrdcp] # %8s                 : %s\n", dstCksum.Name, Buff));
      }
   COUT(("[xrdcp] #################################################################\n"));
}

/******************************************************************************/
/*                         p r i n t _ p r o g b a r                          */
/******************************************************************************/
  
void print_progbar(unsigned long long bytesread, unsigned long long size) {
   CERR(("[xrootd] Total %.02f MB\t|",(float)size/1024/1024));
   for (int l=0; l< 20;l++) {
      if (l< ( (int)(20.0*bytesread/size)))
	 CERR(("="));
      if (l==( (int)(20.0*bytesread/size)))
	 CERR((">"));
      if (l> ( (int)(20.0*bytesread/size)))
	 CERR(("."));
   }
  
   float abs_time=((float)((abs_stop_time.tv_sec - abs_start_time.tv_sec) *1000 +
			   (abs_stop_time.tv_usec - abs_start_time.tv_usec) / 1000));
   CERR(("| %.02f %% [%.01f MB/s]\r",100.0*bytesread/size,bytesread/abs_time/1000.0));
}

/******************************************************************************/
/*                          p r i n t _ c h k s u m                           */
/******************************************************************************/
  
void print_chksum(const char* src, unsigned long long bytesread)
{
   const char *csName;
   char Buff[64];
   int csLen;
   XrdOucString xsrc(src);
   xsrc.erase(xsrc.rfind('?'));

   if (lclCks && csObj)
      {const void *csVal  = csObj->Final();
       csName = csObj->Type(csLen);
       srcCksum.Set(csVal, csLen);
       srcCksum.Get(Buff, sizeof(Buff));
      } else {
       dstCksum.Get(Buff, sizeof(Buff));
       csName = dstCksum.Name;
      }
   cout <<csName <<": " <<Buff <<' ' <<xsrc <<' ' <<bytesread <<endl;
}

/******************************************************************************/
/*                             d o P r o g B a r                              */
/******************************************************************************/

void *doProgBar(void *Parm)
{
   XrdClientUrlInfo *dUrl = (XrdClientUrlInfo *)Parm;
   XrdClientAdmin Adm(dUrl->GetUrl().c_str());
   const char *fName = dUrl->File.c_str();
   long long fSize;
   long id, flags, mtime;

// Prevent cancelation as the admin client can't handle that
//
   pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, 0);
   pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);

// Open a path to the destination
//
   if (!Adm.Connect()) return 0;

// Print the progress bar until we are canceled
//
   while(Adm.Stat(fName, id, fSize, flags, mtime))
        {gettimeofday(&abs_stop_time,&tz);
         print_progbar(fSize, tpcFileSize);
         pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
         pthread_testcancel();
         sleep(3);
         pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
        }

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                           u n d o P r o g B a r                            */
/******************************************************************************/

void undoProgBar(int isOK)
{
   void *thret;

   if (tpcPB)
      {tpcPB = 0;
       pthread_cancel(tpcTID);
       pthread_join(tpcTID, &thret);	
       if (isOK)
        {gettimeofday(&abs_stop_time,&tz);
         print_progbar(tpcFileSize, tpcFileSize);
        }
       cerr <<endl;
      }
}

/******************************************************************************/
/*                               c p F a t a l                                */
/******************************************************************************/
  
int cpFatal(const char *Act, XrdClient *cSrc, XrdClient *cDst, const char *hn=0)
{
   XrdClient *cObj;
   const char *Msg;

   if (tpcPB) undoProgBar(0);

   if (cSrc) {cObj = cSrc; Msg = "Copy from ";}
      else   {cObj = cDst; Msg = "Copy to ";}

   if (!hn) hn = cObj->GetCurrentUrl().Host.c_str();

   EMSG(Msg <<hn <<" failed on " <<Act <<"!");
   EMSG(ServerError(cObj));
   return -1;
}
  
/******************************************************************************/
/*                              g e t F N a m e                               */
/******************************************************************************/
  
const char *getFName(const char *Url)
{
   static char fBuff[2048];
   const char *Qmark = index(Url, '?');

   if (!Qmark) return Url;

   int n = (Qmark - Url);
   if (n >= (int)sizeof(fBuff)) n = sizeof(fBuff)-1;
   strncpy(fBuff, Url, n);
   fBuff[n] = 0;
   return fBuff;
}

/******************************************************************************/
/*                      R e a d e r T h r e a d _ x r d                       */
/******************************************************************************/

// The body of a thread which reads from the global
//  XrdClient and keeps the queue filled
//____________________________________________________________________________
void *ReaderThread_xrd(void *)
{

   Info(XrdClientDebug::kHIDEBUG,
	"ReaderThread_xrd",
	"Reader Thread starting.");
   
   pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, 0);
   pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);


   void *buf;
   long long offs = 0;
   int nr = 1;
   long long bread = 0, len = 0;
   long blksize;

   len = cpnfo.len;

   while ((nr > 0) && (offs < len)) {
      buf = malloc(XRDCP_BLOCKSIZE);
      if (!buf) {
   EMSG("Copy failed; out of memory.");
   _exit(13);
      }

      
      blksize = xrdmin(XRDCP_BLOCKSIZE, len-offs);

      if ( (nr = cpnfo.XrdCli->Read(buf, offs, blksize)) ) {
         cpnfo.queue.PutBuffer(buf, offs, nr);
         cpnfo.XrdCli->RemoveDataFromCache(offs, offs+nr-1, false);
	 bread += nr;
	 offs += nr;
      }

      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
      pthread_testcancel();
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
   }

   cpnfo.bread = bread;

   // This ends the transmission... bye bye
   cpnfo.queue.PutBuffer(0, 0, 0);

   return 0;
}

/******************************************************************************/
/*               R e a d e r T h r e a d _ x r d _ x t r e m e                */
/******************************************************************************/
  
// The body of a thread which reads from the global
//  XrdClient and keeps the queue filled
// This is the thread for extreme reads, in this case we may have multiple of these
// threads, reading the same file from different server endpoints
//____________________________________________________________________________
struct xtreme_threadnfo {
   XrdXtRdFile *xtrdhandler;

   // The client used by this thread
   XrdClient *cli;

   // A unique integer identifying the client instance
   int clientidx;

   // The block from which to start prefetching/reading
   int startfromblk;

   // Max convenient number of outstanding blks
   int maxoutstanding;
}; 
void *ReaderThread_xrd_xtreme(void *parm)
{

   Info(XrdClientDebug::kHIDEBUG,
	"ReaderThread_xrd_xtreme",
	"Reader Thread starting.");
   
   pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, 0);
   pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);

   void *buf;

   int nr = 1;
   int noutstanding = 0;


   // Which block to read
   XrdXtRdBlkInfo *blknfo = 0;
   xtreme_threadnfo *thrnfo = (xtreme_threadnfo *)parm;

   // Block to prefetch
   int lastprefetched = thrnfo->startfromblk;
   int lastread = lastprefetched;

   thrnfo->cli->Open(0, 0, true);

   thrnfo->cli->SetCacheParameters(XRDCP_BLOCKSIZE*4*thrnfo->maxoutstanding*2, 0, XrdClientReadCache::kRmBlk_FIFO);
   if (thrnfo->cli->IsOpen_wait())
   while (nr > 0) {

      // Keep always some blocks outstanding from the point of view of this reader
      while (noutstanding < thrnfo->maxoutstanding) {
         int lp;
         lp = thrnfo->xtrdhandler->GetBlkToPrefetch(lastprefetched, thrnfo->clientidx, blknfo);
         if (lp >= 0) {
            //cout << "cli: " << thrnfo->clientidx << " prefetch: " << lp << " offs: " << blknfo->offs << " len: " << blknfo->len << endl;
            if ( thrnfo->cli->Read_Async(blknfo->offs, blknfo->len) == kOK ) {  
               lastprefetched = lp;
               noutstanding++;
            }
            else break;
         }
         else break;
      }

      int lr = thrnfo->xtrdhandler->GetBlkToRead(lastread, thrnfo->clientidx, blknfo);
      if (lr >= 0) {

         buf = malloc(blknfo->len);
         if (!buf) {
            EMSG("Copy failed; out of memory.");
            _exit(13);
         }

         //cout << "cli: " << thrnfo->clientidx << "     read: " << lr << " offs: " << blknfo->offs << " len: " << blknfo->len << endl;

         // It is very important that the search for a blk to read starts from the first block upwards
         nr = thrnfo->cli->Read(buf, blknfo->offs, blknfo->len);
         if ( nr >= 0 ) {
            lastread = lr;
            noutstanding--;

            // If this block was stolen by somebody else then this client has to be penalized
            // If this client stole the blk to some other client, then this client has to be rewarded
            int reward = thrnfo->xtrdhandler->MarkBlkAsRead(lr);
            if (reward >= 0) 
               // Enqueue the block only if it was not already read
               cpnfo.queue.PutBuffer(buf, blknfo->offs, nr);

            if (reward > 0) {
               thrnfo->maxoutstanding++;
               thrnfo->maxoutstanding = xrdmin(20, thrnfo->maxoutstanding);
               thrnfo->cli->SetCacheParameters(XRDCP_BLOCKSIZE*4*thrnfo->maxoutstanding*2, 0, XrdClientReadCache::kRmBlk_FIFO);
            }
            if (reward < 0) {
               thrnfo->maxoutstanding--;
               free(buf);
            }

            if (thrnfo->maxoutstanding <= 0) {
               sleep(1);
               thrnfo->maxoutstanding = 1;
            }

         }

         // It is very important that the search for a blk to read starts from the first block upwards
         thrnfo->cli->RemoveDataFromCache(blknfo->offs, blknfo->offs+blknfo->len-1, false);
      }
      else {

         if (thrnfo->xtrdhandler->AllDone()) break;
         pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
         sleep(1);
      }


      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
      pthread_testcancel();
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
   }

   // We get here if there are no more blocks to read or to steal from other readers
   // This ends the transmission... bye bye
   cpnfo.queue.PutBuffer(0, 0, 0);

   return 0;
}

/******************************************************************************/
/*                      R e a d e r T h r e a d _ l o c                       */
/******************************************************************************/

// The body of a thread which reads from the global filehandle
//  and keeps the queue filled
//____________________________________________________________________________
void *ReaderThread_loc(void *) {

   Info(XrdClientDebug::kHIDEBUG,
	"ReaderThread_loc",
	"Reader Thread starting.");

   pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, 0);
   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);

   void *buf;
   long long offs = 0;
   int nr = 1;
   long long bread = 0;

   while (nr > 0) {
      buf = malloc(XRDCP_BLOCKSIZE);
      if (!buf) {
         EMSG("Copy failed; out of memory.");
         _exit(13);
      }

      //------------------------------------------------------------------------
      // If this read fails it means that either the program logic is
      // flawed, or there was a low level hardware failure. In either case
      // continuing may cause more harm than good.
      //------------------------------------------------------------------------
      nr = read( cpnfo.localfile, buf, XRDCP_BLOCKSIZE );
      if( nr < 0 )
      {
        EMSG(strerror( errno ) <<" reading local file.");
        _exit(17);
      }
      if( nr > 0)
      {
        cpnfo.queue.PutBuffer(buf, offs, nr);
        bread += nr;
        offs += nr;
      }
   }

   cpnfo.bread = bread;

   // This ends the transmission... bye bye
   cpnfo.queue.PutBuffer(0, 0, 0);

   return 0;
}

/******************************************************************************/
/*                    C r e a t e D e s t P a t h _ l o c                     */
/******************************************************************************/

int CreateDestPath_loc(XrdOucString path, bool isdir) {
   // We need the path name without the file
   if (!isdir) {
      int pos = path.rfind('/');

      if (pos != STR_NPOS)
	 path.erase(pos);
      else path = "";


   }

   if (path != "")
      return ( MAKEDIR(
		     path.c_str(),
		     S_IRUSR | S_IWUSR | S_IXUSR |
		     S_IRGRP | S_IWGRP | S_IXGRP |
		     S_IROTH | S_IXOTH)
	       );
   else
      return 0;

}

/******************************************************************************/
/*                              g e t C k s u m                               */
/******************************************************************************/
  
int getCksum(XrdCksData &cksData, const char *Path)
{
   const char *Lfn;
   char *csResp, *tP;

// Point to absolute path
//                    0123456
   if (!strcmp(Path, "root://")) Lfn = Path + 7;
      else Lfn = Path + 8;
   if ((Lfn = index(Lfn, '/'))) Lfn++;
      else Lfn = Path;

// Get the checksum from the server
//
   XrdClientAdmin Adm(Path);
   if (!(Adm.Connect()) || !(Adm.GetChecksum((kXR_char *)Lfn,(kXR_char **)&csResp)))
      {EMSG("Unable to obtain checksum for '"<< getFName(Path) <<"'.");
       EMSG(Adm.LastServerError()->errmsg);
       return 0;
      }

// Get checksum name and make sure it matches
//
   XrdOucTokenizer csData(csResp);
   csData.GetLine();
   if ((tP = csData.GetToken()) && strcmp(tP, Config.CksData.Name))
      {EMSG("Only " <<tP <<" checksums supported by "
            <<Adm.GetCurrentUrl().Host.c_str());
       free(csResp);
       return 0;
      }

// Get the token value
//
   if (tP && (tP = csData.GetToken()) && !cksData.Set(tP, strlen(tP))) tP = 0;

// Check that all went well
//
   if (!tP) EMSG("Invalid checksum returned for '" <<getFName(Path) <<"'.");

// Return result
//
   free(csResp);
   return (tP ? 1 : 0);
}

/******************************************************************************/
/*                                v a l T P C                                 */
/******************************************************************************/
  
int valTPC(XrdClient *cObj, int isDest)
{
   kXR_char qArg[4];
   kXR_char respBuff[128];

// Ask the sever if it supports tpc
//
   strcpy((char *)qArg, "tpc");
   if (cObj->Query(kXR_Qconfig, qArg, respBuff, sizeof(respBuff))
   &&  isdigit(*respBuff) && atoi((const char *)respBuff) > 0) return 1;

// Nope, we don't support this
//
   EMSG("Host " <<cObj->GetCurrentUrl().Host.c_str()
        <<" does not support third party copies.");

// If we are the destination, unlink any partially created file
//
   if (isDest)
      {XrdClientAdmin Adm(cObj->GetCurrentUrl().GetUrl().c_str());
       if (Adm.Connect()) Adm.Rm(cObj->GetCurrentUrl().File.c_str());
       // cerr <<cObj->GetCurrentUrl().GetUrl().c_str() <<endl;
       // cerr <<cObj->GetCurrentUrl().File.c_str() <<endl;
      }

   return 0;
}

/******************************************************************************/
/*                            g e n D e s t C g i                             */
/******************************************************************************/
  
char *genDestCgi(XrdClient *xrdsrc, const char *src)
{
   union {int *intP;
          int  intV[2];
         } iKey;
   XrdClientStatInfo stat;
   XrdOucString dCGI;
   int myKey[3];
   const char *Path, *cksVal, *cgiP;
   char *qP, aszBuff[128], lfnBuff[1032], cgiBuff[2048];

// Make sure that the source supports 3rd party copy
//
   if (!valTPC(xrdsrc, 0)) return 0;

// Extract out the source lfn
//
   Path = src + (*src == 'x' ? 8 : 7);
   if (!(Path = index(Path, '/')))
      {EMSG("Unable to extract lfn from '" <<getFName(src) <<"'."); return 0;}
   strncpy(lfnBuff, Path+1, sizeof(lfnBuff));
   lfnBuff[sizeof(lfnBuff)-1] = 0;
   if ((qP = index(lfnBuff, '?'))) *qP = 0;

// Generate a key
//
   gettimeofday(&abs_start_time,&tz);
   myKey[0] = abs_start_time.tv_usec;
   myKey[1] = getpid() | (getppid() << 16);
   iKey.intP = &myKey[0];
   myKey[2] = iKey.intV[0] ^ iKey.intV[1];
   sprintf(tpcKey, "%08x%08x%08x", myKey[0], myKey[1], myKey[2]);

// Check if we should add checksum information
//
   cksVal = (verCks ? Config.CksVal : 0);

// Generate the cgi for the destination
//
   std::ostringstream o; o << xrdsrc->GetCurrentUrl().Host.c_str() << ":";
   o << xrdsrc->GetCurrentUrl().Port;
   cgiP = XrdOucTPC::cgiC2Dst(tpcKey, o.str().c_str(),
                              lfnBuff, cksVal, cgiBuff, sizeof(cgiBuff));
   if (*cgiP == '!')
      {EMSG("Unable to setup destination url. " <<cgiP+1); return 0;}

// Start the url with the size hint
//
   xrdsrc->Stat(&stat);
   tpcFileSize = static_cast<long long>(stat.size);
   sprintf(aszBuff, "?oss.asize=%lld&", tpcFileSize);
   dCGI = aszBuff;

// Add all other information
//
   if (Config.dstOpq) {dCGI += Config.dstOpq; dCGI += '&';}
   dCGI += cgiBuff;
// cerr <<"Dest url: " <<dCGI.c_str() <<endl;

// All done
//
   return strdup(dCGI.c_str());
}

/******************************************************************************/
/*                          d o C p _ x r d 3 x r d                           */
/******************************************************************************/
  
int doCp_xrd3xrd(XrdClient *xrddest, const char *src, const char *dst)
{
   struct sdHelper
         {XrdClient *Src;
                     sdHelper() : Src(0) {}
                    ~sdHelper() {if (Src) delete Src;}
         } Client;
   XrdClientUrlInfo dUrl;
   XrdOucString sUrl(tpcSrc->GetCurrentUrl().GetUrl().c_str());
   XrdOucString *rCGI, dstUrl;
   int xTTL = -1;
   const char *cgiP;
   char cgiBuff[1024];

// Append any redirection cgi information to our source spec
//
   rCGI = &(tpcSrc->GetClientConn()->fRedirCGI);
   if (rCGI->length() > 0)
      {if (sUrl.find("?") == STR_NPOS) sUrl += '?';
          else sUrl += '&';
       sUrl += *rCGI;
      }

//cerr <<"tpc: bfr src=" <<src <<endl;
//cerr <<"tpc: bfr dst=" <<dst <<endl;
//cerr <<"tpc: bfr scl=" <<tpcSrc->GetCurrentUrl().GetUrl().c_str() <<endl;
//cerr <<"tpc: bfr dcl=" <<xrddest->GetCurrentUrl().GetUrl().c_str() <<endl;

// Verify that the destination supports 3rd party stuff
//
   if (!valTPC(xrddest, 1)) return 8;

// Generate source cgi string
//
   cgiP = XrdOucTPC::cgiC2Src(tpcKey, xrddest->GetCurrentUrl().Host.c_str(),
                              xTTL, cgiBuff, sizeof(cgiBuff));
   if (*cgiP == '!')
      {EMSG("Unable to setup source url. " <<cgiP+1); return 8;}

// Add the cgi string to the source
//
   if (sUrl.find("?") == STR_NPOS) sUrl += '?';
      else sUrl += '&';
   sUrl += cgiBuff;
//cerr <<"tpc: aft scl=" <<sUrl.c_str()<<endl;
//cerr <<"tpc: aft dcl=" <<xrddest->GetCurrentUrl().GetUrl().c_str() <<endl;

// Open the source
//
   Client.Src = new XrdClient(sUrl.c_str());
   const char *hName = Client.Src->GetCurrentUrl().Host.c_str();
   if ((!Client.Src->Open(0, kXR_async) ||
      (Client.Src->LastServerResp()->status != kXR_ok)))
      return cpFatal("open", Client.Src, 0, hName);
   
// Start the progress bar if so wanted
//
   tpcPB = !Config.Want(XrdCpConfig::DoNoPbar);
   if (tpcPB)
//    {dUrl = xrddest->GetCurrentUrl();
//cerr <<"tpc: pbr dcl=" <<dst<<endl;
      {dstUrl = dst; dUrl = dstUrl;
       tpcPB = !XrdSysThread::Run(&tpcTID, doProgBar, (void *)&dUrl,
                                  XRDSYSTHREAD_HOLD);
      }

// Now do a sync operation on the destination
//
   if (!xrddest->Sync()) return cpFatal("rendezvous", 0, xrddest);

// One more sync will start the copy
//
   gettimeofday(&abs_start_time,&tz);
   if (!xrddest->Sync()) return cpFatal("sync", 0, xrddest);

// Stop the progress bar
//
   if (tpcPB) undoProgBar(1);

// Close the file
//
   if(!xrddest->Close()) return cpFatal("close", 0, xrddest);

// Do checksum processing
//
   if (xeqCks && prtCks)
      {if (!getCksum(dstCksum, dst)) {EMSG("Unable to print checksum!")}
          else print_chksum(src, tpcFileSize);
       if (summary) print_summary(src, dst, tpcFileSize);
      }

// All done
//
   return 0;
}

/******************************************************************************/
/*                          d o C p _ x r d 2 x r d                           */
/******************************************************************************/
  
int doCp_xrd2xrd(XrdClient **xrddest, const char *src, const char *dst) {
   // ----------- xrd to xrd affair
   pthread_t myTID;
   XrdClientVector<pthread_t> myTIDVec;

   void *thret;
   XrdClientStatInfo stat;
   int retvalue = 0;

// If we need to verify checksums then we will need to get the checksum
// from the source unless a specific checksum has been specified.
//
   if (lclCks) csObj = Config.CksObj;
      else if (verCks && getCks && !getCksum(srcCksum, src)) return -ENOTSUP;

   gettimeofday(&abs_start_time,&tz);

   // Open the input file (xrdc)
   // If Xrdcli is non-null, the correct src file has already been opened
   if (!cpnfo.XrdCli)
      {cpnfo.XrdCli = new XrdClient(src);
       const char *hName = cpnfo.XrdCli->GetCurrentUrl().Host.c_str();
       if ( ( !cpnfo.XrdCli->Open(0, kXR_async) ||
          (cpnfo.XrdCli->LastServerResp()->status != kXR_ok) ) )
          {cpFatal("open", cpnfo.XrdCli, 0, hName);
           delete cpnfo.XrdCli;
           cpnfo.XrdCli = 0;
           return 1;
          }
      }
   
   cpnfo.XrdCli->Stat(&stat);
   cpnfo.len = stat.size;
   
   XrdOucString dest = AddSizeHint( dst, stat.size );

   // if xrddest if nonzero, then the file is already opened for writing
   if (!*xrddest) {
      *xrddest = new XrdClient(dest.c_str());
       const char *hName = (*xrddest)->GetCurrentUrl().Host.c_str();
      
      if (!PedanticOpen4Write(*xrddest, rwMode, xrd_wr_flags))
         {cpFatal("open", 0, *xrddest, hName);
          delete cpnfo.XrdCli;
          delete *xrddest;
          *xrddest = 0;
          cpnfo.XrdCli = 0;
          return -1;
         }
      
   }
   
   // If the Extreme Copy flag is set, we try to find more sources for this file
   // Each source gets assigned to a different reader thread
   XrdClientVector<XrdClient *> xtremeclients;
   XrdXtRdFile *xrdxtrdfile = 0;
   
   if (doXtremeCp) 
      XrdXtRdFile::GetListOfSources(cpnfo.XrdCli, XtremeCpRdr,
                                    xtremeclients, Config.nSrcs);
   
   // Start reader on xrdc
   if (doXtremeCp && (xtremeclients.GetSize() > 1)) {
      
      // Beware... with the extreme copy the normal read ahead mechanism
      // makes no sense at all.
      //EnvPutInt(NAME_REMUSEDCACHEBLKS, 1);
      xrdxtrdfile = new XrdXtRdFile(XRDCP_BLOCKSIZE*4, cpnfo.len);
      
      for (int iii = 0; iii < xtremeclients.GetSize(); iii++) {
         xtreme_threadnfo *nfo = new(xtreme_threadnfo);
         nfo->xtrdhandler = xrdxtrdfile;
         nfo->cli = xtremeclients[iii];
         nfo->clientidx = xrdxtrdfile->GimmeANewClientIdx();
         nfo->startfromblk = iii*xrdxtrdfile->GetNBlks() / xtremeclients.GetSize();
         nfo->maxoutstanding = xrdmin( 5, xrdxtrdfile->GetNBlks() / xtremeclients.GetSize() );
         if (nfo->maxoutstanding < 1) nfo->maxoutstanding = 1;

         XrdSysThread::Run(&myTID, ReaderThread_xrd_xtreme, 
                           (void *)nfo, XRDSYSTHREAD_HOLD);
         myTIDVec.Push_back(myTID);
      }
      
   }
   else {
      XrdSysThread::Run(&myTID,ReaderThread_xrd,(void *)&cpnfo,XRDSYSTHREAD_HOLD);
      myTIDVec.Push_back(myTID);
   }
   
   int len = 1;
   void *buf;
   long long offs = 0;
   long long bytesread=0;
   long long size = cpnfo.len;
   bool draining = false;
   
   // Loop to write until ended or timeout err
   while (1) {
      
      if (xrdxtrdfile && xrdxtrdfile->AllDone()) draining = true;
      if (draining && !cpnfo.queue.GetLength()) break;

      if ( cpnfo.queue.GetBuffer(&buf, offs, len) ) {

         if (len && buf) {

            bytesread+=len;
            if (progbar) {
               gettimeofday(&abs_stop_time,&tz);
               print_progbar(bytesread,size);
            }

            if (csObj) csObj->Update((const char *)buf,len);

            if (!(*xrddest)->Write(buf, offs, len)) {
               cpFatal("write", 0, *xrddest);
               retvalue = 11;
               break;
            }

            if (cpnfo.mon)
               cpnfo.mon->PutProgressInfo(bytesread, cpnfo.len, (float)bytesread / cpnfo.len * 100.0);

            free(buf);

         }
         else
            if (!xrdxtrdfile && ( ((buf == 0) && (len == 0)) || (bytesread >= size))) {
               if (buf) free(buf);
               break;
            }

      }
      else {
         EMSG("Critical read timeout. Unable to read data from the source.");
         retvalue = 17;
         break;
      }

      buf = 0;
   }

   if (cpnfo.mon)
      cpnfo.mon->PutProgressInfo(bytesread, cpnfo.len, (float)bytesread / cpnfo.len * 100.0, 1);

   if(progbar) {
      cout << endl;
   }

   if (cpnfo.len != bytesread) {
      EMSG("File length mismatch. Read:" << bytesread << " Length:" << cpnfo.len);
      retvalue = 13;
   }
      
      for (int i = 0; i < myTIDVec.GetSize(); i++) {
         pthread_cancel(myTIDVec[i]);
         pthread_join(myTIDVec[i], &thret);	 
      }

      delete cpnfo.XrdCli;
      cpnfo.XrdCli = 0;

   if(!(*xrddest)->Close()) return cpFatal("close", 0, *xrddest);

   delete *xrddest;
   *xrddest = 0;

   if (!retvalue && xeqCks)
      {if (!getCksum(dstCksum, dst)) retvalue = -ENOTSUP;
          else if (verCks && srcCksum != dstCksum)
                  {EMSG(getFName(dst) <<' ' <<srcCksum.Name <<" is incorrect!");
                   retvalue = -1;
                  }
      }
   if (!retvalue)
      {if (prtCks) print_chksum(src, bytesread);
       if (summary) print_summary(src, dst, bytesread);
      }
   return retvalue;
}

/******************************************************************************/
/*                          d o C p _ x r d 2 l o c                           */
/******************************************************************************/

int doCp_xrd2loc(const char *src, const char *dst) {
   // ----------- xrd to loc affair
   pthread_t myTID;
   XrdClientVector<pthread_t> myTIDVec;

   void *thret;
   XrdClientStatInfo stat;
   int f;
   int retvalue = 0;

// If we need to verify checksums then we will need to get the checksum
// from the source unless a specific checksum has been specified.
//
   if (xeqCks && getCks && !getCksum(srcCksum, src)) return -ENOTSUP;

   gettimeofday(&abs_start_time,&tz);

   // Open the input file (xrdc)
   // If Xrdcli is non-null, the correct src file has already been opened
   if (!cpnfo.XrdCli)
      {cpnfo.XrdCli = new XrdClient(src);
       const char *hName = cpnfo.XrdCli->GetCurrentUrl().Host.c_str();
       if ( ( !cpnfo.XrdCli->Open(0, kXR_async) ||
          (cpnfo.XrdCli->LastServerResp()->status != kXR_ok) ) )
          {cpFatal("open", cpnfo.XrdCli, 0, hName);
           delete cpnfo.XrdCli;
           cpnfo.XrdCli = 0;
           return 1;
          }
      }

   // Open the output file (loc)
   cpnfo.XrdCli->Stat(&stat);
   cpnfo.len = stat.size;

   if (strcmp(dst, "-"))
      // Copy to local fs
      //unlink(dst);
     {f = open(getFName(dst), loc_wr_flags,
          S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
      if (f < 0)
         {EMSG(strerror(errno) <<" creating '" <<getFName(dst) <<"'.");
          cpnfo.XrdCli->Close();
          delete cpnfo.XrdCli;
          cpnfo.XrdCli = 0;
          return -1;
         }
      if (verCks || lclCks) csObj = Config.CksObj;
     } else {
      f = STDOUT_FILENO;  // Copy to stdout
     }

   // If the Extreme Copy flag is set, we try to find more sources for this file
   // Each source gets assigned to a different reader thread
   XrdClientVector<XrdClient *> xtremeclients;
   XrdXtRdFile *xrdxtrdfile = 0;

   if (doXtremeCp) 
      XrdXtRdFile::GetListOfSources(cpnfo.XrdCli, XtremeCpRdr,
                                    xtremeclients, Config.nSrcs);

   // Start reader on xrdc
   if (doXtremeCp && (xtremeclients.GetSize() > 1)) {

      // Beware... with the extreme copy the normal read ahead mechanism
      // makes no sense at all.

      xrdxtrdfile = new XrdXtRdFile(XRDCP_BLOCKSIZE*4, cpnfo.len);

      for (int iii = 0; iii < xtremeclients.GetSize(); iii++) {
         xtreme_threadnfo *nfo = new(xtreme_threadnfo);
         nfo->xtrdhandler = xrdxtrdfile;
         nfo->cli = xtremeclients[iii];
         nfo->clientidx = xrdxtrdfile->GimmeANewClientIdx();
         nfo->startfromblk = iii*xrdxtrdfile->GetNBlks() / xtremeclients.GetSize();
         nfo->maxoutstanding = xrdmax(xrdmin( 3, xrdxtrdfile->GetNBlks() / xtremeclients.GetSize() ), 1);

         XrdSysThread::Run(&myTID, ReaderThread_xrd_xtreme, 
                           (void *)nfo, XRDSYSTHREAD_HOLD);
         myTIDVec.Push_back(myTID);
      }

   }
   else {
      doXtremeCp = false;
      XrdSysThread::Run(&myTID,ReaderThread_xrd,(void *)&cpnfo,XRDSYSTHREAD_HOLD);
      myTIDVec.Push_back(myTID);
   }

   int len = 1;
   void *buf;
   long long bytesread=0, offs = 0;
   long long size = cpnfo.len;
   bool draining = false;

   // Loop to write until ended or timeout err
   while (1) {

      if (xrdxtrdfile && xrdxtrdfile->AllDone()) draining = true;
      if (draining && !cpnfo.queue.GetLength()) break;

      if ( cpnfo.queue.GetBuffer(&buf, offs, len) ) {

	 if (len && buf) {

	    bytesread+=len;
	    if (progbar) {
	       gettimeofday(&abs_stop_time,&tz);
	       print_progbar(bytesread,size);
	    }

     if (csObj) csObj->Update((const char *)buf,len);

	    if (doXtremeCp && (f != STDOUT_FILENO) && lseek(f, offs, SEEK_SET) < 0) {
	       EMSG(strerror(errno) <<" while seeking in '" << getFName(dst) <<"'.");
	       retvalue = 10;
	       break;
	    }
	    if (write(f, buf, len) <= 0) {
	       EMSG(strerror(errno) <<" writing to '" << getFName(dst) <<"'.");
	       retvalue = 10;
	       break;
	    }

	    if (cpnfo.mon)
	      cpnfo.mon->PutProgressInfo(bytesread, cpnfo.len, (float)bytesread / cpnfo.len * 100.0);

	    free(buf);

	 }
         else
            if (!xrdxtrdfile && ( ((buf == 0) && (len == 0)) || (bytesread >= size)) ) {
               if (buf) free(buf);
               break;
            }


      }
      else {
	 EMSG("Critical read timeout. Unable to read data from the source.");
	 retvalue = 17;
	 break;
      }
	 
      buf = 0;

   }

   if (cpnfo.mon)
     cpnfo.mon->PutProgressInfo(bytesread, cpnfo.len, (float)bytesread / cpnfo.len * 100.0, 1);

   if(progbar) {
      cout << endl;
   }

   if (cpnfo.len != bytesread)
      {cpFatal("read", cpnfo.XrdCli, 0);
       retvalue = 13;
      }

   if (close(f))
      {EMSG(strerror(errno) <<" closing '" <<getFName(dst) <<"'.");
       retvalue = -1;
      }

      for (int i = 0; i < myTIDVec.GetSize(); i++) {
         pthread_cancel(myTIDVec[i]);
         pthread_join(myTIDVec[i], &thret);	 
      }
      delete cpnfo.XrdCli;
      cpnfo.XrdCli = 0;

   if (!retvalue && xeqCks)
      {if (!csObj) retvalue = Config.CksMan->Calc(dst, dstCksum, setCks);
          else {char *csVal = csObj->Final();
                if (!dstCksum.Set((const void *)csVal, Config.CksLen))
                   retvalue = -EINVAL;
               }
       if (retvalue)
          {retvalue = (retvalue < 0 ? -retvalue : retvalue);
           EMSG(strerror(retvalue) <<" calculating "
                <<dstCksum.Name << " checksum for " << getFName(dst));
          } else if (verCks && srcCksum != dstCksum)
                    {EMSG(getFName(dst) <<' ' <<srcCksum.Name
                          <<" checksum is incorrect!");
                     retvalue = -1;
                    }
      }

   if (!retvalue)
      {if (prtCks)  print_chksum(src, bytesread);
       if (summary) print_summary(src, dst, bytesread);
      }

   return retvalue;
}

/******************************************************************************/
/*                          d o C p _ l o c 2 x r d                           */
/******************************************************************************/

int doCp_loc2xrd(XrdClient **xrddest, const char *src, const char * dst) {
// ----------- loc to xrd affair
   pthread_t myTID;
   void * thret;
   int retvalue = 0;
   struct stat stat;

   gettimeofday(&abs_start_time,&tz);

   // Open the input file (loc)
   cpnfo.localfile = open(src, O_RDONLY | O_BINARY);   
   if (cpnfo.localfile < 0)
      {EMSG(strerror(errno) << " opening '" << getFName(src) <<"'.");
       cpnfo.localfile = 0;
       return -1;
      }

   if (fstat(cpnfo.localfile, &stat))
      {EMSG(strerror(errno) << " stating '" << getFName(src) <<"'.");
       cpnfo.localfile = 0;
       return -1;
      }

   XrdOucString dest = AddSizeHint( dst, stat.st_size );

   // if xrddest if nonzero, then the file is already opened for writing
   if (!*xrddest)
      {*xrddest = new XrdClient(dest.c_str());
       const char *hName = (*xrddest)->GetCurrentUrl().Host.c_str();
       if (!PedanticOpen4Write(*xrddest, rwMode, xrd_wr_flags) )
          {cpFatal("open", 0 , *xrddest, hName);
           close(cpnfo.localfile);
           delete *xrddest;
           *xrddest = 0;
           cpnfo.localfile = 0;
           return -1;
          }
      }
      
   // Start reader on loc
   XrdSysThread::Run(&myTID,ReaderThread_loc,(void *)&cpnfo,XRDSYSTHREAD_HOLD);

   int len = 1;
   void *buf;
   long long offs = 0;
   unsigned long long bytesread=0;
   unsigned long long size = stat.st_size;
   int blkcnt = 0;

// If we need to verify checksums then we will need to get the checksum
// from the source unless a specific checksum has been specified.
//
   if ((xeqCks && verCks && getCks) || lclCks) csObj = Config.CksObj;

   // Loop to write until ended or timeout err
   while(len > 0)
        {if ( cpnfo.queue.GetBuffer(&buf, offs, len) )
            {if (len && buf)
                {bytesread+=len;
                 if (progbar)
                    {gettimeofday(&abs_stop_time,&tz);
                     print_progbar(bytesread,size);
                    }
                 if (csObj) csObj->Update((const char *)buf,len);
                 if ( !(*xrddest)->Write(buf, offs, len) )
                    {cpFatal("write", 0 , *xrddest);
                     retvalue = 12;
                     break;
                    }
                 if (cpnfo.mon)
                    cpnfo.mon->PutProgressInfo(bytesread, cpnfo.len, (float)bytesread / cpnfo.len * 100.0);
                 free(buf);
                } else {
                 // If we get len == 0 then we have to stop
                 if (buf) free(buf);
                 break;
                }
            } else {
             EMSG("Critical read timeout. Unable to read data from the source.");
             retvalue = 17;
             break;
            }
         buf = 0; blkcnt++;
        }

   if (cpnfo.mon)
     cpnfo.mon->PutProgressInfo(bytesread, cpnfo.len, (float)bytesread / cpnfo.len * 100.0, 1);

   if(progbar) cout << endl;

   if (size != bytesread) retvalue = 13;

   pthread_cancel(myTID);
   pthread_join(myTID, &thret);

   if(!(*xrddest)->Close()) return cpFatal("close", 0, *xrddest);

   delete *xrddest;
   *xrddest = 0;

   close(cpnfo.localfile);
   cpnfo.localfile = 0;

   if (!retvalue && xeqCks)
      {if (!getCksum(dstCksum, dst)) retvalue = -ENOTSUP;
       if (csObj)
          {char *csVal = csObj->Final();
           if (!srcCksum.Set((const void *)csVal, Config.CksLen))
              retvalue = -EINVAL;
          }
       if (retvalue)
          {retvalue = (retvalue < 0 ? -retvalue : retvalue);
           EMSG(strerror(retvalue) <<" calculating "
                <<dstCksum.Name << " checksum for " <<getFName(dst));
          } else if (verCks && srcCksum != dstCksum)
                    {EMSG(getFName(dst) <<' ' <<srcCksum.Name
                          <<" checksum is incorrect!");
                     retvalue = -1;
                    }
      }

   if (!retvalue)
      {if (prtCks)  print_chksum(src, bytesread);
       if (summary) print_summary(src, dst, bytesread);
      }

   return retvalue;
}

/******************************************************************************/
/*                                  d o C p                                   */
/******************************************************************************/
  
int doCp(XrdOucString &src, XrdOucString &dest, XrdClient *xrddest)
{
   int rmtSrc = (src.beginswith("root://"))  || (src.beginswith("xroot://"));
   int rmtDst = (dest.beginswith("root://")) || (dest.beginswith("xroot://"));

// Provide some debugging
//
   Info(XrdClientDebug::kUSERDEBUG, "main", src << " --> " << dest);
      
// Preprocess cksum calculation desires
//
   if (xeqCks)
      {srcCksum = Config.CksData;
       dstCksum = Config.CksData;
       if (Config.CksObj) Config.CksObj->Init();
      }
   csObj = 0;

// Handle when source is xrootd
//
   if (rmtSrc)
      {if (Config.srcOpq) {src += "?"; src += Config.srcOpq;}
       if (rmtDst)
          {XrdOucString d = dest;
           if (Config.dstOpq) {d += "?"; d += Config.dstOpq;}
           if (isTPC) return doCp_xrd3xrd( xrddest, src.c_str(), d.c_str());
              else    return doCp_xrd2xrd(&xrddest, src.c_str(), d.c_str());
          }
       return doCp_xrd2loc(src.c_str(), dest.c_str());
      }

// Handle when source is the local filesystem
//
   if (rmtDst)
      {XrdOucString d = dest;
       if (Config.dstOpq) {d += "?"; d += Config.dstOpq;}
       return doCp_loc2xrd(&xrddest, src.c_str(), d.c_str());
      }

// We should never get here
//
   EMSG("Better to use cp for this copy.");
   return 2;
}

/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char**argv)
{
   const char *Opaque;
   char *hName, *srcpath = 0, *destpath = 0;

// Preset globals
//
   tpcPB = 0;

#ifdef WIN32
   WORD wVersionRequested;
   WSADATA wsaData;
   int err;
   wVersionRequested = MAKEWORD( 2, 2 );
   err = WSAStartup( wVersionRequested, &wsaData );
#endif

// Invoke config; it it returns then all went well.
//
   Config.Config(argc, argv, XrdCpConfig::opt1Src|XrdCpConfig::optNoStdIn
                            |XrdCpConfig::optNoXtnd);

// Turn off any blab from the client
//
   DebugSetLevel(-1);

// We want this tool to be able to copy from/to everywhere
// Note that the side effect of these calls here is to initialize the
// XrdClient environment.
// This is crucial if we want to later override its default values
//
   EnvPutString( NAME_REDIRDOMAINALLOW_RE, "*" );
   EnvPutString( NAME_CONNECTDOMAINALLOW_RE, "*" );
   EnvPutString( NAME_REDIRDOMAINDENY_RE, "" );
   EnvPutString( NAME_CONNECTDOMAINDENY_RE, "" );

   EnvPutInt( NAME_READAHEADSIZE, XRDCP_XRDRASIZE);
   EnvPutInt( NAME_READCACHESIZE, 2*XRDCP_XRDRASIZE );
   EnvPutInt( NAME_READCACHEBLKREMPOLICY, XrdClientReadCache::kRmBlk_LeastOffs );
   EnvPutInt( NAME_PURGEWRITTENBLOCKS, 1 );

   EnvPutInt( NAME_DEBUG, -1);

// Extract out config information and set global vars (that's how it was done)
//
   if ((Verbose = Config.Verbose))          summary = true;
   if (Config.Want(XrdCpConfig::DoSilent )) summary = progbar = false;
   if (Config.Want(XrdCpConfig::DoNoPbar )) progbar = false;
   if (Config.Want(XrdCpConfig::DoRecurse)) recurse = true;
   if (Config.Want(XrdCpConfig::DoCoerce )) xrd_wr_flags |=  kXR_force;
   if (Config.Want(XrdCpConfig::DoPosc   )) xrd_wr_flags |=  kXR_posc;
   if (Config.Want(XrdCpConfig::DoForce  )
   ||  Config.Want(XrdCpConfig::DoServer ))
      {xrd_wr_flags &= ~kXR_new;
       xrd_wr_flags |=  kXR_delete;
       loc_wr_flags = LOC_WR_FLAGS_FORCE; // Flags for the local fs
      }
   if (Config.Want(XrdCpConfig::DoRetry  ) && Config.Retry >= 0)
      {EnvPutInt(NAME_CONNECTTIMEOUT , 60);
       EnvPutInt(NAME_FIRSTCONNECTMAXCNT, Config.Retry);
      }

   if (Config.strDefs)
      {XrdCpConfig::defVar *dvP = Config.strDefs;
       do {EnvPutString(dvP->vName, dvP->strVal);}
          while((dvP = dvP->Next));
      }

   if (Config.intDefs)
      {XrdCpConfig::defVar *dvP = Config.intDefs;
       do {EnvPutInt(dvP->vName, dvP->intVal);}
          while((dvP = dvP->Next));
      }

   if (Config.Want(XrdCpConfig::DoProxy  ))
      {EnvPutString(NAME_SOCKS4HOST, Config.pHost);
       EnvPutInt(NAME_SOCKS4PORT, Config.pPort);
      }

   if (Config.Dlvl > 0) EnvPutInt( NAME_DEBUG, Config.Dlvl);

   if (Config.Want(XrdCpConfig::DoStreams))
      EnvPutInt(NAME_MULTISTREAMCNT, Config.nStrm);

   isTPC = (Config.Want(XrdCpConfig::DoTpc) ? 1 : 0);

   destpath = Config.dstFile->Path;
   srcpath  = Config.srcFile->Path;

// Do some debugging
//
   DebugSetLevel(EnvGetLong(NAME_DEBUG));
   Info(XrdClientDebug::kUSERDEBUG, "main", XRDCP_VERSION);

// Prehandle extreme copy
//
   if (Config.Want(XrdCpConfig::DoSources) && Config.nSrcs > 1)
      {doXtremeCp = true;
       XtremeCpRdr = srcpath;
       if (Verbose) EMSG("Extreme Copy enabled.");
      }

// Establish checksum processing
//
   setCks = 0;
   lclCks = Config.Want(XrdCpConfig::DoCksrc);
   xeqCks = Config.Want(XrdCpConfig::DoCksum);
   prtCks =(xeqCks || lclCks) &&  Config.Want(XrdCpConfig::DoCkprt);
   getCks = xeqCks && (Config.CksData.Length == 0);
   verCks = xeqCks && !prtCks;

// Force certain defaults when in server mode
//
   if (Config.Want(XrdCpConfig::DoServer))
      {summary = progbar = false;
       setCks = true;
       isSrv  = true;
      } else {
       isSrv = false;
 //    if (dup2(STDOUT_FILENO, STDERR_FILENO)) cerr <<"??? " <<errno <<endl;
      }

// Extract source host if present
//
   if (strncmp(srcpath, "root://", 7) || strncmp(srcpath, "xroot://", 8) )
      {XrdClientUrlInfo sUrl(srcpath);
       hName = (sUrl.IsValid() ? strdup(sUrl.Host.c_str()) : 0);
      } else hName = 0;

// Prepare to generate a copy list
//
   XrdCpWorkLst *wklst = new XrdCpWorkLst();
   XrdOucString src, dest;
   XrdClient *xrddest = 0;
   cpnfo.XrdCli = 0;
  
// Generate the sources
//
   if (wklst->SetSrc(&cpnfo.XrdCli, srcpath, Config.srcOpq, recurse, 1))
      {cpFatal("open", cpnfo.XrdCli, 0, hName);
       exit(1);
      }

// Generate destination opaque data now
//
   if (!isTPC) Opaque = Config.dstOpq;
      else {if (!(Opaque = genDestCgi(cpnfo.XrdCli, srcpath))) exit(4);
            tpcSrc = cpnfo.XrdCli;
           }

// Extract source host if present
//
   if (hName) free(hName);
   if (strncmp(destpath, "root://", 7) || strncmp(destpath, "xroot://", 8) )
      {XrdClientUrlInfo dUrl(destpath);
       hName = (dUrl.IsValid() ? strdup(dUrl.Host.c_str()) : 0);
      } else hName = 0;

// Verify the correctness of the destination
//
   if (wklst->SetDest(&xrddest, destpath, Opaque, xrd_wr_flags, 1))
      {cpFatal("open", 0, xrddest, hName);
       exit(1);
      }
   if (hName) {free(hName); hName = 0;}

      // Initialize monitoring client, if a plugin is present
      cpnfo.mon = 0;
#ifndef WIN32
      void *monhandle = dlopen (monlibname.c_str(), RTLD_LAZY);

      if (monhandle) {
	XrdClientMonIntfHook monlibhook = (XrdClientMonIntfHook)dlsym(monhandle, "XrdClientgetMonIntf");

	const char *err = 0;
	if ((err = dlerror())) {
   EMSG(err <<" loading library " << monhandle);
	  dlclose(monhandle);
	  monhandle = 0;
	}
	else	
	  cpnfo.mon = (XrdClientAbsMonIntf *)monlibhook(src.c_str(), dest.c_str());
      }
#endif
      
      if (cpnfo.mon) {

	char *name=0, *ver=0, *rem=0;
	if (!cpnfo.mon->GetMonLibInfo(&name, &ver, &rem)) {
	  Info(XrdClientDebug::kUSERDEBUG,
	       "main", "Monitoring client plugin found. Name:'" << name <<
	       "' Ver:'" << ver << "' Remarks:'" << rem << "'");
	}
	else {
	  delete cpnfo.mon;
	  cpnfo.mon = 0;
	}

      }

#ifndef WIN32
      if (!cpnfo.mon && monhandle) {
	dlclose(monhandle);
	monhandle = 0;
      }
#endif

// From here, we will have:
// the knowledge if the dest is a dir name or file name
// an open instance of xrdclient if it's a file
//
   int retval = 0;
   while(!retval && wklst->GetCpJob(src, dest))
        {if (cpnfo.mon)
            {cpnfo.mon->Init(src.c_str(), dest.c_str(), (DebugLevel() > 0) );
             cpnfo.mon->PutProgressInfo(0, cpnfo.len, 0, 1);
            }
         retval = doCp(src, dest, xrddest);
         if (cpnfo.mon) cpnfo.mon->DeInit();
        }

// Delete the monitor object
//
   delete cpnfo.mon;
   cpnfo.mon = 0;
#ifndef WIN32
   if (monhandle) dlclose(monhandle);
   monhandle = 0;
#endif

// All done
//
   if (retval < 0) retval = -retval;
   _exit(retval);
   return retval;
}
