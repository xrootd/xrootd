//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientAdminConn                                                   //
//                                                                      //
// Author: G. Ganis (CERN, 2007)                                        //
//                                                                      //
// High level handler of connections for XrdClientAdmin.                //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

const char *XrdClientAdminConnCVSID = "$Id$";

#include "XrdClient/XrdClientDebug.hh"
#include "XrdClient/XrdClientAdminConn.hh"
#include "XrdClient/XrdClientLogConnection.hh"

//_____________________________________________________________________________
bool XrdClientAdminConn::SendGenCommand(ClientRequest *req, const void *reqMoreData,
                                        void **answMoreDataAllocated, 
                                        void *answMoreData, bool HasToAlloc,
                                        char *CmdName,
                                        int substreamid)
{
    // SendGenCommand tries to send a single command for a number of times 

    // Notify
    Info(XrdClientDebug::kHIDEBUG, "XrdClientAdminConn::SendGenCommand",
        " CmdName: " << CmdName << ", fInit: " << fInit);

    // Run the command
    bool fInitSv = fInit;
    fInit = 0;
    fRedirected = 0;
    bool rc = XrdClientConn::SendGenCommand(req, reqMoreData, answMoreDataAllocated, 
                                            answMoreData, HasToAlloc,
                                            CmdName, substreamid);
    fInit = fInitSv;
    if (fInit && fRedirected) {
       if (GoToAnotherServer(*GetLBSUrl()) != kOK)
          return 0;
       fGlobalRedirCnt = 0;
    }

    //  Done
    return rc;
}

//_____________________________________________________________________________
XReqErrorType XrdClientAdminConn::GoToAnotherServer(XrdClientUrlInfo newdest)
{
   // Re-directs to another server

   // Notify
   Info(XrdClientDebug::kHIDEBUG, "XrdClientAdminConn::GoToAnotherServer",
        " going to "<<newdest.Host<<":"<<newdest.Port);

   // Disconnect existing logical connection
   Disconnect(0);

   // Connect to the new destination
   if (Connect(newdest, fUnsolMsgHandler) == -1) {
      // Note: if Connect is unable to work then we are in trouble.
      // It seems that we have been redirected to a non working server
      Error("GoToAnotherServer",
            "Error connecting to ["<<newdest.Host<<":"<<newdest.Port);
      // If no conn is possible then we return to the load balancer
      return kREDIRCONNECT;
   }
   //
   // Set fUrl to the new data/lb server if the connection has been succesfull
   fUrl = newdest;

   // ID key
   XrdOucString key = XrdClientConn::GetKey(newdest);

   // If we have one connection for 'key', just use it
   int *gc = fDataConn.Find(key.c_str());
   if (gc && *gc == 1) {
      // Flag redirection
      fRedirected = 1;
      // Already handshaked successfully
      return kOK;
   }

   // Notify
   Info(XrdClientDebug::kHIDEBUG, "XrdClientAdminConn::GoToAnotherServer",
        " getting access to "<<newdest.Host<<":"<<newdest.Port);

   // Run the handshake
   if (IsConnected() && !GetAccessToSrv()) {
      Error("GoToAnotherServer",
            "Error handshaking to ["<<newdest.Host<<":"<<newdest.Port << "]");
      fDataConn.Add(key.c_str(), new int(0));
      return kREDIRCONNECT;
   }

   // Ok
   SetStreamID(ConnectionManager->GetConnection(GetLogConnID())->Streamid());
   fDataConn.Add(key.c_str(), new int(1));
   // Flag redirection
   fRedirected = 1;

   // Notify
   Info(XrdClientDebug::kHIDEBUG, "XrdClientAdminConn::GoToAnotherServer",
        " added to hash table with key: "<<key);

   // Done
   return kOK;
}

//_____________________________________________________________________________
bool XrdClientAdminConn::GetAccessToSrv()
{
   // Gets access to the connected server. The login and authorization steps
   // are performed here (calling method DoLogin() that performs logging-in
   // and calls DoAuthentication() ).
   // If the server redirects us, this is gently handled by the general
   // functions devoted to the handling of the server's responses.
   // Nothing is visible here, and nothing is visible from the other high
   // level functions.

   bool rc = 0;

   // ID key
   XrdOucString key = XrdClientConn::GetKey(fUrl);

   // Notify
   Info(XrdClientDebug::kHIDEBUG,
        "XrdClientAdminConn::GetAccessToSrv", "key: "<<key);

   if ((rc = XrdClientConn::GetAccessToSrv()))
      fDataConn.Add(key.c_str(), new int(1));
   else
      fDataConn.Add(key.c_str(), new int(0));

   // Trim the LSBUrl
   if (fLBSUrl)
      fLBSUrl->File = "";

   // Flag we went through here at least once
   fInit = 1;

   // Done
   return rc;
}
