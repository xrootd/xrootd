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

#ifndef _XrdPhyConnection
#define _XrdPhyConnection

#include "XrdClientSock.hh"
#include "XrdMessage.hh"
#include "XrdUnsolMsg.hh"
#include "XrdInputBuffer.hh"
#include "XrdUrlInfo.hh"
#include <string>

#include <time.h> // for time_t data type
#include <pthread.h>

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

class XrdPhyConnection: public XrdUnsolicitedMsgSender {
private:
   time_t              fLastUseTimestamp;
   enum ELoginState    fLogged;       // only 1 login/auth is needed for physical  
   XrdInputBuffer      fMsgQ;         // The queue used to hold incoming messages
   int                 fRequestTimeout;
  
   pthread_mutex_t     fRwMutex;     // Lock before using the physical channel 
                                      // (for reading and/or writing)

   pthread_t           fReaderthreadhandler; // The thread which is going to pump
                                             // out the data from the socket
                                             // in the async mode
   bool                fReaderthreadrunning;

   XrdUrlInfo          fServer;

   XrdClientSock       *fSocket;

   void HandleUnsolicited(XrdMessage *m);

public:
   ERemoteServer       fServerType;
   long                fTTLsec;

   XrdPhyConnection(XrdAbsUnsolicitedMsgHandler *h);
   ~XrdPhyConnection();

   XrdMessage     *BuildMessage(bool IgnoreTimeouts, bool Enqueue);
   bool           Connect(XrdUrlInfo RemoteHost);
   void           Disconnect();
   bool           ExpiredTTL();
   long           GetTTL() { return fTTLsec; }

   bool           IsAddress(string &addr) {
      return ( (fServer.Host == addr) ||
	       (fServer.HostAddr == addr) );
   }

   ELoginState    IsLogged() const { return fLogged; }
   bool           IsPort(int port) { return (fServer.Port == port); };
   bool           IsValid() const { return (fSocket && fSocket->IsConnected());}
   void           LockChannel();
   int            ReadRaw(void *buffer, int BufferLength);
   XrdMessage     *ReadMessage(int streamid);
   bool           ReConnect(XrdUrlInfo RemoteHost);
   void           SetLogged(ELoginState status) { fLogged = status; }
   inline void    SetTTL(long ttl) { fTTLsec = ttl; }
   void           StartReader();
   void           Touch();
   void           UnlockChannel();
   int            WriteRaw(const void *buffer, int BufferLength);

};




//
// Class implementing a trick to automatically unlock an XrdPhyConnection
//
class XrdPhyConnLocker {
private:
   XrdPhyConnection *phyconn;

public:
   XrdPhyConnLocker(XrdPhyConnection *phyc) {
      // Constructor
      phyconn = phyc;
      phyconn->LockChannel();
   }

   ~XrdPhyConnLocker(){
      // Destructor. 
      phyconn->UnlockChannel();
   }

};

#endif
