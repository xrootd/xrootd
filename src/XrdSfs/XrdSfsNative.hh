#ifndef __SFS_NATIVE_H__
#define __SFS_NATIVE_H__
/******************************************************************************/
/*                                                                            */
/*                       X r d S f s N a t i v e . h h                        */
/*                                                                            */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <sys/types.h>
#include <cstring>
#include <dirent.h>
  
#include "XrdSfs/XrdSfsInterface.hh"

class XrdSysError;
class XrdSysLogger;

/******************************************************************************/
/*                 X r d S f s N a t i v e D i r e c t o r y                  */
/******************************************************************************/
  
class XrdSfsNativeDirectory : public XrdSfsDirectory
{
public:

        int         open(const char              *dirName,
                         const XrdSecClientName  *client = 0,
                         const char              *opaque = 0);

        const char *nextEntry();

        int         close();

const   char       *FName() {return (const char *)fname;}

                    XrdSfsNativeDirectory(char *user=0, int monid=0)
                                : XrdSfsDirectory(user, monid)
                                {ateof = 0; fname = 0;
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
  
class XrdSfsAio;

class XrdSfsNativeFile : public XrdSfsFile
{
public:

        int            open(const char                *fileName,
                                  XrdSfsFileOpenMode   openMode,
                                  mode_t               createMode,
                            const XrdSecClientName    *client = 0,
                            const char                *opaque = 0);

        int            close();

        using          XrdSfsFile::fctl;
        int            fctl(const int               cmd,
                            const char             *args,
                                  XrdOucErrInfo    &out_error);

        const char    *FName() {return fname;}

        int            getMmap(void **Addr, off_t &Size)
                              {if (Addr) Addr = 0; Size = 0; return SFS_OK;}

        int            read(XrdSfsFileOffset   fileOffset,
                            XrdSfsXferSize     preread_sz) {return SFS_OK;}

        XrdSfsXferSize read(XrdSfsFileOffset   fileOffset,
                            char              *buffer,
                            XrdSfsXferSize     buffer_size);

        int            read(XrdSfsAio *aioparm);

        XrdSfsXferSize readv(XrdOucIOVec      *readV,
                             int               readCount);

        XrdSfsXferSize write(XrdSfsFileOffset   fileOffset,
                             const char        *buffer,
                             XrdSfsXferSize     buffer_size);

        int            write(XrdSfsAio *aioparm);

        int            sync();

        int            sync(XrdSfsAio *aiop);

        int            stat(struct stat *buf);

        int            truncate(XrdSfsFileOffset   fileOffset);

        int            getCXinfo(char cxtype[4], int &cxrsz) {return cxrsz = 0;}

                       XrdSfsNativeFile(char *user=0, int monid=0)
                                       : XrdSfsFile(user, monid)
                                          {oh = -1; fname = 0;}
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
        XrdSfsDirectory *newDir(char *user=0, int monid=0)
                        {return (XrdSfsDirectory *)new XrdSfsNativeDirectory(user,monid);}

        XrdSfsFile      *newFile(char *user=0,int monid=0)
                        {return      (XrdSfsFile *)new XrdSfsNativeFile(user,monid);}

// Other Functions
//
        int            chmod(const char             *Name,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client = 0,
                             const char             *opaque = 0);

        int            exists(const char                *fileName,
                                    XrdSfsFileExistence &exists_flag,
                                    XrdOucErrInfo       &out_error,
                              const XrdSecClientName    *client = 0,
                              const char                *opaque = 0);

        int            fsctl(const int               cmd,
                             const char             *args,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client = 0);

        int            getStats(char *buff, int blen) {return 0;}

const   char          *getVersion();

        int            mkdir(const char             *dirName,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client = 0,
                             const char             *opaque = 0);

        int            prepare(      XrdSfsPrep       &pargs,
                                     XrdOucErrInfo    &out_error,
                               const XrdSecClientName *client = 0) {return 0;}

        int            rem(const char             *path,
                                 XrdOucErrInfo    &out_error,
                           const XrdSecClientName *client = 0,
                           const char             *opaque = 0);

        int            remdir(const char             *dirName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecClientName *client = 0,
                              const char             *opaque = 0);

        int            rename(const char             *oldFileName,
                              const char             *newFileName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecClientName *client = 0,
                              const char             *opaqueO = 0,
                              const char             *opaqueN = 0);

        int            stat(const char             *Name,
                                  struct stat      *buf,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecClientName *client = 0,
                            const char             *opaque = 0);

        int            stat(const char             *Name,
                                  mode_t           &mode,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecClientName *client = 0,
                            const char             *opaque = 0)
                       {struct stat bfr;
                        int rc = stat(Name, &bfr, out_error, client);
                        if (!rc) mode = bfr.st_mode;
                        return rc;
                       }

        int            truncate(const char             *Name,
                                      XrdSfsFileOffset fileOffset,
                                      XrdOucErrInfo    &out_error,
                                const XrdSecEntity     *client = 0,
                                const char             *opaque = 0);

// Common functions
//
static  int            Mkpath(const char *path, mode_t mode, 
                              const char *info=0);

static  int            Emsg(const char *, XrdOucErrInfo&, int, const char *x,
                            const char *y="");

                       XrdSfsNative(XrdSysError *lp);
virtual               ~XrdSfsNative() {}

private:

static XrdSysError *eDest;
};
#endif
