/*****************************************************************************/
/*                                                                           */
/*                   X r d X r C l i e n t W o r k e r . c c                 */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*      All Rights Reserved.  See XrdInfo.cc for complete License Terms      */
/*            Produced by Heinz Stockinger for Stanford University           */
/*****************************************************************************/

//         $Id$

const char *XrdXrClientWorkerCVSID = "$Id$";

/*****************************************************************************/
/*                             i n c l u d e s                               */
/*****************************************************************************/

#include <stdio.h>
#include <sys/stat.h>

#include "XrdXr/XrdXrClientWorker.hh"
#include "XrdXr/XrdXrTrace.hh"
#include "XrdNet/XrdNetDNS.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdXrootd/XrdXrootdProtocol.hh"

/*****************************************************************************/
/*                  E r r o r   R o u t i n g   O b j e c t                  */
/*****************************************************************************/

XrdOucError XrEroute(0, "xr_");

XrdOucTrace XrTrace(&XrEroute);

extern XrdSecProtocol *(*XrdXrootdSecGetProtocol)(const char *,
                                                  const struct sockaddr  &,
                                                  const XrdSecParameters &,
                                                  XrdOucErrInfo    *);

/*****************************************************************************/
/*                          c o n s t r u c t o r                            */
/*****************************************************************************/

/**
 * Constructor
 * 
 * Input:  hostname  - Hostname of the xrootd to connect
 *         port      - port number of the remote xrootd
 *         logger    - logger object
 */
XrdXrClientWorker::XrdXrClientWorker(const char*   hostname, 
				     int           port, 
				     XrdOucLogger *logger) 
{
  // Assign the values needed for the connection to the remote xrootd
  //
  hostname_ = hostname;
  port_ = port;

  // Use Logger/Error for the network connection
  //
  tident = (char *) "XrClientWorker";  // This value is used in TRACE messages
  XrEroute.logger(logger);
  errInfo = new XrdOucErrInfo();

  // Set stream ID base to 10. This value is used to generate a stream id
  //
  streamIdBase = 10;

  waitTime = redirectPort = redirect = 0;
} // Constructor 


/*****************************************************************************/
/*                               l o g i n                                   */
/*****************************************************************************/

/**
 * Login to a remote xrootd server.
 *
 * Input:   username  - Unix id (username) to login at the server
 *                      The username can have a maximum of 8 characters.
 *          role      - role can have one of following two two values:
 *                            kXR_useruser  = 0
 *			      kXR_useradmin = 1
 *          tlen      - length of the token supplied in the next parameter
 *          token     - Char token supplied by a previous redirection response
 *          resevered - Can have a maximum of 3 characters.
 *  
 * Output:  return 0 if OK, or an error number otherwise.
 */
int XrdXrClientWorker::login(kXR_char      *username,
			     kXR_char       role[1],
			     kXR_int32      tlen,
			     kXR_char      *token,
			     kXR_char      *reserved)
{
  static const char *epname = "login";
  int rc;  // return code

  TRACE(Login, "Try to connect to " <<  hostname_);

  // Connect to a remote xrootd using the created link
  //
  xrootd = new XrdNetWork(&XrEroute, 0);

  lp = xrootd->Connect((char *) hostname_, port_, 0);

  // Check if we really got a connection and the remote host is available
  // 
  if (!lp)  {
    delete xrootd; xrootd = 0;
    return -kXR_noserver;
  }

  // Do the initial handshake that is required for the Xrootd protocol. Only
  // if this is passed correctly, we can try to login to the remote machine
  //
  rc = initialHandshake();
  if (rc) {
    delete xrootd; xrootd = 0;
    return rc;
  } else {
    TRACE(Login, "initial handshake ok.");
  }

  // We have now completed the initial client-server handshake and prepare
  // to send a login request to the server
  //
  // Initialise the login structure with the respective parameters
  //
  ClientLoginRequest loginRequest;

  memcpy(loginRequest.streamid, getStreamId(), 2);
  loginRequest.requestid = static_cast<kXR_unt16>(htons(kXR_login));
  loginRequest.pid       = static_cast<kXR_int32>(htonl(getpid()));
  memcpy(loginRequest.username, username, sizeof(loginRequest.username));
  loginRequest.role[0]   = role[0];
  loginRequest.dlen      = static_cast<kXR_int32>(htonl(tlen));

  if (reserved) { 
    // assign the value only if it is set
    memcpy(loginRequest.reserved, reserved, sizeof(loginRequest.reserved));
  } else {
    memset(loginRequest.reserved, 0, 3);
  }

  // Pack the loginRequest structure and the variable length token in an
  // I/O vector to be sent to the server
  //
  struct iovec iov[2]; 
  iov[0].iov_base = (caddr_t) &loginRequest;        
  iov[0].iov_len  = sizeof(loginRequest);

  int length = 1; 
  if (token != NULL) {
    iov[1].iov_base = (caddr_t) token;
    iov[1].iov_len  = strlen((char*) token); 
    length = 2;
  } else {
    iov[1].iov_base = NULL;
    iov[1].iov_len  = 0;
  }

  // Send the login request to the server
  //                
  if (lp->Send(iov, length)) {  
    XrEroute.Emsg(epname, "login request not sent correctly to server");
    delete xrootd; xrootd = 0;
    return -1;
  } 

  // Get ready to receive the server response
  //
  ServerResponseHeader lrh;         // login response header (lrh)
  rc = lp->Recv((char*) &lrh, sizeof(lrh));
  
  // Check if everything went fine with the login procedure
  //
  if (strncmp((char *) loginRequest.streamid, 
	      (char *) lrh.streamid, 2) != 0) {
    XrEroute.Emsg(epname, "stream ID for login process does not match.");
  }

  kXR_unt16 status = ntohs(lrh.status);
  int dlen = ntohl(lrh.dlen);

  if (status != kXR_ok) {
    return handleError(dlen, status, (char *) epname);
  }

  // Check if the server requires authentication. If yes, a security token
  // is sent back that includes the security protocol(s) the server speaks.
  // If dlen>0, we know that such a token is sent, and we need to do further
  // authentication.
  //
  if (dlen>0) {
    sec = (char*) malloc(dlen*sizeof(kXR_char)+1);
    rc = lp->Recv(sec, dlen);
    *(sec+dlen) = '\0';         // add null char to determine end of token

    // For future extension, we might also pass a user supplied cred and 
    // credtype to the authentication method
    //
    kXR_char credType[4]= {'k', 'r', 'b', '5'};
    rc = auth(credType);

    // Free the security token since we don't need it anymore
    //
    free(sec);
  }

  // Check if an error occured, and return the error code if required
  //
  if (rc < 0 ) {
    return rc;
  }
  else {
    TRACE(Login, "login ok.");
    return 0;
  }
} // login


/*****************************************************************************/
/*                               a u t h                                     */
/*****************************************************************************/



/**
 * Authenticate a client username to a server
 *
 * Input:   credtype  - This can be a supported creditial type like
 *                         krb4 ... Kerberos 4
 *                         krb5 ... Kerberos 5
 *          cred      - User supplied credential
 *
 * Output:  return 0 if OK, or an error number otherwise.
 */
int XrdXrClientWorker::auth(kXR_char        credtype[4],
                            kXR_char       *cred)
{

  static const char  *epname = "auth";
  int                 rc;           // return code
  ClientAuthRequest   authRequest;
  
  TRACE(Auth, "Try to authenticate.");

  // Prepare host/IP information of the remote xrootd. This is required
  // for the authentication.
  //
  struct sockaddr netaddr;
  char *etext;

  if (XrdNetDNS::getHostAddr((char *)hostname_, netaddr, &etext))
     {XrEroute.Emsg(epname, "Unable to get host address;", etext);
      return -1;
     }
      
  XrdSecParameters   secToken;
  XrdSecProtocol    *protocol;
  XrdSecCredentials *credentials;

  // Assign the security token that we have received at the login request
  //
  secToken.buffer = sec;   
  secToken.size   = strlen(sec);

  // Retrieve the security protocol context from the xrootd server
  //
  protocol = XrdXrootdSecGetProtocol(hostname_,
                                     (const struct sockaddr &)netaddr,
                                      secToken, 0);
  if (!protocol) {
    XrEroute.Emsg(epname, "Unable to get protocol.");
    return -1;
  }

  // Once we have the protocol, get user credentials using the given
  // security protocol. If a user has supplied a credential, use that one.
  //
  if (!cred) {
    credentials = protocol->getCredentials();
  } else {
    credentials = new XrdSecCredentials((char*) cred, strlen((char*)cred));
  }

  if (!credentials) {
    XrEroute.Emsg(epname, "Cannot obtain credential.");
    return -1;
  } else {
    TRACE(Auth, "cred=" << credentials->buffer << ", size=" 
	  << credentials->size);
  }

  // Retrieve the credential type (i.e. the protocol type). If user suplied,
  // use that one. Otherwise, get if from the security library
  //
  int haveCred = 0;
  if (!credtype) {
    haveCred = 1;
    // TODO get the cred type from a sec lib call 
    //
    credtype = (kXR_char*) malloc(4);
    memcpy(credtype, "krb5", 4); // for now, we use krb5 by default
  }

  // Initialise the authRequest structure with the respective parameters
  //
  memcpy(authRequest.streamid, getStreamId(), 2);
  authRequest.requestid          =  static_cast<kXR_unt16>(htons(kXR_auth));
  authRequest.dlen               =  static_cast<kXR_int32>(htonl(credentials->size));
  memset(authRequest.reserved, 0, 12);
  memcpy(authRequest.credtype, credtype, 4);

  // Pack the authRequest structure and the variable length cred in an
  // I/O vector to be sent to the server
  //
  struct iovec iov[2];
  iov[0].iov_base = (caddr_t) &authRequest;        
  iov[0].iov_len  = sizeof(authRequest); 
  iov[1].iov_base = (caddr_t) credentials->buffer;
  iov[1].iov_len  = credentials->size; 

  // All variables are initialised and now send the auth request
  //
  rc = lp->Send(iov, 2);

  // Get ready to receive the server response
  //
  ServerResponseHeader authResponse;
  rc = lp->Recv((char*) &authResponse, sizeof(authResponse));

  // Check if everything went fine with the auth procedure
  //
  if (strncmp((char *) authRequest.streamid, 
	     (char *) authResponse.streamid, 2) != 0) {
    XrEroute.Emsg(epname, "stream ID for authentication process does not match.");
  }

  kXR_unt16 status = ntohs(authResponse.status);
  int dlen = ntohl(authResponse.dlen);

  if (status != kXR_ok) {
    handleError(dlen, status, (char *) epname);

    // For future extension: still need to handle the case: kXR_authmore
    //
    // server sends back: XrdSecParameters *parm
    // client then uses these parms in getCredentials
    // protocol->getCredentials(parm)
    //
    // For Kerberos 5 this is currently not required
    if (status == kXR_authmore) {
      XrEroute.Emsg(epname, 
      "More authentication required for the specific protocol: not yet implemented");
    }

    return -status;
  }

  if (dlen) {
    XrEroute.Emsg(epname, "Authentication not correct - dlen != 0.");
  }

  TRACE (Auth, "authentication ok.");
  delete credentials; credentials = 0;
  if (haveCred) { free(credtype); credtype = 0;};

  return 0;
} // auth

/*****************************************************************************/
/*                               o p e n                                     */
/*****************************************************************************/

/**
 * Open a remote file.
 *
 * Input:  path  - filepath of the remote file to be opened
 *         oflag - open flags such as: 0_RDONLY, 0_WRONLY, O_RDWR
 *                 Indicates if the file is opened for read, write or both.
 *         mode  - mode in which file will be opened when newly created
 *                 This corresponds to the file access permissions set at
 *                 file creation time.   
 *
 * Output: return 0 upon success; -errno otherwise.
 */
int XrdXrClientWorker::open(kXR_char    *path,
			    kXR_unt16    oflag,
			    kXR_unt16    mode)
{
  int                 rc;           // return code
  static const char  *epname = "open";
  ClientOpenRequest   openRequest;

  TRACE(Open, "Try to open file " <<  path);

  // Initialise the openRequest structure with the respective parameters
  //
  memcpy(openRequest.streamid, getStreamId(), 2);
  openRequest.requestid = static_cast<kXR_unt16>(htons(kXR_open));
  openRequest.mode      = static_cast<kXR_unt16>(htons(mode));
  openRequest.options   = static_cast<kXR_unt16>(htons(oflag));
  openRequest.dlen      = static_cast<kXR_int32>(htonl(strlen((char *)path)));
  memset(openRequest.reserved, 0, 12);

  // Pack the openRequest structure and the variable length path in an
  // I/O vector to be sent to the server
  //
  struct iovec iov[2];
  iov[0].iov_base = (caddr_t) &openRequest;        
  iov[0].iov_len  = sizeof(openRequest); 
  iov[1].iov_base = (caddr_t) path;
  iov[1].iov_len  = strlen((char*)path); 

  // All variables are initialised and now send the open request
  //
  rc = lp->Send(iov, 2);

  // Get ready to receive the server response
  //
  ServerResponseHeader openResponse;
  rc = lp->Recv((char*) &openResponse, sizeof(openResponse));

  // Check if everything went fine with the open procedure
  //
  if (strncmp((char *) openRequest.streamid, 
	      (char *) openResponse.streamid, 2) != 0) {
    XrEroute.Emsg(epname, "stream ID for open process does not match.");
  }

  kXR_unt16 status = ntohs(openResponse.status);
  kXR_int32 resplen = ntohl(openResponse.dlen);

  // Handle potenial errors
  //
  if (status != kXR_ok) {
    return handleError(resplen, status, (char*) epname);
  }

  // The response length has to be either 4 (for non-compressed files) or
  // 12 for compressed files
  // 
  int compress      = 0;
  if ((resplen != 4 ) && (resplen != 12)) {
    XrEroute.Emsg(epname,"Response length is not correct.");
    errInfo->setErrInfo(-1, "Response length is not correct.");
    return errInfo->getErrInfo();
  } else {
    if (resplen == 12) {
      TRACE(Open, "file compression not yet implemented.");
      compress = 0; // set 0 since not yet implemented
    }
  }

  // Receive the response body and the file handle
  //
  ServerResponseBody_Open rbo;    // response body open (rbo)
  rc = lp->Recv((char*)&rbo, 4*sizeof(kXR_char) + compress);

  memcpy(fhandle, (char*)rbo.fhandle, 4*sizeof(kXR_char));

  TRACE(Open, "open ok.");
  errInfo->setErrInfo(0,(const char*) "");
  filepath = path;
  return 0;
} // open

/*****************************************************************************/
/*                               r e a d                                     */
/*****************************************************************************/

/**
 * Read 'blen' bytes from the associated file, placing in 'buff'
 * the data and returning the actual number of bytes read.
 *
 * Input:    buff      - Address of the buffer in which to place the data.
 *           offset    - The absolute 64-bit byte offset at which to read.
 *           blen      - The size of the buffer. This is the maximum number
 *                       of bytes that will be read.
 *
 * Output:   Returns the number of bytes read upon success and -errno upon 
 *           failure.
 */
ssize_t XrdXrClientWorker::read(void       *buffer, 
				kXR_int64   offset, 
				kXR_int32   blen)
{
  static const char   *epname = "read";
  ClientReadRequest    readRequest;
  kXR_int32            rc;           // return code
  kXR_int32            dlen = 0;     // size of receive buffer (i.e. data)

  TRACE(Read, "Try to read file.");

  // Initialise the readRequest structure with the respective parameters
  //
  memcpy(readRequest.streamid, getStreamId(), 2);
  readRequest.requestid        = static_cast<kXR_unt16>(htons(kXR_read));
  memcpy(readRequest.fhandle, fhandle, 4*sizeof(kXR_char));
  readRequest.offset           = static_cast<kXR_int64>(htonll(offset));
  readRequest.rlen             = static_cast<kXR_int32>(htonl(blen));
  readRequest.dlen             = static_cast<kXR_int32>(htonl(0));  // do not allow preread

  // All variables are initialised and now send the read request
  //
  if (lp->Send((void *) &readRequest, sizeof(readRequest))) {
    XrEroute.Emsg(epname, "Request not sent correctly.");
    return -1;
  }

  // Get ready to receive the server response
  //
  ServerResponseHeader readResponse;
  kXR_unt16            status;
  ssize_t              received = 0;
  long long            start;

  // If the number of bytes requested is large, the server sends 
  // several buffers that we have to put together into one. The server 
  // indicates this with an kXR_oksofar response.
  // 
  do {
    start = 0;
    rc = lp->Recv((char*) &readResponse,  sizeof(readResponse));
    status = ntohs(readResponse.status);
    dlen = ntohl(readResponse.dlen);

    // Check if everything went fine with the read procedure
    //
    if (strncmp((char *) readRequest.streamid, 
		(char *) readResponse.streamid, 2) != 0) {
      XrEroute.Emsg(epname, "stream ID for read process does not match.");
    }

    if (status != kXR_ok && status != kXR_oksofar) {
      return handleError(dlen, status, (char *) epname);
    }

    // Recv can only receive a certain number of bytes at a time. We loop
    // as long as all bytes are read that were sent by the server
    //
    do {
      rc = lp->Recv((char*) buffer+start+received, dlen-start); 

      if (rc == -1) {
	XrEroute.Emsg(epname, "Data not received correctly.");
	return -1;
      } else {
	start = start + rc;
      }
    } while (start < dlen && rc > 0 );

    received = received + start;
    TRACE(Read, "bytes read: " << received);
    
    if (status == kXR_oksofar) {
      TRACE(Read, "kXR_oksofar: need to read more.");
    }

  } while (status == kXR_oksofar);

  TRACE(Read, "read ok.");

  return received;
} // read

/*****************************************************************************/
/*                               s t a t                                     */
/*****************************************************************************/

/**
 *  Get file status.
 * 
 *  Input:  buffer    - buffer to hold the stat structure
 *          path      - Path of the file to get status: optional. By default,
 *                      the file currently open will be used
 *
 * Output:  return 0 upon success; -errno otherwise.
 */
int XrdXrClientWorker::stat(struct stat *buffer,
			    kXR_char    *path)
{
  static const char  *epname = "stat";
  ClientStatRequest   statRequest;
  int                 rc;           // return code
  char               *lasttk;       // For strtok_r()

  TRACE(Stat, "Try to get status for file " << path);

  // Check if a optional file path is assigned. If not, we use the default
  // file identified by the file handle. For that case, the file needs to be 
  // open
  //
  if (path == NULL) {
    if (!filepath) {
      XrEroute.Emsg(epname, 
		    "No file is open and therefore stat cannot be obtained.");
      return -kXR_FileNotOpen;
    } else {
      path = filepath;
    }
  }

  // Initialise the statRequest structure with the respective parameters
  //
  memcpy(statRequest.streamid, getStreamId(), 2);
  statRequest.requestid          = static_cast<kXR_unt16>(htons(kXR_stat));
  statRequest.dlen               = static_cast<kXR_int32>(htonl(strlen((char *)path)));
  memset(statRequest.reserved, 0, 16);

  // Pack the authRequest structure and the variable length cred in an
  // I/O vector to be sent to the server
  //
  struct iovec iov[2];
  iov[0].iov_base = (caddr_t) &statRequest;        
  iov[0].iov_len  = sizeof(statRequest); 
  iov[1].iov_base = (caddr_t) path;
  iov[1].iov_len  = strlen((char*)path); 

  // All variables are initialised and now send the status request
  //
  rc = lp->Send(iov, 2);

  // Get ready to receive the server response
  //
  ServerResponseHeader statResponse;
  rc = lp->Recv((char*) &statResponse, sizeof(statResponse));

  // Check if everything went fine with the status procedure
  //
  if (strncmp((char *) statRequest.streamid, 
 	      (char *) statResponse.streamid, 2) != 0) {
    XrEroute.Emsg(epname, "stream ID for close process does not match.");
    rc = -1; // indicate that an error occured
  }

  kXR_unt16 status = ntohs(statResponse.status);
  int dlen = ntohl(statResponse.dlen);

  if (status != kXR_ok) {
    return handleError(dlen, status, (char *) epname);
  }

  if (dlen) {
    //CDJ: pad buffer by 1 for null character
    char* buff = (char*) malloc((dlen+1)*sizeof(kXR_char));
    rc = lp->Recv(buff, dlen*sizeof(kXR_char));
    buff[(rc >= 0 ? rc : 0)] = '\0';

    // Convert the buffer into the stat structure
    //
    union {long long uuid; struct {int hi; int lo;} id;} Dev;
    char *temp = strtok_r(buff, (const char*) " ", &lasttk);

    Dev.uuid = (atoll(temp));
    buffer->st_ino = Dev.id.lo;
    buffer->st_dev = Dev.id.hi;

    for (int i = 0; i< 4; i ++) {
      temp = strtok_r(NULL, " ,.", &lasttk);
      switch (i) {
      case 0: buffer->st_size = atoll(temp); break;
      case 1: buffer->st_mode = atoll(temp); break;
      case 2: buffer->st_mtime = atol(temp); break;
      }
    } 

    free(buff);
  } else {
    XrEroute.Emsg(epname, "No status information available.");
    return -1;
  }

  TRACE(Stat, "status ok.");
  return 0;
} // stat

/*****************************************************************************/
/*                               c l o s e                                   */
/*****************************************************************************/

/**
 * Close a remote file
 *
 * Output: return 0 upon success; -errno otherwise.
 */
int XrdXrClientWorker::close() 
{

  static const char  *epname = "close";
  ClientCloseRequest  closeRequest;
  kXR_char            streamId[2];  // hold stream id for the request
  int                 rc;           // return code

  TRACE(Close, "Try to close file.");

  // Initialise the closeRequest structure with the respective parameters
  //
  strcpy ((char *) streamId, "9"); // set an arbitrary number 
  memcpy(closeRequest.streamid, streamId, sizeof(closeRequest.streamid));
  closeRequest.requestid = static_cast<kXR_unt16>(htons(kXR_close));
  for (int i=0; i < 4; i++) {closeRequest.fhandle[i] = fhandle[i];}
  for (int i=0; i <12; i++) {closeRequest.reserved[i] = (kXR_char) '\0';}
  closeRequest.dlen = static_cast<kXR_int32>(htonl(0));

  rc = lp->Send((void *) &closeRequest, sizeof(closeRequest));

  // Get ready to receive the server response
  //
  ServerResponseHeader closeResponse;
  rc = lp->Recv((char*) &closeResponse, sizeof(closeResponse));

  // Check if everything went fine with the close procedure
  //
  if (strncmp((char *) closeRequest.streamid, 
	     (char *) closeResponse.streamid, 2) != 0) {
    XrEroute.Emsg(epname, "stream ID for close process does not match.");
    rc = -1; // indicate that an error occured
  }

  kXR_unt16 status = ntohs(closeResponse.status);
  int dlen = ntohl(closeResponse.dlen);

  if (status != kXR_ok) {
    return handleError(dlen, status, (char *) epname);
  }


  if (dlen != 0){
    XrEroute.Emsg(epname, "server error in closing the file - dlen !=0.");
    rc = -1; // indicate that an error occured
  }

  // Check if an error occcured, and return the error code if required
  //
  if (rc < 0) {
    return rc;
  } else {
    TRACE(Close, "close ok.");
    filepath = 0;
    return 0;
  }
} // close



/*****************************************************************************/
/*                          d e s t r u c t o r                              */
/*****************************************************************************/

XrdXrClientWorker::~XrdXrClientWorker() 
{

  // Unbind the Network
  //
  if (lp) {
    xrootd->unBind();
  }

  // Delete all pointers
  //
  delete xrootd;  xrootd  = 0;
  delete errInfo; errInfo = 0;
  if (redirect) { free(redirectHost); redirectHost = 0; redirect = 0;};
}


/*****************************************************************************/
/*                       P r i v a t e   m e t h o d s                       */
/*****************************************************************************/


/*****************************************************************************/
/*                       i n i t i a l H a n d s h a k e                     */
/*****************************************************************************/


/**
 * Do the initial handshake that is required prior to the login request
 * 
 * Output: return 0 if ok, otherwise -1 for an error
 */
int XrdXrClientWorker::initialHandshake()
{

  // Prepare the initial handshake values for the client
  //
  ClientInitHandShake handshake = {0, 0, 0, 
                                   static_cast<kXR_int32>(htonl(4)),
                                   static_cast<kXR_int32>(htonl(2012))
                                  };

  if (lp->Send((void *) &handshake, int(sizeof(handshake)))) {
    XrEroute.Emsg("login", 
		  "initial client handshake not sent correctly");
    return -1;
  }

  // Prepare to receive the server header handshake. 
  //
  ServerResponseHeader serverResponse;
  if (lp->Recv((char*) &serverResponse, sizeof(serverResponse)) != 8) {
    XrEroute.Emsg("login", 
		  "initial server handshake header not received correctly.");
    return -1;
  } 
 
  // Deserialise the response and check if the server responds correctly
  // using the XRootd protocol
  //
  serverResponse.status = ntohs(serverResponse.status);
  serverResponse.dlen   = ntohl(serverResponse.dlen);
  
  if ((serverResponse.streamid[0] != '\0')  ||
      (serverResponse.streamid[1] != '\0'))
    {
    XrEroute.Emsg("login", 
		"stream ID for handshake process does not match '\0' '\0'.");
    return -1;
  }

  if (serverResponse.status != kXR_ok) {
    XrEroute.Emsg("login", 
		"server handshake error -  status does not match 0. ");
    return -1;
  }
 
  if (serverResponse.dlen != 8) {
    XrEroute.Emsg("login", "server handshake error - length not correct");
    return -1;
  }

  // Since we did not encounter any error in the handshake header, let's
  // read the response body and check for correct values
  //
  ServerResponseBody_Protocol responseBody;
  if (lp->Recv((char*) &responseBody, sizeof(responseBody)) != 8 ) {
    XrEroute.Emsg("login", "server handshake error - length not correct");
    return -1;
  }
  
  responseBody.pval   = ntohl(responseBody.pval);
  responseBody.flags  = ntohl(responseBody.flags);

  if (!responseBody.pval) { 
    // current default value for version 2.2.0: 0x00000220
    XrEroute.Emsg("login", "server handshake error - wrong protocol version");
    return -1;
  }

  hostType = responseBody.flags;
  if ((responseBody.flags != kXR_DataServer) &&
      (responseBody.flags != kXR_LBalServer)) {
    XrEroute.Emsg("login", "server handshake error:" ,
       (char *) "remote server is neither a data nor a load balancing server");
    return -1;
  }

  return 0;
} // initialHandshake

/*****************************************************************************/
/*                          g e t S t r e a m I d                            */
/*****************************************************************************/

/**
 *  Generate a stream ID and return it.
 */
const char* XrdXrClientWorker::getStreamId() 
{

  // Generate a simple stream id in the range of 10 to 99. If 99 has been
  // reached, reset the id back to 10. 
  //
  if (streamIdBase++ >= 100) {
    streamIdBase = 10;
  }

  // Convert the int value into a two char string
  //
  snprintf(streamIdBuff, 2, "%02d", streamIdBase);
  return (const char *)streamIdBuff;
} // getStreamId


/*****************************************************************************/
/*                           h a n d l e E r r o r                           */
/*****************************************************************************/
 
/**
 * Receive an error response from the server and put the result into an
 * internal error structure 
 *
 * Input:  dlen   - length of error message to be received from the server
 *         staus  - error type. Can be kXR_redirect, kXR_wait etc.
 *         method - method from where the error occured
 *
 * Output: return negative error number
 */
int XrdXrClientWorker::handleError(kXR_int32      dlen, 
				   kXR_unt16      status,
				   char          *method)
{
  ServerResponseBody_Error error;
  int                      rc    = 0;

  rc = lp->Recv((char*)&error, 
		sizeof(kXR_int32) + (dlen-4)*sizeof(kXR_char));
  
  // Check if message has been received correctly
  //
  if (rc != (int)(sizeof(kXR_int32) + (dlen-4)*sizeof(kXR_char))) {
    XrEroute.Emsg(method, 
		(char *)"Error message not received correctly.");
  }

  // If error type is kXR_error, print the error message. For kXR_wait and
  // kXR_redirect assign the respective variables
  //
  switch(status) {
  case kXR_error:    XrEroute.Emsg(method, error.errmsg);
                     status       = ntohl(error.errnum);  break;
  case kXR_wait:     waitTime     = ntohl(error.errnum);  break;
  case kXR_redirect: redirectPort = ntohl(error.errnum);  
                     redirectHost = (char*) malloc(rc - 3);
		     memcpy((char *) redirectHost, error.errmsg, rc - 3);
		     *(redirectHost+(rc-4)) = '\0';
		     redirect = 1;
		     break;
  }

  return -status;
} // handleError
