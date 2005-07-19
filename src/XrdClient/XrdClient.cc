//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClient                                                            //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// A UNIX reference client for xrootd.                                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//         $Id$

#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientUrlSet.hh"
#include "XrdClient/XrdClientConn.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdClient/XrdClientConnMgr.hh"
#include "XrdClient/XrdClientSid.hh"

#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


//_____________________________________________________________________________
XrdClient::XrdClient(const char *url) {
   fReadAheadLast = 0;

   memset(&fStatInfo, 0, sizeof(fStatInfo));

   int CacheSize = EnvGetLong(NAME_READCACHESIZE);

   fUseCache = (CacheSize > 0);

   if (!XrdClientConnectionMgr::IsAlive())
     Info(XrdClientDebug::kNODEBUG,
	  "Create",
	  "(C) 2004 SLAC INFN XrdClient " << XRD_CLIENT_VERSION);

   signal(SIGPIPE, SIG_IGN);

   fInitialUrl = url;

   fConnModule = new XrdClientConn();


   if (!fConnModule) {
      Error("Create","Object creation failed.");
      abort();
   }

   fConnModule->SetRedirHandler(this);
}

//_____________________________________________________________________________
XrdClient::~XrdClient()
{
   // Destructor
   Close();
   if (fConnModule)
      delete fConnModule;
}

//_____________________________________________________________________________
bool XrdClient::Open(kXR_unt16 mode, kXR_unt16 options) {
  short locallogid;
  
  // But we initialize the internal params...
  fOpenPars.opened = FALSE;  
  fOpenPars.options = options;
  fOpenPars.mode = mode;
  

  // Now we try to set up the first connection
  // We cycle through the list of urls given in fInitialUrl
  

  // Max number of tries
  int connectMaxTry = EnvGetLong(NAME_FIRSTCONNECTMAXCNT);
  
  // Construction of the url set coming from the resolution of the hosts given
  XrdClientUrlSet urlArray(fInitialUrl);
  if (!urlArray.IsValid()) {
     Error("Create", "The URL provided is incorrect.");
     return FALSE;
  }

  //
  // Now start the connection phase, picking randomly from UrlArray
  //
  urlArray.Rewind();
  locallogid = -1;
  int urlstried = 0;
  for (int connectTry = 0;
      (connectTry < connectMaxTry) && (!fConnModule->IsConnected()); 
       connectTry++) {

     XrdClientUrlInfo *thisUrl;
     urlstried = (urlstried == urlArray.Size()) ? 0 : urlstried;
     
     // Get an url from the available set
     thisUrl = urlArray.GetARandomUrl();
     
     if (thisUrl) {

        if (fConnModule->CheckHostDomain(thisUrl->Host,
					 EnvGetString(NAME_CONNECTDOMAINALLOW_RE),
					 EnvGetString(NAME_CONNECTDOMAINDENY_RE))) {

	   Info(XrdClientDebug::kHIDEBUG,
		"Open", "Trying to connect to " <<
		thisUrl->Host << ":" << thisUrl->Port <<
		". Connect try " << connectTry+1);
	   
           locallogid = fConnModule->Connect(*thisUrl, this);
        } else {
           // Invalid domain: drop the url and move to next, if any
           urlArray.EraseUrl(thisUrl);
           continue;
        }
        // To find out if we have tried the whole URLs set
        urlstried++;
     }
     
     // We are connected to a host. Let's handshake with it.
     if (fConnModule->IsConnected()) {

        // Now the have the logical Connection ID, that we can use as streamid for 
        // communications with the server

	   Info(XrdClientDebug::kHIDEBUG, "CreateTXNf",
		"The logical connection id is " << fConnModule->GetLogConnID() <<
		".");

        fConnModule->SetUrl(*thisUrl);
        fUrl = *thisUrl;
        
	Info(XrdClientDebug::kHIDEBUG, "CreateTXNf",
	     "Working url is " << thisUrl->GetUrl());
        
        // after connection deal with server
        if (!fConnModule->GetAccessToSrv())
           
           if (fConnModule->LastServerError.errnum == kXR_NotAuthorized) {
              if (urlstried == urlArray.Size()) {
                 // Authentication error: we tried all the indicated URLs:
                 // does not make much sense to retry
                 fConnModule->Disconnect(TRUE);
                 XrdClientString msg(fConnModule->LastServerError.errmsg);
                 msg.EraseFromEnd(1);
                 Error("CreateTXNf", "Authentication failure: " << msg);
                 break;
              } else {
                 XrdClientString msg(fConnModule->LastServerError.errmsg);
                 msg.EraseFromEnd(1);
                 Info(XrdClientDebug::kHIDEBUG, "CreateTXNf",
                                                "Authentication failure: " << msg);
              }
           } else {
              Error("CreateTXNf", "Access to server failed: error: " <<
                         fConnModule->LastServerError.errnum << " (" << 
                         fConnModule->LastServerError.errmsg << ") - retrying.");
           }
        else {
	   Info(XrdClientDebug::kUSERDEBUG, "Create", "Access to server granted.");
           break;
	}
     }
     
     // The server denied access. We have to disconnect.
     Info(XrdClientDebug::kHIDEBUG, "CreateTXNf", "Disconnecting.");
     
     fConnModule->Disconnect(FALSE);
     
     if (connectTry < connectMaxTry-1) {

	if (DebugLevel() >= XrdClientDebug::kUSERDEBUG)
	   Info(XrdClientDebug::kUSERDEBUG, "Create",
		"Connection attempt failed. Sleeping " <<
		EnvGetLong(NAME_RECONNECTTIMEOUT) << " seconds.");
     
	sleep(EnvGetLong(NAME_RECONNECTTIMEOUT));

     }

  } //for connect try


  if (!fConnModule->IsConnected()) {
     return FALSE;
  }

  
  //
  // Variable initialization
  // If the server is a new xrootd ( load balancer or data server)
  //
  if ((fConnModule->GetServerType() != XrdClientConn::kSTRootd) && 
      (fConnModule->GetServerType() != XrdClientConn::kSTNone)) {
     // Now we are connected to a server that didn't redirect us after the 
     // login/auth phase
     // let's continue with the openfile sequence

     Info(XrdClientDebug::kUSERDEBUG,
	  "Create", "Opening the remote file " << fUrl.File); 

     if (!TryOpen(mode, options)) {
	Error("Create", "Error opening the file " <<
	      fUrl.File << " on host " << fUrl.Host << ":" <<
	      fUrl.Port);

	return FALSE;

     } else {

	Info(XrdClientDebug::kUSERDEBUG, "Create", "File opened succesfully.");

     }
  } else {
     // the server is an old rootd
     if (fConnModule->GetServerType() == XrdClientConn::kSTRootd) {
        return FALSE;
     }
     if (fConnModule->GetServerType() == XrdClientConn::kSTNone) {
        return FALSE;
     }
  }

  return TRUE;

}

//_____________________________________________________________________________
int XrdClient::Read(void *buf, long long offset, int len) {

   if (!IsOpen()) {
      Error("Read", "File not opened.");
      return FALSE;
   }

  kXR_int32 rasize = EnvGetLong(NAME_READAHEADSIZE);

  // If the cache is enabled and gives the data to us
  //  we don't need to ask the server for them
  if( fUseCache &&
      fConnModule->GetDataFromCache(buf, offset,
				    len + offset - 1, TRUE) ) {

     Info(XrdClientDebug::kHIDEBUG, "Read",
	  "Found data in cache. len=" << len <<
	  " offset=" << offset);

     // Are we using async read ahead?
     if (fUseCache && EnvGetLong(NAME_GOASYNC) && (rasize > 0) &&
	 EnvGetLong(NAME_READAHEADTYPE) ) {
	kXR_int64 araoffset;
	kXR_int32 aralen;

	// This is a HIT case. Async readahead will try to put some data
	// in advance into the cache. The higher the araoffset will be,
	// the best chances we have not to cause overhead
	araoffset = xrdmax(fReadAheadLast, offset + len);
	aralen = xrdmin(rasize,
			offset + len + rasize -
			xrdmax(offset + len, fReadAheadLast));

	if (aralen > 0) {
           TrimReadRequest(araoffset, aralen, rasize);
	   Read_Async(araoffset, aralen);
	   fReadAheadLast = araoffset + aralen;
	}
     }

     return len;
  }




  // Prepare request
  ClientRequest readFileRequest;
  memset( &readFileRequest, 0, sizeof(readFileRequest) );
  fConnModule->SetSID(readFileRequest.header.streamid);
  readFileRequest.read.requestid = kXR_read;
  memcpy( readFileRequest.read.fhandle, fHandle, sizeof(fHandle) );
  readFileRequest.read.offset = offset;
  readFileRequest.read.rlen = len;
  readFileRequest.read.dlen = 0;


  // We assume the buffer has been pre-allocated to contain length
  // bytes by the caller of this function
  kXR_int32 rlen = 0;

  // Here the read-ahead decision should be done.
  // We are in a MISS case, so the read ahead is done the sync way.

  // We are not going async, hence the readahead is performed
  // by reading a larger block
  if (len > rasize)
     rlen = len;
  else {
//     rlen = xrdmax(len, rasize * 3 / 2);
//     rlen += rasize  / 3;
//     readFileRequest.read.offset -= rasize / 3;
     rlen = rasize;
  }

  Info(XrdClientDebug::kHIDEBUG, "ReadBuffer",
       "Sync reading " << rlen << "@" <<
       readFileRequest.read.offset);

  if (fUseCache && TrimReadRequest(readFileRequest.read.offset, rlen, rasize)) {

     readFileRequest.read.rlen = rlen;

     // We are not interested in getting the data here.
     // A side effect of this is to populate the cache with
     //  all the received messages. And avoiding the memcpy.
     if (fConnModule->SendGenCommand(&readFileRequest, 0, 0, 0, FALSE,
                                     (char *)"ReadBuffer") ) {
	int minlen = len;
	if (minlen > fConnModule->LastServerResp.dlen)
	   minlen = fConnModule->LastServerResp.dlen;

	fReadAheadLast = offset + minlen;

        // The processing of the answers from the server should have
        // populated the cache, so we get the formerly requested buffer
        if (minlen && fConnModule->GetDataFromCache(buf, offset,
					  minlen + offset - 1, FALSE) ) {

	   return minlen;
	}
        else {
	   Info(XrdClientDebug::kHIDEBUG,
		"ReadBuffer", "Internal cache error");
	   return 0;
        }
     } else 
        return 0;
    
  } else {
     
     // Without caching
     fConnModule->SendGenCommand(&readFileRequest, 0, 0, (void *)buf,
				 FALSE, (char *)"ReadBuffer");

     return fConnModule->LastServerResp.dlen;
  }

}

//_____________________________________________________________________________
bool XrdClient::Write(const void *buf, long long offset, int len) {

   if (!IsOpen()) {
      Error("WriteBuffer", "File not opened.");
      return FALSE;
   }


   // Prepare request
   ClientRequest writeFileRequest;
   memset( &writeFileRequest, 0, sizeof(writeFileRequest) );
   fConnModule->SetSID(writeFileRequest.header.streamid);
   writeFileRequest.write.requestid = kXR_write;
   memcpy( writeFileRequest.write.fhandle, fHandle, sizeof(fHandle) );
   writeFileRequest.write.offset = offset;
   writeFileRequest.write.dlen = len;
   
   
   return fConnModule->SendGenCommand(&writeFileRequest, buf, 0, 0,
				      FALSE, (char *)"Write");
}


//_____________________________________________________________________________
bool XrdClient::Sync()
{
   // Flushes un-written data

 
   if (!IsOpen()) {
      Error("Sync", "File not opened.");
      return FALSE;
   }


   // Prepare request
   ClientRequest flushFileRequest;
   memset( &flushFileRequest, 0, sizeof(flushFileRequest) );

   fConnModule->SetSID(flushFileRequest.header.streamid);

   flushFileRequest.sync.requestid = kXR_sync;

   memcpy(flushFileRequest.sync.fhandle, fHandle, sizeof(fHandle));

   flushFileRequest.sync.dlen = 0;

   return fConnModule->SendGenCommand(&flushFileRequest, 0, 0, 0, 
                                       FALSE, (char *)"Sync");
  
}

//_____________________________________________________________________________
bool XrdClient::TryOpen(kXR_unt16 mode, kXR_unt16 options) {
   
   // First attempt to open a remote file
   bool lowopenRes = LowOpen(fUrl.File.c_str(), mode, options);

   if (lowopenRes) return TRUE;

   // If the open request failed for the error "file not found" proceed, 
   // otherwise return FALSE
   if (fConnModule->GetOpenError() != kXR_NotFound)
      return FALSE;


   // If connected to a host saying "File not Found" then...

   // If we are currently connected to a host which is different
   // from the one we formerly connected, then we resend the request
   // specifyng the supposed failing server as opaque info
   if (fConnModule->GetLBSUrl() &&
       (fConnModule->GetCurrentUrl().Host != fConnModule->GetLBSUrl()->Host) ) {
      XrdClientString opinfo;

      opinfo = "&tried=" + fConnModule->GetCurrentUrl().Host;

      Info(XrdClientDebug::kUSERDEBUG,
	   "Open", "Back to " << fConnModule->GetLBSUrl()->Host <<
	   ". Refreshing cache. Opaque info: " << opinfo);

      if ( (fConnModule->GoToAnotherServer(*fConnModule->GetLBSUrl()) == kOK) &&
	   LowOpen(fUrl.File.c_str(),
		   mode, options | kXR_refresh,
		   (char *)opinfo.c_str() ) )
	 return TRUE;
      else {
	      
	 Error("Open", "Error opening the file.");
	 return FALSE;
	 
      }

   }

   return FALSE;

}

//_____________________________________________________________________________
bool XrdClient::LowOpen(const char *file, kXR_unt16 mode, kXR_unt16 options,
			char *additionalquery) {

   // Low level Open method
   XrdClientString finalfilename(file);

   if (additionalquery)
      finalfilename += additionalquery;

   // Send a kXR_open request in order to open the remote file
   ClientRequest openFileRequest;

   struct ServerResponseBody_Open openresp;
  
   memset(&openFileRequest, 0, sizeof(openFileRequest));

   fConnModule->SetSID(openFileRequest.header.streamid);
  
   openFileRequest.header.requestid = kXR_open;

   // Now set the options field basing on user's requests
   openFileRequest.open.options = options;

   // Set the open mode field
   openFileRequest.open.mode = mode;

   // Set the length of the data (in this case data describes the path and 
   // file name)
   openFileRequest.open.dlen = finalfilename.GetSize();

   // Send request to server and receive response
   bool resp = fConnModule->SendGenCommand(&openFileRequest,
					   (const void *)finalfilename.c_str(),
					   0, &openresp, FALSE, (char *)"Open");

   if (resp) {
      // Get the file handle to use for future read/write...
      memcpy( fHandle, openresp.fhandle, sizeof(fHandle) );

      fOpenPars.opened = TRUE;
      fOpenPars.options = options;
      fOpenPars.mode = mode;
    
   }

   return fOpenPars.opened;
}

//_____________________________________________________________________________
bool XrdClient::Stat(struct XrdClientStatInfo *stinfo) {

   if (!IsOpen()) {
      Error("Stat", "File not opened.");
      return FALSE;
   }

   if (fStatInfo.stated) {
      if (stinfo)
	 memcpy(stinfo, &fStatInfo, sizeof(fStatInfo));
      return TRUE;
   }
   
   // asks the server for stat file informations
   ClientRequest statFileRequest;
   
   memset(&statFileRequest, 0, sizeof(ClientRequest));
   
   fConnModule->SetSID(statFileRequest.header.streamid);
   
   statFileRequest.stat.requestid = kXR_stat;
   memset(statFileRequest.stat.reserved, 0, 
          sizeof(statFileRequest.stat.reserved));

   statFileRequest.stat.dlen = fUrl.File.GetSize();
   
   char fStats[2048];
   memset(fStats, 0, 2048);

   bool ok = fConnModule->SendGenCommand(&statFileRequest,
					 (const char*)fUrl.File.c_str(),
					 0, fStats , FALSE, (char *)"Stat");
   
   if (ok) {

      Info(XrdClientDebug::kHIDEBUG,
	   "Stat", "Returned stats=" << fStats);
   
      sscanf(fStats, "%ld %lld %ld %ld",
	     &fStatInfo.id,
	     &fStatInfo.size,
	     &fStatInfo.flags,
	     &fStatInfo.modtime);

      if (stinfo)
	 memcpy(stinfo, &fStatInfo, sizeof(fStatInfo));
   }

   return ok;
}

//_____________________________________________________________________________
bool XrdClient::Close() {

   if (!IsOpen()) {
      Info(XrdClientDebug::kUSERDEBUG, "Close", "File not opened.");
      return TRUE;
   }

   ClientRequest closeFileRequest;
  
   memset(&closeFileRequest, 0, sizeof(closeFileRequest) );

   fConnModule->SetSID(closeFileRequest.header.streamid);

   closeFileRequest.close.requestid = kXR_close;
   memcpy(closeFileRequest.close.fhandle, fHandle, sizeof(fHandle) );
   closeFileRequest.close.dlen = 0;
  
   fConnModule->SendGenCommand(&closeFileRequest, 0,
			       0, 0, FALSE, (char *)"Close");
  
   // No file is opened for now
   fOpenPars.opened = FALSE;

   return TRUE;
}


//_____________________________________________________________________________
bool XrdClient::OpenFileWhenRedirected(char *newfhandle, bool &wasopen)
{
   // Called by the comm module when it needs to reopen a file
   // after a redir

   wasopen = fOpenPars.opened;

   if (!fOpenPars.opened)
      return TRUE;

   fOpenPars.opened = FALSE;

   Info(XrdClientDebug::kHIDEBUG,
	"OpenFileWhenRedirected", "Trying to reopen the same file." );

   kXR_unt16 options = fOpenPars.options;

   if (fOpenPars.options & kXR_delete) {
      Info(XrdClientDebug::kHIDEBUG,
         "OpenFileWhenRedirected", "Stripping off the 'delete' option." );

      options &= !kXR_delete;
      options |= kXR_open_updt;
   }

   if (fOpenPars.options & kXR_new) {
      Info(XrdClientDebug::kHIDEBUG,
         "OpenFileWhenRedirected", "Stripping off the 'new' option." );

      options &= !kXR_new;
      options |= kXR_open_updt;
   }

   if ( TryOpen(fOpenPars.mode, options) ) {

      fOpenPars.opened = TRUE;

      Info(XrdClientDebug::kHIDEBUG,
	   "OpenFileWhenRedirected",
	   "Open successful." );

      memcpy(newfhandle, fHandle, sizeof(fHandle));

      return TRUE;
   } else {
      Error("OpenFileWhenRedirected", 
	    "New redir destination server refuses to open the file.");
      
      return FALSE;
   }
}

//_____________________________________________________________________________
bool XrdClient::Copy(const char *localpath) {

   if (!IsOpen()) {
      Error("Copy", "File not opened.");
      return FALSE;
   }

   Stat(0);
   int f = open(localpath, O_CREAT | O_RDWR);   
   if (f < 0) {
      Error("Copy", "Error opening local file.");
      return FALSE;
   }

   void *buf = malloc(100000);
   long long offs = 0;
   int nr = 1;

   while ((nr > 0) && (offs < fStatInfo.size))
      if ( (nr = Read(buf, offs, 100000)) )
	 offs += write(f, buf, nr);
	 
   close(f);
   free(buf);
   
   return TRUE;
}

//_____________________________________________________________________________
UnsolRespProcResult XrdClient::ProcessUnsolicitedMsg(XrdClientUnsolMsgSender *sender,
                                        XrdClientMessage *unsolmsg) {
   // We are here if an unsolicited response comes from a logical conn
   // The response comes in the form of an TXMessage *, that must NOT be
   // destroyed after processing. It is destroyed by the first sender.
   // Remember that we are in a separate thread, since unsolicited 
   // responses are asynchronous by nature.

   Info(XrdClientDebug::kHIDEBUG,
	"ProcessUnsolicitedMsg", "Incoming unsolicited response from streamid " <<
	unsolmsg->HeaderSID() << " father=" <<
	SidManager->GetSidInfo(unsolmsg->HeaderSID())->fathersid );

   // Local processing ....


   if ( SidManager->JoinedSids(fConnModule->GetStreamID(), unsolmsg->HeaderSID()) ) {
      struct SidInfo *si = SidManager->GetSidInfo(unsolmsg->HeaderSID());
      ClientRequest *req = &(si->outstandingreq);

      Info(XrdClientDebug::kHIDEBUG,
	   "ProcessUnsolicitedMsg",
	   "Processing unsolicited response from streamid " <<
	   unsolmsg->HeaderSID() << " father=" <<
	   si->fathersid );

      if ( (req->header.requestid == kXR_read) &&
	   ( (unsolmsg->HeaderStatus() == kXR_oksofar) || 
             (unsolmsg->HeaderStatus() == kXR_ok) ) ) {

	 long long offs = req->read.offset + si->reqbyteprogress;

	 Info(XrdClientDebug::kHIDEBUG, "ProcessUnsolicitedMsg",
	      "Putting data into cache. Offset=" <<
	      offs <<
	      " len " <<
	      unsolmsg->fHdr.dlen);

	 // To compute the end offset of the block we have to take 1 from the size!
	 fConnModule->SubmitDataToCache(unsolmsg, offs,
				       offs + unsolmsg->fHdr.dlen - 1);

	 si->reqbyteprogress += unsolmsg->fHdr.dlen;

	 if (unsolmsg->HeaderStatus() == kXR_ok) return kUNSOL_DISPOSE;
	 else return kUNSOL_KEEP;
      }

   }


   return kUNSOL_CONTINUE;
}

XReqErrorType XrdClient::Read_Async(long long offset, int len) {

   if (!IsOpen()) {
      Error("Read", "File not opened.");
      return kGENERICERR;
   }

  if (!len) return kOK;

  // Prepare request
  ClientRequest readFileRequest;
  memset( &readFileRequest, 0, sizeof(readFileRequest) );

  // No need to initialize the streamid, it will be filled by XrdClientConn
  readFileRequest.read.requestid = kXR_read;
  memcpy( readFileRequest.read.fhandle, fHandle, sizeof(fHandle) );
  readFileRequest.read.offset = offset;
  readFileRequest.read.rlen = len;
  readFileRequest.read.dlen = 0;



  Info(XrdClientDebug::kHIDEBUG, "Read_Async",
       "Requesting to read " <<
       readFileRequest.read.rlen <<
       " bytes of data at offset " <<
       readFileRequest.read.offset);

     

  return (fConnModule->WriteToServer_Async(&readFileRequest, 0));


}


bool XrdClient::TrimReadRequest(kXR_int64 &offs, kXR_int32 &len, kXR_int32 rasize) {

   kXR_int64 newoffs;
   kXR_int32 newlen;

   if (!fUseCache ) return false;

   newoffs = offs / rasize * rasize;
   //newoffs = xrdmax(0, newoffs);
   
   newlen = (offs + len - newoffs);
   newlen = ((newlen+rasize/2) / rasize * rasize);


   newlen = xrdmax(rasize, newlen);

   if (fConnModule->CacheWillFit(newlen)) {
      offs = newoffs;
      len = newlen;
      return true;
   }

   return false;

}


