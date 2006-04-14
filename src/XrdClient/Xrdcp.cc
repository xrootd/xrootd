//////////////////////////////////////////////////////////////////////////
//                                                                      //
// xrdcp                                                                //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A cp-like command line tool for xrootd environments                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include "XrdClient/XrdClientUrlInfo.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdCpMthrQueue.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdCpWorkLst.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdSys/XrdSysPlatform.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef WIN32
#include <sys/time.h>
#include <unistd.h>
#endif
#include <stdarg.h>
#include <stdio.h>

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
      cerr << cerr_buff;
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
} cpnfo;

#define XRDCP_BLOCKSIZE          (DFLT_READAHEADSIZE)
#define XRDCP_VERSION            "(C) 2004 SLAC INFN xrdcp 0.2 beta"

///////////////////////////////////////////////////////////////////////
// Coming from parameters on the cmd line

bool summary=false;            // print summary
bool progbar=true;             // print progbar
char *srcopaque=0,
   *dstopaque=0;   // opaque info to be added to urls
// Default open flags for opening a file (xrd)
kXR_unt16 xrd_wr_flags=kXR_async | kXR_mkpath | kXR_open_updt | kXR_new;
bool recurse = false;
///////////////////////

// To compute throughput etc
struct timeval abs_start_time;
struct timeval abs_stop_time;
struct timezone tz;


void print_summary(const char* src, const char* dst, unsigned long long bytesread) {
   gettimeofday (&abs_stop_time, &tz);
   float abs_time=((float)((abs_stop_time.tv_sec - abs_start_time.tv_sec) *1000 +
			   (abs_stop_time.tv_usec - abs_start_time.tv_usec) / 1000));


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
      COUT(("[xrdcp] # Eff.Copy. Rate[Mb/s]     : %f\n",bytesread/abs_time/1000.0));
   }
   //  if (strlen(authzfilename)) {
   //    COUT(("[xrootd] # Authz Filename           : %s\n",authzfilename));
   //  }
   COUT(("[xrdcp] #################################################################\n"));
}

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
   CERR(("| %.02f %% [%.01f Mb/s]\r",100.0*bytesread/size,bytesread/abs_time/1000.0));
}




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
	 cerr << "Out of memory." << endl;
	 abort();
      }

      
      blksize = xrdmin(XRDCP_BLOCKSIZE, len-offs);

      if ( (nr = cpnfo.XrdCli->Read(buf, offs, blksize)) ) {
	 bread += nr;
	 offs += nr;
	 cpnfo.queue.PutBuffer(buf, nr);
      }

      pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
      pthread_testcancel();
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0);
   }

   cpnfo.bread = bread;

   // This ends the transmission... bye bye
   cpnfo.queue.PutBuffer(0, 0);

   return 0;
}




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
	 cerr << "Out of memory." << endl;
	 abort();
      }

      if ( (nr = read(cpnfo.localfile, buf, XRDCP_BLOCKSIZE)) ) {
	 bread += nr;
	 offs += nr;
	 cpnfo.queue.PutBuffer(buf, nr);
      }
   }

   cpnfo.bread = bread;

   // This ends the transmission... bye bye
   cpnfo.queue.PutBuffer(0, 0);

   return 0;
}


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
   
void BuildFullDestFilename(XrdOucString &src, XrdOucString &dest, bool destisdir) {
   if (destisdir) {
      // We need the filename from the source
      XrdOucString fn(src);
      fn.erase(fn.find('?'));
      int lsl = fn.rfind('/');
      if (lsl != STR_NPOS)
         fn.erase(0, lsl+1);
      dest += fn;
   }
}

int CreateDestPath_xrd(XrdOucString url, bool isdir) {
   // We need the path name without the file
   bool statok = FALSE, done = FALSE, direxists = TRUE;
   long id, flags, modtime;
   long long size;
   char *path, *slash;

   if (url == "-") return 0;

   //   if (!isdir)
   url.erase(url.rfind('/') + 1);

   XrdClientAdmin *adm = new XrdClientAdmin(url.c_str());
   if (adm->Connect()) {
     XrdClientUrlInfo u(url);

     statok = adm->Stat((char *)u.File.c_str(), id, size, flags, modtime);

     // We might have been redirected to a destination server. Better to remember it and use
     //  only this one as output.
     if (adm->GetCurrentUrl().IsValid()) {
	u.Host = adm->GetCurrentUrl().Host;
	u.Port = adm->GetCurrentUrl().Port;
	url = u.GetUrl();
     }

     path = (char *)u.File.c_str();
     slash = path;

     // FIXME: drop the top level directory as it cannot be stat by the xrootd server
     slash += strspn(slash, "/");
     slash += strcspn(slash, "/");
     
     // If the path already exists, it's good
     done = (statok && (flags & kXR_isDir));

     // The idea of slash pointer is taken from the BSD mkdir implementation
     while (!done) {
       slash += strspn(slash, "/");
       slash += strcspn(slash, "/");
       
       char nextChar = *(slash+1);
       done = (*slash == '\0' || nextChar == '\0');
       *(slash+1) = '\0';

       if (direxists) {
	 statok = adm->Stat(path, id, size, flags, modtime);
	 if (!statok || (!(flags & kXR_xset) && !(flags & kXR_other))) {
	   direxists = FALSE;
	 }
       }
	 
       if (!direxists) {
	 Info(XrdClientDebug::kHIDEBUG,
	      "CreateDestPath__xrd",
	      "Creating directory " << path);
	 
	 adm->Mkdir(path, 7, 5, 5);
	 
       }
       *(slash+1) = nextChar;
     }
   }

   delete adm;
   return 0;
}

int doCp_xrd2xrd(XrdClient **xrddest, const char *src, const char *dst) {
   // ----------- xrd to xrd affair
   pthread_t myTID;
   void *thret;
   XrdClientStatInfo stat;
   int retvalue = 0;

   gettimeofday(&abs_start_time,&tz);

   // Open the input file (xrdc)
   // If Xrdcli is non-null, the correct src file has already been opened
   if (!cpnfo.XrdCli) {
      cpnfo.XrdCli = new XrdClient(src);
      if ( ( !cpnfo.XrdCli->Open(0, kXR_async) ||
	     (cpnfo.XrdCli->LastServerResp()->status != kXR_ok) ) ) {
	 cerr << "Error opening remote source file " << src << endl;

	 delete cpnfo.XrdCli;
	 cpnfo.XrdCli = 0;
	 return 1;
      }
   }


      cpnfo.XrdCli->Stat(&stat);
      cpnfo.len = stat.size;

      // if xrddest if nonzero, then the file is already opened for writing
      if (!*xrddest) {
	 *xrddest = new XrdClient(dst);
	 if (!(*xrddest)->Open(kXR_ur | kXR_uw | kXR_gw | kXR_gr | kXR_or,
			       xrd_wr_flags)) {
	    cerr << "Error opening remote destination file " << dst << endl;
	    
	    delete cpnfo.XrdCli;
	    delete *xrddest;
	    *xrddest = 0;
	    cpnfo.XrdCli = 0;
	    return -1;
	 }
      }

      // Start reader on xrdc
      XrdOucThread::Run(&myTID, ReaderThread_xrd, (void *)&cpnfo);

      int len = 1;
      void *buf;
      long long offs = 0;
      unsigned long long bytesread=0;
      unsigned long long size = cpnfo.len;

      // Loop to write until ended or timeout err
      while (len > 0) {

	 if ( cpnfo.queue.GetBuffer(&buf, len) ) {
	    if (len && buf) {

	       bytesread+=len;
	       if (progbar) {
		 gettimeofday(&abs_stop_time,&tz);
		 print_progbar(bytesread,size);
	       }

	       if (!(*xrddest)->Write(buf, offs, len)) {
		  cerr << "Error writing to output server." << endl;
		  retvalue = 11;
		  break;
	       }

	       offs += len;
	       free(buf);
	    }
	    else {
	       // If we get len == 0 then we have to stop
	       if (buf) free(buf);
	       break;
	    }
	 }
	 else {
	    cerr << endl << endl << "Critical read timeout. Unable to read data from the source." << endl;
            retvalue = -1;
            break;
         }

	 buf = 0;
      }

      if(progbar) {
	cout << endl;
      }
      
      if (summary) {        
	print_summary(src,dst,bytesread);
      }

      if (retvalue >= 0) {
	 pthread_cancel(myTID);
	 pthread_join(myTID, &thret);	 

	 delete cpnfo.XrdCli;
	 cpnfo.XrdCli = 0;
      }

   delete *xrddest;

   return retvalue;
}

int doCp_xrd2loc(const char *src, const char *dst) {
   // ----------- xrd to loc affair
   pthread_t myTID;
   void *thret;
   XrdClientStatInfo stat;
   int f;
   int retvalue = 0;

   gettimeofday(&abs_start_time,&tz);

   // Open the input file (xrdc)
   // If Xrdcli is non-null, the correct src file has already been opened
   if (!cpnfo.XrdCli) {
      cpnfo.XrdCli = new XrdClient(src);
      if ( ( !cpnfo.XrdCli->Open(0, kXR_async) ||
	     (cpnfo.XrdCli->LastServerResp()->status != kXR_ok) ) ) {

	 delete cpnfo.XrdCli;
	 cpnfo.XrdCli = 0;

	 cerr << "Error opening remote source file " << src << endl;
	 return 1;
      }
   }

   // Open the output file (loc)
   cpnfo.XrdCli->Stat(&stat);
   cpnfo.len = stat.size;

   if (strcmp(dst, "-")) {
      // Copy to local fs
      unlink(dst);
      f = open(dst, 
	       O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, 
          S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
      if (f < 0) {
	 cerr << "Error " << strerror(errno) <<
	    " creating " << dst << endl;

	 cpnfo.XrdCli->Close();
	 delete cpnfo.XrdCli;
	 cpnfo.XrdCli = 0;
	 return -1;
      }
      
   }
   else
      // Copy to stdout
      f = STDOUT_FILENO;

   // Start reader on xrdc
   XrdOucThread::Run(&myTID, ReaderThread_xrd, (void *)&cpnfo);

   int len = 1;
   void *buf;
   unsigned long long bytesread=0;
   unsigned long long size = cpnfo.len;

   // Loop to write until ended or timeout err
   while (len > 0) {
	      
      if ( cpnfo.queue.GetBuffer(&buf, len) ) {

	 if (len && buf) {

	    bytesread+=len;
	    if (progbar) {
	       gettimeofday(&abs_stop_time,&tz);
	       print_progbar(bytesread,size);
	    }

	    if (write(f, buf, len) <= 0) {
	       cerr << "Error " << strerror(errno) <<
		  " writing to " << dst << endl;
	       retvalue = 10;
	       break;
	    }

	    free(buf);
	 }
	 else  {
	    // If we get len == 0 then we have to stop
	    if (buf) free(buf);
	    break;
	 }
      }
      else {
	 cerr << endl << endl << "Critical read timeout. Unable to read data from the source." << endl;
	 retvalue = -1;
	 break;
      }
	 
      buf = 0;

   }

   if(progbar) {
      cout << endl;
   }
      
   if (summary) {        
      print_summary(src,dst,bytesread);
   }      

   int closeres = close(f);
   if (!retvalue) retvalue = closeres;

   if (retvalue >= 0) {

      pthread_cancel(myTID);
      pthread_join(myTID, &thret);

      delete cpnfo.XrdCli;
      cpnfo.XrdCli = 0;
   }

   return retvalue;
}



int doCp_loc2xrd(XrdClient **xrddest, const char *src, const char * dst) {
// ----------- loc to xrd affair
   pthread_t myTID;
   void * thret;
   int retvalue = 0;
   struct stat stat;

   gettimeofday(&abs_start_time,&tz);

   // Open the input file (loc)
   cpnfo.localfile = open(src, O_RDONLY);   
   if (cpnfo.localfile < 0) {
      cerr << "Error " << strerror(errno) << " opening " << src << endl;
      cpnfo.localfile = 0;
      return -1;
   }

   if (fstat(cpnfo.localfile, &stat)) {
     cerr << "Error " << strerror(errno) << " stat " << src << endl;
     cpnfo.localfile = 0;
     return -1;
   }

   // if xrddest if nonzero, then the file is already opened for writing
   if (!*xrddest) {

      *xrddest = new XrdClient(dst);
      if (!(*xrddest)->Open(kXR_ur | kXR_uw | kXR_gw | kXR_gr | kXR_or,
			    xrd_wr_flags)) {
	 cerr << "Error opening remote destination file " << dst << endl;
	 close(cpnfo.localfile);
	 delete *xrddest;
	 *xrddest = 0;
	 cpnfo.localfile = 0;
	 return -1;
      }
   }
      
   // Start reader on loc
   XrdOucThread::Run(&myTID, ReaderThread_loc, (void *)&cpnfo);

   int len = 1;
   void *buf;
   long long offs = 0;
   unsigned long long bytesread=0;
   unsigned long long size = stat.st_size;

   // Loop to write until ended or timeout err
   while (len > 0) {

      if ( cpnfo.queue.GetBuffer(&buf, len) ) {
	 if (len && buf) {

 	    bytesread+=len;
	    if (progbar) {
	      gettimeofday(&abs_stop_time,&tz);
	      print_progbar(bytesread,size);
	    }

	    if (!(*xrddest)->Write(buf, offs, len)) {
	       cerr << "Error writing to output server." << endl;
	       retvalue = 12;
	       break;
	    }

	    offs += len;
	    free(buf);
	 }
	 else {
	    // If we get len == 0 then we have to stop
	    if (buf) free(buf);
	    break;
	 }
      }
      else {
	 cerr << endl << endl << "Critical read timeout. Unable to read data from the source." << endl;
	 retvalue = -1;
	 break;
      }

      buf = 0;
   }

   if(progbar) {
     cout << endl;
   }
   
   if (summary) {        
     print_summary(src,dst,bytesread);
   }	 

   pthread_cancel(myTID);
   pthread_join(myTID, &thret);

   delete *xrddest;
   *xrddest = 0;

   close(cpnfo.localfile);
   cpnfo.localfile = 0;

   return retvalue;
}


void PrintUsage() {
   cerr << "usage: xrdcp <source> <dest> "
     "[-d lvl] [-DSparmname stringvalue] ... [-DIparmname intvalue] [-s] [-ns] [-v] [-OS<opaque info>] [-OD<opaque info>] [-force]" << endl;
   cerr << " -d lvl :         debug level: 1 (low), 2 (medium), 3 (high)" << endl;
   cerr << " -s     :         silent mode, no summary output, no progress bar" << endl;
   cerr << " -np    :         no progress bar" << endl;
   cerr << " -v     :         display summary output" << endl <<endl;
   cerr << " -OS    :         adds some opaque information to any SOURCE xrootd url" << endl;
   cerr << " -OD    :         adds some opaque information to any DEST xrootd url" << endl;
   cerr << " -f     :         set the 'force' flag for xrootd dest file opens" << endl;
   cerr << " -force :         set 1-min (re)connect attempts to retry for up to 1 week, to block until xrdcp is executed" << endl << endl;
   cerr << " -R     :         recurse subdirectories" << endl;
   cerr << " where:" << endl;
   cerr << "   parmname     is the name of an internal parameter" << endl;
   cerr << "   stringvalue  is a string to be assigned to an internal parameter" << endl;
   cerr << "   intvalue     is an int to be assigned to an internal parameter" << endl;
}


// Main program
int main(int argc, char**argv) {
   char *srcpath = 0, *destpath = 0;

   if (argc < 3) {
      PrintUsage();
      exit(1);
   }

#ifdef WIN32
   WORD wVersionRequested;
   WSADATA wsaData;
   int err;
   wVersionRequested = MAKEWORD( 2, 2 );
   err = WSAStartup( wVersionRequested, &wsaData );
#endif

   DebugSetLevel(-1);

   // We want this tool to be able to copy from/to everywhere
   // Note that the side effect of these calls here is to initialize the
   // XrdClient environment.
   // This is crucial if we want to later override its default values
   EnvPutString( NAME_REDIRDOMAINALLOW_RE, "*" );
   EnvPutString( NAME_CONNECTDOMAINALLOW_RE, "*" );
   EnvPutString( NAME_REDIRDOMAINDENY_RE, "" );
   EnvPutString( NAME_CONNECTDOMAINDENY_RE, "" );

   EnvPutInt( NAME_DEBUG, -1);

   for (int i=1; i < argc; i++) {

      if ( (strstr(argv[i], "-s") == argv[i])) {
	summary=false;
	progbar=false;
	continue;
      }

      if ( (strstr(argv[i], "-np") == argv[i])) {
	progbar=false;
	continue;
      }

      if ( (strstr(argv[i], "-v") == argv[i])) {
	summary=true;
	continue;
      }

      if ( (strstr(argv[i], "-R") == argv[i])) {
	recurse=true;
	continue;
      }

      if ( (strstr(argv[i], "-OS") == argv[i])) {
	 srcopaque=argv[i]+3;
	 continue;
      }
      
      if ( (strstr(argv[i], "-OD") == argv[i])) {
	 dstopaque=argv[i]+3;
	 continue;
      }

      if ( (strstr(argv[i], "-f") == argv[i])) {
	 xrd_wr_flags |= kXR_force;
	continue;
      }

      if ( (strstr(argv[i], "-force") == argv[i])) {
 	 EnvPutInt( NAME_CONNECTTIMEOUT , 60);
	 EnvPutInt( NAME_FIRSTCONNECTMAXCNT, 7*24*60);
	 continue;
      }
      
      if ( (strstr(argv[i], "-DS") == argv[i]) &&
	   (argc >= i+2) ) {
	cerr << "Overriding " << argv[i]+3 << " with value " << argv[i+1] << ". ";
	 EnvPutString( argv[i]+3, argv[i+1] );
	 cerr << " Final value: " << EnvGetString(argv[i]+3) << endl;
	 i++;
	 continue;
      }

      if ( (strstr(argv[i], "-DI") == argv[i]) &&
	   (argc >= i+2) ) {
	cerr << "Overriding '" << argv[i]+3 << "' with value " << argv[i+1] << ". ";
	 EnvPutInt( argv[i]+3, atoi(argv[i+1]) );
	 cerr << " Final value: " << EnvGetLong(argv[i]+3) << endl;
	 i++;
	 continue;
      }

      if ( (strstr(argv[i], "-d") == argv[i]) &&
           (argc >= i+2) ) {
         int dbglvl = atoi(argv[i+1]);
         if (dbglvl > 0) {
            EnvPutInt( NAME_DEBUG, dbglvl);
            cerr << "Setting debug level " <<  EnvGetLong(NAME_DEBUG)<< endl;
         }
         i++;
         continue;
      }

      // Any other par is ignored
      if ( (strstr(argv[i], "-") == argv[i]) && (strlen(argv[i]) > 1) ) {
	 cerr << "Unknown parameter " << argv[i] << endl;
	 continue;
      }

      if (!srcpath) srcpath = argv[i];
      else
	 if (!destpath) destpath = argv[i];
      

   }

   if (!srcpath || !destpath) {
      PrintUsage();
      exit(1);
   }


   DebugSetLevel(EnvGetLong(NAME_DEBUG));

   Info(XrdClientDebug::kNODEBUG, "main", XRDCP_VERSION);

   XrdCpWorkLst *wklst = new XrdCpWorkLst();
   XrdOucString src, dest;
   XrdClient *xrddest;

   cpnfo.XrdCli = 0;
  
   if (wklst->SetSrc(&cpnfo.XrdCli, srcpath, srcopaque, recurse)) {
     cerr << "Error accessing path/file for " << srcpath << endl;
     exit(1);
   }

   xrddest = 0;

   // From here, we will have:
   // the knowledge if the dest is a dir name or file name
   // an open instance of xrdclient if it's a file
   if (wklst->SetDest(&xrddest, destpath, dstopaque, xrd_wr_flags)) {
      cerr << "Error accessing path/file for " << destpath << endl;
      exit(1);
   }

   int retval = 0;
   while (!retval && wklst->GetCpJob(src, dest)) {
      Info(XrdClientDebug::kUSERDEBUG, "main", src << " --> " << dest);

      if ( (src.beginswith("root://")) || (src.beginswith("xroot://")) ) {
	 // source is xrootd

	 if (srcopaque) {
	    src += "?";
	    src += srcopaque;
	 }

	 if ( (dest.beginswith("root://")) || (dest.beginswith("xroot://")) ) {
	    XrdOucString d;
	    bool isd;
	    wklst->GetDest(d, isd);

	    BuildFullDestFilename(src, d, isd);

	    if (dstopaque) {
	       d += "?";
	       d += dstopaque;
	    }

	    retval = doCp_xrd2xrd(&xrddest, src.c_str(), d.c_str());

	 }
	 else {
	    XrdOucString d;
	    bool isd;
	    int res;
	    wklst->GetDest(d, isd);
	    res = CreateDestPath_loc(d, isd);
	    if (!res || (errno == EEXIST) || !errno) {
	       BuildFullDestFilename(src, d, isd);
	       retval = doCp_xrd2loc(src.c_str(), d.c_str());
	    }
	    else
	       cerr << "Error " << strerror(errno) <<
		     " accessing path for " << d << endl;
	 }
      }
      else {
	 // source is localfs

	 if ( (dest.beginswith("root://")) || (dest.beginswith("xroot://")) ) {
	    XrdOucString d;
	    bool isd;
	    wklst->GetDest(d, isd);

	    BuildFullDestFilename(src, d, isd);

	    if (dstopaque) {
	       d += "?";
	       d += dstopaque;
	    }

	    retval = doCp_loc2xrd(&xrddest, src.c_str(), d.c_str());

	 }
	 else {
	    cerr << "Better to use cp in this case. (dest: "<<dest<<")" << endl;
	    exit(2);
	 }

      }

   }
   return retval;
}
