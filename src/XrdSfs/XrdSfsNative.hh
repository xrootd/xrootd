#ifndef __SFS_NATIVE_H__
#define __SFS_NATIVE_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d S f s N a t i v e . h h                        */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*       All Rights Reserved. See XrdInfo.cc for complete License Terms       */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC03-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <sys/types.h>
#include <string.h>
#include <dirent.h>
  
#include "XrdSfs/XrdSfsInterface.hh"

class XrdOucError;
class XrdOucLogger;

/******************************************************************************/
/*                 X r d S f s N a t i v e D i r e c t o r y                  */
/******************************************************************************/
  
class XrdSfsNativeDirectory : public XrdSfsDirectory
{
public:

        int         open(const char              *dirName,
                         const XrdSecClientName  *client = 0);

        const char *nextEntry();

        int         close();

const   char       *FName() {return (const char *)fname;}

                    XrdSfsNativeDirectory() {ateof = 0; fname = 0;
                                             dh    = (DIR *)0;
                                             d_pnt = &dirent_full.d_entry;
                                            }
                   ~XrdSfsNativeDirectory() {if (dh) close();}
private:

DIR           *dh;  // Directory stream handle
char           ateof;
char          *fname;

struct {struct dirent d_entry;
               char   pad[MAXNAMLEN];   // This is only required for Solaris!
       } dirent_full;

struct dirent *d_pnt;

};

/******************************************************************************/
/*                      X r d S f s N a t i v e F i l e                       */
/******************************************************************************/
  
class XrdSfsNativeFile : public XrdSfsFile
{
public:

        int            open(const char                *fileName,
                                  XrdSfsFileOpenMode   openMode,
                                  mode_t               createMode,
                            const XrdSecClientName    *client = 0,
                            const char                *opaque = 0);

        int            close();

        const char    *FName() {return fname;}

        int            read(XrdSfsFileOffset   fileOffset,
                            XrdSfsXferSize     buffer_size) {return SFS_OK;}

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

        int            getCXinfo(char cxtype[4], int &cxrsz) {return cxrsz = 0;}

                       XrdSfsNativeFile() {oh = -1; fname = 0;}
                      ~XrdSfsNativeFile() {if (oh) close();}
private:

int   oh;
char *fname;

};

/******************************************************************************/
/*                          X r d S f s N a t i v e                           */
/******************************************************************************/
  
class XrdSfsNative : public XrdSfsFileSystem
{
public:

// Object Allocation Functions
//
        XrdSfsDirectory *newDir()  
                        {return (XrdSfsDirectory *)new XrdSfsNativeDirectory;}

        XrdSfsFile      *newFile() 
                        {return      (XrdSfsFile *)new XrdSfsNativeFile;}

// Other Functions
//
        int            chmod(const char             *Name,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client = 0);

        int            exists(const char                *fileName,
                                    XrdSfsFileExistence &exists_flag,
                                    XrdOucErrInfo       &out_error,
                              const XrdSecClientName    *client = 0);

        int            fsctl(const int               cmd,
                             const char             *args,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client = 0) {return 0;}

        int            getStats(char *buff, int blen) {return 0;}

const   char          *getVersion();

        int            mkdir(const char             *dirName,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client = 0);

        int            prepare(      XrdSfsPrep       &pargs,
                                     XrdOucErrInfo    &out_error,
                               const XrdSecClientName *client = 0) {return 0;}

        int            rem(const char             *path,
                                 XrdOucErrInfo    &out_error,
                           const XrdSecClientName *client = 0);

        int            remdir(const char             *dirName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecClientName *client = 0);

        int            rename(const char             *oldFileName,
                              const char             *newFileName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecClientName *client = 0);

        int            stat(const char             *Name,
                                  struct stat      *buf,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecClientName *client = 0);

        int            stat(const char             *Name,
                                  mode_t           &mode,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecClientName *client = 0)
                       {struct stat bfr;
                        int rc = stat(Name, &bfr, out_error, client);
                        if (!rc) mode = bfr.st_mode;
                        return rc;
                       }

// Common functions
//
static  int            Emsg(const char *, XrdOucErrInfo&, int, const char *x,
                            const char *y="");

                       XrdSfsNative(XrdOucError *lp);
virtual               ~XrdSfsNative() {}

private:

static XrdOucError *eDest;
};
#endif
