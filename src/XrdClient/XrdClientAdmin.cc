//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientAdmin                                                       //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from XTNetAdmin (root.cern.ch) originally done by            //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// A UNIX reference admin client for xrootd.                            //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#include "XrdClientAdmin.hh"
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
#include <netinet/in.h>

using namespace std;

//_____________________________________________________________________________
void joinStrings(string &buf, vecString vs)
{
  //  string buf;
  if (vs.size() == 1)
    buf = vs[0];
  else {
    for(unsigned int j=0; j <= vs.size()-1; j++)
      {
	buf += vs[j];
	buf += "\n";
      }
  }
  if (buf[buf.size()-1] == '\n')
     buf.erase(buf.size()-1);
}





//_____________________________________________________________________________
XrdClientAdmin::XrdClientAdmin(const char *url) {

   Info(XrdClientDebug::kNODEBUG,
	"",
	"(C) 2004 SLAC XrdClientAdmin " << XRD_CLIENT_VERSION);

   fInitialUrl.TakeUrl(url);

   fConnModule = new XrdClientConn();

   if (!fConnModule) {
      Error("XrdClientAdmin",
	    "Object creation failed.");
      abort();
   }

  // Set this instance as a handler for handling the consequences of a redirection
  fConnModule->SetRedirHandler(this);

}

//_____________________________________________________________________________
XrdClientAdmin::~XrdClientAdmin()
{
  SafeDelete(fConnModule);
}



//_____________________________________________________________________________
bool XrdClientAdmin::Connect() {
  short locallogid;
  
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

     Info(XrdClientDebug::kUSERDEBUG, "Connect", "Connected.");

     
  } else {
     // the server is an old rootd
     if (fConnModule->GetServerType() == XrdClientConn::kSTRootd) {
	fConnModule->Disconnect(TRUE);
        return FALSE;
     }
     if (fConnModule->GetServerType() == XrdClientConn::kSTNone) {
	fConnModule->Disconnect(TRUE);
        return FALSE;
     }
  }

  return TRUE;

}



//_____________________________________________________________________________
int XrdClientAdmin::Stat(char *fname, long &id, long &size, long &flags, long &modtime)
{
  // Return file stat information. The interface and return value is
  // identical to TSystem::GetPathInfo().

  bool ok;

  // asks the server for stat file informations
  ClientRequest statFileRequest;

  memset( &statFileRequest, 0, sizeof(ClientRequest) );

  fConnModule->SetSID(statFileRequest.header.streamid);

  statFileRequest.stat.requestid = kXR_stat;

  memset(statFileRequest.stat.reserved, 0,
         sizeof(statFileRequest.stat.reserved));

  statFileRequest.header.dlen = strlen(fname);

  char fStats[2048];

  ok = fConnModule->SendGenCommand(&statFileRequest, (const char*)fname,
                             NULL, fStats , FALSE, (char *)"Stat");

  sscanf(fStats, "%ld %ld %ld %ld", &id, &size, &flags, &modtime);

  return ok;
}





//_____________________________________________________________________________
bool XrdClientAdmin::SysStatX(const char *paths_list, kXR_char *binInfo, int numPath)
{
  string pl(paths_list);
  bool ret;
  // asks the server for stat file informations
  ClientRequest statXFileRequest;
  
  memset( &statXFileRequest, 0, sizeof(ClientRequest) );
  fConnModule->SetSID(statXFileRequest.header.streamid);
  statXFileRequest.header.requestid = kXR_statx;

  // Note that we need to terminate the list with a newline!
  if (pl[pl.size()-1] != '\n')
    pl += '\n';

  statXFileRequest.stat.dlen = pl.size();
  
  ret = fConnModule->SendGenCommand(&statXFileRequest, pl.c_str(),
				    NULL, binInfo , FALSE, (char *)"SysStatX");
  
  return(ret);
}

//_____________________________________________________________________________
bool XrdClientAdmin::ExistFiles(vecString vs, vecBool &vb)
{
  bool ret;
  string buf;
  joinStrings(buf, vs);

  kXR_char *Info;
  Info = (kXR_char*) malloc(vs.size()+1);
  memset((void *)Info, 0, vs.size()+1);
  
  ret = this->SysStatX(buf.c_str(), Info, vs.size());

  for(unsigned int j=0; j <= vs.size()-1; j++) 
    {
      if( !(*(Info+j) & kXR_other) && !(*(Info+j) & kXR_isDir) )
	{
	  bool tmp = TRUE;
	  vb.push_back(tmp);
	} else {
	  bool tmp = FALSE;
	  vb.push_back(tmp);
	}
    }

  return ret;
}

//_____________________________________________________________________________
bool XrdClientAdmin::ExistDirs(vecString vs, vecBool &vb)
{
  bool ret;
  string buf;
  joinStrings(buf, vs);

  kXR_char *Info;
  Info = (kXR_char*) malloc(vs.size()+1);
  memset((void *)Info, 0, vs.size()+1);
  
  ret = this->SysStatX(buf.c_str(), Info, vs.size());
  
  for(unsigned int j=0; j <= vs.size()-1; j++) 
    {
      if( (*(Info+j) & kXR_isDir) )
	{
	  bool tmp = TRUE;
	  vb.push_back(tmp);
	} else {
	  bool tmp = FALSE;
	  vb.push_back(tmp);
	}
    }

  return ret;
}

//_____________________________________________________________________________
bool XrdClientAdmin::IsFileOnline(vecString vs, vecBool &vb)
{
  bool ret;
  string buf;
  joinStrings(buf, vs);

  kXR_char *Info;
  Info = (kXR_char*) malloc(vs.size()+1);
  memset((void *)Info, 0, vs.size()+1);
  
  ret = this->SysStatX(buf.c_str(), Info, vs.size());
  
  for(unsigned int j=0; j <= vs.size()-1; j++) 
    {
      if( !(*(Info+j) & kXR_offline) )
	{
	  bool tmp = TRUE;
	  vb.push_back(tmp);
	} else {
	  bool tmp = FALSE;
	  vb.push_back(tmp);
	}
    }

  return ret;
}


// Called by the conn module after a redirection has been succesfully handled
//_____________________________________________________________________________
bool XrdClientAdmin::OpenFileWhenRedirected(char *newfhandle, bool &wasopen) {
  // We simply do nothing...
  wasopen = FALSE;
  return TRUE;
}

//_____________________________________________________________________________
bool XrdClientAdmin::Rmdir(const char *path) 
{
  // Remove an empty remote directory
  ClientRequest rmdirFileRequest;

  memset( &rmdirFileRequest, 0, sizeof(rmdirFileRequest) );
  fConnModule->SetSID(rmdirFileRequest.header.streamid);
  rmdirFileRequest.header.requestid = kXR_rmdir;
  rmdirFileRequest.header.dlen = strlen(path);
  
  return (fConnModule->SendGenCommand(&rmdirFileRequest, path, 
		 NULL, NULL, FALSE, (char *)"Rmdir"));

}

//_____________________________________________________________________________
bool XrdClientAdmin::Rm(const char *file) 
{
  // Remove a remote file
  ClientRequest rmFileRequest;

  memset( &rmFileRequest, 0, sizeof(rmFileRequest) );
  fConnModule->SetSID(rmFileRequest.header.streamid);
  rmFileRequest.header.requestid = kXR_rm;
  rmFileRequest.header.dlen = strlen(file);
  
  return (fConnModule->SendGenCommand(&rmFileRequest, file,
		 NULL, NULL, FALSE, (char *)"Rm"));
}

//_____________________________________________________________________________
bool XrdClientAdmin::Chmod(const char *file, int user, int group, int other)
{
  // Change the permission of a remote file
  ClientRequest chmodFileRequest;

  memset( &chmodFileRequest, 0, sizeof(chmodFileRequest) );

  fConnModule->SetSID(chmodFileRequest.header.streamid);
  chmodFileRequest.header.requestid = kXR_chmod;

  if((user  & 4) > 0) 
    chmodFileRequest.chmod.mode |= kXR_ur;
  if((user  & 2) > 0) 
    chmodFileRequest.chmod.mode |= kXR_uw;
  if((user  & 1) > 0) 
    chmodFileRequest.chmod.mode |= kXR_ux;
  if((group & 4) > 0) 
    chmodFileRequest.chmod.mode |= kXR_gr;
  if((group & 2) > 0)
    chmodFileRequest.chmod.mode |= kXR_gw;
  if((group & 1) > 0)
    chmodFileRequest.chmod.mode |= kXR_gx;
  if((other & 4) > 0)
    chmodFileRequest.chmod.mode |= kXR_or;
  if((other & 2) > 0)
    chmodFileRequest.chmod.mode |= kXR_ow;
  if((other & 1) > 0)
    chmodFileRequest.chmod.mode |= kXR_ox;

  chmodFileRequest.header.dlen = strlen(file);
  
  
  return (fConnModule->SendGenCommand(&chmodFileRequest, file,
				     NULL, NULL, FALSE, (char *)"Chmod")); 

}

//_____________________________________________________________________________
bool XrdClientAdmin::Mkdir(const char *dir, int user, int group, int other)
{
  // Create a remote directory
  ClientRequest mkdirRequest;

  memset( &mkdirRequest, 0, sizeof(mkdirRequest) );

  fConnModule->SetSID(mkdirRequest.header.streamid);

  mkdirRequest.header.requestid = kXR_mkdir;

  memset(mkdirRequest.mkdir.reserved, 0, 
         sizeof(mkdirRequest.mkdir.reserved));

  if(user  & 4 > 0) 
    mkdirRequest.mkdir.mode |= kXR_ur;
  if(user  & 2 > 0) 
    mkdirRequest.mkdir.mode |= kXR_uw;
  if(user  & 1 > 0) 
    mkdirRequest.mkdir.mode |= kXR_ux;
  if(group & 4 > 0) 
    mkdirRequest.mkdir.mode |= kXR_gr;
  if(group & 2 > 0)
    mkdirRequest.mkdir.mode |= kXR_gw;
  if(group & 1 > 0)
    mkdirRequest.mkdir.mode |= kXR_gx;
  if(other & 4 > 0)
    mkdirRequest.mkdir.mode |= kXR_or;
  if(other & 2 > 0)
    mkdirRequest.mkdir.mode |= kXR_ow;
  if(other & 1 > 0)
    mkdirRequest.mkdir.mode |= kXR_ox;

  mkdirRequest.header.dlen = strlen(dir);
  
  return (fConnModule->SendGenCommand(&mkdirRequest, dir,
			     NULL, NULL, FALSE, (char *)"Mkdir"));

}

//_____________________________________________________________________________
bool XrdClientAdmin::Mv(const char *fileDest, const char *fileSrc)
{
  bool ret;

  // Rename a remote file
  ClientRequest mvFileRequest;

  memset( &mvFileRequest, 0, sizeof(mvFileRequest) );

  fConnModule->SetSID(mvFileRequest.header.streamid);
  mvFileRequest.header.requestid = kXR_mv;

  mvFileRequest.header.dlen = strlen( fileDest ) + strlen( fileSrc ) + 1; // len + len + string terminator \0

  char *data = new char[mvFileRequest.header.dlen+2]; // + 1 for space separator + 1 for \0
  memset(data, 0, mvFileRequest.header.dlen+2);
  strcpy( data, fileSrc );
  strcat( data, " " );
  strcat( data, fileDest );
  
  ret = fConnModule->SendGenCommand(&mvFileRequest, data,
			     NULL, NULL, FALSE, (char *)"Mv");

  delete(data);

  return ret;
}

//_____________________________________________________________________________
bool XrdClientAdmin::ProcessUnsolicitedMsg(XrdClientUnsolMsgSender *sender, XrdClientMessage *unsolmsg)
{
  // We are here if an unsolicited response comes from a logical conn
  // The response comes in the form of an XMessage *, that must NOT be destroyed after
  //  processing. It is destroyed by the first sender.
  // Remember that we are in a separate thread, since unsolicited responses are
  //  asynchronous by nature.

  Error("ProcessUnsolicitedMsg", "Processing unsolicited response");

  // Local processing ....

  return TRUE;
}



//_____________________________________________________________________________
bool XrdClientAdmin::Protocol(kXR_int32 &proto, kXR_int32 &kind)
{
  ClientRequest protoRequest;

  memset( &protoRequest, 0, sizeof(protoRequest) );

  fConnModule->SetSID(protoRequest.header.streamid);

  protoRequest.header.requestid = kXR_protocol;

  char buf[8]; // For now 8 bytes are returned... in future could increase with more infos
  bool ret = fConnModule->SendGenCommand(&protoRequest, NULL,
				   NULL, buf, FALSE, (char *)"Protocol");
  
  memcpy(&proto, buf, sizeof(proto));
  memcpy(&kind, buf + sizeof(proto), sizeof(kind));

  proto = ntohl(proto);
  kind  = ntohl(kind);
    
  return ret;
}

//_____________________________________________________________________________
bool XrdClientAdmin::Prepare(vecString vs, kXR_char option, kXR_char prty)
{
  ClientRequest prepareRequest;

  memset( &prepareRequest, 0, sizeof(prepareRequest) );

  fConnModule->SetSID(prepareRequest.header.streamid);

  prepareRequest.header.requestid    = kXR_prepare;
  prepareRequest.prepare.options     = option;
  prepareRequest.prepare.prty        = prty;

  string buf;
  joinStrings(buf, vs);
  prepareRequest.header.dlen = buf.size();
  
  kXR_char respBuf[1024];
  memset (respBuf, 0, 1024);

  bool ret = fConnModule->SendGenCommand(&prepareRequest, buf.c_str(),
					  NULL, respBuf , FALSE, (char *)"Prepare");

  return ret;
}
