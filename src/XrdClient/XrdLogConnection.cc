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

#include "XrdLogConnection.hh"
#include "XrdPhyConnection.hh"
#include "XrdDebug.hh"



//_____________________________________________________________________________
XrdLogConnection::XrdLogConnection() {
   // Constructor
}

//_____________________________________________________________________________
XrdLogConnection::~XrdLogConnection() {
   // Destructor
}

//_____________________________________________________________________________
int XrdLogConnection::WriteRaw(const void *buffer, int bufferlength)
{
   // Send over the open physical connection 'bufferlength' bytes located
   // at buffer.
   // Return number of bytes sent.

   Info(XrdDebug::kDUMPDEBUG,
	"WriteRaw",
	"Writing " << bufferlength << " bytes to physical connection");
  
   return fPhyConnection->WriteRaw(buffer, bufferlength);
}

//_____________________________________________________________________________
int XrdLogConnection::ReadRaw(void *buffer, int bufferlength)
{
   // Receive from the open physical connection 'bufferlength' bytes and 
   // save in buffer.
   // Return number of bytes received.

   Info(XrdDebug::kDUMPDEBUG,
	"ReadRaw",
	"Reading " << bufferlength << " bytes from physical connection");

   return fPhyConnection->ReadRaw(buffer, bufferlength);
  

}

//_____________________________________________________________________________
bool XrdLogConnection::ProcessUnsolicitedMsg(XrdUnsolicitedMsgSender *sender,
                                              XrdMessage *unsolmsg)
{
   // We are here if an unsolicited response comes from the connmgr
   // The response comes in the form of an XrdMessage *, that must NOT be
   // destroyed after processing. It is destroyed by the first sender.
   // Remember that we are in a separate thread, since unsolicited responses
   // are asynchronous by nature.

   Info(XrdDebug::kNODEBUG,
	"ProcessUnsolicitedMsg",
	"Processing unsolicited response");

   // Local processing ....

   // We propagate the event to the obj which registered itself here
   SendUnsolicitedMsg(sender, unsolmsg);
   return TRUE;
}
