//////////////////////////////////////////////////////////////////////////
//                                                                      //
// xrdcp                                                                //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// A cp-like command line tool for xrootd environments                  //
// Usage:                                                               //
//       xrdcp <source> <dest>                                          //
//  where:                                                              //
//       <source> is the path/name of a file or                         //
//                its xrootd URL in the form accepted by XrdClient, i.e.//
//   root://host1[:port1][,host2[:port2]]..[,hostN[:portN]]//filepath   //
//                                                                      //
//       <dest> is the path/name of the dest file or its URL            //
//                                                                      //
// Note that in this version you have to specify *2* filenames,         //
// not directory names                                                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include "XrdClientUrlInfo.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdClient.hh"
#include "XrdCpMthrQueue.hh"
#include "XrdClientDebug.hh"



#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define min(a, b) (a < b? a:b)

struct XrdCpInfo {
   XrdClient                    *XrdCli;
   int                          localfile;
   long long                    len, bread, bwritten;
   XrdCpMthrQueue               queue;
} cpnfo;

#define XRDCP_BLOCKSIZE          200000
#define XRDCP_VERSION            "(C) 2004 SLAC INFN xrdcp 0.1.0alpha"

// The body of a thread which reads from the global
//  XrdClient and keeps the queue filled
//____________________________________________________________________________
extern "C" void *ReaderThread_xrd(void *)
{

   Info(XrdClientDebug::kHIDEBUG,
	"ReaderThread_xrd",
	"Reader Thread starting.");
   
   pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);


   void *buf;
   long long offs = 0;
   int nr = 1;
   long long bread = 0, len = 0;

   len = cpnfo.len;

   while ((nr > 0) && (offs < len)) {
      buf = malloc(XRDCP_BLOCKSIZE);
      if (!buf) {
	 Error("xrdcp", "Out of memory.");
	 abort();
      }

      if ( (nr = cpnfo.XrdCli->Read(buf, offs, XRDCP_BLOCKSIZE)) ) {
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




// The body of a thread which reads from the global filehandle
//  and keeps the queue filled
//____________________________________________________________________________
extern "C" void *ReaderThread_loc(void *) {

   Info(XrdClientDebug::kHIDEBUG,
	"ReaderThread_loc",
	"Reader Thread starting.");

   pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);

   void *buf;
   long long offs = 0;
   int nr = 1;
   long long bread = 0;

   while (nr > 0) {
      buf = malloc(XRDCP_BLOCKSIZE);
      if (!buf) {
	 Error("xrdcp", "Out of memory.");
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


int doCp_xrd2xrd(char *src, char *dst) {
   // ----------- xrd to xrd affair
   pthread_t myTID;
   XrdClientStatInfo stat;
   XrdClient *xrddest = 0;

   // Open the input file (xrdc)
   cpnfo.XrdCli = new XrdClient(src);

   if (cpnfo.XrdCli->Open(0, 0)) {

      // Open the output file (loc)
      cpnfo.XrdCli->Stat(&stat);
      cpnfo.len = stat.size;

      xrddest = new XrdClient(dst);
      if (!xrddest->Open(kXR_ur | kXR_uw | kXR_gw | kXR_gr | kXR_or,
			 kXR_open_updt | kXR_delete | kXR_force)) {
	 Error("xrdcp", "Error opening remote destination file " << dst);
	 delete cpnfo.XrdCli;
	 delete xrddest;
	 cpnfo.XrdCli = 0;
	 return -1;
      }
      
      // Start reader on xrdc
      XrdOucThread_Run(&myTID, ReaderThread_xrd, (void *)&cpnfo);

      int len = 1;
      void *buf;
      long long offs = 0;
      // Loop to write until ended or timeout err
      while (len > 0)
	 if ( cpnfo.queue.GetBuffer(&buf, len) ) {
	    if (len && buf) {

	       if (!xrddest->Write(buf, offs, len)) {
		  Error("xrdcp", "Error writing to output server.");
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
	 else break;
	 

   }

   delete xrddest;
   delete cpnfo.XrdCli;
   cpnfo.XrdCli = 0;

   return 0;
}

int doCp_xrd2loc(char *src, char *dst) {
   // ----------- xrd to loc affair
   pthread_t myTID;
   XrdClientStatInfo stat;

   // Open the input file (xrdc)
   cpnfo.XrdCli = new XrdClient(src);
   if (cpnfo.XrdCli->Open(0, 0)) {

      // Open the output file (loc)
      cpnfo.XrdCli->Stat(&stat);
      cpnfo.len = stat.size;

      unlink(dst);
      int f = open(dst, O_CREAT | O_WRONLY |
		   O_TRUNC | S_IRWXU | S_IRWXG | S_IROTH);   
      if (f < 0) {
	 Error("xrdcp", "Error " << strerror(errno) <<
	       " creating " << dst);
	 delete cpnfo.XrdCli;
	 cpnfo.XrdCli = 0;
	 return -1;
      }
      
      // Start reader on xrdc
      XrdOucThread_Run(&myTID, ReaderThread_xrd, (void *)&cpnfo);

      int len = 1;
      void *buf;
      // Loop to write until ended or timeout err
      while (len > 0)
	 if ( cpnfo.queue.GetBuffer(&buf, len) ) {

	    if (len && buf) {

	       if (write(f, buf, len) <= 0) {
		  Error ("xrdcp", "Error " << strerror(errno) <<
			 " writing to " << dst);
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
	 else break;
	 
      close(f);
   }

   delete cpnfo.XrdCli;
   cpnfo.XrdCli = 0;

   return 0;
}

int doCp_loc2xrd(char *src, char * dst) {
// ----------- loc to xrd affair
   pthread_t myTID;
   XrdClient *xrddest;

   // Open the input file (loc)
   cpnfo.localfile = open(src, O_RDONLY);   
   if (cpnfo.localfile < 0) {
      Error("xrdcp", "Error " << strerror(errno) << " opening " << src);
      cpnfo.localfile = 0;
      return -1;
   }

   xrddest = new XrdClient(dst);
   if (!xrddest->Open(kXR_ur | kXR_uw | kXR_gw | kXR_gr | kXR_or,
		      kXR_open_updt | kXR_delete | kXR_force)) {
      Error("xrdcp", "Error opening remote destination file " << dst);
      close(cpnfo.localfile);
      delete xrddest;
      cpnfo.localfile = 0;
      return -1;
   }
      
   // Start reader on loc
   XrdOucThread_Run(&myTID, ReaderThread_loc, (void *)&cpnfo);

   int len = 1;
   void *buf;
   long long offs = 0;
   // Loop to write until ended or timeout err
   while (len > 0)
      if ( cpnfo.queue.GetBuffer(&buf, len) ) {
	 if (len && buf) {

	    if (!xrddest->Write(buf, offs, len)) {
	       Error("xrdcp", "Error writing to output server.");
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
	 Error("xrdcp", "Read timeout.");
	 break;
      }
	 

   

   delete xrddest;
   close(cpnfo.localfile);
   cpnfo.localfile = 0;

   return 0;
}





// Main program
int main(int argc, char**argv) {

   if (argc != 3) {
      Error("xrdcp", "usage: xrdcp <source> <dest>");
      exit(1);
   }

   Info(XrdClientDebug::kNODEBUG, "main", XRDCP_VERSION);

   if (strstr(argv[1], "root://") == argv[1]) {
      // source is xrootd

      if (strstr(argv[2], "root://") == argv[2])
	  doCp_xrd2xrd(argv[1], argv[2]);
      else
	 doCp_xrd2loc(argv[1], argv[2]);
   }
   else {
      // source is localfs

      if (strstr(argv[2], "root://") == argv[2])
	 doCp_loc2xrd(argv[1], argv[2]);
      else {
	 Error("xrdcp", "Better to use cp in this case.");
	 exit(2);
      }

   }

   return 0;
}
