#ifndef __SFS_INTERFACE_H__
#define __SFS_INTERFACE_H__
/******************************************************************************/
/*                                                                            */
/*                    X r d S f s I n t e r f a c e . h h                     */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//        $Id$

#include <string.h>      // For strlcpy()
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>  // for sockaddr

#include "XrdOuc/XrdOucErrInfo.hh"

/******************************************************************************/
/*                            O p e n   M o d e s                             */
/******************************************************************************/

#define SFS_O_RDONLY           0         // open read/only
#define SFS_O_WRONLY           1         // open write/only
#define SFS_O_RDWR             2         // open read/write
#define SFS_O_CREAT        0x100         // used for file creation
#define SFS_O_TRUNC        0x200         // used for file truncation
#define SFS_O_RAWIO   0x02000000         // allow client-side decompression
#define SFS_O_RESET   0x04000000         // Reset any cached information

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/

// Longest filename returned by interface (including trailing null)
//
#define SFS_MAX_FILE_NAME_LEN (1024+1)

// Maximum number of bytes of opaque information that can be set
//
#define SFS_MAX_ERROR_LEN 1280

// Return Values for Integer Returning XrdSfs Interface
//
#define SFS_STALL         1 // Return code -> Seconds to stall client
#define SFS_OK            0 // Return code -> All is well
#define SFS_ERROR        -1 // Return code -> Error occurred
#define SFS_REDIRECT   -256 // Return code -> Port number to redirect to

/******************************************************************************/
/*                 S t r u c t u r e s   &   T y p e d e f s                  */
/******************************************************************************/

typedef long long     XrdSfsFileOffset;
typedef int           XrdSfsFileOpenMode;
typedef int           XrdSfsMode;
typedef long          XrdSfsXferSize;

enum XrdSfsFileExistence 
{
     XrdSfsFileExistNo,
     XrdSfsFileExistIsFile,
     XrdSfsFileExistIsDirectory
};
//------------------------------------------------

struct XrdSfsAIO    // Asychronous I/O Parameters
{
       XrdSfsAIO       *next;      // Used for queuing inside and out
       char            *buffer;    // In (0 for preread action)
       XrdSfsFileOffset offset;    // In
       XrdSfsXferSize   size;      // In
       XrdSfsXferSize   result;    // Out (result of read/write)
       int              errcode;   // Out (errno  if result < 0)
};
//------------------------------------------------

#define Prep_PRTY0 0
#define Prep_PRTY1 1
#define Prep_PRTY2 2
#define Prep_PRTY3 3
#define Prep_PMASK 3
#define Prep_SENDAOK 4
#define Prep_SENDERR 8
#define Prep_SENDACK 12
#define Prep_WMODE   16

class XrdOucTList;

struct XrdSfsPrep  // Prepare parameters
{
       char            *reqid;     // Request ID
       char            *notify;    // Notification path or 0
       int              opts;      // Prep_xxx
       XrdOucTList     *paths;
};

/******************************************************************************/
/*                      A b s t r a c t   C l a s s e s                       */
/******************************************************************************/
/******************************************************************************/
/*                     s f s F i l e S y s t e m D e s c                      */
/******************************************************************************/

class  XrdOucTList;
class  XrdSfsFile;
class  XrdSfsDirectory;
struct XrdSecClientName;

class XrdSfsFileSystem
{
public:

// File Functions
//
virtual XrdSfsFile *openFile(const char             *fileName,
                                 XrdSfsFileOpenMode  openMode,
                                 mode_t              createMode,
                                 XrdOucErrInfo      &out_error,
                           const XrdSecClientName   *client = 0,
                           const char               *opaque = 0) = 0;

// Directory Functions
//
virtual XrdSfsDirectory *openDir(const char           *directoryPath,
                                     XrdOucErrInfo    &out_error,
                               const XrdSecClientName *client = 0) = 0;

virtual int            mkdir(const char             *dirName,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client = 0) = 0;

// Other Functions
//
virtual int            chmod(const char             *Name,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client = 0) = 0;

virtual int            fsctl(const int               cmd,
                             const char             *args,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client = 0) = 0;

virtual int            getStats(char *buff, int blen) = 0;

virtual const char    *getVersion() = 0;

virtual int            exists(const char                *fileName,
                                    XrdSfsFileExistence &exists_flag,
                                    XrdOucErrInfo       &out_error,
                              const XrdSecClientName    *client = 0) = 0;

virtual int            prepare(      XrdSfsPrep       &pargs,
                                     XrdOucErrInfo    &out_error,
                               const XrdSecClientName *client = 0) = 0;

virtual int            rem(const char             *path,
                                 XrdOucErrInfo    &out_error,
                           const XrdSecClientName *client = 0) = 0;

virtual int            remdir(const char             *dirName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecClientName *client = 0) = 0;

virtual int            rename(const char             *oldFileName,
                              const char             *newFileName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecClientName *client = 0) = 0;

virtual int            stat(const char             *Name,
                                  struct stat      *buf,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecClientName *client = 0) = 0;

virtual int            stat(const char             *Name,
                                  mode_t           &mode,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecClientName *client = 0) = 0;

// destruct called at server exit
//
void (*destruct)(); // can set to 0, to not destruct

XrdSfsFileSystem() {destruct = NULL;}
};

/******************************************************************************/
/*              F i l e   S y s t e m   I n s t a n t i a t o r               */
/******************************************************************************/

class XrdOucLogger;

XrdSfsFileSystem *XrdSfsGetFileSystem(XrdSfsFileSystem *native_fs,
                                     XrdOucLogger *Logger=0);

/******************************************************************************/
/*                               s f s F i l e                                */
/******************************************************************************/
  
class XrdSfsFile
{
public:
        XrdOucErrInfo  error;

virtual int            open(const char                *fileName,
                                  XrdSfsFileOpenMode   openMode,
                                  mode_t               createMode,
                            const XrdSecClientName    *client = 0,
                            const char                *opaque = 0) = 0;

virtual int            close() = 0;

virtual const char    *FName() = 0;

virtual int            read(XrdSfsFileOffset   fileOffset,
                          XrdSfsXferSize       buffer_size) = 0;

virtual XrdSfsXferSize read(XrdSfsFileOffset   fileOffset,
                          char                *buffer,
                          XrdSfsXferSize       buffer_size) = 0;

virtual int            read(XrdSfsAIO *aioparm) = 0;

virtual XrdSfsXferSize write(XrdSfsFileOffset  fileOffset,
                           const char         *buffer,
                           XrdSfsXferSize      buffer_size) = 0;

virtual int            write(XrdSfsAIO *aioparm) = 0;

virtual XrdSfsAIO     *waitaio() = 0;

virtual int            stat(struct stat *buf) = 0;

virtual int            sync() = 0;

virtual int            truncate(XrdSfsFileOffset fileOffset) = 0;

virtual int            getCXinfo(char cxtype[4], int &cxrsz) = 0;

virtual               ~XrdSfsFile() {}

}; // class XrdSfsFile

/******************************************************************************/
/*                          s f s D i r e c t o r y                           */
/******************************************************************************/
  
class XrdSfsDirectory
{
public:
        XrdOucErrInfo error;

virtual int         open(const char              *dirName,
                         const XrdSecClientName  *client = 0) = 0;

virtual const char *nextEntry() = 0;

virtual int         close() = 0;

virtual const char *FName() = 0;

virtual            ~XrdSfsDirectory() {}

}; // class XrdSfsDirectory
#endif
