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


#include "XrdClient.hh"
#include "XrdClientDebug.hh"
#include "XrdClientUrlSet.hh"
#include "XrdClientConn.hh"
#include "XrdClientEnv.hh"

#include <stdio.h>
#include <string>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



//_____________________________________________________________________________
XrdClient::XrdClient(const char *url) {

   memset(&fStatInfo, 0, sizeof(fStatInfo));

   int CacheSize = EnvGetLong(NAME_READCACHESIZE);

   fUseCache = (CacheSize > 0);
   fReadAheadSize = EnvGetLong(NAME_READAHEADSIZE);

   Info(XrdClientDebug::kNODEBUG,
	"Create",
	"(C) 2004 SLAC INFN XrdClient " << XRD_CLIENT_VERSION);

   // Using ROOT mechanism to IGNORE SIGPIPE signal
   //gSystem->IgnoreSignal(kSigPipe);

   fInitialUrl.TakeUrl(url);

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

   SafeDelete(fConnModule);
}

//_____________________________________________________________________________
bool XrdClient::Open(kXR_int16 mode, kXR_int16 options) {
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



  // Check for allowed domains
  bool validDomain = FALSE;

  for (int jj=0; jj <=urlArray.Size()-1; jj++) {
     XrdClientUrlInfo *thisUrl;
     thisUrl = urlArray.GetNextUrl();

     if (fConnModule->CheckHostDomain(thisUrl->Host,
				      EnvGetString(NAME_CONNECTDOMAINALLOW_RE),
				      EnvGetString(NAME_CONNECTDOMAINDENY_RE))) {
	validDomain = TRUE;
	break;
     }
  }

  if (!validDomain) {
     Error("CreateTXNf", "All the specified servers are disallowed. ");
     return FALSE;
  }

  //
  // Now start the connection phase, picking randomly from UrlArray
  //
  urlArray.Rewind();
  locallogid = -1;
  for (int connectTry = 0;
      (connectTry < connectMaxTry) && (!fConnModule->IsConnected()); 
       connectTry++) {

     XrdClientUrlInfo *thisUrl;
     
     // Get an url from the available set
     thisUrl = urlArray.GetARandomUrl();
     
     if (thisUrl) {

        if (fConnModule->CheckHostDomain(thisUrl->Host,
					 EnvGetString(NAME_CONNECTDOMAINALLOW_RE),
					 EnvGetString(NAME_CONNECTDOMAINDENY_RE))) {

	   Info(XrdClientDebug::kHIDEBUG,
		"CreateTXNf", "Trying to connect to " <<
		thisUrl->Host << ":" << thisUrl->Port <<
		". Connect try " << connectTry+1);
	   
           locallogid = fConnModule->Connect(*thisUrl);
        }
     }
     
     // We are connected to a host. Let's handshake with it.
     if (fConnModule->IsConnected()) {

        // Now the have the logical Connection ID, that we can use as streamid for 
        // communications with the server

	   Info(XrdClientDebug::kHIDEBUG, "CreateTXNf",
		"The logical connection id is " << fConnModule->GetLogConnID() <<
		". This will be the streamid for this client");

        fConnModule->SetUrl(*thisUrl);
        
	Info(XrdClientDebug::kHIDEBUG, "CreateTXNf",
	     "Working url is " << thisUrl->GetUrl());
        
        // after connection deal with server
        if (!fConnModule->GetAccessToSrv())
           Error("CreateTXNf", "Access to server failed")
        else {
	   Info(XrdClientDebug::kUSERDEBUG, "Create", "Access to server granted.");
           break;
	}
     }
     
     // The server denied access. We have to disconnect.
     Info(XrdClientDebug::kHIDEBUG, "CreateTXNf", "Disconnecting.");
     
     fConnModule->Disconnect(FALSE);
     
     if (DebugLevel() >= XrdClientDebug::kUSERDEBUG)
        Info(XrdClientDebug::kUSERDEBUG, "Create",
	     "Connection attempt failed. Sleeping " <<
	     EnvGetLong(NAME_RECONNECTTIMEOUT) << " seconds.");
     
     sleep(EnvGetLong(NAME_RECONNECTTIMEOUT));

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
	  "Create", "Opening the remote file " << fInitialUrl.File); 

     if (!TryOpen(mode, options)) {
	Error("Create", "Error opening the file " <<
	      fInitialUrl.File << " on host " << fInitialUrl.Host << ":" <<
	      fInitialUrl.Port);

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
int XrdClient::Read(const void *buf, long long offset, int len) {

   struct ServerResponseHeader srh;

   if (!IsOpen()) {
      Error("Read", "File not opened.");
      return FALSE;
   }

  // If the cache is enabled and gives the data to us
  //  we don't need to ask the server for them
  if( fUseCache &&
      fConnModule->GetDataFromCache(buf, offset,
				    len + offset, TRUE) ) {

     Info(XrdClientDebug::kHIDEBUG, "Read",
	  "Found data in cache. len=" << len <<
	  " offset=" << offset);

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
  long long rlen = 0;

  // Here the read-ahead decision should be done
  // We put into this one, as a trivial read-ahead mechanism
  if (len > fReadAheadSize)
     rlen = len;
  else
     rlen = fReadAheadSize;

     Info(XrdClientDebug::kHIDEBUG, "ReadBuffer",
	  "Calling TXNetConn::SendGenCommand to read " <<
	  readFileRequest.read.rlen <<
	  " bytes of data at offset " <<
	  readFileRequest.read.offset);

  if (fUseCache && fConnModule->CacheWillFit(rlen)) {

     readFileRequest.read.rlen = rlen;

     // We are not interested in getting the data here.
     // A side effect of this is to populate the cache with
     //  all the received messages. And avoiding the memcpy.
     if (fConnModule->SendGenCommand(&readFileRequest, 0, 0, 0, FALSE,
                                     "ReadBuffer", &srh) ) {
	int minlen = len;
	if (minlen > srh.dlen) minlen = srh.dlen;

        // The processing of the answers from the server should have
        // populated the cache
        if (fConnModule->GetDataFromCache(buf, offset,
					  minlen + offset, FALSE) ) {

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
				 FALSE, "ReadBuffer", &srh);

     return srh.dlen;
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
				      FALSE, "Write");
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
                                       FALSE, "XrdClient::Flush");
  
}

//_____________________________________________________________________________
bool XrdClient::TryOpen(kXR_int16 mode, kXR_int16 options) {
   
   // First attempt to open a remote file
   bool lowopenRes = LowOpen(fInitialUrl.File.c_str(), mode, options);

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
      string opinfo;

      opinfo = "&tried=" + fConnModule->GetCurrentUrl().Host;

      Info(XrdClientDebug::kUSERDEBUG,
	   "Open", "Trying to re-open the file with kXR_refresh opt and "
	   "opaque info set to " << opinfo);
     
      if ( !LowOpen(fInitialUrl.File.c_str(), mode, options | kXR_refresh, (char *)opinfo.c_str() ) ) {
	      
	 Error("Open", "Error opening the file.");
	 return FALSE;
	 
      } else
	 // Open succeded after the refresh; 
	 return TRUE;
   }
   else return FALSE;

}

//_____________________________________________________________________________
bool XrdClient::LowOpen(const char *file, kXR_int16 mode, kXR_int16 options,
			char *additionalquery) {

   // Low level Open method
   string finalfilename(file);

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
   openFileRequest.open.dlen = finalfilename.size();

   // Send request to server and receive response
   bool resp = fConnModule->SendGenCommand(&openFileRequest,
					   (const void *)finalfilename.c_str(),
					   0, &openresp, FALSE, "Open");

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

   statFileRequest.stat.dlen = fInitialUrl.File.size();
   
   char fStats[2048];
   
   fConnModule->SendGenCommand(&statFileRequest, (const char*)fInitialUrl.File.c_str(),
                               0, fStats , FALSE, (char *)"SysStat");
   
   if (DebugLevel() >= XrdClientDebug::kHIDEBUG)
      Info(XrdClientDebug::kHIDEBUG,
	   "Stat", "Returned stats=" << fStats);
   
   sscanf(fStats, "%ld %Ld %ld %ld",
	  &fStatInfo.id,
	  &fStatInfo.size,
	  &fStatInfo.flags,
	  &fStatInfo.modtime);

   if (stinfo)
      memcpy(stinfo, &fStatInfo, sizeof(fStatInfo));

   return TRUE;
}

//_____________________________________________________________________________
bool XrdClient::Close() {

   if (!IsOpen()) {
      Error("Close", "File not opened.");
      return FALSE;
   }

   ClientRequest closeFileRequest;
  
   memset(&closeFileRequest, 0, sizeof(closeFileRequest) );

   fConnModule->SetSID(closeFileRequest.header.streamid);

   closeFileRequest.close.requestid = kXR_close;
   memcpy(closeFileRequest.close.fhandle, fHandle, sizeof(fHandle) );
   closeFileRequest.close.dlen = 0;
  
   fConnModule->SendGenCommand(&closeFileRequest, 0,
			       0, 0, FALSE, "Close");
  
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

   // After a redirection we must not reinit the TFile ancestor...
   if ( Open(fOpenPars.mode, fOpenPars.options) ) {

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
bool XrdClient::ProcessUnsolicitedMsg(XrdClientUnsolMsgSender *sender,
                                        XrdClientMessage *unsolmsg) {
   // We are here if an unsolicited response comes from a logical conn
   // The response comes in the form of an TXMessage *, that must NOT be
   // destroyed after processing. It is destroyed by the first sender.
   // Remember that we are in a separate thread, since unsolicited 
   // responses are asynchronous by nature.

   Info(XrdClientDebug::kNODEBUG,
	"ProcessUnsolicitedMsg", "Processing unsolicited response");

   // Local processing ....

   return TRUE;
}

