//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientLogConnection                                               //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Class implementing logical connections                               //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#ifndef XRD_CLOGCONNECTION_H
#define XRD_CLOGCONNECTION_H


#include "XrdClient/XrdClientUnsolMsg.hh"
#include "XrdClient/XrdClientPhyConnection.hh"


class XrdClientLogConnection: public XrdClientAbsUnsolMsgHandler, 
   XrdClientUnsolMsgSender {
private:
   XrdClientPhyConnection *fPhyConnection;

public:
   XrdClientLogConnection();
   virtual ~XrdClientLogConnection();

   inline XrdClientPhyConnection *GetPhyConnection() { return fPhyConnection; }

   bool          ProcessUnsolicitedMsg(XrdClientUnsolMsgSender *sender,
                                       XrdClientMessage *unsolmsg);

   int           ReadRaw(void *buffer, int BufferLength);

   inline void   SetPhyConnection(XrdClientPhyConnection *PhyConn) 
                 { fPhyConnection = PhyConn; }

   int           WriteRaw(const void *buffer, int BufferLength);

};

#endif
