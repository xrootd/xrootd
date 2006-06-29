//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientMStream                                                     //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2006)                          //
//                                                                      //
// Helper code for XrdClient to handle multistream behavior             //
// Functionalities dealing with                                         //
//  mstream creation on init                                            //
//  decisions to add/remove one                                         //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#include "XrdClient/XrdClientMStream.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdClient/XrdClientDebug.hh"

int XrdClientMStream::EstablishParallelStreams(XrdClientConn *cliconn) {
    int mx = EnvGetLong(NAME_MULTISTREAMCNT);
    int i;

    for (i = 0; i < mx; i++) {
	Info(XrdClientDebug::kHIDEBUG,
	     "XrdClientMStream::EstablishParallelStreams", "Trying to establish " << i+1 << "th substream." );
	// If something goes wrong, stop adding new streams
	if (AddParallelStream(cliconn))
	    break;

    }


    return i;
}

// Add a parallel stream to the pool used by the given client inst
// Returns 0 if ok
int XrdClientMStream::AddParallelStream(XrdClientConn *cliconn) {

    // Get the XrdClientPhyconn to be used
    XrdClientPhyConnection *phyconn =
	ConnectionManager->GetConnection(cliconn->GetLogConnID())->GetPhyConnection();

    // Connect a new connection, get the socket fd
    if (phyconn->TryConnectParallelStream() < 0) return -1;

    // The connection now is here with a temp id XRDCLI_PSOCKTEMP
    // Do the handshake
    ServerInitHandShake xbody;
    phyconn->DoHandShake(xbody, XRDCLI_PSOCKTEMP);

    // Send the kxr_bind req to get a new substream id
    int newid = -1;
    int res = -1;
    if (BindPendingStream(cliconn, XRDCLI_PSOCKTEMP, newid)) {
      
	// Everything ok, Establish the new connection with the new id
	res = phyconn->EstablishPendingParallelStream(newid);
    
	if (res) {
	    // If the establish failed we have to remove the pending stream
	    RemoveParallelStream(cliconn, XRDCLI_PSOCKTEMP);
	    return res;
	}

    }
    else {
	// If the bind failed we have to remove the pending stream
	RemoveParallelStream(cliconn, XRDCLI_PSOCKTEMP);
	return -1;
    }

    Info(XrdClientDebug::kHIDEBUG,
	 "XrdClientMStream::EstablishParallelStreams", "Substream added." );
    return 0;

}

// Remove a parallel stream to the pool used by the given client inst
    int XrdClientMStream::RemoveParallelStream(XrdClientConn *cliconn, int substream) {

    return 1;

}



// Binds the pending temporary parallel stream to the current session
// Returns the substreamid assigned by the server into newid
bool XrdClientMStream::BindPendingStream(XrdClientConn *cliconn, int substreamid, int &newid) {
    bool res = false;

    // Prepare request
    ClientRequest bindFileRequest;
    XrdClientConn::SessionIDInfo sess;
    ServerResponseBody_Bind bndresp;

    // Note: this phase has not to overwrite XrdClientConn::LastServerresp
    struct ServerResponseHeader
	LastServerResptmp = cliconn->LastServerResp;

    // Get the XrdClientPhyconn to be used
    XrdClientPhyConnection *phyconn =
	ConnectionManager->GetConnection(cliconn->GetLogConnID())->GetPhyConnection();
    phyconn->ReinitFDTable();

    cliconn->GetSessionID(sess);

    memset( &bindFileRequest, 0, sizeof(bindFileRequest) );
    cliconn->SetSID(bindFileRequest.header.streamid);
    bindFileRequest.bind.requestid = kXR_bind;
    memcpy( bindFileRequest.bind.sessid, sess.id, sizeof(sess.id) );
   

    // The request has to be sent through the stream which has to be bound!
    res =  cliconn->SendGenCommand(&bindFileRequest, (void *)&bndresp, 0, 0,
				   FALSE, (char *)"Bind", substreamid);

    if (res && (cliconn->LastServerResp.status == kXR_ok)) newid = bndresp.substreamid;

    cliconn->LastServerResp = LastServerResptmp;

    return res;

}
