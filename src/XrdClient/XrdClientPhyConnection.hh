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

#include "XrdClient/XrdClientPSock.hh"
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

enum ERemoteServerType {
    kSTError      = -1,  // Some error occurred: server type undetermined 
    kSTNone       = 0,   // Remote server type un-recognized
    kSTRootd      = 1,   // Remote server type: old rootd server
    kSTBaseXrootd = 2,   // Remote server type: xrootd dynamic load balancer
    kSTDataXrootd = 3    // Remote server type: xrootd data server
}; 

class XrdSecProtocol;

class XrdClientPhyConnection: public XrdClientUnsolMsgSender {

private:
    time_t              fLastUseTimestamp;
    enum ELoginState    fLogged;       // only 1 login/auth is needed for physical  
    XrdSecProtocol     *fSecProtocol;  // authentication protocol

    XrdClientInputBuffer
    fMsgQ;         // The queue used to hold incoming messages

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
    ERemoteServerType   fServerType;
    long                fTTLsec;

    XrdClientPhyConnection(XrdClientAbsUnsolMsgHandler *h);
    ~XrdClientPhyConnection();

    XrdClientMessage     *BuildMessage(bool IgnoreTimeouts, bool Enqueue);
    bool                  CheckAutoTerm();

    bool           Connect(XrdClientUrlInfo RemoteHost, bool isUnix = 0);
    void           Disconnect();

    ERemoteServerType
    DoHandShake(ServerInitHandShake &xbody,
		int substreamid = 0);

    bool           ExpiredTTL();
    long           GetTTL() { return fTTLsec; }

    XrdSecProtocol *GetSecProtocol() const { return fSecProtocol; }
    int            GetSocket() { return fSocket ? fSocket->fSocket : -1; }

    // Tells to the sock to rebuild the list of interesting selectors
    void           ReinitFDTable() { if (fSocket) fSocket->ReinitFDTable(); }

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

    // see XrdClientSock for the meaning of the parameters
    int            ReadRaw(void *buffer, int BufferLength, int substreamid = -1,
			   int *usedsubstreamid = 0);

    XrdClientMessage     *ReadMessage(int streamid);
    bool           ReConnect(XrdClientUrlInfo RemoteHost);
    void           SetLogged(ELoginState status) { fLogged = status; }
    inline void    SetTTL(long ttl) { fTTLsec = ttl; }
    void           StartReader();
    void           Touch();
    void           UnlockChannel();
    int            WriteRaw(const void *buffer, int BufferLength, int substreamid = 0);

    int TryConnectParallelStream() { return ( fSocket ? fSocket->TryConnectParallelSock() : -1); }
    int EstablishPendingParallelStream(int newid) { return ( fSocket ? fSocket->EstablishParallelSock(newid) : -1); }
    void RemoveParallelStream(int substream) { if (fSocket) fSocket->RemoveParallelSock(substream); }

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
