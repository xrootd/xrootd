//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientPhyConnection                                               //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Class handling physical connections to xrootd servers                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include <time.h>
#include <stdlib.h>
#include "XrdClient/XrdClientPhyConnection.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientMessage.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdClient/XrdClientSid.hh"
#include "XrdSec/XrdSecInterface.hh"
#ifndef WIN32
#include <sys/socket.h>
#else
#include <Winsock2.h>
#endif

//____________________________________________________________________________
void *SocketReaderThread(void * arg, XrdClientThread *thr)
{
   // This thread is the base for the async capabilities of TXPhyConnection
   // It repeatedly keeps reading from the socket, while feeding the
   // MsqQ with a stream of TXMessages containing what's happening
   // at the socket level

   XrdClientPhyConnection *thisObj;

   Info(XrdClientDebug::kHIDEBUG,
	"SocketReaderThread",
	"Reader Thread starting.");

   thr->SetCancelDeferred();
   thr->SetCancelOn();

   thisObj = (XrdClientPhyConnection *)arg;

   thisObj->StartedReader();

   while (1) {
     thisObj->BuildMessage(TRUE, TRUE);

     if (thisObj->CheckAutoTerm())
	break;
   }

   Info(XrdClientDebug::kHIDEBUG,
        "SocketReaderThread",
        "Reader Thread exiting.");

   return 0;
}

//____________________________________________________________________________
XrdClientPhyConnection::XrdClientPhyConnection(XrdClientAbsUnsolMsgHandler *h):
   fReaderCV(0) {

   // Constructor
   fServerType = kUnknown;

   Touch();

   fServer.Clear();

   SetLogged(kNo);

   fRequestTimeout = EnvGetLong(NAME_REQUESTTIMEOUT);

   UnsolicitedMsgHandler = h;

   fReaderthreadhandler = 0;
   fReaderthreadrunning = FALSE;

   fSecProtocol = 0;
}

//____________________________________________________________________________
XrdClientPhyConnection::~XrdClientPhyConnection()
{
   // Destructor
  Info(XrdClientDebug::kHIDEBUG,
       "XrdClientPhyConnection",
       "Destroying. [" << fServer.Host << ":" << fServer.Port << "]");

   Disconnect();

   if (fSocket) {
      delete fSocket;
      fSocket = 0;
   }

   UnlockChannel();

   if (fReaderthreadrunning)
      fReaderthreadhandler->Cancel();

   if (fSecProtocol) {
      // This insures that the right destructor is called
      // (Do not do C++ delete).
      fSecProtocol->Delete();
      fSecProtocol = 0;
   }
}

//____________________________________________________________________________
bool XrdClientPhyConnection::Connect(XrdClientUrlInfo RemoteHost, bool isUnix)
{
   // Connect to remote server
   XrdOucMutexHelper l(fMutex);

   if (isUnix) {
      Info(XrdClientDebug::kHIDEBUG, "Connect", "Connecting to " << RemoteHost.File);
   } else {
      Info(XrdClientDebug::kHIDEBUG,
      "Connect", "Connecting to [" << RemoteHost.Host << ":" <<	RemoteHost.Port << "]");
   } 
   fSocket = new XrdClientSock(RemoteHost);

   if(!fSocket) {
      Error("Connect","Unable to create a client socket. Aborting.");
      abort();
   }

   fSocket->TryConnect(isUnix);

   if (!fSocket->IsConnected()) {
      if (isUnix) {
         Error("Connect", "can't open UNIX connection to " << RemoteHost.File);
      } else {
         Error("Connect", "can't open connection to [" <<
               RemoteHost.Host << ":" << RemoteHost.Port << "]");
      }
      Disconnect();

      return FALSE;
   }

   Touch();

   fTTLsec = DATA_TTL;

   if (isUnix) {
      Info(XrdClientDebug::kHIDEBUG, "Connect", "Connected to " << RemoteHost.File);
   } else {
      Info(XrdClientDebug::kHIDEBUG, "Connect", "Connected to [" <<
           RemoteHost.Host << ":" << RemoteHost.Port << "]");
   }

   fServer = RemoteHost;
   fReaderthreadrunning = FALSE;

   return TRUE;
}

//____________________________________________________________________________
void XrdClientPhyConnection::StartReader() {
   bool running;

   {
      XrdOucMutexHelper l(fMutex);
      running = fReaderthreadrunning;
   }
   // Start reader thread

   // Parametric asynchronous stuff.
   // If we are going Sync, then nothing has to be done,
   // otherwise the reader thread must be started
   if ( !running && EnvGetLong(NAME_GOASYNC) ) {

      Info(XrdClientDebug::kHIDEBUG,
	   "StartReader", "Starting reader thread...");

      // Now we launch  the reader thread
      fReaderthreadhandler = new XrdClientThread(SocketReaderThread);
      if (!fReaderthreadhandler) {
	 Error("PhyConnection",
	       "Can't create reader thread: out of system resources");
// HELP: what do we do here
         std::exit(-1);
      }

      if (fReaderthreadhandler->Run(this)) {
         Error("PhyConnection",
               "Can't run reader thread: out of system resources. Critical error.");
// HELP: what do we do here
         std::exit(-1);
      }

      if (fReaderthreadhandler->Detach())
      {
	 Error("PhyConnection", "Thread detach failed");
	 return;
      }
      
      // sleep until the detached thread starts running, which hopefully
      // is not forever.
      int maxRetries = 10;
      while (--maxRetries >= 0 && !fReaderthreadrunning)
      {
	 fReaderCV.Wait(100);
      }
   }
}


//____________________________________________________________________________
void XrdClientPhyConnection::StartedReader() {
   XrdOucMutexHelper l(fMutex);
   fReaderthreadrunning = TRUE;
   fReaderCV.Post();
}

//____________________________________________________________________________
bool XrdClientPhyConnection::ReConnect(XrdClientUrlInfo RemoteHost)
{
   // Re-connection attempt

   Disconnect();
   return Connect(RemoteHost);
}

//____________________________________________________________________________
void XrdClientPhyConnection::Disconnect()
{
   XrdOucMutexHelper l(fMutex);

   // Disconnect from remote server

   if (fSocket) {
      Info(XrdClientDebug::kHIDEBUG,
	   "PhyConnection", "Disconnecting socket...");
      fSocket->Disconnect();
   }

   // We do not destroy the socket here. The socket will be destroyed
   // in CheckAutoTerm or in the ConnMgr
}


//____________________________________________________________________________
bool XrdClientPhyConnection::CheckAutoTerm() {
   bool doexit = FALSE;

  {
   XrdOucMutexHelper l(fMutex);

   // Parametric asynchronous stuff
   // If we are going async, we might be willing to term ourself
   if (!IsValid() && EnvGetLong(NAME_GOASYNC)) {

         Info(XrdClientDebug::kHIDEBUG,
              "CheckAutoTerm", "Self-Cancelling reader thread.");


         fReaderthreadrunning = FALSE;
         fReaderthreadhandler = 0;

         //delete fSocket;
         //fSocket = 0;

         doexit = TRUE;
      }

  }


  if (doexit) {
	UnlockChannel();
        return true;
   }

  return false;
}


//____________________________________________________________________________
void XrdClientPhyConnection::Touch()
{
   // Set last-use-time to present time
   XrdOucMutexHelper l(fMutex);

   time_t t = time(0);

   Info(XrdClientDebug::kDUMPDEBUG,
      "Touch",
      "Setting last use to current time" << t);

   fLastUseTimestamp = t;
}

//____________________________________________________________________________
int XrdClientPhyConnection::ReadRaw(void *buf, int len) {
   // Receive 'len' bytes from the connected server and store them in 'buf'.
   // Return 0 if OK. 

   int res;


   Touch();

   if (IsValid()) {

      Info(XrdClientDebug::kDUMPDEBUG,
	   "ReadRaw",
	   "Reading from " <<
	   fServer.Host << ":" << fServer.Port);

      res = fSocket->RecvRaw(buf, len);

      if ((res < 0) && (res != TXSOCK_ERR_TIMEOUT) && errno ) {
	 //strerror_r(errno, errbuf, sizeof(buf));

         Info(XrdClientDebug::kHIDEBUG,
	      "ReadRaw", "Read error on " <<
	      fServer.Host << ":" << fServer.Port << ". errno=" << errno );
      }

      // If a socket error comes, then we disconnect
      // but we have not to disconnect in the case of a timeout
      if (((res < 0) && (res != TXSOCK_ERR_TIMEOUT)) ||
          (!fSocket->IsConnected())) {

	 Info(XrdClientDebug::kHIDEBUG,
	      "ReadRaw", 
	      "Disconnection reported on" <<
	      fServer.Host << ":" << fServer.Port);

         Disconnect();

      }

      Touch();

      // Let's dump the received bytes
      if ((res > 0) && (DebugLevel() > XrdClientDebug::kDUMPDEBUG)) {
	  XrdOucString s = "   ";
	  char b[256]; 

	  for (int i = 0; i < xrdmin(res, 256); i++) {
	      sprintf(b, "%.2x ", *((unsigned char *)buf + i));
	      s += b;
	      if (!((i + 1) % 16)) s += "\n   ";
	  }

	  Info(XrdClientDebug::kHIDEBUG,
	       "ReadRaw", "Read " << res <<  "bytes. Dump:" << endl << s );

      }

      return res;
   }
   else {
      // Socket already destroyed or disconnected
      Info(XrdClientDebug::kUSERDEBUG,
	   "ReadRaw", "Socket is disconnected.");

      return TXSOCK_ERR;
   }

}

//____________________________________________________________________________
XrdClientMessage *XrdClientPhyConnection::ReadMessage(int streamid) {
   // Gets a full loaded XrdClientMessage from this phyconn.
   // May be a pure msg pick from a queue

   Touch();
   return fMsgQ.GetMsg(streamid, fRequestTimeout );

 }

//____________________________________________________________________________
XrdClientMessage *XrdClientPhyConnection::BuildMessage(bool IgnoreTimeouts, bool Enqueue)
{
   // Builds an XrdClientMessage, and makes it read its header/data from the socket
   // Also put automatically the msg into the queue

   XrdClientMessage *m;
   bool parallelsid;

   m = new XrdClientMessage();
   if (!m) {
      Error("BuildMessage",
	    "Cannot create a new Message. Aborting.");
      abort();
   }

   m->ReadRaw(this);

   parallelsid = SidManager->GetSidInfo(m->HeaderSID());

   if ( parallelsid || (m->IsAttn()) ) {
      UnsolRespProcResult res;

      // Here we insert the PhyConn-level support for unsolicited responses
      // Some of them will be propagated in some way to the upper levels
      // The path should be
      //  here -> XrdClientConnMgr -> all the involved XrdClientLogConnections ->
      //   -> all the corresponding XrdClient

      Info(XrdClientDebug::kDUMPDEBUG,
          "BuildMessage"," propagating unsol id " << m->HeaderSID());


      res = HandleUnsolicited(m);

      // The purpose of this message ends here
      if ( (parallelsid) && (res != kUNSOL_KEEP) )
	 SidManager->ReleaseSid(m->HeaderSID());

      delete m;
      m = 0;
   }
   else
      if (Enqueue) {
         // If we have to ignore the socket timeouts, then we have not to
         // feed the queue with them. In this case, the newly created XrdClientMessage
         // has to be freed.
	 //if ( !IgnoreTimeouts || !m->IsError() )

         //bool waserror;

         if (IgnoreTimeouts) {

            if (m->GetStatusCode() != XrdClientMessage::kXrdMSC_timeout) {
               //waserror = m->IsError();

            Info(XrdClientDebug::kDUMPDEBUG,
                 "BuildMessage"," posting id "<<m->HeaderSID());

               fMsgQ.PutMsg(m);

               //if (waserror)
               //   for (int kk=0; kk < 10; kk++) fMsgQ.PutMsg(0);
            }
            else {

            Info(XrdClientDebug::kDUMPDEBUG,
                 "BuildMessage"," deleting id "<<m->HeaderSID());

               delete m;
               m = 0;
            }

         } else
            fMsgQ.PutMsg(m);
      }
  
   return m;
}

//____________________________________________________________________________
UnsolRespProcResult XrdClientPhyConnection::HandleUnsolicited(XrdClientMessage *m)
{
   // Local processing of unsolicited responses is done here

   bool ProcessingToGo = TRUE;
   struct ServerResponseBody_Attn *attnbody;

   Touch();

   // Local pre-processing of the unsolicited XrdClientMessage
   attnbody = (struct ServerResponseBody_Attn *)m->GetData();

   if (attnbody && (m->IsAttn())) {
      attnbody->actnum = ntohl(attnbody->actnum);

      switch (attnbody->actnum) {
      case kXR_asyncms:
         // A message arrived from the server. Let's print it.
         Info(XrdClientDebug::kNODEBUG,
	      "HandleUnsolicited",
              "Message from " <<
	      fServer.Host << ":" << fServer.Port << ". '" <<
              attnbody->parms << "'");

         ProcessingToGo = FALSE;
         break;

      case kXR_asyncab:
	 // The server requested to abort the execution!!!!
         Info(XrdClientDebug::kNODEBUG,
	      "HandleUnsolicited",
              "******* Abort request received ******* Server: " <<
	      fServer.Host << ":" << fServer.Port << ". Msg: '" <<
              attnbody->parms << "'");
	 
	 exit(255);

         ProcessingToGo = FALSE;
         break;
      }
   }

   // Now we propagate the message to the interested object, if any
   // It could be some sort of upper layer of the architecture
   if (ProcessingToGo) {
      UnsolRespProcResult retval;

      retval = SendUnsolicitedMsg(this, m);

      // Request post-processing
      if (attnbody && (m->IsAttn())) {
         switch (attnbody->actnum) {

         case kXR_asyncrd:
	    // After having set all the belonging object, we disconnect.
	    // The next commands will redirect-on-error where we want

	    Disconnect();
	    break;
      
         case kXR_asyncdi:
	    // After having set all the belonging object, we disconnect.
	    // The next connection attempt will behave as requested,
	    // i.e. waiting some time before reconnecting

            Disconnect();
	    break;

         } // switch
      }
      return retval;

   }
   else 
      return kUNSOL_CONTINUE;
}

//____________________________________________________________________________
int XrdClientPhyConnection::WriteRaw(const void *buf, int len)
{
   // Send 'len' bytes located at 'buf' to the connected server.
   // Return number of bytes sent. 

   int res;

   Touch();

   if (IsValid()) {

      Info(XrdClientDebug::kDUMPDEBUG,
	   "WriteRaw",
	   "Writing to" <<
	   XrdClientDebug::kDUMPDEBUG);
    
      res = fSocket->SendRaw(buf, len);

      if ((res < 0)  && (res != TXSOCK_ERR_TIMEOUT) && errno) {
	 //strerror_r(errno, errbuf, sizeof(buf));

         Info(XrdClientDebug::kHIDEBUG,
	      "WriteRaw", "Write error on " <<
	      fServer.Host << ":" << fServer.Port << ". errno=" << errno );

      }

      // If a socket error comes, then we disconnect (and destroy the fSocket)
      if ((res < 0) || (!fSocket) || (!fSocket->IsConnected())) {

	 Info(XrdClientDebug::kHIDEBUG,
	      "WriteRaw", 
	      "Disconnection reported on" <<
	      fServer.Host << ":" << fServer.Port);

         Disconnect();
      }

      Touch();
      return( res );
   }
   else {
      // Socket already destroyed or disconnected
      Info(XrdClientDebug::kUSERDEBUG,
	   "WriteRaw",
	   "Socket is disconnected.");
      return TXSOCK_ERR;
   }
}


//____________________________________________________________________________
bool XrdClientPhyConnection::ExpiredTTL()
{
   // Check expiration time
   return( (time(0) - fLastUseTimestamp) > fTTLsec );
}

//____________________________________________________________________________
void XrdClientPhyConnection::LockChannel()
{
   // Lock 
   fRwMutex.Lock();
}

//____________________________________________________________________________
void XrdClientPhyConnection::UnlockChannel()
{
   // Unlock
   fRwMutex.UnLock();
}
