#ifndef _XRDOSS_PROXY_H
#define _XRDOSS_PROXY_H
/*****************************************************************************/
/*                                                                           */
/*                         X r d O s s P r o x y . h h                       */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*           Produced by Heinz Stockinger for Stanford University            */
/*****************************************************************************/

//         $Id$

// The following class is used to create an Oss proxy object for communication 
// with a remote xrootd.


/*****************************************************************************/
/*                             i n c l u d e s                               */
/*****************************************************************************/

#include <stdlib.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>             // for size_t

#include "XrdOss/XrdOss.hh"
#include "XrdXr/XrdXrTrace.hh"
#include "XrdXr/XrdXrClient.hh"


class  XrdOssProxy : public XrdOssDF
{
public:

  XrdOssProxy(const char *hostname, int port)
             {hostname_ = strdup(hostname); port_ = port;}

  /**
   * Open the file 'path' in the mode indicated by 'mode'.
   *
   * Input:    path      - The fully qualified name of the file to open.
   *           oflag     - Standard open flags.
   *           mode      - Create mode (i.e., rwx).
   *           env       - Environmental information.
   *
   * Output:   XrdOssOK upon success; -errno otherwise.
   */
  int     Open(const char *path, int oflag, mode_t mode, XrdOucEnv &env);


  /**
   * Return file status for the associated file.
   *
   * Input:    buff      - Pointer to buffer to hold file status.
   *
   * Output:   Returns XrdOssOK upon success and -errno upon failure.
   */
  int     Fstat(struct stat *buff) {return client->stat(buff);};   


  /**
   * Read 'blen' bytes from the associated file, placing in 'buff'
   * the data and returning the actual number of bytes read.
   *
   * Input:    buff      - Address of the buffer in which to place the data.
   *           offset    - The absolute 64-bit byte offset at which to read.
   *           blen      - The size of the buffer. This is the maximum number
   *                       of bytes that will be read.
   *
   * Output:Returns the number bytes read upon success and -errno upon failure.
   */
  size_t  Read(void *buff, off_t offset, size_t blen)
                                   {return client->read(buff, offset, blen);}; 

  /**
   * Close a remote file
   *
   * Output: return 0 upon success; -errno otherwise.
   */
  int     Close()                  {return client->close();};

  XrdOssProxy() {hostname_ = 0;};
  XrdOssProxy(const char *) {hostname_ = 0;};
  ~XrdOssProxy() {if (hostname_) free(hostname_); delete client;};

  // Unsupported methods
  //
  size_t  Read(off_t, size_t)                {return 0;}; 
  int     Opendir(const char*)               {return 0;};
  int     Readdir(char *,int)                {return 0;};
  int     Fsync()                            {return 0;};
  int     Ftruncate(unsigned long long)      {return 0;};
  int     isCompressed(char *cxidp=0)        {return 0;};
  size_t  ReadRaw(void *, off_t, size_t)     {return 0;};
  size_t  Write(const void *, off_t, size_t) {return 0;};
  int     Handle()                           {return 0;};    

private:
  XrdXrClient *client;      // object to access remote file data 
  char        *hostname_;   // hostname of the remote xrootd
  int          port_;       // port number of xrootd
 
};

#endif
