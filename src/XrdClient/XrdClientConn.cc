//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientConn                                                        // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// High level handler of connections to xrootd.                         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientDebug.hh"

#include "XrdClient/XrdClientConnMgr.hh"
#include "XrdClient/XrdClientConn.hh"
#include "XrdClient/XrdClientPhyConnection.hh"
#include "XrdClient/XrdClientProtocol.hh"

#include "XrdSec/XrdSecInterface.hh"
#include "XrdNet/XrdNetDNS.hh"
#include "XrdClient/XrdClientUrlInfo.hh"
#include "XrdClient/XrdClientStringMatcher.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdClient/XrdClientAbs.hh"

#include <stdio.h>      // needed by printf
#include <stdlib.h>     // needed by getenv()
#include <pwd.h>        // needed by getpwuid()
#include <sys/types.h>  // needed by getpid()
#include <unistd.h>     // needed by getpid() and getuid()
#include <string.h>     // needed by memcpy() and strcspn()
#include <ctype.h>


//_____________________________________________________________________________
void ParseRedir(XrdClientMessage* xmsg, int &port, XrdClientString &host, XrdClientString &token)
{
   // Small utility function... we want to parse the content
   // of a redir response from the server.

   int pos;

   // Remember... an instance of XrdClientMessage automatically 0-terminates the
   // data if present
   struct ServerResponseBody_Redirect* redirdata =
      (struct ServerResponseBody_Redirect*)xmsg->GetData();

   port = 0;

   if (redirdata) {

      host = redirdata->host;
      token = "";
      if ( (pos = host.Find((char *)"?")) != STR_NPOS ) {
         token = host.Substr(pos+1);
         host.EraseFromStart(pos);
      }
      port = ntohl(redirdata->port);
   }
}

//_____________________________________________________________________________
XrdClientConn::XrdClientConn(): fOpenError((XErrorCode)0), fConnected(FALSE), 
                        fLBSUrl(0), fUrl("")
{
   // Constructor
   char buf[255];

   gethostname(buf, sizeof(buf));

   fClientHostDomain = GetDomainToMatch(buf);

   if (fClientHostDomain == "")
      Error("XrdClientConn",
	    "Error resolving this host's domain name." );

   XrdClientString goodDomainsRE = fClientHostDomain + "|*";

   if (EnvGetString(NAME_REDIRDOMAINALLOW_RE) == 0)
      EnvPutString(NAME_REDIRDOMAINALLOW_RE,
		   (char *)goodDomainsRE.c_str());

   if (EnvGetString(NAME_REDIRDOMAINDENY_RE) == 0)
      EnvPutString(NAME_REDIRDOMAINDENY_RE, "<unknown>");

   if (EnvGetString(NAME_CONNECTDOMAINALLOW_RE) == 0)
      EnvPutString(NAME_CONNECTDOMAINALLOW_RE,
		   (char *)goodDomainsRE.c_str());

   if (EnvGetString(NAME_CONNECTDOMAINDENY_RE) == 0)
      EnvPutString(NAME_CONNECTDOMAINDENY_RE, "<unknown>");


   fRedirHandler = 0;

   // Init the redirection counter parameters
   fGlobalRedirLastUpdateTimestamp = time(0);
   fGlobalRedirCnt = 0;
   fMaxGlobalRedirCnt = EnvGetLong(NAME_MAXREDIRECTCOUNT);

   fMainReadCache = NULL;
   if (EnvGetLong(NAME_READCACHESIZE))
      fMainReadCache = new XrdClientReadCache();
}

//_____________________________________________________________________________
XrdClientConn::~XrdClientConn()
{
   // Destructor
   if (fMainReadCache && (DebugLevel() >= XrdClientDebug::kUSERDEBUG))
      fMainReadCache->PrintPerfCounters();

   if (fLBSUrl) delete fLBSUrl;

   delete fMainReadCache;
}

//_____________________________________________________________________________
short XrdClientConn::Connect(XrdClientUrlInfo Host2Conn)
{
   // Connect method (called the first time when XrdNetFile is first created, 
   // and used for each redirection). The global static connection manager 
   // object is firstly created here. If another XrdNetFile object is created
   // inside the same application this connection manager will be used and
   // no new one will be created.
   // No login/authentication are performed at this stage.

   // We try to connect to the host. What we get is the logical conn id
   short logid;
   logid = -1;

   logid = ConnectionManager->Connect(Host2Conn);

   Info(XrdClientDebug::kHIDEBUG,
	"Connect", "Connect(" << Host2Conn.Host << ", " <<
	Host2Conn.Port << ") returned " <<
	logid );

   if (logid < 0) {
      Error("XrdNetFile",
	    "Error creating logical connection to " << 
	    Host2Conn.Host << ":" << Host2Conn.Port );
      SetLogConnID(logid);
      fConnected = FALSE;
      return -1;
   }

   fConnected = TRUE;

   SetLogConnID(logid);
   return logid;
}

//_____________________________________________________________________________
void XrdClientConn::Disconnect(bool ForcePhysicalDisc)
{
   // Disconnect

   ConnectionManager->Disconnect(GetLogConnID(), ForcePhysicalDisc);
   fConnected = FALSE;
}

//_____________________________________________________________________________
XrdClientMessage *XrdClientConn::ClientServerCmd(ClientRequest *req, const void *reqMoreData,
                                      void **answMoreDataAllocated,
                                      void *answMoreData, bool HasToAlloc) 
{
   // ClientServerCmd tries to send a command to the server and to get a response.
   // Here the kXR_redirect is handled, as well as other things.
   //
   // If the calling function requests the memory allocation (HasToAlloc is true) 
   // then:  
   //  o answMoreDataAllocated is filled with a pointer to the new block.
   //  o The caller MUST free it when it's no longer used if 
   //    answMoreDataAllocated is 0 
   //    then the caller is not interested in getting the data.
   //  o We must anyway read it from the stream and throw it away.
   //
   // If the calling function does NOT request the memory allocation 
   // (HasToAlloc is false) then: 
   //  o answMoreData is filled with the data read
   //  o the caller MUST be sure that the arriving data will fit into the
   //  o passed memory block
   //
   // We need to do this here because the calling func *may* not know the size 
   // to allocate for the request to be submitted. For instance, for the kXR_read
   // cmd the size is known, while for the kXR_getfile cmd is not.

   int len;
   ClientRequest reqtmp;

   size_t TotalBlkSize = 0;

   void *tmpMoreData;
   XReqErrorType errorType = kOK;

   XrdClientMessage *xmsg = 0;

   // In the case of an abort due to errors, better to return
   // a blank struct. Also checks the validity of the pointer.
   // memset(answhdr, 0, sizeof(answhdr));

   // Cycle for redirections...
   do {

      // Send to the server the request
      len = sizeof(ClientRequest);

      // We have to unconditionally set the streamid inside the
      // header, because, in case of 'rebouncing here', the Logical Connection 
      // ID might have changed, while in the header to write it remained the 
      // same as before, not valid anymore
      SetSID(req->header.streamid);

      reqtmp = *req;

      if (DebugLevel() >= XrdClientDebug::kDUMPDEBUG)
	 smartPrintClientHeader(&reqtmp);

      clientMarshall(&reqtmp);

      errorType = WriteToServer(&reqtmp, req, reqMoreData, fLogConnID);
      
      // Read from server the answer
      // Note that the answer can be composed by many reads, in the case that
      // the status field of the responses is kXR_oksofar

      TotalBlkSize = 0;

      // A temp pointer to the mem block growing across the multiple kXR_oksofar
      tmpMoreData = 0;
      if ((answMoreData != 0) && !HasToAlloc)
         tmpMoreData = answMoreData;
      
      // Cycle for the kXR_oksofar i.e. partial answers to be collected
      do {

         XrdClientConn::EThreeStateReadHandler whatToDo;

	 delete xmsg;

         xmsg = ReadPartialAnswer(errorType, TotalBlkSize, req, HasToAlloc,
                                  &tmpMoreData, whatToDo);

         // If the cmd went ok and was a read request, we use it to populate
         // the cache
         if (xmsg && fMainReadCache && (req->header.requestid == kXR_read) &&
            ((xmsg->HeaderStatus() == kXR_oksofar) || 
             (xmsg->HeaderStatus() == kXR_ok)))
	    // To compute the end offset of the block we have to take 1 from the size!
            fMainReadCache->SubmitXMessage(xmsg, req->read.offset + TotalBlkSize - xmsg->fHdr.dlen,
                                           req->read.offset + TotalBlkSize - 1);

         if (whatToDo == kTSRHReturnNullMex) {
	    delete xmsg;
            return 0;
	 }

         if (whatToDo == kTSRHReturnMex)
            return xmsg;
	
         if (xmsg && (xmsg->HeaderStatus() == kXR_oksofar) && 
                     (xmsg->DataLen() == 0))
            return xmsg;
	
      } while (xmsg && (xmsg->HeaderStatus() == kXR_oksofar));

   } while ((fGlobalRedirCnt < fMaxGlobalRedirCnt) &&
            xmsg && (xmsg->HeaderStatus() == kXR_redirect)); 

   // We collected all the partial responses into a single memory block.
   // If the block has been allocated here then we must pass its address
   if (HasToAlloc && (answMoreDataAllocated)) {
      *answMoreDataAllocated = tmpMoreData;
   }

   // We might have collected multiple partial response also in a given mem block
   if (xmsg)
      xmsg->fHdr.dlen = TotalBlkSize;

   return xmsg;
}

//_____________________________________________________________________________
bool XrdClientConn::SendGenCommand(ClientRequest *req, const void *reqMoreData,
				 void **answMoreDataAllocated, 
                                 void *answMoreData, bool HasToAlloc,
                                 char *CmdName) {
   // SendGenCommand tries to send a single command for a number of times 

   short retry = 0;
   bool resp = FALSE, abortcmd = FALSE;

   // if we're going to open a file for the 2nd time we should reset fOpenError, 
   // just in case...
   if (req->header.requestid == kXR_open)
      fOpenError = (XErrorCode)0;

   while (!abortcmd && !resp) {
      abortcmd = FALSE;

      // Send the cmd, dealing automatically with redirections and
      // redirections on error
      Info(XrdClientDebug::kHIDEBUG,
	   "SendGenCommand","Sending command " << CmdName);

      XrdClientMessage *cmdrespMex = ClientServerCmd(req, reqMoreData,
                                              answMoreDataAllocated, 
                                              answMoreData, HasToAlloc);

      // Save server response header if requested
      if (cmdrespMex)
         memcpy(&LastServerResp, &cmdrespMex->fHdr,sizeof(struct ServerResponseHeader));

      // Check for the redir count limit
      if (fGlobalRedirCnt >= fMaxGlobalRedirCnt) {
         Error("SendGenCommand",
               "Too many redirections for request  " <<
	       convertRequestIdToChar(req->header.requestid) <<
	       ". Aborting command.");

         abortcmd = TRUE;
      }
      else {

         // On serious communication error we retry for a number of times,
         // waiting for the server to come back
         if (!cmdrespMex || cmdrespMex->IsError()) {

	    Info(XrdClientDebug::kHIDEBUG,
		 "SendGenCommand", "Communication error detected with [" <<
		 fUrl.Host << ":" << fUrl.Port);

            // For the kxr_open request we don't rely on the count limit of other
            // reqs. The open request is bounded only by the redir count limit
            if (req->header.requestid != kXR_open) 
               retry++;

            if (retry > kXR_maxReqRetry) {
               Error("SendGenCommand",
                     "Too many errors communication errors with server"
		     ". Aborting command.");

               abortcmd = TRUE;
            } else
               abortcmd = FALSE;
         } else {

	    // We are here if we got an answer for the command, so
	    // the server (original or redirected) is alive
	    resp = CheckResp(&cmdrespMex->fHdr, CmdName);
	    retry++;
	    

	    // If the answer was not (or not totally) positive, we must 
            // investigate on the result
	    if (!resp) {
	          
                               
               abortcmd = CheckErrorStatus(cmdrespMex, retry, CmdName);

	       // An open request which fails for an application reason like kxr_wait
	       // must have its kXR_Refresh bit cleared.
	       if (req->header.requestid == kXR_open)
		  req->open.options &= ((kXR_unt16)~kXR_refresh);
	    }

	    if (retry > kXR_maxReqRetry) {
               Error("SendGenCommand",
                     "Too many errors messages from server."
                     " Aborting command.");

               abortcmd = TRUE;
	    }
         } // else... the case of a correct server response but declaring an error
      }

      delete cmdrespMex;
   } // while

   return (!abortcmd);
}

//_____________________________________________________________________________
bool XrdClientConn::CheckHostDomain(XrdClientString hostToCheck, XrdClientString allow, XrdClientString deny)
{
   // Checks domain matching

   XrdClientString domain;
   XrdClientString reAllow, reDeny;

   // Get the domain for the url to check
   domain = GetDomainToMatch(hostToCheck);

   Info(XrdClientDebug::kHIDEBUG,
	"CheckHostDomain",
	"Resolved [" <<
	hostToCheck << "]'s domain name into [" <<
	domain << "]" );

   // If we are unable to get the domain for the url to check --> access denied to it
   if (!domain.GetSize()) {
      Error("CheckHostDomain",
            "Error resolving domain name for " << hostToCheck <<
	    ". Denying access.");

      return FALSE;
   }

   // Given a list of |-separated regexps for the hosts to DENY, 
   // match every entry with domain. If any match is found, deny access.
   XrdClientStringMatcher redeny((char *)deny.c_str());
   if ( redeny.Matches((char *)domain.c_str()) ) {
      Error("CheckHostDomain",
	    "Access denied to the domain of [" << hostToCheck << "].");
      
      return FALSE;
   }


   // Given a list of |-separated regexps for the hosts to ALLOW, 
   // match every entry with domain. If any match is found, grant access.

   XrdClientStringMatcher reallow((char *)allow.c_str());
   if ( reallow.Matches((char *)domain.c_str()) ) {
      Info(XrdClientDebug::kHIDEBUG, "CheckHostDomain",
	    "Access granted to the domain of [" << hostToCheck << "].");
      
      return TRUE;
   }


   Error("CheckHostDomain",
	 "Access to domain " << domain <<
	 " is not allowed nor denied. Not Allowed.");

   return FALSE;
}

//_____________________________________________________________________________
bool XrdClientConn::CheckResp(struct ServerResponseHeader *resp, const char *method)
{
   // Checks if the server's response is the ours.
   // If the response's status is "OK" returns TRUE; if the status is "redirect", it 
   // means that the max number of redirections has been achieved, so returns FALSE.

   if (MatchStreamid(resp)) {

      // ok the response belongs to me
      if (resp->status == kXR_redirect) {
         // too many redirections. Exit!
         Error(method, "Too many redirections. System error.");
         return FALSE;
      }

      if (resp->status != kXR_ok) {
         if (resp->status != kXR_wait)
            Error(method, "Server [" <<
		  fUrl.Host << ":" << fUrl.Port <<
		  "] did not return OK message for" <<
		  " last request.");

         return FALSE;
      }
      return TRUE;

   } else {
      Error(method, "The return message doesn't belong to this client.");
      return FALSE;
   }
}

//_____________________________________________________________________________
bool XrdClientConn::MatchStreamid(struct ServerResponseHeader *ServerResponse)
{
   // Check stream ID matching

   char sid[2];

   memcpy(sid, &fLogConnID, sizeof(sid));

   // Matches the streamid contained in the server's response with the ours
   return (memcmp(ServerResponse->streamid, sid, sizeof(sid)) == 0 );
}

//_____________________________________________________________________________
void XrdClientConn::SetSID(kXR_char *sid) {
   // Set our stream id, to match against that one in the server's response.

   memcpy((void *)sid, (const void*)&fLogConnID, 2);
}


//_____________________________________________________________________________
XReqErrorType XrdClientConn::WriteToServer(ClientRequest *reqtmp, ClientRequest *req, 
				       const void* reqMoreData, short LogConnID) 
{
   // Send message to server

   // Strong mutual exclusion over the physical channel
   {
      XrdClientPhyConnLocker pcl(ConnectionManager->GetConnection(fLogConnID)
                                           ->GetPhyConnection());

      // Now we write the request to the logical connection through the
      // connection manager

      short len = sizeof(req->header);

      int writeres = ConnectionManager->WriteRaw(LogConnID, reqtmp, len);
      fLastDataBytesSent = req->header.dlen;
  
      // A complete communication failure has to be handled later, but we
      // don't have to abort what we are doing
      if (writeres) {
         Error("WriteToServer",
	       "Error sending " << len << " bytes in the header part"
               " to server [" <<
	       fUrl.Host << ":" << fUrl.Port << "].");

         return kWRITE;
      }

      // Send to the server the data.
      // If we got an error we can safely skip this... no need to get more
      if (req->header.dlen > 0) {

         // Now we write the data associated to the request. Through the
         //  connection manager
         writeres = ConnectionManager->WriteRaw(LogConnID, reqMoreData,
                                                  req->header.dlen);
    
         // A complete communication failure has to be handled later, but we
         //  don't have to abort what we are doing
         if (writeres) {
            Error("WriteToServer", 
	       "Error sending " << req->header.dlen << " bytes in the data part"
               " to server [" <<
	       fUrl.Host << ":" << fUrl.Port << "].");

            return kWRITE;
         }
      }

      fLastDataBytesSent = req->header.dlen;
      return kOK;
   }
}

//_____________________________________________________________________________
bool XrdClientConn::CheckErrorStatus(XrdClientMessage *mex, short &Retry, char *CmdName)
{
   // Check error status

   if (mex->HeaderStatus() == kXR_redirect) {
      // Too many redirections
      Error("SendGenCommand",
	    "Max redirection count reached for request" << CmdName );
      return TRUE;
   }
 
   if (mex->HeaderStatus() == kXR_error) {
      // The server declared an error. 
      // In this case it's better to exit, unhandled error

      struct ServerResponseBody_Error *body_err;

      body_err = (struct ServerResponseBody_Error *)mex->GetData();


      if (body_err) {
         // Print out the error information, as received by the server

         Error("SendGenCommand",
	       "Server declared error " << 
               ntohl(body_err->errnum) << ":" <<
	       (const char*)body_err->errmsg);

         fOpenError = (XErrorCode)ntohl(body_err->errnum);
      }
      return TRUE;
   }
    
   if (mex->HeaderStatus() == kXR_wait) {
      // We have to wait for a specified number of seconds and then
      // retry the same cmd

      struct ServerResponseBody_Wait *body_wait;

      body_wait = (struct ServerResponseBody_Wait *)mex->GetData();
    
      if (body_wait) {

            if (mex->DataLen() > 4) 
               Info(XrdClientDebug::kUSERDEBUG, "SendGenCommand", "Server [" << 
		    fUrl.Host << ":" << fUrl.Port <<
		    "] requested " << ntohl(body_wait->seconds) << " seconds"
                    " of wait. Server message is " << body_wait->infomsg)
            else
               Info(XrdClientDebug::kUSERDEBUG, "SendGenCommand", "Server [" << 
		    fUrl.Host << ":" << fUrl.Port <<
		    "] requested " << ntohl(body_wait->seconds) << " seconds"
                    " of wait")


         sleep(ntohl(body_wait->seconds));
      }

      // We don't want kxr_wait to count as an error
      Retry--;
      return FALSE;
   }
    
   // We don't understand what the server said. Better investigate on it...
   Error("SendGenCommand", 
	 "Answer from server [" << 
		    fUrl.Host << ":" << fUrl.Port <<
	 "]  not recognized after executing " << CmdName);

   return TRUE;
}

//_____________________________________________________________________________
XrdClientMessage *XrdClientConn::ReadPartialAnswer(XReqErrorType &errorType,
                                        size_t &TotalBlkSize, 
                                        ClientRequest *req,  
                                        bool HasToAlloc, void** tmpMoreData,
                                        EThreeStateReadHandler &what_to_do)
{
   // Read server answer

   int len;
   XrdClientMessage *Xmsg = 0;
   void *tmp2MoreData;

   // No need to actually read if we are in error...
   if (errorType == kOK) {
    
      len = sizeof(ServerResponseHeader);

      Info(XrdClientDebug::kHIDEBUG, "ReadPartialAnswer",
	   "Reading a XrdClientMessage from the server [" << 
	   fUrl.Host << ":" << fUrl.Port << "]...");
    
      // A complete communication failure has to be handled later, but we
      //  don't have to abort what we are doing
    
      // Beware! Now Xmsg contains ALSO the information about the esit of
      // the communication at low level.
      Xmsg = ConnectionManager->ReadMsg(fLogConnID);

      if(Xmsg)
         fLastDataBytesRecv = Xmsg->DataLen();
      else 
         fLastDataBytesRecv = 0;

      if ( !Xmsg || (Xmsg->IsError()) ) {
         Error("ReadPartialAnswer", "Error reading msg from connmgr"
               " (server [" << 
	       fUrl.Host << ":" << fUrl.Port << "]).");

         if (HasToAlloc) {
            if (*tmpMoreData)
               free(*tmpMoreData);
            *tmpMoreData = 0;
         }
         errorType = kREAD;
      }
      else
         // is not necessary because the Connection Manager unmarshall the mex
         Xmsg->Unmarshall(); 
   }

   if (Xmsg != 0)
      if (DebugLevel() >= XrdClientDebug::kDUMPDEBUG)
	 smartPrintServerHeader(&Xmsg->fHdr);

   // Now we have all the data. We must copy it back to the buffer where
   // they are needed, only if we are not in troubles with errorType
   if ((errorType == kOK) && (Xmsg->DataLen() > 0)) {
    
      // If this is a redirection answer, its data MUST NOT overwrite 
      // the given buffer
      if ( (Xmsg->HeaderStatus() == kXR_ok) ||
           (Xmsg->HeaderStatus() == kXR_oksofar) ||
           (Xmsg->HeaderStatus() == kXR_authmore) ) 
      {
         // Now we allocate a sufficient memory block, if needed
         // If the calling function passed a null pointer, then we 
         // fill it with the new pointer, otherwise the func knew
         // about the size of the expected answer, and we use
         // the given pointer.
         // We need to do this here because the calling func *may* not 
         // know the size to allocate
         // For instance, for the ReadBuffer cmd the size is known, while 
         // for the ReadFile cmd is not
         if (HasToAlloc) {
            tmp2MoreData = realloc(*tmpMoreData, TotalBlkSize + Xmsg->DataLen());
            if (!tmp2MoreData) {

               Error("ReadPartialAnswer", "Error reallocating " << 
                     TotalBlkSize << " bytes.");

               free(*tmpMoreData);
               *tmpMoreData = 0;
               what_to_do = kTSRHReturnNullMex;

	       delete Xmsg;

               return 0;
            }
            *tmpMoreData = tmp2MoreData;
         }
	
         // Now we copy the content of the Xmsg to the buffer where
         // the data are needed
         if (*tmpMoreData)
            memcpy(((kXR_char *)(*tmpMoreData)) + TotalBlkSize,
                     Xmsg->GetData(), Xmsg->DataLen());
	
         // Dump the buffer tmpMoreData
         if (DebugLevel() >= XrdClientDebug::kDUMPDEBUG) {

            Info (XrdClientDebug::kDUMPDEBUG, "ReadPartialAnswer","Dumping read data...");
            for(int jj = 0; jj < Xmsg->DataLen(); jj++) {
               printf("0x%.2x ", *( ((kXR_char *)Xmsg->GetData()) + jj ) );
               if ( !(jj % 10) ) printf("\n");
            }
         }
         TotalBlkSize += Xmsg->DataLen();
	
      } else {

	    Info(XrdClientDebug::kHIDEBUG, "ReadPartialAnswer", 
		  "Server [" <<
		  fUrl.Host << ":" << fUrl.Port << "] did not answer OK." <<
		  " Resp status is [" << convertRespStatusToChar(Xmsg->fHdr.status) <<
		  "]");
      }
   } // End of DATA reading
  
   // Now answhdr contains the server response. We pass it as is to the
   // calling function.
   // The only exception is that we must deal here with redirections.
   // If the server redirects us, then we
   //   add 1 to redircnt
   //   close the logical connection
   //   try to connect to the new destination.
   //   login/auth to the new destination (this can generate other calls
   //       to this method if it has been invoked by DoLogin!)
   //   Reopen the file if the current fhandle value is not null (this 
   //     can generate other calls to this method, not for the dologin 
   //     phase)
   //   resend the command
   //
   // Also a READ/WRITE error requires a redirection
   // 
   if ( (errorType == kREAD) || 
        (errorType == kWRITE) || 
        isRedir(&Xmsg->fHdr) ) 
   {
      // this procedure can decide if return to caller or
      // continue with processing
      
      ESrvErrorHandlerRetval Return = HandleServerError(errorType, Xmsg, req);
      
      if (Return == kSEHRReturnMsgToCaller) {
         // The caller is allowed to continue its processing
         //  with the current Xmsg
         // Note that this can be a way to stop retrying
         //  e.g. if the resp in Xmsg is kxr_redirect, it means
         //  that the redir limit has been reached
         if (HasToAlloc) { 
            free(*tmpMoreData);
            *tmpMoreData = 0;
         }
	
         // Return the message to the client (SendGenCommand)
         what_to_do = kTSRHReturnMex;
         return Xmsg;
      }
      
      if (Return == kSEHRReturnNoMsgToCaller) {
         // There was no Xmsg to return, or the previous one
         //  has no meaning anymore
	
         // The caller will retry the cmd for some times,
         // If we are connected the attempt will go OK,
         //  otherwise the next retry will fail, causing a
         //  redir to the lb or a rebounce.
         if (HasToAlloc) { 
            free(*tmpMoreData);
            *tmpMoreData = 0;
         }
	
         delete Xmsg;
         Xmsg = 0;

         what_to_do = kTSRHReturnMex;
         return Xmsg;
      }
   }

   what_to_do = kTSRHContinue;
   return Xmsg;
}


//_____________________________________________________________________________
bool XrdClientConn::GetAccessToSrv()
{
   // Gets access to the connected server. The login and authorization steps
   // are performed here (calling method DoLogin() that performs loggin-in
   // and calls DoAuthentication() ).
   // If the server redirects us, this is gently handled by the general
   // functions devoted to the handling of the server's responses.
   // Nothing is visible here, and nothing is visible from the other high
   // level functions.

   XrdClientLogConnection *logconn = 0;

   // Now we are connected and we ask for the kind of the server
   //ConnectionManager->GetConnection(fLogConnID)->GetPhyConnection()->LockChannel();
   SetServerType(DoHandShake(fLogConnID));
   //ConnectionManager->GetConnection(fLogConnID)->GetPhyConnection()->UnlockChannel();

   // Now we can start the reader thread in the phyconn, if needed
   ConnectionManager->GetConnection(fLogConnID)->GetPhyConnection()->StartReader();

   switch (GetServerType()) {
   case kSTError:
      Info(XrdClientDebug::kNODEBUG,
	   "GetAccessToSrv",
	   "HandShake failed with server [" <<
	   fUrl.Host << ":" << fUrl.Port << "]");

      ConnectionManager->Disconnect(fLogConnID, TRUE);

      return FALSE;

   case XrdClientConn::kSTNone: 
      Info(XrdClientDebug::kNODEBUG,
	   "GetAccessToSrv", "The server on [" <<
	   fUrl.Host << ":" << fUrl.Port << "] is unknown");

      ConnectionManager->Disconnect(fLogConnID, TRUE);

      return FALSE;

   case XrdClientConn::kSTRootd: 

         Info(XrdClientDebug::kHIDEBUG,
	      "GetAccessToSrv","Ok: the server on [" <<
	   fUrl.Host << ":" << fUrl.Port << "] is a rootd."
	      " Not supported.");

	 ConnectionManager->Disconnect(fLogConnID, TRUE);

	 return FALSE;

   case XrdClientConn::kSTBaseXrootd: 

      Info(XrdClientDebug::kHIDEBUG,
	   "GetAccessToSrv", 
	   "Ok: the server on [" <<
	   fUrl.Host << ":" << fUrl.Port << "] is an xrootd redirector.");
      
      logconn = ConnectionManager->GetConnection(fLogConnID);
      logconn->GetPhyConnection()->SetTTL(DLBD_TTL);
      logconn->GetPhyConnection()->fServerType = kBase;
      break;

   case XrdClientConn::kSTDataXrootd: 

      Info( XrdClientDebug::kHIDEBUG,
	    "GetAccessToSrv", 
	    "Ok, the server on [" <<
	    fUrl.Host << ":" << fUrl.Port << "] is an xrootd data server.");

      logconn = ConnectionManager->GetConnection(fLogConnID);
      logconn->GetPhyConnection()->SetTTL(DATA_TTL);        // = DATA_TTL;
      logconn->GetPhyConnection()->fServerType = kData;
      break;
   }

   // Execute a login if connected to a xrootd server. For an old rootd, 
   // TNetFile takes care of the login phase
   if (GetServerType() != XrdClientConn::kSTRootd) {
      if (logconn->GetPhyConnection()->IsLogged() == kNo)
         return DoLogin();
      else {

	 Info( XrdClientDebug::kHIDEBUG,
	       "GetAccessToSrv", "Reusing physical connection to server [" <<
	       fUrl.Host << ":" << fUrl.Port << "]).");

         return TRUE;
      }
   }
   else
      return TRUE;
}

//_____________________________________________________________________________
XrdClientConn::ServerType XrdClientConn::DoHandShake(short int log)
{
   // Performs initial hand-shake with the server in order to understand which 
   // kind of server is there at the other side and to make the server know who 
   // we are (XrdNetFile instead of an old TNetFile)
   struct ClientInitHandShake initHS;
   struct ServerInitHandShake xbody;
   ServerResponseType type;

   int writeres, readres, len;
  
   // Set field in network byte order
   memset(&initHS, 0, sizeof(initHS));
   initHS.fourth = (kXR_int32)htonl(4);
   initHS.fifth  = (kXR_int32)htonl(2012);

   if (ConnectionManager->GetConnection(log)->GetPhyConnection()->fServerType == kBase) {

      Info(XrdClientDebug::kHIDEBUG,
	   "DoHandShake",
	   "The physical channel is already bound to a load balancer"
	   " server [" <<
	   fUrl.Host << ":" << fUrl.Port << "]. No handshake is needed.");

      if (!fLBSUrl || (fLBSUrl->Host == "")) {

	 Info(XrdClientDebug::kHIDEBUG,
	      "DoHandShake", "Setting Load Balancer Server Url = " <<
	      fUrl.GetUrl() );

         // Save the url of load balancer server for future uses...
         fLBSUrl = new XrdClientUrlInfo(fUrl.GetUrl());
         if(!fLBSUrl) {
            Error("DoHandShake","Object creation "
                  " failed. Probable system resources exhausted.");
            abort();
         }
      }
      return kSTBaseXrootd;
   }
   if (ConnectionManager->GetConnection(log)->GetPhyConnection()->fServerType == kData) {

      if (DebugLevel() >= XrdClientDebug::kHIDEBUG)
         Info(XrdClientDebug::kHIDEBUG,
	      "DoHandShake",
              "The physical channel is already bound to the data server"
              " [" << fUrl.Host << ":" << fUrl.Port << "]. No handshake is needed.");

      return kSTDataXrootd;
   }

   // Send to the server the initial hand-shaking message asking for the 
   // kind of server
   len = sizeof(initHS);

   Info(XrdClientDebug::kHIDEBUG,
	"DoHandShake",
	"HandShake step 1: Sending " << len << " bytes to the server [" <<
	    fUrl.Host << ":" << fUrl.Port << "]");

   writeres = ConnectionManager->WriteRaw(log, &initHS, len);

   if (writeres) {
      Error("DoHandShake", "Error sending " << len <<
	    " bytes to the server  [" <<
	    fUrl.Host << ":" << fUrl.Port << "]");

      return kSTError;
   }

   // Read from server the first 4 bytes
   len = sizeof(type);

   Info(XrdClientDebug::kHIDEBUG,
	"DoHandShake",
	"HandShake step 2: Reading " << len <<
	" bytes from server [" <<
	fUrl.Host << ":" << fUrl.Port << "].");
 
   //
   // Read returns the return value of TSocket->RecvRaw... that returns the 
   // return value of recv (unix low level syscall)
   //
   readres = ConnectionManager->ReadRaw(log, &type, 
					len); // Reads 4(2+2) bytes
               
   if (readres) {
      Error("DoHandShake", "Error reading " << len <<
	    " bytes from server [" <<
	    fUrl.Host << ":" << fUrl.Port << "].");

      return kSTError;
   }

   // to host byte order
   type = ntohl(type);

   // Check if the server is the eXtended rootd or not, checking the value 
   // of type
   if (type == 0) { // ok, eXtended!
      len = sizeof(xbody);

      Info(XrdClientDebug::kHIDEBUG,
	   "DoHandShake",
	   "HandShake step 3: Reading " << len << 
	   " bytes from server [" <<
	   fUrl.Host << ":" << fUrl.Port << "].");

      readres = ConnectionManager->ReadRaw(log, &xbody, 
					   len); // Read 12(4+4+4) bytes

      if (readres) {
         Error("DoHandShake", "Error reading " << len << 
	       " bytes from server [" <<
	       fUrl.Host << ":" << fUrl.Port << "].");

         return kSTError;
      }

      ServerInitHandShake2HostFmt(&xbody);

      fServerProto = xbody.msgtype;
    
      // check if the eXtended rootd is a data server
      switch (xbody.msgval) {
      case kXR_DataServer:
         // This is a data server
         return kSTDataXrootd;

      case kXR_LBalServer:
         // This is a load balancing server
         if (!fLBSUrl || (fLBSUrl->Host == "")) {

	    Info(XrdClientDebug::kHIDEBUG, "DoHandShake", "Setting Load Balancer Server Url = " <<
		 fUrl.GetUrl() );

            // Save the url of load balancer server for future uses...
            fLBSUrl = new XrdClientUrlInfo(fUrl.GetUrl());
            if (!fLBSUrl) {
               Error("DoHandShake","Object creation failed.");
               abort();
            }
         }
         return XrdClientConn::kSTBaseXrootd;

      default:
         // Unknown server type
         return kSTNone;
      }
   } else {
      // We are here if it wasn't an XRootd
      // and we need to complete the reading
      if (type == 8)
         return kSTRootd;
      else 
         // We dunno the server type
         return kSTNone;
   }
}

//_____________________________________________________________________________
bool XrdClientConn::DoLogin() 
{
   // This method perform the loggin-in into the server just after the
   // hand-shake. It also calls the DoAuthentication() method

   ClientRequest reqhdr;
   bool resp;
  
   // We fill the header struct containing the request for login
   memset( &reqhdr, 0, sizeof(reqhdr));

   SetSID(reqhdr.header.streamid);
   reqhdr.header.requestid = kXR_login;
   reqhdr.login.capver[0] = XRD_CLIENT_CAPVER;
   reqhdr.login.pid = getpid();

   // Get username from Url
   XrdClientString User = fUrl.User;
   if (!User.GetSize()) {
      // Use local username, if not specified
      struct passwd *u = getpwuid(getuid());
      if (u >= 0)
         User = u->pw_name;

   }
   if (User.GetSize())
      strcpy( (char *)reqhdr.login.username, User.c_str() );
   else
      strcpy( (char *)reqhdr.login.username, "????" );

   // set the token with the value provided by a previous 
   // redirection (if any)
   reqhdr.header.dlen = fRedirInternalToken.GetSize(); 
  
   // We call SendGenCommand, the function devoted to sending commands. 
   Info(XrdClientDebug::kHIDEBUG,
	"DoLogin",
	"Logging into the server [" << fUrl.Host << ":" << fUrl.Port <<
	"]. pid=" << reqhdr.login.pid << " uid=" << reqhdr.login.username);

   ConnectionManager->GetConnection(fLogConnID)->GetPhyConnection()
      ->SetLogged(kNo);


   char *plist = 0;
   resp = SendGenCommand(&reqhdr, fRedirInternalToken.c_str(), 
                         (void **)&plist, 0, 
                         TRUE, (char *)"XTNetconn::doLogin");

   // Check if we need to authenticate 
   if (resp && LastServerResp.dlen && plist) {

      // Terminate server reply
      plist[LastServerResp.dlen]=0;

      Info(XrdClientDebug::kHIDEBUG,
	   "DoLogin","server requires authentication");

      resp = DoAuthentication(User, plist);
   }

   // Flag success if everything went ok
   if (resp) 
      ConnectionManager->GetConnection(fLogConnID)->GetPhyConnection()
         ->SetLogged(kYes);
   if (plist)
      delete[] plist;

   return resp;

}

//_____________________________________________________________________________
bool XrdClientConn::DoAuthentication(XrdClientString username, XrdClientString plist) {
  // Negotiate authentication with the remote server. The XrdSecgetProtocol()
  // function tries all available protocols proposed by the server (in plist).

  // if no sectoken here then no need to do security at all
  //
  if (plist == "")
     return TRUE;

  Info(XrdClientDebug::kHIDEBUG,
       "DoAuthentication", "remote host: " << fUrl.Host <<
       " list of available protocols: " << plist << "-" <<
       plist.GetSize() );
 
  // Prepare host/IP information of the remote xrootd. This is required
  // for the authentication.
  //
  struct sockaddr_in netaddr;

  char **hosterrmsg = 0;

  int numaddr = XrdNetDNS::getHostAddr((char *)fUrl.HostAddr.c_str(), (struct sockaddr &)netaddr, hosterrmsg);
  
  if (!numaddr) {
     Error("DoAuthentication",
	   "GetHostAddr said '" << *hosterrmsg << "'");
     return FALSE;
  }

  netaddr.sin_port   = fUrl.Port;

  // Variables for negotiation
  XrdSecParameters   secToken;
  XrdSecProtocol    *protocol;
  XrdSecCredentials *credentials;

  // Now try in turn the available methods (first preferred)
  //
  bool resp = FALSE;


  // Assign the security token that we have received at the login request
  //
  secToken.buffer = (char *)plist.c_str();
  secToken.size   = plist.GetSize();
     
  // Retrieve the security protocol context from the xrootd server
  //
//   protocol = XrdSecGetProtocol((const struct sockaddr &)netaddr, secToken, 0);
     // future code will be:
  protocol = XrdSecGetProtocol((char *)fUrl.Host.c_str(), (const struct sockaddr &)netaddr, secToken, 0);
  if (!protocol) {

	Info(XrdClientDebug::kHIDEBUG,
	     "DoAuthentication", 
	     "Unable to get protocol object.");
      return FALSE;
     }
     
  // Once we have the protocol, get the credentials
  //
     credentials = protocol->getCredentials(&secToken);
     if (!credentials) {

	Info(XrdClientDebug::kHIDEBUG,
	     "DoAuthentication", 
	     "Cannot obtain credentials");
        return FALSE;
     } else
	Info(XrdClientDebug::kHIDEBUG,
	     "DoAuthentication", "cred size=" << credentials->size);
     
     // We fill the header struct containing the request for login
     ClientRequest reqhdr;
     SetSID(reqhdr.header.streamid);
     reqhdr.header.requestid = kXR_auth;
     memset(reqhdr.auth.reserved, 0, 16);
//   memcpy(reqhdr.auth.credtype, protname.c_str(), protname.GetSize());
     
     LastServerResp.status = kXR_authmore;
     char *srvans = 0;
     
     resp = FALSE;
     while (LastServerResp.status == kXR_authmore) {
        
        // Length of the credentials buffer
        reqhdr.header.dlen = credentials->size;
        
        resp = SendGenCommand(&reqhdr, credentials->buffer, 
                              (void **)&srvans, 0, TRUE, 
                              (char *)"XTNetconn::DoAuthentication");

	Info(XrdClientDebug::kHIDEBUG,
	     "DoAuthenticate", "Server reply: status: " << LastServerResp.status <<
	     " dlen: " << LastServerResp.dlen);
     
	if (resp && (LastServerResp.status == kXR_authmore)) {
           // We are required to send additional information
           // First assign the security token that we have received
           // at the login request
           //
           secToken.buffer = srvans;   
           secToken.size   = strlen(srvans);
     
           // then get next part of the credentials
           //
           credentials = protocol->getCredentials(&secToken);
           if (!credentials) {

                 Error("DoAuthentication", 
		       "Cannot obtain credentials (token: " << srvans << ")");

              break;
           } else {
	      Info(XrdClientDebug::kHIDEBUG,
		   "DoAuthentication", "cred= " << credentials->buffer <<
		   " size=" << credentials->size);
           }
        }

        // Release buffer allocated for the server reply
        if (srvans)
           delete[] srvans;
     }

  // Return the result of the negotiation
  //
  return resp;
}

//_____________________________________________________________________________
XrdClientConn::ESrvErrorHandlerRetval
XrdClientConn::HandleServerError(XReqErrorType &errorType, XrdClientMessage *xmsg,
                             ClientRequest *req)
{
   // Handle errors from server

   int newport; 	
   XrdClientString newhost; 	
   XrdClientString token;
  
   // Close the log connection at this point the fLogConnID is no longer valid.
   // On read/write error the physical channel may be not OK, so it's a good
   // idea to shutdown it.
   // If there are other logical conns pointing to it, they will get an error,
   // which will be handled
   if ((errorType == kREAD) || (errorType == kWRITE))
      ConnectionManager->Disconnect(fLogConnID, TRUE);
   else
      ConnectionManager->Disconnect(fLogConnID, FALSE);
  
   // We cycle repeatedly trying to ask the dlb for a working redir destination
   do {
    
      // Consider the timeout for the count of the redirections
      // this instance got in the last period of time
      if ( (time(0) - fGlobalRedirLastUpdateTimestamp) > EnvGetLong(NAME_REDIRCNTTIMEOUT)) {
         fGlobalRedirCnt = 0;
         fGlobalRedirLastUpdateTimestamp = time(0);
      }

      // Anyway, let's update the counter, we have just been redirected
      fGlobalRedirCnt++;

      Info(XrdClientDebug::kHIDEBUG,
	   "HandleServerError",
	   "Redir count=" << fGlobalRedirCnt);

      if ( fGlobalRedirCnt >= fMaxGlobalRedirCnt ) 
         return kSEHRContinue;
    
      newhost = "";
      newport = 0;
      token = "";
    
      if ((errorType == kREAD) || 
          (errorType == kWRITE) || 
          (errorType == kREDIRCONNECT)) {
         // We got some errors in the communication phase
         // the physical connection has been closed;
         // then we must go back to the load balancer
         // if there is any
         if ( fLBSUrl && (fLBSUrl->GetUrl().GetSize() > 0) ) {
            newhost = fLBSUrl->Host;
            newport = fLBSUrl->Port;
         }
         else {

            Error("HandleServerError",
                  "No Load Balancer to contact after a communication error"
                  " with server [" << fUrl.Host << ":" << fUrl.Port <<
		  "]. Rebouncing here.");

	    if (fUrl.HostAddr.GetSize())  newhost = fUrl.HostAddr;
	    else
	       newhost = fUrl.Host;
            newport = fUrl.Port;
         }
      
      } else if (isRedir(&xmsg->fHdr)) {
         // No comm errors, but we got an explicit redir message      
         // If we did not meet a dlb before, we consider this as a dlb
         // to return to after an error
         if (!fLBSUrl || (fLBSUrl->GetUrl().GetSize() == 0) ) {

	    Info(XrdClientDebug::kHIDEBUG,
		 "HandleServerError", 
		 "Setting Load Balancer Server Url = " << fUrl.GetUrl() );

            // Save the url of load balancer server for future uses...
            fLBSUrl = new XrdClientUrlInfo(fUrl.GetUrl());
            if (!fLBSUrl) {
               Error("HandleServerError",
                     "Object creation failed.");
               abort();
            }
         }
      
         // Extract the info (new host:port) from the response
         newhost = "";
         token   = "";
         newport = 0;
         ParseRedir(xmsg, newport, newhost, token);
      }
    
      // Now we should have the parameters needed for the redir
      // a member class 'internalToken' is needed because the host that 
      // answers with a kXR_redirect
      // message also provides a token that must be passed to the new host...
      fRedirInternalToken = token;

      CheckPort(newport);

      if ((newhost.GetSize() > 0) && newport) {
	 XrdClientUrlInfo NewUrl(fUrl.GetUrl());

         if (DebugLevel() >= XrdClientDebug::kUSERDEBUG)
            Info(XrdClientDebug::kUSERDEBUG,
		 "HandleServerError",
                 "Received redirection to [" << newhost << ":" << newport <<
		 "]. Token=[" << fRedirInternalToken << "].");

         errorType = kOK;

	 NewUrl.Host = NewUrl.HostAddr = newhost;
	 NewUrl.Port = newport;
	 NewUrl.SetAddrFromHost();


	 if ( !CheckHostDomain( newhost,
				EnvGetString(NAME_REDIRDOMAINALLOW_RE),
				EnvGetString(NAME_REDIRDOMAINDENY_RE) ) ) {
	    Error("HandleServerError",
		  "Redirection to a server out-of-domain disallowed. Abort.");
	    abort();
	 }

	 errorType = GoToAnotherServer(NewUrl);
      }
      else {
         // Host or port are not valid or empty
         Error("HandleServerError", 
               "Received redirection to [" << newhost << ":" << newport <<
	       "]. Token=[" << fRedirInternalToken << "]. No server to go...");

         errorType = kREDIRCONNECT;
      }
    
      // We don't want to flood servers...
      if (errorType == kREDIRCONNECT)
         sleep(EnvGetLong(NAME_RECONNECTTIMEOUT));

      // We keep trying the connection to the same host (we have only one)
      //  until we are connected, or the max count for
      //  redirections is reached

   } while (errorType == kREDIRCONNECT);


   // We are here if correctly connected and handshaked and logged
   if (!IsConnected()) {
      Error("HandleServerError", 
            "Not connected. Internal error. Abort.");
      abort();
   }

   // If the former request was a kxr_open,
   //  there is no need to reissue it, since it will be the next attempt
   //  to rerun the cmd.
   // We simply return to the caller, which will retry
   // The same applies to kxr_login. No need to reopen a file if we are
   // just logging into another server.
   // The open request will surely follow if needed.
   if ((req->header.requestid == kXR_open) ||
       (req->header.requestid == kXR_login))  return kSEHRReturnNoMsgToCaller;

   // Here we are. If we had a filehandle then we must
   //  request a new one.
   char localfhandle[4];
   bool wasopen;

   if (fRedirHandler &&
      (fRedirHandler->OpenFileWhenRedirected(localfhandle, wasopen) && wasopen)) {
      // We are here if the file has been opened succesfully
      // or if it was not open
      // Tricky thing: now we have a new filehandle, perhaps in
      // a different server. Then we must correct the filehandle in
      // the msg we were sending and that we must repeat...
      PutFilehandleInRequest(req, localfhandle);
    
      // Everything should be ok here.
      // If we have been redirected,then we are connected, logged and reopened
      // the file. If we had a r/w error (xmsg==0 or xmsg->IsError) we are
      // OK too. Since we may come from a comm error, then xmsg can be null.
      if (xmsg && !xmsg->IsError())
         return kSEHRContinue; // the case of explicit redir
      else
         return kSEHRReturnNoMsgToCaller; // the case of recovered error
   }

   // We are here if we had no fRedirHandler or the reopen failed
   // If we have no fRedirHandler then treat it like an OK
   if (!fRedirHandler) {
      // Since we may come from a comm error, then xmsg can be null.
      //if (xmsg) xmsg->SetHeaderStatus( kXR_ok );
      if (xmsg && !xmsg->IsError())
         return kSEHRContinue; // the case of explicit redir
      else
         return kSEHRReturnNoMsgToCaller; // the case of recovered error
   }

   // We are here if we have been unable to connect somewhere to handle the
   //  troubled situation
   return kSEHRContinue;
}

//_____________________________________________________________________________
XReqErrorType XrdClientConn::GoToAnotherServer(XrdClientUrlInfo newdest)
{
   // Re-directs to another server
   
   
   if ( (fLogConnID = Connect( newdest )) == -1) {
	  
      // Note: if Connect is unable to work then we are in trouble.
      // It seems that we have been redirected to a non working server
      Error("GoToAnotherServer", "Error connecting to [" <<  
            newdest.Host << ":" <<  newdest.Port);
      
      // If no conn is possible then we return to the load balancer
      return kREDIRCONNECT;
   }
   
   //
   // Set fUrl to the new data/lb server if the 
   // connection has been succesfull
   //
   fUrl.TakeUrl(newdest.GetUrl());

   if (IsConnected() && !GetAccessToSrv()) {
      Error("GoToAnotherServer", "Error handshaking to [" << 
            newdest.Host.c_str() << ":" <<  newdest.Port << "]");
      return kREDIRCONNECT;
   }
   return kOK;
}

//_____________________________________________________________________________
XrdClientString XrdClientConn::GetDomainToMatch(XrdClientString hostname) {
   // Return net-domain of host hostname in 's'.
   // If the host is unknown in the DNS world but it's a
   //  valid inet address, then that address is returned, in order
   //  to be matched later for access granting

   char *fullname;
   XrdClientString res;

   // Let's look up the hostname
   // It may also be a w.x.y.z type address.
   fullname = XrdNetDNS::getHostName((char *)hostname.c_str(), 0);
   
   if (fullname) {
      // The looked up address is valid
      // The hostname domain can still be unknown
     
      Info(XrdClientDebug::kHIDEBUG,
	   "GetDomainToMatch", "GetHostName(" << hostname <<
	   ") returned name=" << fullname);

      res = ParseDomainFromHostname(fullname);

      if (res == "") {
	 Info(XrdClientDebug::kHIDEBUG,
	   "GetDomainToMatch", "No domain contained in " << fullname);

	 res = ParseDomainFromHostname(hostname);
      }
      if (res == "") {
	 Info(XrdClientDebug::kHIDEBUG,
	   "GetDomainToMatch", "No domain contained in " << hostname);

	 res = hostname;
      }

      free(fullname);

   } else {

      Info(XrdClientDebug::kHIDEBUG,
	   "GetDomainToMatch", "GetHostName(" << hostname << ") returned a non valid address.");

      res = ParseDomainFromHostname(hostname);
   }

   Info(XrdClientDebug::kHIDEBUG,
	"GetDomainToMatch", "GetDomain(" << hostname << ") --> " << res);
   
   return res;
}

//_____________________________________________________________________________
XrdClientString XrdClientConn::ParseDomainFromHostname(XrdClientString hostname) {

   XrdClientString res;
   int pos;

   res = hostname;

   // Isolate domain
   pos = res.Find((char *)".");

   if (pos == STR_NPOS)
      res = "";
   else
      res.EraseFromStart(pos+1);

   return res;
}


//_____________________________________________________________________________
void XrdClientConn::CheckPort(int &port) {

   if(port <= 0) {

      Info(XrdClientDebug::kHIDEBUG,
	   "checkPort", 
	   "TCP port not specified. Trying to get it from /etc/services...");

      struct servent *S = getservbyname("rootd", "tcp");
      if(!S) {

	    Info(XrdClientDebug::kHIDEBUG,
		 "checkPort", "Service rootd not specified in /etc/services. "
		 "Using default IANA tcp port 1094");
	 port = 1094;
      } else {
	 Info(XrdClientDebug::kNODEBUG,
	      "checkPort", "Found tcp port " << ntohs(S->s_port) <<
	      " in /etc/service");

	 port = (int)ntohs(S->s_port);
      }

   }
}


//___________________________________________________________________________
bool XrdClientConn::GetDataFromCache(const void *buffer, long long begin_offs,
				   long long end_offs, bool PerfCalc)
{
   // Copies the requested data from the cache. False if not possible.
   // Perfcalc = kFALSE forces the call not to impact the perf counters

   if (!fMainReadCache)
      return FALSE;

   return (fMainReadCache->GetDataIfPresent(buffer,
					    begin_offs,
					    end_offs,
					    PerfCalc));
}
