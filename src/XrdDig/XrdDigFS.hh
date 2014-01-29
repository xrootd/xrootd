#ifndef __XRD_DIGFS_H__
#define __XRD_DIGFS_H__
/******************************************************************************/
/*                                                                            */
/*                           X r d D i g F S . h h                            */
/*                                                                            */
/* (c) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <string.h>
#include <dirent.h>
  
#include "XrdSfs/XrdSfsInterface.hh"

class XrdSysError;
class XrdSysLogger;

/******************************************************************************/
/*                       X r d D i g D i r e c t o r y                        */
/******************************************************************************/
  
class XrdDigDirectory : public XrdSfsDirectory
{
public:

        int         open(const char              *dirName,
                         const XrdSecClientName  *client = 0,
                         const char              *opaque = 0);

        const char *nextEntry();

        int         close();

const   char       *FName() {return (const char *)fname;}

                    XrdDigDirectory(char *user=0, int monid=0)
                                : XrdSfsDirectory(user, monid),
                                  dh((DIR *)0), fname(0),
                                  d_pnt(&dirent_full.d_entry),
                                  dirFD(-1), ateof(false),
                                  isProc(false), isBase(false) {}

                   ~XrdDigDirectory() {if (dh) close();}
private:

DIR           *dh;  // Directory stream handle
char          *fname;
struct dirent *d_pnt;
int            dirFD;
bool           ateof;
bool           isProc;
bool           isBase;

static const int aESZ = (MAXNAMLEN+MAXPATHLEN)/sizeof(const char *);

struct {struct dirent d_entry;
        union {const  char *aEnt[aESZ];
               char   nbf[MAXNAMLEN+MAXPATHLEN];
               char   pad[MAXNAMLEN];   // This is only required for Solaris!
              };
       } dirent_full;
};

/******************************************************************************/
/*                            X r d D i g F i l e                             */
/******************************************************************************/
  
class XrdSfsAio;

class XrdDigFile : public XrdSfsFile
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
                             XrdSfsXferSize     buffer_size) {return SFS_OK;}

        int            write(XrdSfsAio *aioparm) {return SFS_OK;}

        int            sync() {return SFS_OK;}

        int            sync(XrdSfsAio *aiop) {return SFS_OK;}

        int            stat(struct stat *buf);

        int            truncate(XrdSfsFileOffset   fileOffset) {return SFS_OK;}

        int            getCXinfo(char cxtype[4], int &cxrsz) {return cxrsz = 0;}

                       XrdDigFile(char *user=0, int monid=0)
                                       : XrdSfsFile(user, monid),
                                         oh(-1), fname(0), isProc(false) {}
                      ~XrdDigFile() {if (oh >= 0) close();}
private:

int   oh;
char *fname;
bool  isProc;
};

/******************************************************************************/
/*                              X r d D i g F S                               */
/******************************************************************************/
  
class XrdDigFS : public XrdSfsFileSystem
{
public:

// Object Allocation Functions
//
        XrdSfsDirectory *newDir(char *user=0, int monid=0)
                        {return (XrdSfsDirectory *)new XrdDigDirectory(user,monid);}

        XrdSfsFile      *newFile(char *user=0,int monid=0)
                        {return      (XrdSfsFile *)new XrdDigFile(user,monid);}

// Other Functions
//
        int            chmod(const char             *Name,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecClientName *client = 0,
                             const char             *opaque = 0)
                             {return Reject("chmod", Name, out_error);}

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
                             const char             *opaque = 0)
                            {return Reject("mkdir", dirName, out_error);}

        int            prepare(      XrdSfsPrep       &pargs,
                                     XrdOucErrInfo    &out_error,
                               const XrdSecClientName *client = 0) {return 0;}

        int            rem(const char             *path,
                                 XrdOucErrInfo    &out_error,
                           const XrdSecClientName *client = 0,
                           const char             *opaque = 0)
                          {return Reject("rm", path, out_error);}

        int            remdir(const char             *dirName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecClientName *client = 0,
                              const char             *opaque = 0)
                             {return Reject("rmdir", dirName, out_error);}

        int            rename(const char             *oldFileName,
                              const char             *newFileName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecClientName *client = 0,
                              const char             *opaqueO = 0,
                              const char             *opaqueN = 0)
                             {return Reject("rename", oldFileName, out_error);}

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
                                const char             *opaque = 0)
                               {return Reject("truncate", Name, out_error);}

// Common functions
//
static  int            Emsg(const char *, XrdOucErrInfo&, int, const char *x,
                            const char *y="");

static  int            Validate(const char *);

                       XrdDigFS() {}
virtual               ~XrdDigFS() {}

private:
int Reject(const char *op, const char *trg, XrdOucErrInfo&);
};
#endif
