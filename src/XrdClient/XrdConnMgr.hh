//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdConnMgr                                                           //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// The Connection Manager handles socket communications for TXNetFile   //
// action: connect, disconnect, read, write. It is a static object of   //
// the TXNetFile class such that within a single application multiple   //
// TXNetFile objects share the same connection manager.                 //
// The connection manager maps multiple logical connections on a single //
// physical connection.                                                 //
// There is one and only one logical connection per client              //
// and one and only one physical connection per server:port.            //
// Thus multiple objects withing a given application share              //
// the same physical TCP channel to communicate with the server.        //
// This reduces the time overhead for socket creation and reduces also  //
// the server load due to handling many sockets.                        //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_CONNMGR_H
#define XRC_CONNMGR_H



#include <vector>
#include <pthread.h>
#include "XrdUnsolMsg.hh"
#include "XrdLogConnection.hh"
#include "XrdMessage.hh"

#define ConnectionManager XrdConnectionMgr::Instance()



// Ugly prototype to avoid warnings under solaris
extern "C" void * GarbageCollectorThread(void * arg);

class XrdConnectionMgr: public XrdAbsUnsolicitedMsgHandler, 
                       XrdUnsolicitedMsgSender {
private:
   vector <XrdLogConnection*> fLogVec;
   vector <XrdPhyConnection*> fPhyVec;
   pthread_mutex_t            fMutex; // mutex used to protect local variables
                                      // of this and TXLogConnection, TXPhyConnection
                                      // classes; not used to protect i/o streams

   pthread_t                  fGarbageColl;


   static XrdConnectionMgr*   fgInstance;

   void          GarbageCollect();
   friend void * GarbageCollectorThread(void *);
   bool          ProcessUnsolicitedMsg(XrdUnsolicitedMsgSender *sender,
                                       XrdMessage *unsolmsg);
protected:
   XrdConnectionMgr();

public:
   virtual ~XrdConnectionMgr();

   short         Connect(XrdUrlInfo RemoteAddress);
   void          Disconnect(short LogConnectionID, bool ForcePhysicalDisc);
   XrdLogConnection *GetConnection(short LogConnectionID);
   short         GetPhyConnectionRefCount(XrdPhyConnection *PhyConn);

   XrdMessage*   ReadMsg(short LogConnectionID);

   int           ReadRaw(short LogConnectionID, void *buffer, int BufferLength);
   int           WriteRaw(short LogConnectionID, const void *buffer, 
                          int BufferLength);

   static XrdConnectionMgr* Instance();
   static void              Reset();


};


#endif
