#ifndef __SSI_SFS_H__
#define __SSI_SFS_H__
/******************************************************************************/
/*                                                                            */
/*                          X r d S s i S f s . h h                           */
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

#include "XrdSfs/XrdSfsInterface.hh"

#include "XrdSsi/XrdSsiDir.hh"
#include "XrdSsi/XrdSsiFile.hh"
  
struct XrdVersionInfo;

class  XrdOucEnv;
class  XrdSecEntity;
  
class XrdSsiSfs : public XrdSfsFileSystem
{
friend class XrdSsiFile;

public:

// Object allocation
//
        XrdSfsDirectory *newDir(char *user=0, int MonID=0)
                                {return new XrdSsiDir(user, MonID);}

        XrdSfsFile      *newFile(char *user=0,int MonID=0)
                                {return new XrdSsiFile(user, MonID);}

// Other functions
//
        int            chksum(      csFunc            Func,
                              const char             *csName,
                              const char             *path,
                                    XrdOucErrInfo    &eInfo,
                              const XrdSecEntity     *client = 0,
                              const char             *opaque = 0);

        int            chmod(const char             *Name,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client,
                             const char             *opaque = 0);

        void           EnvInfo(XrdOucEnv *envP);

        int            exists(const char                *fileName,
                                    XrdSfsFileExistence &exists_flag,
                                    XrdOucErrInfo       &eInfo,
                              const XrdSecEntity        *client,
                              const char                *opaque = 0);

        int            fsctl(const int               cmd,
                             const char             *args,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client);

        int            getStats(char *buff, int blen);

const   char          *getVersion();

        int            mkdir(const char             *dirName,
                                   XrdSfsMode        Mode,
                                   XrdOucErrInfo    &eInfo,
                             const XrdSecEntity     *client,
                             const char             *opaque = 0);

        int            prepare(      XrdSfsPrep       &pargs,
                                     XrdOucErrInfo    &eInfo,
                               const XrdSecEntity     *client = 0);

        int            rem(const char             *path,
                                 XrdOucErrInfo    &eInfo,
                           const XrdSecEntity     *client,
                           const char             *info = 0);

        int            remdir(const char             *dirName,
                                    XrdOucErrInfo    &eInfo,
                              const XrdSecEntity     *client,
                              const char             *info = 0);

        int            rename(const char             *oldFileName,
                              const char             *newFileName,
                                    XrdOucErrInfo    &eInfo,
                              const XrdSecEntity     *client,
                              const char             *infoO = 0,
                              const char             *infoN = 0);

        int            stat(const char             *Name,
                                  struct stat      *buf,
                                  XrdOucErrInfo    &eInfo,
                            const XrdSecEntity     *client,
                            const char             *opaque = 0);

        int            stat(const char             *Name,
                                  mode_t           &mode,
                                  XrdOucErrInfo    &eInfo,
                            const XrdSecEntity     *client,
                            const char             *opaque = 0);

        int            truncate(const char             *Name,
                                      XrdSfsFileOffset fileOffset,
                                      XrdOucErrInfo    &eInfo,
                                const XrdSecEntity     *client = 0,
                                const char             *opaque = 0);

// Management functions
//
static  void           setMax(int mVal) {freeMax = mVal;}

                       XrdSsiSfs() {}
virtual               ~XrdSsiSfs() {}  // Too complicate to delete :-)

private:
static int             freeMax;

int         Emsg(const char *pfx, XrdOucErrInfo &einfo, int ecode,
                 const char *op,  const char    *target);
const char *Split(const char *Args, const char **Opq, char *Path, int Plen);
};
#endif
