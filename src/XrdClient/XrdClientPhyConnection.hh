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

#ifndef _XrdClientPhyConnection
#define _XrdClientPhyConnection

#include "XrdClient/XrdClientSock.hh"
#include "XrdClient/XrdClientMessage.hh"
#include "XrdClient/XrdClientUnsolMsg.hh"
#include "XrdClient/XrdClientInputBuffer.hh"
#include "XrdClient/XrdClientUrlInfo.hh"
#include "XrdClient/XrdClientThread.hh"
#include "XrdOuc/XrdOucSemWait.hh"

#include <time.h> // for time_t data type

enum ELoginState {
   kNo      = 0,
   kYes     = 1, 
   kPending = 2
};
enum ERemoteServer {
   kBase    = 100, 
   kData    = 101, 
   kUnknown = 102
};

class XrdSecProtocol;

class XrdClientPhyConnection: public XrdClientUnsolMsgSender {

private:
   time_t              fLastUseTimestamp;
   enum ELoginState    fLogged;       // only 1 login/auth is needed for physical  
   XrdSecProtocol     *fSecProtocol;  // authentication protocol

   XrdClientInputBuffer      fMsgQ;         // The queue used to hold incoming messages
   int                 fRequestTimeout;

   XrdOucRecMutex         fRwMutex;     // Lock before using the physical channel 
                                      // (for reading and/or writing)

   XrdOucRecMutex         fMutex;

   XrdClientThread     *fReaderthreadhandler; // The thread which is going to pump
                                             // out the data from the socket
                                             // in the async mode
   bool                fReaderthreadrunning;

   XrdClientUrlInfo          fServer;

   XrdClientSock       *fSocket;

   UnsolRespProcResult HandleUnsolicited(XrdClientMessage *m);

   XrdOucSemWait       fReaderCV;

public:
   ERemoteServer       fServerType;
   long                fTTLsec;

   XrdClientPhyConnection(XrdClientAbsUnsolMsgHandler *h);
   ~XrdClientPhyConnection();

   XrdClientMessage     *BuildMessage(bool IgnoreTimeouts, bool Enqueue);
   bool                  CheckAutoTerm();

   bool           Connect(XrdClientUrlInfo RemoteHost, bool isUnix = 0);
   void           Disconnect();
   bool           ExpiredTTL();
   long           GetTTL() { return fTTLsec; }

   XrdSecProtocol *GetSecProtocol() const { return fSecProtocol; }
   int            GetSocket() { return fSocket ? fSocket->fSocket : -1; }

   int            SaveSocket() { return fSocket ? (fSocket->SaveSocket()) : -1; }
   void           SetInterrupt() { if (fSocket) fSocket->SetInterrupt(); }
   void           SetSecProtocol(XrdSecProtocol *sp) { fSecProtocol = sp; }

   void           StartedReader();

   bool           IsAddress(const XrdOucString &addr) {
      return ( (fServer.Host == addr) ||
	       (fServer.HostAddr == addr) );
   }

   ELoginState    IsLogged() const { return fLogged; }
   bool           IsPort(int port) { return (fServer.Port == port); };
   bool           IsUser(const XrdOucString &usr) { return (fServer.User == usr); };
   bool           IsValid() const { return (fSocket && fSocket->IsConnected());}
   void           LockChannel();
   int            ReadRaw(void *buffer, int BufferLength);
   XrdClientMessage     *ReadMessage(int streamid);
   bool           ReConnect(XrdClientUrlInfo RemoteHost);
   void           SetLogged(ELoginState status) { fLogged = status; }
   inline void    SetTTL(long ttl) { fTTLsec = ttl; }
   void           StartReader();
   void           Touch();
   void           UnlockChannel();
   int            WriteRaw(const void *buffer, int BufferLength);

};




//
// Class implementing a trick to automatically unlock an XrdClientPhyConnection
//
class XrdClientPhyConnLocker {
private:
   XrdClientPhyConnection *phyconn;

public:
   XrdClientPhyConnLocker(XrdClientPhyConnection *phyc) {
      // Constructor
      phyconn = phyc;
      phyconn->LockChannel();
   }

   ~XrdClientPhyConnLocker(){
      // Destructor. 
      phyconn->UnlockChannel();
   }

};

#endif
