//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClient                                                            //
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// A UNIX reference client for xrootd.                                  //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//         $Id$

#ifndef XRD_CLIENT_H
#define XRD_CLIENT_H


//////////////////////////////////////////////////////////////////////////
//                                                                      //
//                                                                      //
// Some features:                                                       //
//  - Automatic server kind recognition (xrootd load balancer, xrootd   //
//    data server, old rootd)                                           //
//  - Fault tolerance for read/write operations (read/write timeouts    //
//    and retry)                                                        //
//  - Internal connection timeout (tunable indipendently from the OS    //
//    one)                                                              //
//  - Full implementation of the xrootd protocol                        //
//  - handling of redirections from server                              //
//  - Connection multiplexing                                           //
//  - Asynchronous operation mode                                       //
//  - High performance read caching with read-ahead                     //
//  - Thread safe                                                       //
//  - Tunable log verbosity level (0 = nothing, 3 = dump read/write     //
//    buffers too!)                                                     //
//  - Many parameters configurable. But the default are OK for nearly   //
//    all the situations.                                               //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdClient/XrdClientAbs.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdClient/XrdClientThread.hh"
#include "XrdOuc/XrdOucSemWait.hh"

struct XrdClientOpenInfo {
    bool      inprogress;
    bool      opened;
    kXR_unt16 mode;
    kXR_unt16 options;
};

struct XrdClientStatInfo {
    int stated;
    long long size;
    long id;
    long flags;
    long modtime;
};

class XrdClient : public XrdClientAbs {
    friend void *FileOpenerThread(void*, XrdClientThread*);

private:

    char                        fHandle[4];  // The file handle returned by the server,
    // to use for successive requests

    struct XrdClientOpenInfo    fOpenPars;   // Just a container for the last parameters
    // passed to a Open method

    // The open request can be in progress. Further requests must be delayed until
    //  finished.
    XrdOucCondVar              *fOpenProgCnd;

    // Used to open a file in parallel
    XrdClientThread            *fOpenerTh;

    // Used to limit the maximum number of concurrent opens
    static XrdOucSemWait        fConcOpenSem;

    bool                        fOpenWithRefresh;

    XrdOucCondVar              *fReadWaitData;  // Used to wait for outstanding data   

    struct XrdClientStatInfo    fStatInfo;

    bool                        fUseCache;

    XrdOucString                fInitialUrl;
    XrdClientUrlInfo            fUrl;

    bool                        TryOpen(kXR_unt16 mode,
					kXR_unt16 options,
					bool doitparallel);

    bool                        LowOpen(const char *file,
					kXR_unt16 mode,
					kXR_unt16 options,
					char *additionalquery = 0);

    // The first position not read by the last read ahead
    long long                   fReadAheadLast;

    void                        TerminateOpenAttempt();

    bool                        TrimReadRequest(kXR_int64 &offs, kXR_int32 &len, kXR_int32 rasize);

    void                        WaitForNewAsyncData();

protected:

    virtual bool                OpenFileWhenRedirected(char *newfhandle,
						       bool &wasopen);

    virtual bool                CanRedirOnError() {
	// Can redir away on error if no file is opened
	// or the file is opened in read mode
	return ( !fOpenPars.opened || (fOpenPars.opened && (fOpenPars.options & kXR_open_read)) );
    }


public:

    XrdClient(const char *url);
    virtual ~XrdClient();

    UnsolRespProcResult         ProcessUnsolicitedMsg(XrdClientUnsolMsgSender *sender,
						      XrdClientMessage *unsolmsg);

    bool                        Close();

    // Ask the server to flush its cache
    bool                        Sync();

    // Copy the whole file to the local filesystem. Not very efficient.
    bool                        Copy(const char *localpath);

    // Quickly tells if the file is open
    inline bool                 IsOpen() { return fOpenPars.opened; }

    // Tells if the file is open, waiting for the completion of the parallel open
    bool                        IsOpen_wait();

    // Open the file. See the xrootd documentation for mode and options
    // If parallel, then the open is done by a separate thread, and
    // all the operations are delayed until the open has finished
    bool                        Open(kXR_unt16 mode, kXR_unt16 options, bool doitparallel=true);

    // Read a block of data. If no error occurs, it returns all the requested bytes.
    int                         Read(void *buf, long long offset, int len);

    // Submit an asynchronous read request. Its result will only populate the cache
    //  (if any!!)
    XReqErrorType               Read_Async(long long offset, int len);

    // Get stat info about the file
    bool                        Stat(struct XrdClientStatInfo *stinfo);

    // Write data to the file
    bool                        Write(const void *buf, long long offset, int len);

    // Hook to the open connection (needed by TXNetFile)
    XrdClientConn              *GetClientConn() const { return fConnModule; }

};

#endif
