#ifndef _XRDXR_CLIENT_WORKER_H
#define _XRDXR_CLIENT_WORKER_H
/*****************************************************************************/
/*                                                                           */
/*                X r d X r C l i e n t W o r k e r . h h                    */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*          Produced by Heinz Stockinger for Stanford University             */
/*****************************************************************************/

//         $Id$

// This class interacts with an xrootd server using the xrootd protocol. 
// The class is supposed to be used for a single file that is requested. 
// Therefore, there is also a constant connection to the remote server as
// long as the file is open and a valid handle exists. In case a connection
// to a server is broken, the class does not try to reconnect. All 
// reconnections, redirections and waits are passed back to the caller. 
// Usually, the class XrdXrClient takes care of such issues.

/*****************************************************************************/
/*                             i n c l u d e s                               */
/*****************************************************************************/

#include "XProtocol/XProtocol.hh"
#include "XrdOuc/XrdOucLogger.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucLink.hh"
#include "XrdOuc/XrdOucNetwork.hh"
#include "XrdOuc/XrdOucErrInfo.hh"    


class XrdXrClientWorker 
{
public:

  /**
   * Constructor
   * 
   * Input:  hostname  - Hostname of the xrootd to connect
   *         port      - port number of the remote xrootd
   *         logger    - logger object
   */
  XrdXrClientWorker(const char   *host,
		    int           port, 
		    XrdOucLogger *logger);

  
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
  int login(kXR_char      *username,
	    kXR_char       role[1],
	    kXR_int32      tlen = 0,
	    kXR_char      *token = 0,
	    kXR_char      *reserved = 0);

  /**
   * Authenticate a client to a server
   *
   * Input:   credtype  - This can be a supported creditial type like
   *                         krb4 ... Kerberos 4
   *                         krb5 ... Kerberos 5
   *          cred      - User supplied credential
   *
   * Output:  return 0 if OK, or an error number otherwise.
   */
  int auth(kXR_char        credtype[4] = 0,
	   kXR_char       *cred        = 0);

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
  int open(kXR_char  *path,
	   kXR_int16  oflag,
	   kXR_int16  mode);

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
  ssize_t read(void      *buffer, 
	       kXR_int64  offset, 
	       kXR_int32  blen);

  /**
   *  Get file status.
   *
   *  Input:  buffer    - buffer to hold the stat structure
   *          path      - Path of the file to get status: optional. By default,
   *                      the file currently open will be used
   *
   * Output:  return 0 upon success; -errno otherwise.
   */
  int stat(struct stat *buffer,
	   kXR_char    *path);
 
  /**
   * Close a remote file
   *
   * Output: return 0 upon success; -errno otherwise.
   */
  int close();
  
  /**
   * Return the error info object.
   */
  XrdOucErrInfo* getErrorInfo() {return errInfo;};


  /**
   * Get host type (either data server or load balancer)
   *
   * Output:  returns either kXR_DataServer or kXR_LBalServer
   */
  int getHostType()             {return hostType;};


  /**
   * Return the time to wait in case the server sent kXR_wait 
   */
  int getWaitTime()             {return waitTime;};

  /**
   * Return the redirection host in case the server sent kXR_redirect
   */
  char* getRedirectHost() {return redirectHost;};

  /**
   * Return the redirection port in case the server sent kXR_redirect
   */
  int getRedirectPort()         {return redirectPort;};

  ~XrdXrClientWorker();

private:

   /**
   * Do the initial handshake that is required prior to the login request
   * 
   * Output: return 0 if ok, otherwise an error number
   */
  int initialHandshake();

  /**
   *  Generate a stream ID and return it.
   */
  const char* getStreamId();

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
  int handleError(kXR_int32      dlen, 
		  kXR_int16      status,
		  char          *method);

  const char* hostname_;         // hostname of the remote xrootd to connect
  int port_;                     // port number of the remote xrootd

  XrdOucLink *lp;                // object for sending and receiving info
  XrdOucNetwork *xrootd;         // information about remote host

  kXR_char fhandle[4];           // file handle 
  kXR_char *filepath;            // file path of the file that was opened
  
  char *tident;                  // used for TRACE messages
  char *sec;                     // security token used for authentication 

  int streamIdBase;              // used to generate a stream id    
 
  XrdOucErrInfo *errInfo;        // holds the current error object
  int hostType;                  // data server or load balancer
  int waitTime;                  // time to wait in kXR_wait response
  int redirectPort;              // redirection port in kXR_redirect response
  char *redirectHost;            // redirection host in kXR_redirect response
  int redirect;                  // indicates if a redirection occured

};


// Helper methods for converting long long 
//
unsigned long long my_htonll(unsigned long long x);
unsigned long long my_ntohll(unsigned long long x);
bool               isLittleEndian();


#endif
