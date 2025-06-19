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

#include "XrdOssArc/XrdOssArcCompose.hh"
#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcFile.hh"
#include "XrdOssArc/XrdOssArcStage.hh"
#include "XrdOssArc/XrdOssArcStopMon.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"
#include "XrdOssArc/XrdOssArcZipFile.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucECMsg.hh"

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

extern thread_local XrdOucECMsg ecMsg;
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
// oss did not open the file, so we do not issue a close to that. 
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
/*                             g e t E r r M s g                              */
/******************************************************************************/

bool XrdOssArcFile::getErrMsg(std::string& eText)
{
// Return any extened error mesage associated with this thread
//
   if (ecMsg.hasMsg())
      {std::string xMsg;
       if (ossDF->getErrMsg(xMsg))
          {ecMsg.Append();
           ecMsg.Msg("oss", xMsg.c_str());
          }
       ecMsg.Get(eText);
       return true;
      }
   return ossDF->getErrMsg(eText);
}
  
/******************************************************************************/
/*                                  O p e n                                   */
/******************************************************************************/
  
int XrdOssArcFile::Open(const char *path,int Oflag,mode_t Mode,XrdOucEnv &env)
{
   int rc, arcFD;
   bool isRW = (Oflag & (O_APPEND|O_CREAT|O_TRUNC|O_WRONLY|O_RDWR)) != 0; 

// Check what we should be doing here. Options are:
// a) Forward open to underlying storage system (not our path),
// b) Handle and error,
// c) Open an actual arvhive, or
// d) Open a file inside an archive.
//
   XrdOssArcCompose dsInfo(path, &env, rc, isRW);

   if (rc == EDOM) return ossDF->Open(path, Oflag, Mode, env);
   if (rc != 0)    return -rc;

// We will be doing a MSS orinted restore. This is subject to pausing.
// So, we must run under the control of the stop monitor.
//
   XrdOssArcStopMon stopMon(Config.stopMon);

// Whether this is a request for an archve or a file in the archive, we
// need to bring the archive file online. We do this first.
//
   char arcPath[MAXPATHLEN];
   if ((rc = dsInfo.ArcPath(arcPath, sizeof(arcPath), true))) return -rc;
   if ((rc = XrdOssArcStage::Stage(arcPath)))
      {if (rc == EINPROGRESS) return Config.wtpStage;
       return -rc;
      }

// If this was a request for the actual archive file, then open it and promote
// the open to the underlying file system as it will handle all I/O.
//
   if (dsInfo.didType == XrdOssArcCompose::isARC)
      {if ((arcFD = XrdSysFD_Open(arcPath, O_RDONLY, Mode)) < 0)
          {rc = errno;
           Elog.Emsg("open", rc, "open", arcPath);
           return -rc;
          }
       rc = ossDF->Fctl(XrdOssDF::Fctl_setFD,sizeof(int),(const char*)&arcFD);
       if (rc)
          {Elog.Emsg("open", rc, "promote open", arcPath);
           close(arcFD);
           return Neg(rc);
          }
       return XrdOssOK;
      }

// Create the name of the archive member in the archive file
//
   char arcMember[MAXPATHLEN];
   if ((rc = dsInfo.ArcMember(arcMember, sizeof(arcMember)))) return -rc;

// This is a request for a particular file. Get a zip file object and open it.
//
   zFile = new XrdOssArcZipFile(arcPath, rc);

// Open the member in the archive if possibe
//
   if (!rc) rc = zFile->Open(arcMember);

// Diagnose any errors
//
   if (rc)
      {Elog.Emsg("open", rc, "open archive", path);
       ecMsg.Msg("open", "Unable to access member", arcMember, "in archive",
                         arcPath);
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
