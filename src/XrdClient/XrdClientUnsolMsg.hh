//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdUnsolMsg                                                          //
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

#include "XrdMessage.hh"

class XrdUnsolicitedMsgSender;

// Handler

class XrdAbsUnsolicitedMsgHandler {
public:
  
  // To be called when an unsolicited response arrives from the lower layers
  virtual bool ProcessUnsolicitedMsg(XrdUnsolicitedMsgSender *sender, 
                                       XrdMessage *unsolmsg) = 0;

};

// Sender

class XrdUnsolicitedMsgSender {
public:
   // The upper level handler for unsolicited responses
  XrdAbsUnsolicitedMsgHandler *UnsolicitedMsgHandler;

  inline void SendUnsolicitedMsg(XrdUnsolicitedMsgSender *sender, XrdMessage *unsolmsg) {
    // We simply send the event
    if (UnsolicitedMsgHandler)
      UnsolicitedMsgHandler->ProcessUnsolicitedMsg(sender, unsolmsg);
  }

  inline XrdUnsolicitedMsgSender() { UnsolicitedMsgHandler = 0; }
};

#endif
