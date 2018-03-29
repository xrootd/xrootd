/******************************************************************************/
/*                                                                            */
/*           X r d C l i e n t L o g C o n n e c t i o n . c c                */
/*                                                                            */
/* Author: Fabrizio Furano (INFN Padova, 2004)                                */
/* Adapted from TXNetFile (root.cern.ch) originally done by                   */
/*  Alvise Dorigo, Fabrizio Furano                                            */
/*          INFN Padova, 2003                                                 */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Class implementing logical connections                               //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientLogConnection.hh"
#include "XrdClient/XrdClientPhyConnection.hh"
#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientSid.hh"

//_____________________________________________________________________________
XrdClientLogConnection::XrdClientLogConnection(XrdClientSid *sidmgr):
  fSidManager(sidmgr) {

   // Constructor

   fPhyConnection = 0;
   fStreamid = fSidManager ? fSidManager->GetNewSid() : 0;
}

//_____________________________________________________________________________
XrdClientLogConnection::~XrdClientLogConnection() {
   // Destructor

   // Decrement counter in the reference phy conn
   if (fPhyConnection)
      fPhyConnection->CountLogConn(-1);
   if (fSidManager) 
      fSidManager->ReleaseSidTree(fStreamid);
}

//_____________________________________________________________________________
int XrdClientLogConnection::WriteRaw(const void *buffer, int bufferlength,
				     int substreamid)
{
   // Send over the open physical connection 'bufferlength' bytes located
   // at buffer.
   // Return number of bytes sent.

   Info(XrdClientDebug::kDUMPDEBUG,
	"WriteRaw",
	"Writing " << bufferlength << " bytes to physical connection");
  
   return fPhyConnection->WriteRaw(buffer, bufferlength, substreamid);
}

//_____________________________________________________________________________
int XrdClientLogConnection::ReadRaw(void *buffer, int bufferlength)
{
   // Receive from the open physical connection 'bufferlength' bytes and 
   // save in buffer.
   // Return number of bytes received.

   Info(XrdClientDebug::kDUMPDEBUG,
	"ReadRaw",
	"Reading " << bufferlength << " bytes from physical connection");

   return fPhyConnection->ReadRaw(buffer, bufferlength);
  

}

//_____________________________________________________________________________
UnsolRespProcResult XrdClientLogConnection::ProcessUnsolicitedMsg(XrdClientUnsolMsgSender *sender,
								  XrdClientMessage *unsolmsg)
{
   // We are here if an unsolicited response comes from the connmgr
   // The response comes in the form of an XrdClientMessage *, that must NOT be
   // destroyed after processing. It is destroyed by the first sender.
   // Remember that we are in a separate thread, since unsolicited responses
   // are asynchronous by nature.

   //Info(XrdClientDebug::kDUMPDEBUG, "LogConnection",
   //     "Processing unsolicited response (ID:"<<unsolmsg->HeaderSID()<<")");

   // Local processing ....

   // We propagate the event to the obj which registered itself here
   return SendUnsolicitedMsg(sender, unsolmsg);

}
