/******************************************************************************/
/*                                                                            */
/*           X r d C l i e n t P h y C o n n e c t i o n . c c                */
/*                                                                            */
/* Author: Fabrizio Furano (INFN Padova, 2004)                                */
/* Adapted from TXNetFile (root.cern.ch) originally done by                   */
/*  Alvise Dorigo, Fabrizio Furano                                            */
/*          INFN Padova, 2003                                                 */
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
/******************************************************************************/

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Class handling physical connections to xrootd servers                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include <time.h>
#include <stdlib.h>
#include "XrdClient/XrdClientPhyConnection.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientMessage.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdClient/XrdClientSid.hh"
#include "XrdClient/XrdClientPSock.hh"
#include "XrdClient/XrdClientThread.hh"
#include "XrdSec/XrdSecInterface.hh"
#ifndef WIN32
#include <sys/socket.h>
#else
#include <Winsock2.h>
#endif


#define READERCOUNT (xrdmin(50, EnvGetLong(NAME_MULTISTREAMCNT)+1))

//____________________________________________________________________________
void *SocketReaderThread(void * arg, XrdClientThread *thr)
{
   // This thread is the base for the async capabilities of XrdClientPhyConnection
   // It repeatedly keeps reading from the socket, while feeding the
   // MsqQ with a stream of XrdClientMessages containing what's happening
   // at the socket level

   // Mask all allowed signals
   if (thr->MaskSignal(0) != 0)
      Error("SocketReaderThread", "Warning: problems masking signals");

   XrdClientPhyConnection *thisObj;


   Info(XrdClientDebug::kHIDEBUG,
	"SocketReaderThread",
	"Reader Thread starting.");

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
XrdClientPhyConnection::XrdClientPhyConnection(XrdClientAbsUnsolMsgHandler *h,
					       XrdClientSid *sid):
    fMStreamsGoing(false), fReaderCV(0), fLogConnCnt(0), fSidManager(sid),
    fServerProto(0) {

   // Constructor
   fServerType = kSTNone;

   // Immediate destruction of this object is always a bad idea
   fTTLsec = 30;

   Touch();

   fServer.Clear();

   SetLogged(kNo);

   fRequestTimeout = EnvGetLong(NAME_REQUESTTIMEOUT);

   UnsolicitedMsgHandler = h;

   for (int i = 0; i < READERCOUNT; i++)
     fReaderthreadhandler[i] = 0;
   fReaderthreadrunning = 0;

   fSecProtocol = 0;
}

//____________________________________________________________________________
XrdClientPhyConnection::~XrdClientPhyConnection()
{
   // Destructor
  Info(XrdClientDebug::kUSERDEBUG,
       "XrdClientPhyConnection",
       "Destroying. [" << fServer.Host << ":" << fServer.Port << "]");

   Disconnect();
   for (int i = 0; i < READERCOUNT; i++)
   {
     if(fReaderthreadhandler[i])
     {
       fReaderthreadhandler[i]->Join();
       delete fReaderthreadhandler[i];
     }
   }



     if (fSocket) {
        delete fSocket;
        fSocket = 0;
   }

   UnlockChannel();


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
   return Connect( RemoteHost, isUnix, -1 );
}

bool XrdClientPhyConnection::Connect(XrdClientUrlInfo RemoteHost, bool isUnix, int fd)
{
   // Connect to remote server
   XrdSysMutexHelper l(fMutex);


   if (isUnix) {
      Info(XrdClientDebug::kHIDEBUG, "Connect", "Connecting to " << RemoteHost.File);
   } else {
      Info(XrdClientDebug::kHIDEBUG,
      "Connect", "Connecting to [" << RemoteHost.Host << ":" <<	RemoteHost.Port << "]");
   } 

   if (EnvGetLong(NAME_MULTISTREAMCNT))
       fSocket = new XrdClientPSock(RemoteHost);
   else
       fSocket = new XrdClientSock(RemoteHost, 0, fd );

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

   fTTLsec = EnvGetLong(NAME_DATASERVERCONN_TTL);

   if (isUnix) {
      Info(XrdClientDebug::kHIDEBUG, "Connect", "Connected to " << RemoteHost.File);
   } else {
      Info(XrdClientDebug::kHIDEBUG, "Connect", "Connected to [" <<
           RemoteHost.Host << ":" << RemoteHost.Port << "]");
   }

   fServer = RemoteHost;

   {
      XrdSysMutexHelper l(fMutex);
      fReaderthreadrunning = 0;
   }

   return TRUE;
}

//____________________________________________________________________________
void XrdClientPhyConnection::StartReader() {
   bool running;

   {
      XrdSysMutexHelper l(fMutex);
      running = fReaderthreadrunning;
   }
   // Start reader thread

   // Parametric asynchronous stuff.
   // If we are going Sync, then nothing has to be done,
   // otherwise the reader thread must be started
   if ( !running ) {

      Info(XrdClientDebug::kHIDEBUG,
	   "StartReader", "Starting reader thread...");

      int rdcnt = READERCOUNT;
      if (fServerType == kSTBaseXrootd) rdcnt = 1;

      for (int i = 0; i < rdcnt; i++) {

      // Now we launch  the reader thread
      fReaderthreadhandler[i] = new XrdClientThread(SocketReaderThread);
      if (!fReaderthreadhandler[i]) {
	 Error("PhyConnection",
	       "Can't create reader thread: out of system resources");
// HELP: what do we do here
         exit(-1);
      }

      if (fReaderthreadhandler[i]->Run(this)) {
         Error("PhyConnection",
               "Can't run reader thread: out of system resources. Critical error.");
// HELP: what do we do here
         exit(-1);
      }

      }
      // sleep until at least one thread starts running, which hopefully
      // is not forever.
      int maxRetries = 10;
      while (--maxRetries >= 0) {
         {  XrdSysMutexHelper l(fMutex);
            if (fReaderthreadrunning)
               break;
         }
         fReaderCV.Wait(100);
      }
   }
}


//____________________________________________________________________________
void XrdClientPhyConnection::StartedReader() {
   XrdSysMutexHelper l(fMutex);
   fReaderthreadrunning++;
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
   XrdSysMutexHelper l(fMutex);

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
   XrdSysMutexHelper l(fMutex);

   // Parametric asynchronous stuff
   // If we are going async, we might be willing to term ourself
   if ( !IsValid() ) {

         Info(XrdClientDebug::kHIDEBUG,
              "CheckAutoTerm", "Self-Cancelling reader thread.");

         fReaderthreadrunning--;
         return true;
   }
  return false;
}


//____________________________________________________________________________
void XrdClientPhyConnection::Touch()
{
   // Set last-use-time to present time
   XrdSysMutexHelper l(fMutex);

   time_t t = time(0);

   //Info(XrdClientDebug::kDUMPDEBUG,
   //   "Touch",
   //   "Setting last use to current time" << t);

   fLastUseTimestamp = t;
}

//____________________________________________________________________________
int XrdClientPhyConnection::ReadRaw(void *buf, int len, int substreamid,
			   int *usedsubstreamid) {
   // Receive 'len' bytes from the connected server and store them in 'buf'.
   // Return 0 if OK. 
   // If substreamid = -1 then
   //  gets length bytes from any par socket, and returns the usedsubstreamid
   //   where it got the bytes from
   // Otherwise read bytes from the specified substream. 0 is the main one.

   int res;


   if (IsValid()) {

      Info(XrdClientDebug::kDUMPDEBUG,
	   "ReadRaw",
	   "Reading from " <<
	   fServer.Host << ":" << fServer.Port);

      res = fSocket->RecvRaw(buf, len, substreamid, usedsubstreamid);

      if ((res < 0) && (res != TXSOCK_ERR_TIMEOUT) && errno ) {
	 //strerror_r(errno, errbuf, sizeof(buf));

         Info(XrdClientDebug::kHIDEBUG,
	      "ReadRaw", "Read error on " <<
	      fServer.Host << ":" << fServer.Port << ". errno=" << errno );
      }

      // If a socket error comes, then we disconnect
      // but we have not to disconnect in the case of a timeout
      if (((res < 0) && (res == TXSOCK_ERR)) ||
          (!fSocket->IsConnected())) {

	 Info(XrdClientDebug::kHIDEBUG,
	      "ReadRaw", 
	      "Disconnection reported on" <<
	      fServer.Host << ":" << fServer.Port);

         Disconnect();
      }


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
	       "ReadRaw", "Read " << res <<  "bytes. Dump:" << endl << s << endl);

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
   struct SidInfo *parallelsid = 0;
   UnsolRespProcResult res = kUNSOL_KEEP;

   m = new XrdClientMessage();
   if (!m) {
      Error("BuildMessage",
	    "Cannot create a new Message. Aborting.");
      abort();
   }

   {
//     fMultireadMutex.Lock();
     m->ReadRaw(this);
//     fMultireadMutex.UnLock();
   }

   parallelsid = (fSidManager) ? fSidManager->GetSidInfo(m->HeaderSID()) : 0;

   if ( parallelsid || (m->IsAttn()) || (m->GetStatusCode() == XrdClientMessage::kXrdMSC_readerr)) {
      

      // Here we insert the PhyConn-level support for unsolicited responses
      // Some of them will be propagated in some way to the upper levels
      // The path should be
      //  here -> XrdClientConnMgr -> all the involved XrdClientLogConnections ->
      //   -> all the corresponding XrdClient

      if (m->GetStatusCode() == XrdClientMessage::kXrdMSC_readerr) {
	  Info(XrdClientDebug::kDUMPDEBUG,
	       "BuildMessage"," propagating a communication error message.");
      }
      else {
	  Info(XrdClientDebug::kDUMPDEBUG,
	       "BuildMessage"," propagating unsol id " << m->HeaderSID());
      }

      Touch();
      res = HandleUnsolicited(m);



   }
   
   if (Enqueue && !parallelsid && !m->IsAttn() && (m->GetStatusCode() != XrdClientMessage::kXrdMSC_readerr)) {
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
   else {


       // The purpose of this message ends here
       if ( (parallelsid) && (res != kUNSOL_KEEP) &&
            (m->GetStatusCode() != XrdClientMessage::kXrdMSC_readerr) )
	 if (fSidManager && (m->HeaderStatus() != kXR_oksofar))
	    fSidManager->ReleaseSid(m->HeaderSID());
       
       //       if (m->GetStatusCode() != XrdClientMessage::kXrdMSC_readerr) {
       delete m;
       m = 0;
       //       }

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
int XrdClientPhyConnection::WriteRaw(const void *buf, int len, int substreamid) {
    // Send 'len' bytes located at 'buf' to the connected server.
    // Return number of bytes sent.
    // usesubstreams tells if we have to select a substream to send the data through or
    // the main stream is to be used
    // substreamid == 0 means to use the main stream

   int res;

   Touch();

   if (IsValid()) {

      Info(XrdClientDebug::kDUMPDEBUG,
	   "WriteRaw",
	   "Writing to substreamid " <<
	   substreamid);
    
      res = fSocket->SendRaw(buf, len, substreamid);

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

//_____________________________________________________________________________
ERemoteServerType XrdClientPhyConnection::DoHandShake(ServerInitHandShake &xbody,
						      int substreamid)
{
   // Performs initial hand-shake with the server in order to understand which 
   // kind of server is there at the other side and to make the server know who 
   // we are. Note that if the substreamid is negative, this is a handshake for
   // a parallel stream and we can do a short handshake.
   struct ClientInitHandShake initHS;
   ServerResponseType type;
   ERemoteServerType typeres = kSTNone;
   int isPS = (substreamid < 0);

   int writeres, readres, len = 0;

   // Set field in network byte order
   memset(&initHS, 0, sizeof(initHS));
   initHS.fourth = (kXR_int32)htonl(4);
   initHS.fifth  = (kXR_int32)htonl(2012);

   //---------------------------------------------------------------------------
   // Create  protocol request
   //---------------------------------------------------------------------------
   ClientRequest req;
   memset( &req, 0, sizeof( req ) );
   req.header.requestid  = kXR_protocol;
   req.protocol.clientpv = kXR_PROTOCOLVERSION;

   //---------------------------------------------------------------------------
   // Send the handshake and the kXR_protocol request
   //---------------------------------------------------------------------------

   if( DebugLevel() >= XrdClientDebug::kDUMPDEBUG )
     smartPrintClientHeader( &req );
   // For parallel streams we only send the handshake. For normal streams we
   // piggy-back a protocol request (i.e., extended handshake). This is for
   // historical reasons to keep backward compatability.
   if (isPS)
      {Info( XrdClientDebug::kHIDEBUG, "DoHandShake",
            "HandShake step 1: Sending handshake for a parallel stream" );
       writeres = WriteRaw( &initHS, sizeof(initHS), substreamid );
      } else {
       Info( XrdClientDebug::kHIDEBUG, "DoHandShake",
            "HandShake step 1: Sending handshake with a piggy-backed protocol request" );
       clientMarshall( &req );
       len = sizeof( req ) + sizeof( initHS );
       char buffer[sizeof(req)+sizeof(initHS)];
       memcpy( buffer, &initHS, sizeof( initHS ) );
       memcpy( buffer+sizeof( initHS ), &req, sizeof( req ) );
       writeres = WriteRaw( buffer, len, substreamid );
      }

   if( writeres < 0 )
   {
      Info( XrdClientDebug::kNODEBUG,"DoHandShake", "Failed to send " << len <<
            " bytes of protocol info request. Retrying ...");
      return kSTError;
   }

   // Read from server the first 4 bytes
   len = sizeof(type);

   Info(XrdClientDebug::kHIDEBUG,
	"DoHandShake",
	"HandShake step 2: Reading " << len <<
	" bytes.");
 
   //
   // Read returns the return value of TSocket->RecvRaw... that returns the 
   // return value of recv (unix low level syscall)
   //
   readres = ReadRaw(&type, 
		     len, substreamid); // Reads 4(2+2) bytes
               
   if (readres < 0) {
      Info(XrdClientDebug::kNODEBUG, "DoHandShake", "Failed to read " << len <<
	    " bytes. Retrying ...");

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
	   " bytes.");

      readres = ReadRaw(&xbody, len, substreamid); // Read 12(4+4+4) bytes

      if (readres < 0) {
         Error("DoHandShake", "Error reading " << len << 
	       " bytes.");

         return kSTError;
      }

      ServerInitHandShake2HostFmt(&xbody);

      Info(XrdClientDebug::kHIDEBUG,
	   "DoHandShake",
	   "Server protocol: " << xbody.protover << " type: " << xbody.msgval);

      // For parallel streams we never sent a protocol request. Otherwise, we
      // need to get the protocol request response. Ideally, we would continue
      // doing this using the unlocked ReadRaw() as the handshake is technically
      // atomic. The added code went through the message queue making it not
      // atomic which made it impossible to setup a parallel stream. the ideal
      // fix would cause too much code to change, so we do a quick and dirty.
      //
      if (isPS)
         {typeres = kSTDataXrootd;
          if (xbody.msgval & kXR_DataServer) fServerType = kSTDataXrootd;
             else fServerType = kSTBaseXrootd;
          fServerProto = xbody.protover;
          return fServerType;
         }

      //------------------------------------------------------------------------
      // Get the response to the protocol message
      //------------------------------------------------------------------------
      XrdClientMessage *msg = new XrdClientMessage();
      msg->ReadRaw( this );

      if( DebugLevel() >= XrdClientDebug::kDUMPDEBUG )
        smartPrintServerHeader( &msg->fHdr );

      //------------------------------------------------------------------------
      // Got correct response from the server
      //------------------------------------------------------------------------
      if( !msg->IsError() && msg->HeaderStatus() == kXR_ok )
      {
        ServerResponseBody_Protocol *resp = (ServerResponseBody_Protocol*)msg->GetData();
        resp->pval  = ntohl( resp->pval );
        resp->flags = ntohl( resp->flags );
        Info( XrdClientDebug::kHIDEBUG, "DoHandShake",
              "Server protocol (kXR_protocol): " << resp->pval << " flags: " << resp->flags );

        //----------------------------------------------------------------------
        // Get the server type
        //----------------------------------------------------------------------
        if( resp->pval >= 0x297 )
        {
          if( resp->flags & kXR_isManager )
          {
            if( resp->flags & kXR_attrMeta ) typeres = kSTMetaXrootd;
            else typeres = kSTBaseXrootd;
          }
          else if( resp->flags & kXR_isServer )
            typeres = kSTDataXrootd;

          fServerType  = typeres;
          fServerProto = resp->pval;
          delete msg;
          return typeres;
        }
      }
      //------------------------------------------------------------------------
      // Protocol not supported
      //------------------------------------------------------------------------
      else
      {
        Info( XrdClientDebug::kHIDEBUG, "DoHandShake",
              "No valid response to the protocol request" );
      }
      delete msg;
      msg = 0;

      // check if the eXtended rootd is a data server
      switch (xbody.msgval) {

      case kXR_DataServer:
         // This is a data server
         typeres = kSTDataXrootd;
	 break;

      case kXR_LBalServer:
         typeres = kSTBaseXrootd;
	 break;
      }
      
   } else {

      // We are here if it wasn't an XRootd
      // and we need to complete the reading
      if (type == 8)
         typeres = kSTRootd;
      else 
         // We dunno the server type
         typeres = kSTNone;
   }

   fServerType  = typeres;
   fServerProto = xbody.protover;
   return typeres;
}

//____________________________________________________________________________
void XrdClientPhyConnection::CountLogConn(int d)
{
   // Modify countre of logical connections using this phyconn
   fMutex.Lock();
   fLogConnCnt += d;
   fMutex.UnLock();
}


bool XrdClientPhyConnection::TestAndSetMStreamsGoing() {
  XrdSysMutexHelper mtx(fMutex);
  bool retval = fMStreamsGoing;
  fMStreamsGoing = true;
  return retval;
}

bool XrdClientPhyConnection::IsValid() {
  XrdSysMutexHelper l(fMutex);
  return ( (fSocket != 0) && fSocket->IsConnected());
}

ELoginState XrdClientPhyConnection::IsLogged() {
  const XrdSysMutexHelper l(fMutex);
  return fLogged;
}
