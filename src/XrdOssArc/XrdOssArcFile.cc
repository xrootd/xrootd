/******************************************************************************/
/*                                                                            */
/*                      X r d O s s A r c F i l e . c c                       */
/*                                                                            */
/* (c) 2024 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
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


#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcFile.hh"
#include "XrdOssArc/XrdOssArcRecompose.hh"
#include "XrdOssArc/XrdOssArcStage.hh"
#include "XrdOssArc/XrdOssArcZipFile.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"

#include "XrdOuc/XrdOucEnv.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdOssArcGlobals
{
extern XrdOss*         ossP;

extern XrdOssArcConfig Config;
  
extern XrdSysError     Elog;

extern XrdSysTrace     ArcTrace;
}
using namespace XrdOssArcGlobals;

#define Neg(x) (x > 0 ? -x : x)

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOssArcFile::~XrdOssArcFile()
{   delete ossDF;
    if (zFile) delete zFile;
}
  
/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

int XrdOssArcFile::Close(long long *retsz)
{
   int rc;

// Issue close to possible zipfile appendage and delete it. The underlying
// oss did not open the file, so we not not issue a close to that. 
//
   if (zFile) 
      {rc = zFile->Close();
       if (retsz) *retsz = 0;
       delete zFile;
       zFile = 0;
      } else rc = ossDF->Close(retsz);

// All done
//
   return rc;
}
  
/******************************************************************************/
/*                                 f s t a t                                  */
/******************************************************************************/

int XrdOssArcFile::Fstat(struct stat* buf)
{
// Check if we should forward this call  
//
   if (zFile == 0) return ossDF->Fstat(buf);

// Obtain stat for the archive file member
//
   return zFile->Stat(*buf);
}

/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
int XrdOssArcFile::Open(const char *path,int Oflag,mode_t Mode,XrdOucEnv &env)
{
   int rc, tapeFD;
   bool isRW = (Oflag & (O_APPEND|O_CREAT|O_TRUNC|O_WRONLY|O_RDWR)) != 0; 
   bool isLocal;

// Check if we are trying to open the backup (e.g. /backup/.../Archive.zip)
//
   if (!strncmp(path, Config.bkupPathLFN, Config.bkupPathLEN))
      {if (isRW) return -EROFS;
       if (!XrdOssArcRecompose::isArcFile(path)) return -EPERM;
       if ((rc = XrdOssArcStage::Stage(path+Config.bkupPathLEN)))
          {if (rc == EINPROGRESS) return Config.wtpStage;
           if (rc != EEXIST) return -rc;
          }
       char opPath[MAXPATHLEN];
       rc = Config.GenTapePath(path+Config.bkupPathLEN,opPath,sizeof(opPath));
       if (rc)
          {Elog.Emsg("open", "prepare to open", path);
           return -rc;
          }
       if ((tapeFD = XrdSysFD_Open(opPath, O_RDONLY, Mode)) < 0)
          {rc = errno;
           Elog.Emsg("open", rc, "open", path);
           return -rc;
          }
       rc = ossDF->Fctl(XrdOssDF::Fctl_setFD,sizeof(int),(const char*)&tapeFD);
       if (rc)
          {Elog.Emsg("open", rc, "promote open", path);
           close(tapeFD);
           return rc;
          }
       return XrdOssOK;
      }

// Recompose this request
//
   XrdOssArcRecompose dsInfo(path, rc, isRW);

// Verify we can handle this
//
   if (rc)
      {if (rc == EDOM) return ossDF->Open(path, Oflag, Mode, env);
       Elog.Emsg("open", rc, "open", path);
       return -rc;
      }

// It is possible that the file still exists in our build cache. If this is
// the case, we can serve it from there (we always do for R/W requests.
//
   char opPath[MAXPATHLEN];
   if (!dsInfo.Compose(opPath, sizeof(opPath))) return -ENAMETOOLONG;
   if (!isRW)
      {struct stat Stat;
       isLocal = !(ossP->Stat(opPath, &Stat));
      } isLocal = false;

// This is an open to write an archive file then the file must exist because
// Create() would have been called prior to this open for writing.
//
   if (isRW || isLocal) return ossDF->Open(opPath, Oflag, Mode, env);

// If the client is looking for an individual file then the processing is
// very different as we must extract it from the archive file that has it.
// This file should be accessible via the tape buffer. The first step is to
// generate the path to the archive that holds the file.
//   
   char arcPath[1024];
   if (snprintf(arcPath,sizeof(arcPath),"%s/%s",dsInfo.arcDSN,Config.arFName) 
      >= (int)sizeof(arcPath)) return -ENAMETOOLONG;

// Now stage the file
//
   if ((rc = XrdOssArcStage::Stage(arcPath)))
      {if (rc == EINPROGRESS) return Config.wtpStage;
       if (rc != EEXIST) return -rc;
      }

// Get path as it exists in the tape buffer
//
   rc = Config.GenTapePath(dsInfo.arcDSN, arcPath, sizeof(arcPath), true); 
   if (rc) return Neg(rc);

// The archive is online, Get a zip file object and open it.
//
   zFile = new XrdOssArcZipFile(arcPath, rc);

// Open the member in the archive if possibe
//
   if (!rc) rc = zFile->Open(dsInfo.arcFile);

// Diagnose any errors
//
   if (rc)
      {Elog.Emsg("open", rc, "open archive", path);
       delete zFile; zFile = 0;
       return Neg(rc);
      }
   return 0;
}

/******************************************************************************/
/*                                  R e a d                                   */
/******************************************************************************/
  
ssize_t XrdOssArcFile::Read(void *buffer, off_t offset, size_t size)
{
// Execute read based on what kind of file we currently have
//
   if (zFile) return zFile->Read(buffer, offset, size);
   return ossDF->Read(buffer, offset, size);
}

/******************************************************************************/
/*                                 W r i t e                                  */
/******************************************************************************/
  
// Write calls are valid for regular files but not for archive files
//
ssize_t XrdOssArcFile::Write(const void *buffer, off_t offset, size_t size)
{
// Execute read based on what kind of file we currently have
//
   if (zFile) return -EBADF;
   return ossDF->Write(buffer, offset, size);
}
