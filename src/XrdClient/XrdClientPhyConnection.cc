//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdPhyConnection                                                     //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Class handling physical connections to xrootd servers                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include <time.h>
#include <string>
#include <stdlib.h>
#include "XrdPhyConnection.hh"
#include "XrdDebug.hh"
#include "XrdMessage.hh"

#include <sys/socket.h>


//____________________________________________________________________________
void *SocketReaderThread(void * arg)
{
   // This thread is the base for the async capabilities of TXPhyConnection
   // It repeatedly keeps reading from the socket, while feeding the
   // MsqQ with a stream of TXMessages containing what's happening
   // at the socket level

   XrdPhyConnection *thisObj;

   Info(XrdDebug::kHIDEBUG,
	"SocketReaderThread",
	"Reader Thread starting.");


   pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);

   thisObj = (XrdPhyConnection *)arg;

   while (1) {

      thisObj->BuildMessage(TRUE, TRUE);

   }

   pthread_exit(0);
   return 0;
}

//____________________________________________________________________________
XrdPhyConnection::XrdPhyConnection(XrdAbsUnsolicitedMsgHandler *h) {
   // Constructor
   pthread_mutexattr_t attr;
   int rc;

   // Initialization of lock mutex
   rc = pthread_mutexattr_init(&attr);
   if (rc == 0) {
      rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      if (rc == 0)
	 rc = pthread_mutex_init(&fRwMutex, &attr);
   }
   if (rc) {
      Error("PhyConnection", 
            "Can't create mutex: out of system resources.");
      abort();
   }

   Touch();

   fServer.Clear();

   SetLogged(kNo);

   fRequestTimeout = DFLT_REQUESTTIMEOUT;

   UnsolicitedMsgHandler = h;

   fReaderthreadhandler = 0;
   fReaderthreadrunning = FALSE;
}

//____________________________________________________________________________
XrdPhyConnection::~XrdPhyConnection()
{
   // Destructor

   Disconnect();

   pthread_mutex_destroy(&fRwMutex);

}

//____________________________________________________________________________
bool XrdPhyConnection::Connect(XrdUrlInfo RemoteHost)
{
   // Connect to remote server

   Info(XrdDebug::kHIDEBUG,
	"Connect",
	"Connecting to [" << RemoteHost.Host << ":" <<	RemoteHost.Port << "]");
  
   fSocket = new XrdClientSock(RemoteHost);

   if(!fSocket) {
      Error("Connect","Unable to create a client socket. Aborting.");
      abort();
   }

   fSocket->TryConnect();

   if (!fSocket->IsConnected()) {
      Error("Connect", 
            "can't open connection to [" <<
	    RemoteHost.Host << ":" <<	RemoteHost.Port << "]");

      Disconnect();

      return FALSE;
   }

   Touch();

   fTTLsec = DATA_TTL;

   Info(XrdDebug::kHIDEBUG, "Connect", "Connected to [" <<
	RemoteHost.Host << ":" << RemoteHost.Port << "]");

   fServer = RemoteHost;
   fReaderthreadrunning = FALSE;

   return TRUE;
}

//____________________________________________________________________________
void XrdPhyConnection::StartReader() {
   int pt_ret;

   // Start reader thread

   // Parametric asynchronous stuff.
   // If we are going Sync, then nothing has to be done,
   // otherwise the reader thread must be started
   if ( !fReaderthreadrunning && DFLT_GOASYNC ) {

      Info(XrdDebug::kHIDEBUG,
	   "StartReader", "Starting reader thread...");

      // Now we launch  the reader thread
      pt_ret = pthread_create(&fReaderthreadhandler, NULL, SocketReaderThread, this);
      if (pt_ret)
	 Error("PhyConnection",
	       "Can't create reader thread: out of system resources");

      fReaderthreadrunning = TRUE;

   }
}

//____________________________________________________________________________
bool XrdPhyConnection::ReConnect(XrdUrlInfo RemoteHost)
{
   // Re-connection attempt

   Disconnect();
   return Connect(RemoteHost);
}

//____________________________________________________________________________
void XrdPhyConnection::Disconnect()
{

   // Parametric asynchronous stuff
   // If we are going async, we have to terminate the reader thread
   if (DFLT_GOASYNC) {

      if (fReaderthreadrunning) {

         Info(XrdDebug::kHIDEBUG,
	      "Disconnect", "Cancelling reader thread.");

	 pthread_cancel(fReaderthreadhandler);

	 Info(XrdDebug::kHIDEBUG,
	      "Disconnect", "Waiting for the reader thread termination...");
      
	 pthread_join(fReaderthreadhandler, 0);

	 Info(XrdDebug::kHIDEBUG,
	      "Disconnect", "Reader thread canceled.");

      }

      fReaderthreadrunning = FALSE;
      fReaderthreadhandler = 0;
   }

   // Disconnect from remote server
   Info(XrdDebug::kDUMPDEBUG,
	"Disconnect", "Deleting low level socket...");

   SafeDelete(fSocket);
   fSocket = 0;

}

//____________________________________________________________________________
void XrdPhyConnection::Touch()
{
   // Set last-use-time to present time

   time_t t = time(0);

   Info(XrdDebug::kDUMPDEBUG,
	"Touch",
	"Setting last use to current time" << t);

   fLastUseTimestamp = t;
}

//____________________________________________________________________________
int XrdPhyConnection::ReadRaw(void *buf, int len) {
   // Receive 'len' bytes from the connected server and store them in 'buf'.
   // Return number of bytes received. 

   int res;
   char errbuf[1024];

   Touch();

   if (IsValid()) {

      Info(XrdDebug::kDUMPDEBUG,
	   "ReadRaw",
	   "Reading from " <<
	   fServer.Host << ":" << fServer.Port);

      res = fSocket->RecvRaw(buf, len);

      if (res && (res != TXSOCK_ERR_TIMEOUT) && errno ) {
	 strerror_r(errno, errbuf, sizeof(buf));

         Info(XrdDebug::kHIDEBUG,
	      "ReadRaw", "Read error on " <<
	      fServer.Host << ":" << fServer.Port << ". errno=" << errno <<
	      ":" << buf );
      }

      // If a socket error comes, then we disconnect (and destroy the fSocket)
      // but we have not to disconnect in the case of a timeout
      if ((res && (res != TXSOCK_ERR_TIMEOUT)) ||
          (!fSocket->IsConnected())) {

	 Info(XrdDebug::kHIDEBUG,
	      "ReadRaw", 
	      "Disconnection reported on" <<
	      fServer.Host << ":" << fServer.Port);

         Disconnect();

      }

      Touch();

      return res;
   }
   else {
      // Socket already destroyed or disconnected
      Info(XrdDebug::kUSERDEBUG,
	   "ReadRaw", "Socket is disconnected.");

      return TXSOCK_ERR;
   }

}

//____________________________________________________________________________
XrdMessage *XrdPhyConnection::ReadMessage(int streamid) {
   // Gets a full loaded XrdMessage from this phyconn.
   // May be a pure msg pick from a queue

   Touch();
   return fMsgQ.GetMsg(streamid, fRequestTimeout );

 }

//____________________________________________________________________________
XrdMessage *XrdPhyConnection::BuildMessage(bool IgnoreTimeouts, bool Enqueue)
{
   // Builds an XrdMessage, and makes it read its header/data from the socket
   // Also put automatically the msg into the queue

   XrdMessage *m;

   m = new XrdMessage();
   if (!m) {
      Error("BuildMessage",
	    "Cannot create a new Message. Aborting.");
      abort();
   }

   m->ReadRaw(this);

   if (m->IsAttn()) {

      // Here we insert the PhyConn-level support for unsolicited responses
      // Some of them will be propagated in some way to the upper levels
      //  TLogConn, TConnMgr, TXNetConn
      HandleUnsolicited(m);

      // The purpose of this message ends here
      delete m;
      m = 0;
   }
   else
      if (Enqueue) {
         // If we have to ignore the socket timeouts, then we have not to
         // feed the queue with them. In this case, the newly created XrdMessage
         // has to be freed.
	 if ( !IgnoreTimeouts || !m->IsError() )
	    fMsgQ.PutMsg(m);
	 else {
            delete m;
            m = 0;
	 }
      }
  
   return m;
}

//____________________________________________________________________________
void XrdPhyConnection::HandleUnsolicited(XrdMessage *m)
{
   // Local processing of unsolicited responses is done here

   bool ProcessingToGo = TRUE;
   struct ServerResponseBody_Attn *attnbody;

   Touch();

   // Local processing of the unsolicited XrdMessage
   attnbody = (struct ServerResponseBody_Attn *)m->GetData();

   if (attnbody) {
    
      switch (attnbody->actnum) {
      case kXR_asyncms:
         // A message arrived from the server. Let's print it.
         Info(XrdDebug::kNODEBUG,
	      "HandleUnsolicited",
              "Message from " <<
	      fServer.Host << ":" << fServer.Port << ". '" <<
              attnbody->parms << "'");

         ProcessingToGo = FALSE;
         break;
      }
   }

   // Now we propagate the message to the interested object, if any
   // It could be some sort of upper layer of the architecture
   if (ProcessingToGo)
      SendUnsolicitedMsg(this, m);
}

//____________________________________________________________________________
int XrdPhyConnection::WriteRaw(const void *buf, int len)
{
   // Send 'len' bytes located at 'buf' to the connected server.
   // Return number of bytes sent. 
   char errbuf[1024];
   int res;

   Touch();

   if (IsValid()) {

      Info(XrdDebug::kDUMPDEBUG,
	   "WriteRaw",
	   "Writing to" <<
	   XrdDebug::kDUMPDEBUG);
    
      res = fSocket->SendRaw(buf, len);

      if ((res < 0)  && (res != TXSOCK_ERR_TIMEOUT) && errno) {
	 strerror_r(errno, errbuf, sizeof(buf));

         Info(XrdDebug::kHIDEBUG,
	      "WriteRaw", "Write error on " <<
	      fServer.Host << ":" << fServer.Port << ". errno=" << errno <<
	      ":" << buf );

      }

      // If a socket error comes, then we disconnect (and destroy the fSocket)
      if ((res < 0) || (!fSocket->IsConnected())) {

	 Info(XrdDebug::kHIDEBUG,
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
      Info(XrdDebug::kUSERDEBUG,
	   "WriteRaw",
	   "Socket is disconnected.");
      return TXSOCK_ERR;
   }
}


//____________________________________________________________________________
bool XrdPhyConnection::ExpiredTTL()
{
   // Check expiration time
   return( (time(0) - fLastUseTimestamp) > fTTLsec );
}

//____________________________________________________________________________
void XrdPhyConnection::LockChannel()
{
   // Lock 
   pthread_mutex_lock(&fRwMutex);
}

//____________________________________________________________________________
void XrdPhyConnection::UnlockChannel()
{
   // Unlock
   pthread_mutex_unlock(&fRwMutex);
}
