//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientUnsolMsg                                                          //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// Base classes for unsolicited msg senders/receivers                   //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef XRC_UNSOLMSG_H
#define XRC_UNSOLMSG_H

#include "XrdClientMessage.hh"

class XrdClientUnsolMsgSender;

// Handler

class XrdClientAbsUnsolMsgHandler {
public:
  
  // To be called when an unsolicited response arrives from the lower layers
  virtual bool ProcessUnsolicitedMsg(XrdClientUnsolMsgSender *sender, 
                                       XrdClientMessage *unsolmsg) = 0;

};

// Sender

class XrdClientUnsolMsgSender {
public:
   // The upper level handler for unsolicited responses
  XrdClientAbsUnsolMsgHandler *UnsolicitedMsgHandler;

  inline void SendUnsolicitedMsg(XrdClientUnsolMsgSender *sender, XrdClientMessage *unsolmsg) {
    // We simply send the event
    if (UnsolicitedMsgHandler)
      UnsolicitedMsgHandler->ProcessUnsolicitedMsg(sender, unsolmsg);
  }

  inline XrdClientUnsolMsgSender() { UnsolicitedMsgHandler = 0; }
};

#endif
