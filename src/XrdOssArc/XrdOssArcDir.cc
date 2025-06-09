/******************************************************************************/
/*                                                                            */
/*                       X r d O s s A r c D i r . c c                        */
/*                                                                            */
/* (c) 2025 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include "XrdOssArc/XrdOssArcDir.hh"
#include "XrdOssArc/XrdOssArcZipFile.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"

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
extern XrdSysError     Elog;

extern XrdSysTrace     ArcTrace;

extern thread_local XrdOucECMsg ecMsg;
}
using namespace XrdOssArcGlobals;

#define Neg(x) (x > 0 ? -x : x)

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdOssArcDir::~XrdOssArcDir()
{   delete ossDF;
    if (zFile) delete zFile;
}
  
/******************************************************************************/
/*                                 C l o s e                                  */
/******************************************************************************/

int XrdOssArcDir::Close(long long *retsz)
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
/*                             g e t E r r M s g                              */
/******************************************************************************/

bool XrdOssArcDir::getErrMsg(std::string& eText)
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
/*                               O p e n d i r                                */
/******************************************************************************/
  
int XrdOssArcDir::Opendir(const char *path, XrdOucEnv &env)
{
   TraceInfo("Opendir", ossDF->getTID());
   int rc, arcFD;

// Construct a minimal information object
//
   XrdOssArcCompose dsInfo(path, 0, rc, false);

// If this is not out path, forward it along
//
   if (rc == EDOM) return ossDF->Opendir(path, env);

// We don't support directory listings for backup paths
//
   if (dsInfo.didType == XrdOssArcCompose::isBKP) return EPERM;

// Whether this is a request for an archve or a file in the archive, we
// need to bring the archive file online. We do this first.
//
   char arcPath[MAXPATHLEN];
   if ((rc = dsInfo.ArcPath(arcPath, sizeof(arcPath), true)))
      {Elog.Emsg("opendir", rc, "instantiate path", arcPath);
       return -rc;
      }

// Open the directory
//
   DEBUG("Dir="<<arcPath);
   if ((arcFD = XrdSysFD_Open(arcPath, O_RDONLY)) < 0)
      {rc = errno;
       Elog.Emsg("opendir", rc, "open directory", arcPath);
       return -rc;
      }

// We now promote the newly opened directory to the wrapped directory
// object as it will handle all of the directory methods. We just do the open
// to bypass all of the name2name mapping.
//
    rc = ossDF->Fctl(XrdOssDF::Fctl_setFD,sizeof(int),(const char*)&arcFD);
    if (rc)
       {Elog.Emsg("opendir", rc, "promote open of", arcPath);
        close(arcFD);
        return Neg(rc);
       }

// All done
//
   return XrdOssOK;
}
