#ifndef _XRDXR_CLIENT_H
#define _XRDXR_CLIENT_H
/*****************************************************************************/
/*                                                                           */
/*                       X r d X r C l i e n t . h h                         */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*          Produced by Heinz Stockinger for Stanford University             */
/*****************************************************************************/

//         $Id$

// The following class is used to call XrdXrClientWorker for
// connections with an xrootd server. Whereas XrdXrClientWorker deals with
// direct client-server interactions, this class here mainly deals with
// redirection and wait responses from the server.

/*****************************************************************************/
/*                             i n c l u d e s                               */
/*****************************************************************************/


#include "XProtocol/XProtocol.hh"
#include "XrdOuc/XrdOucPthread.hh"

class XrdXrClientWorker;
class XrdOucLogger;

class  XrdXrClient
{
public:

  /**
   * Constructor
   * 
   * Input:  hostname  - Hostname of the xrootd to connect
   *         port      - port number of the remote xrootd
   *         logger    - logger object
   */
  XrdXrClient(const char   *hostname, 
	      int           port, 
	      XrdOucLogger *logger);

 
  /**
   * Login to a remote xrootd server.
   *
   * Input:   username  - Unix id (username) to login at the server
   *                      The username can have a maximum of 8 characters.
   *          role      - role can have one of following two values:
   *                            kXR_useruser  = 0
   *			        kXR_useradmin = 1
   *          token     - Char token supplied by a previous redirection response
   *
   * Output:  return 0 if OK, or an error number otherwise.
   */
  int login(kXR_char       *username,
	    kXR_char        role[1],
	    kXR_char       *token = 0);

  /**
   * Log out from the remote xrootd server and a potential open file.
   *
   * Output:  return 0 if OK, or an error number otherwise.
   */
  int logout();

  /**
   * Authenticate a client's username to a server
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
   * A possible redirection to another host is handled, too.
   *
   * Input:  path  - filepath of the remote file to be opened
   *         oflag - open flags such as: 0_RDONLY, 0_WRONLY, O_RDWR
   *                 Indicates if the file is opened for read, write or both.
   *         mode  - mode in which file will be opened when newly created
   *                 This corresponds to the file access permissions set at
   *                 file creation time.   
   *         token - the file opaque information or nil
   *         tlen  - is the length of the token or zero
   *
   * Output: return 0 upon success; -errno otherwise.
   */
  int open(kXR_char       *path,
	   kXR_unt16       oflag,
	   kXR_unt16       mode,
	   const char     *token=0,
	   int             tlen=0);

  /**
   * Read 'blen' bytes from the associated file, placing in 'buff'
   * the data and returning the actual number of bytes read.
   * A possible redirection to another host is handled, too.
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
	   kXR_char    *path = 0);
 
  /**
   * Close a remote file
   *
   * Output: return 0 upon success; -errno otherwise.
   */
  int close();

  /**
   *  Get the current host that is used to get file data.
   */
  char* getHost()                  {return hostname_;};

  /**
   *  Get the current port of the host that is used to get file data.
   */
  int getPort()                    {return port_;};


  /**
   *  Set maximum wait time a client waits. If the maximum time is reached,
   *  the client returns the number of waits to the caller (reduced by
   *  the time already waited).
   */
  void setMaxWaitTime(int seconds) {maxWaitTime = seconds;};


  /**
   *  Get maximum wait time 
   */
  int getMaxWaitTime()             {return maxWaitTime;};


  /**
   *  Switch on debugging.
   */
  void setDebug();

  ~XrdXrClient();

private:
  XrdXrClient() {};       // we do not allow that constructor
  
 
  /**
   * Set the current host to a new one.
   */
  void setHost(char *host);

  /**
   * Set the current port to a new one.
   */
  void setPort(int port) { mutex.Lock();     port_ = port; 
                           mutex.UnLock();}

  /**
   *  Logout from the current host and login to the new one: redirection.
   */
  int reconnect(const char        *epname); 

  /**
   * Wait for a certain amount of time. If the wait time is greater than the 
   * maximum wait the client can accept, we just return the error code and do
   * not wait.
   *
   * Input:   waitTime - time to wait as requested by the server
   *          epname   - name of the caller method
   *
   * Output:  0 on success; -4005 if wait time is longer than the max wait time
   */
  int handleWait(int waitTime, const char *epname);

  int mapError(int rc);

  char* hostOrig_;               // hostname of the original xrootd to connect
                                 // i.e. first host that is used  
  int portOrig_;                 // port number of the remote xrootd
  char* hostname_;               // current hostname
  int port_;                     // current port 

  XrdXrClientWorker *worker;     // Worker object that handles all interation
                                 // with a remote xrootd
  static XrdOucLogger *logger;   // global logger
  char *tident;                  // Used for TRACE messages
                                 // host
  int                maxRetry;   // max. retry count 
  int                maxWait;    // max. wait count
  int                maxWaitTime;// max. wait time in seconds a client waits 

  // Data structure to keep the status of a file
  //
  struct fileinfo {
    kXR_char  *path;
    kXR_char  *token;
    bool       open;
    kXR_unt16  oflag;
    kXR_unt16  mode;
    kXR_char  *username;
    kXR_char   role[1];
  } fileInfo;
  
  // Since the current code only allows one file to be opened and worked on at 
  // a time (synchronous mode), we need to lock the critical section. This
  // is a small performance loss but therefore we can guarantee 
  // serializability. In order to overcome this, asynchronous client-server
  // interaction would be required
  //
  XrdOucMutex   mutex;  


};

#endif
