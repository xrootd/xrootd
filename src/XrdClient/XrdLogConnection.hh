//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdLogConnection                                                     // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Class implementing logical connections                               //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_LOGCONNECTION_H
#define XRC_LOGCONNECTION_H


#include "XrdUnsolMsg.hh"
#include "XrdPhyConnection.hh"


class XrdLogConnection: public XrdAbsUnsolicitedMsgHandler, 
   XrdUnsolicitedMsgSender {
private:
   XrdPhyConnection *fPhyConnection;

public:
   XrdLogConnection();
   virtual ~XrdLogConnection();

   inline XrdPhyConnection *GetPhyConnection() { return fPhyConnection; }

   bool          ProcessUnsolicitedMsg(XrdUnsolicitedMsgSender *sender,
                                       XrdMessage *unsolmsg);

   int           ReadRaw(void *buffer, int BufferLength);

   inline void   SetPhyConnection(XrdPhyConnection *PhyConn) 
                 { fPhyConnection = PhyConn; }

   int           WriteRaw(const void *buffer, int BufferLength);

};

#endif
