//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientConnMgr                                                     //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// The connection manager maps multiple logical connections on a single //
// physical connection.                                                 //
// There is one and only one logical connection per client              //
// and one and only one physical connection per server:port.            //
// Thus multiple objects withing a given application share              //
// the same physical TCP channel to communicate with a server.          //
// This reduces the time overhead for socket creation and reduces also  //
// the server load due to handling many sockets.                        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#ifndef XRC_CONNMGR_H
#define XRC_CONNMGR_H


#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdClient/XrdClientUnsolMsg.hh"
#include "XrdClient/XrdClientLogConnection.hh"
#include "XrdClient/XrdClientMessage.hh"
#include "XrdClient/XrdClientVector.hh"
#include "XrdClient/XrdClientThread.hh"




// Ugly prototype to avoid warnings under solaris
//void * GarbageCollectorThread(void * arg, XrdClientThread *thr);

class XrdClientConnectionMgr: public XrdClientAbsUnsolMsgHandler, 
                       XrdClientUnsolMsgSender {

private:
   XrdClientVector<XrdClientLogConnection*> fLogVec;
   XrdOucHash<XrdClientPhyConnection> fPhyHash;
   XrdOucRecMutex                fMutex; // mutex used to protect local variables
                                      // of this and TXLogConnection, TXPhyConnection
                                      // classes; not used to protect i/o streams

   XrdClientThread            *fGarbageColl;

   friend void * GarbageCollectorThread(void *, XrdClientThread *thr);
   UnsolRespProcResult
                 ProcessUnsolicitedMsg(XrdClientUnsolMsgSender *sender,
                                       XrdClientMessage *unsolmsg);
public:
   XrdClientConnectionMgr();

   virtual ~XrdClientConnectionMgr();

   short int     Connect(XrdClientUrlInfo RemoteAddress);
   void          Disconnect(short LogConnectionID, bool ForcePhysicalDisc);

   void          GarbageCollect();

   XrdClientLogConnection 
                 *GetConnection(short LogConnectionID);

   XrdClientMessage*   
                 ReadMsg(short LogConnectionID);

   int           ReadRaw(short LogConnectionID, void *buffer, int BufferLength);
   int           WriteRaw(short LogConnectionID, const void *buffer, 
                          int BufferLength, int substreamid);

};


#endif
