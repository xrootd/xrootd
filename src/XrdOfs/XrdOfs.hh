#ifndef __OFS_API_H__
#define __OFS_API_H__
/******************************************************************************/
/*                                                                            */
/*                             X r d O f s . h h                              */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <string.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
  
#include "XrdOfs/XrdOfsHandle.hh"
#include "XrdOdc/XrdOdcFinder.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include "XrdOuc/XrdOucPList.hh"
#include "XrdSfs/XrdSfsInterface.hh"

class XrdOssDir;
class XrdOucEnv;
class XrdOucError;
class XrdOucLogger;
class XrdOucStream;

/******************************************************************************/
/*                       X r d O f s D i r e c t o r y                        */
/******************************************************************************/
  
class XrdOfsDirectory : public XrdSfsDirectory
{
public:

        int         open(const char              *dirName,
                         const XrdSecClientName  *client);

        const char *nextEntry();

        int         close();

inline  void        copyError(XrdOucErrInfo &einfo) {einfo = error;}

const   char       *FName() {return (const char *)fname;}

        XrdOfsDirectory() {dp    = (XrdOssDir *)0;
                           tident = (char *)""; fname = 0; atEOF = 0;
                          }
       ~XrdOfsDirectory() {if (dp) close();}

private:

XrdOssDir     *dp;
char          *tident;
char          *fname;
int            atEOF;
char           dname[MAXNAMLEN];
};

/******************************************************************************/
/*                            X r d O f s F i l e                             */
/******************************************************************************/
  
class XrdOfsFile : public XrdSfsFile
{
public:

        int          open(const char                *fileName,
                                XrdSfsFileOpenMode   openMode,
                                mode_t               createMode,
                          const XrdSecClientName    *client,
                          const char                *opaque = 0);

        int          close();

        const char  *FName() {return (oh ? oh->Name() : "?");}

        int            read(XrdSfsFileOffset   fileOffset,   // Preread only
                            XrdSfsXferSize     amount);

        XrdSfsXferSize read(XrdSfsFileOffset   fileOffset,
                            char              *buffer,
                            XrdSfsXferSize     buffer_size);

        int            read(XrdSfsAIO *aioparm);

        XrdSfsXferSize write(XrdSfsFileOffset   fileOffset,
                             const char        *buffer,
                             XrdSfsXferSize     buffer_size);

        int            write(XrdSfsAIO *aioparm);

        XrdSfsAIO     *waitaio();

        int            sync();

        int            stat(struct stat *buf);

        int            truncate(XrdSfsFileOffset   fileOffset);

        int            getCXinfo(char cxtype[4], int &cxrsz);

        XrdOfsFile() {oh = (XrdOfsHandle *)0; dorawio = 0;
                      gettimeofday(&tod, 0);
                     }

       ~XrdOfsFile() {if (oh) close();}
private:

       void         setCXinfo(XrdSfsFileOpenMode mode);
       void         TimeStamp() {gettimeofday(&tod, 0);}
        int         Unclose();

XrdOfsHandle  *oh;
int            dorawio;
struct timeval tod;
char          *tident;
};

/******************************************************************************/
/*                          C l a s s   X r d O f s                           */
/******************************************************************************/

class XrdAccAuthorize;
  
class XrdOfs : public XrdSfsFileSystem
{
friend class XrdOfsDirectory;
friend class XrdOfsFile;

public:

// File Functions
//
        XrdSfsFile *openFile(const char              *fileName,
                                 XrdSfsFileOpenMode   openMode,
                                 mode_t               createMode,
                                 XrdOucErrInfo       &out_error,
                           const XrdSecClientName    *client,
                           const char                *opaque = 0);

// Directory Functions
//
        XrdSfsDirectory *openDir(const char            *directoryPath,
                                     XrdOucErrInfo     &out_error,
                               const XrdSecClientName  *client);

        int            mkdir(const char             *dirName,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client);

// Other Functions
//
        int            chmod(const char             *Name,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client);

        int            exists(const char                *fileName,
                                    XrdSfsFileExistence &exists_flag,
                                    XrdOucErrInfo       &out_error,
                              const XrdSecClientName    *client);

        int            fsctl(const int               cmd,
                             const char             *args,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client) {return 0;}

        int            getStats(char *buff, int blen) {return 0;}

const   char          *getVersion();

        int            prepare(      XrdSfsPrep       &pargs,
                                     XrdOucErrInfo    &out_error,
                               const XrdSecClientName *client = 0);

        int            rem(const char             *path,
                                 XrdOucErrInfo    &out_error,
                           const XrdSecClientName *client)
                          {return remove('f', path, out_error, client);}

        int            remdir(const char             *dirName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecClientName *client)
                             {return remove('d', dirName, out_error, client);}

        int            rename(const char             *oldFileName,
                              const char             *newFileName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecClientName *client);

        int            stat(const char             *Name,
                                  struct stat      *buf,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecClientName *client);

        int            stat(const char             *Name,
                                  mode_t           &mode,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecClientName *client);

// Management functions
//
        int            Configure(XrdOucError &);

        void           Config_Display(XrdOucError &);

        int            Unopen(XrdOfsHandle *);

                       XrdOfs();
                      ~XrdOfs() {}  // Too complicate to delete :-)

/******************************************************************************/
/*                  C o n f i g u r a t i o n   V a l u e s                   */
/******************************************************************************/
  
// Configuration values for this filesystem
//
int   Options;        //    Various options

int   FDConn;         //    Number of conn file descriptors
int   FDOpen;         //    Number of open file descriptors
int   FDOpenMax;      //    Max open FD's before we do scan
int   FDMinIdle;      //    Min idle time in seconds
int   FDMaxIdle;      //    Max idle time before close

int   MaxDelay;       //    Max delay imposed during staging

int   LockTries;      //    Number of times to try for a lock
int   LockWait;       //    Number of milliseconds to wait for lock

char *HostName;       //    ->Our hostname
char *HostPref;       //    ->Our hostname with domain removed
char *ConfigFN;       //    ->Configuration filename

/******************************************************************************/
/*                 P r i v a t e   C o n f i g u r a t i o n                  */
/******************************************************************************/

private:
  
XrdAccAuthorize  *Authorization;  //    ->Authorization   Service
XrdOdcFinder     *Finder;         //    ->Distrib Cache   Service
XrdOdcFinderLCL  *Reporter;       //    ->Server Monitor  Reporter
XrdOdcFinderTRG  *Balancer;       //    ->Server Balancer Interface

// The following structure defines an anchor for the valid file list. There is
// one entry in the list for each validpath directive. When a request comes in,
// the named path is compared with entries in the VFlist. If no prefix match is
// found, the request is treated as being invalid (i.e., EACCES).
//
XrdOucPListAnchor VPlist;     // -> Valid file list
XrdOucPListAnchor VPlist_New; // Used during construction

/******************************************************************************/
/*                            O t h e r   D a t a                             */
/******************************************************************************/

// Common functions
//
        int   Close(XrdOfsHandle *);
static  int   Emsg(const char *, XrdOucErrInfo  &, int, const char *x,
                   const char *y="");
        void  Detach_Name(const char *);
        int   remove(const char type, const char *path,
                     XrdOucErrInfo &out_error, const XrdSecClientName *client);
        int   Stall(XrdOucErrInfo  &, int, const char *);

// Function used during Configuration
//
int           ConfigRedir(XrdOucError &Eroute);
int           ConfigXeq(char *var, XrdOucStream &, XrdOucError &);
const char   *Fname(const char *);
int           isMe(XrdOucError &, const char *item, char *hval);
void          List_VPlist(char *, XrdOucPListAnchor &, XrdOucError &);
char         *WaitTime(int, char *, int);
int           xfdscan(XrdOucStream &, XrdOucError &);
int           xforward(XrdOucStream &, XrdOucError &);
int           xlocktry(XrdOucStream &, XrdOucError &);
int           xmaxd(XrdOucStream &, XrdOucError &);
int           xred(XrdOucStream &, XrdOucError &);
int           xtrace(XrdOucStream &, XrdOucError &);
};
#endif
