#ifndef __BWM_API_H__
#define __BWM_API_H__
/******************************************************************************/
/*                                                                            */
/*                             X r d B w m . h h                              */
/*                                                                            */
/* (c) 2008 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstring>
#include <dirent.h>
#include <sys/types.h>
  
#include "XrdBwm/XrdBwmHandle.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSfs/XrdSfsInterface.hh"

class XrdOucEnv;
class XrdSysError;
class XrdSysLogger;
class XrdOucStream;
class XrdSfsAio;

struct XrdVersionInfo;

/******************************************************************************/
/*                       X r d B w m D i r e c t o r y                        */
/******************************************************************************/
  
class XrdBwmDirectory : public XrdSfsDirectory
{
public:

        int         open(const char              *dirName,
                         const XrdSecEntity      *client,
                         const char              *opaque = 0);

        const char *nextEntry();

        int         close();

inline  void        copyError(XrdOucErrInfo &einfo) {einfo = error;}

const   char       *FName() {return "";}

                    XrdBwmDirectory(const char *user, int monid)
                                   : XrdSfsDirectory(user, monid),
                                     tident(user ? user : "") {}

virtual            ~XrdBwmDirectory() {}

protected:
const char    *tident;
};

/******************************************************************************/
/*                            X r d B w m F i l e                             */
/******************************************************************************/
  
class XrdBwmFile : public XrdSfsFile
{
public:

        int          open(const char                *fileName,
                                XrdSfsFileOpenMode   openMode,
                                mode_t               createMode,
                          const XrdSecEntity        *client,
                          const char                *opaque = 0);

        int          close();

        using        XrdSfsFile::fctl;

        int          fctl(const int               cmd,
                          const char             *args,
                                XrdOucErrInfo    &out_error);

        const char  *FName() {return (oh ? oh->Name() : "?");}

        int          getMmap(void **Addr, off_t &Size);

        int            read(XrdSfsFileOffset   fileOffset,   // Preread only
                            XrdSfsXferSize     amount);

        XrdSfsXferSize read(XrdSfsFileOffset   fileOffset,
                            char              *buffer,
                            XrdSfsXferSize     buffer_size);

        int            read(XrdSfsAio *aioparm);

        XrdSfsXferSize write(XrdSfsFileOffset   fileOffset,
                             const char        *buffer,
                             XrdSfsXferSize     buffer_size);

        int            write(XrdSfsAio *aioparm);

        int            sync();

        int            sync(XrdSfsAio *aiop);

        int            stat(struct stat *buf);

        int            truncate(XrdSfsFileOffset   fileOffset);

        int            getCXinfo(char cxtype[4], int &cxrsz);

                       XrdBwmFile(const char *user, int monid);

virtual               ~XrdBwmFile() {if (oh) close();}

protected:
       const char   *tident;

private:

XrdBwmHandle  *oh;
};

/******************************************************************************/
/*                          C l a s s   X r d B w m                           */
/******************************************************************************/

class XrdAccAuthorize;
class XrdBwmLogger;
class XrdBwmPolicy;

class XrdBwm : public XrdSfsFileSystem
{
friend class XrdBwmDirectory;
friend class XrdBwmFile;

public:

// Object allocation
//
        XrdSfsDirectory *newDir(char *user=0, int monid=0)
                        {return (XrdSfsDirectory *)new XrdBwmDirectory(user,monid);}

        XrdSfsFile      *newFile(char *user=0, int monid=0)
                        {return      (XrdSfsFile *)new XrdBwmFile(user,monid);}

// Other functions
//
        int            chmod(const char             *Name,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecEntity     *client,
                             const char             *opaque = 0);

        int            exists(const char                *fileName,
                                    XrdSfsFileExistence &exists_flag,
                                    XrdOucErrInfo       &out_error,
                              const XrdSecEntity        *client,
                              const char                *opaque = 0);

        int            fsctl(const int               cmd,
                             const char             *args,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecEntity     *client);

        int            getStats(char *buff, int blen) {return 0;}

const   char          *getVersion();

        int            mkdir(const char             *dirName,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &out_error,
                             const XrdSecEntity     *client,
                             const char             *opaque = 0);

        int            prepare(      XrdSfsPrep       &pargs,
                                     XrdOucErrInfo    &out_error,
                               const XrdSecEntity     *client = 0);

        int            rem(const char             *path,
                                 XrdOucErrInfo    &out_error,
                           const XrdSecEntity     *client,
                           const char             *info = 0)
                          {return remove('f', path, out_error, client, info);}

        int            remdir(const char             *dirName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecEntity     *client,
                              const char             *info = 0)
                             {return remove('d',dirName,out_error,client,info);}

        int            rename(const char             *oldFileName,
                              const char             *newFileName,
                                    XrdOucErrInfo    &out_error,
                              const XrdSecEntity     *client,
                              const char             *infoO = 0,
                               const char            *infoN = 0);

        int            stat(const char             *Name,
                                  struct stat      *buf,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecEntity     *client,
                            const char             *opaque = 0);

        int            stat(const char             *Name,
                                  mode_t           &mode,
                                  XrdOucErrInfo    &out_error,
                            const XrdSecEntity     *client,
                            const char             *opaque = 0);

        int            truncate(const char             *Name,
                                      XrdSfsFileOffset fileOffset,
                                      XrdOucErrInfo    &out_error,
                                const XrdSecEntity     *client = 0,
                                const char             *opaque = 0);
// Management functions
//
virtual int            Configure(XrdSysError &);

                       XrdBwm();
virtual               ~XrdBwm() {}  // Too complicate to delete :-)

/******************************************************************************/
/*                  C o n f i g u r a t i o n   V a l u e s                   */
/******************************************************************************/

XrdVersionInfo *myVersion;  //    ->Version compiled with

char *ConfigFN;       //    ->Configuration filename
char *HostName;       //    ->Our hostname
char *HostPref;       //    ->Our hostname with domain removed
char *myDomain;       //    ->Our domain name
int   myDomLen;       //
char  Authorize;
char  Reserved[7];

/******************************************************************************/
/*                       P r o t e c t e d   I t e m s                        */
/******************************************************************************/

protected:

virtual int   ConfigXeq(char *var, XrdOucStream &, XrdSysError &);
        int   Emsg(const char *, XrdOucErrInfo  &, int,
                   const char *, const char *y="");
        int   Emsg(const char *, XrdOucErrInfo  &, const char *,
                   const char *, const char *y="");
        int   Stall(XrdOucErrInfo  &, int, const char *);
  
/******************************************************************************/
/*                 P r i v a t e   C o n f i g u r a t i o n                  */
/******************************************************************************/

private:
  
XrdAccAuthorize  *Authorization;  //    ->Authorization   Service
char             *AuthLib;        //    ->Authorization   Library
char             *AuthParm;       //    ->Authorization   Parameters
XrdBwmLogger     *Logger;         //    ->Logger
XrdBwmPolicy     *Policy;         //    ->Policy
char             *PolLib;
char             *PolParm;
char             *locResp;        //    ->Locate Response
int               locRlen;        //      Length of locResp;
int               PolSlotsIn;
int               PolSlotsOut;

static XrdBwmHandle     *dummyHandle;
XrdSysMutex              ocMutex; // Global mutex for open/close

/******************************************************************************/
/*                            O t h e r   D a t a                             */
/******************************************************************************/

int           remove(const char type, const char *path,
                     XrdOucErrInfo &out_error, const XrdSecEntity     *client,
                     const char *opaque);
// Function used during Configuration
//
int           setupAuth(XrdSysError &);
int           setupPolicy(XrdSysError &);
int           xalib(XrdOucStream &, XrdSysError &);
int           xlog(XrdOucStream &, XrdSysError &);
int           xpol(XrdOucStream &, XrdSysError &);
int           xtrace(XrdOucStream &, XrdSysError &);
};
#endif
