/******************************************************************************/
/*                                                                            */
/*                          X r d O s s A r c . c c                           */
/*                                                                            */
/* (c) 2023 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC02-76-SFO0515 with the Deprtment of Energy             */
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

/******************************************************************************/
/*                             i n c l u d e s                                */
/******************************************************************************/

#include <fcntl.h>
#include <sys/stat.h>

#include "XrdVersion.hh"
#include "Xrd/XrdScheduler.hh"

#include "XrdOssArc/XrdOssArc.hh"
#include "XrdOssArc/XrdOssArcCompose.hh"
#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcStage.hh"
#include "XrdOssArc/XrdOssArcZipFile.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucECMsg.hh"

#include "XrdSec/XrdSecEntity.hh"

#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdOssArcGlobals
{
XrdOssArc*      ArcSS  = 0;

XrdOss*         ossP   = 0;

XrdScheduler*   schedP = 0;

XrdOssArcConfig Config;
  
XrdSysError     Elog(0, "OssArc_");

XrdSysTrace     ArcTrace("OssArc");

extern thread_local XrdOucECMsg ecMsg;
}
using namespace XrdOssArcGlobals;

#define Neg(x) (x > 0 ? -x : x)

/******************************************************************************/
/*               X r d O s s A d d S t o r a g e S y s t e m 2                */
/******************************************************************************/
  
// This function is called by the OFS layer to retrieve the Storage System
// object that wraps the previously loaded storage system object. The object
// returned is a storage system wrapper providing archive functionality.
//
XrdVERSIONINFO(XrdOssAddStorageSystem2,XrdOssArc)

extern "C"
{
XrdOss *XrdOssAddStorageSystem2(XrdOss       *curr_oss,
                                XrdSysLogger *Logger,
                                const char   *config_fn,
                                const char   *parms,
                                XrdOucEnv    *envP)
{
// Activate our error message object
//
   Elog.logger(Logger);

// Allocate an instance of the OssArc object
//
   ArcSS = new XrdOssArc(*curr_oss);
   ossP  = curr_oss;

// Initialize it
//
   if (ArcSS->Init(config_fn, parms, envP) != XrdOssOK)
      {delete ArcSS;
       return NULL;
      }

// Upon success, return it.
//
   return (XrdOss*)ArcSS;
}
}

/******************************************************************************/
/*                                 C h m o d                                  */
/******************************************************************************/

int XrdOssArc::Chmod(const char *path, mode_t mode, XrdOucEnv *envP)
{

// chmod is only valid for non-archive paths
//
   if (XrdOssArcCompose::isMine(path))
      {Elog.Emsg("Chmod", EROFS, "chmod using", path);
       return -EROFS;
      }

// Pass this through
//
   return wrapPI.Chmod(path, mode, envP);
}

/******************************************************************************/
/*                                C r e a t e                                 */
/******************************************************************************/
  
int XrdOssArc::Create(const char* tid, const char* path, mode_t mode,
                      XrdOucEnv& env, int opts)
{

// Create is only valid for non-archive paths
//
   if (XrdOssArcCompose::isMine(path))
      {Elog.Emsg("create", EROFS, "create file using", path);
           return -EROFS;
      }

// Pass this through
//
   return wrapPI.Create(tid, path, mode, env, opts);
}
  
/******************************************************************************/
/*                              F e a t u r e s                               */
/******************************************************************************/

uint64_t XrdOssArc::Features()
{
   return XRDOSS_HASXERT | wrapPI.Features(); 
}
  
/******************************************************************************/
/*                                 F S c t l                                  */
/******************************************************************************/

int XrdOssArc:: FSctl(int cmd, int alen, const char *args, char **resp)
{
// Pass this through
//
   return wrapPI.FSctl(cmd, alen, args, resp);
}
  
/******************************************************************************/
/*                             g e t E r r M s g                              */
/******************************************************************************/

bool XrdOssArc::getErrMsg(std::string& eText)
{
// Return any extened error mesage associated with this thread
//
   if (ecMsg.hasMsg())
      {ecMsg.Get(eText);
       return true;
      }
   return false;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
/*
  Function: Initialize staging subsystem

  Input:    None

  Output:   Returns zero upon success otherwise (-errno).
*/
int XrdOssArc::Init(const char* configfn, const char* parms, XrdOucEnv* envP)
{
   const char *ending = "completed.";
   int retc = EINVAL;

// Obtain pointer to the scheduler. If missing allocate one.
//
   if (!envP || !(schedP = (XrdScheduler*)envP->GetPtr("XrdScheduler*")))
      {schedP = new XrdScheduler;
       schedP->Start();
      }

// Print herald
//
   Elog.Say("++++++ Archive Storage System initialization started.");

// Configure the subsystems
//
   if ( (!Config.Configure(configfn, parms, envP)) ) ending = "failed!";
      else retc = XrdOssOK;

// Print closing herald
//
   Elog.Say("------ Archive Storage System initialization ", ending);

// All done.
//
   return retc;
}
  
/******************************************************************************/
/*                            L f n 2 P f n   v 1                             */
/******************************************************************************/

int XrdOssArc::Lfn2Pfn(const char *Path, char *buff, int blen)
{
   int rc = 0;

// Use v2 version to generate the Pfn
//
   if (Lfn2Pfn(Path, buff, blen, rc) == Path && !rc)
      {if ((int)strlen(Path) >= blen) rc = -ENAMETOOLONG;
          else strcpy(buff, Path);
      }
   return rc;
}
  
/******************************************************************************/
/*                            L f n 2 P f n   v 2                             */
/******************************************************************************/

const char *XrdOssArc::Lfn2Pfn(const char *Path, char *buff, int blen, int &rc)
{

// The caller typically needs the LFN2PFN mapping to handle file attributes,
// checksums, and prepare requests. All of these are disallowed for our
// internal paths so we simply fail the mapping here.
//
  if (XrdOssArcCompose::isMine(Path))
     {rc = -EPERM;
      return 0;
     }

// Use the underlying mapping.
//
   return wrapPI.Lfn2Pfn(Path, buff, blen, rc);
}

/******************************************************************************/
/*                                 M k d i r                                  */
/******************************************************************************/

int XrdOssArc::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv *envP)
{

// mkdir is only valid for non-archive paths
//
   if (XrdOssArcCompose::isMine(path))
      {Elog.Emsg("Mkdir", EROFS, "create directory using", path);
       return -EROFS;
      }

// Pass this through
//
   return wrapPI.Mkdir(path, mode, mkpath, envP);
}

/******************************************************************************/
/*                                R e m d i r                                 */
/******************************************************************************/

int XrdOssArc::Remdir(const char *path, int Opts, XrdOucEnv *envP)
{

// Remdir is only valid for non-archive paths
//
   if (XrdOssArcCompose::isMine(path))
      {Elog.Emsg("Remdir", EROFS, "remove", path);
       return -EROFS;
      }

// Pass this through
//
   return wrapPI.Remdir(path, Opts, envP);
}

/******************************************************************************/
/*                                R e n a m e                                 */
/******************************************************************************/

int XrdOssArc::Rename(const char *oldname, const char *newname,
                      XrdOucEnv  *old_env, XrdOucEnv  *new_env)
{

// Rename is only valid for non-archive paths
//
   if (XrdOssArcCompose::isMine(oldname) || (XrdOssArcCompose::isMine(newname)))
      {Elog.Emsg("Rename", EROFS, "rename", newname);
       return -EROFS;
      }

// Pass this through
//
   return wrapPI.Rename(oldname, newname, old_env, new_env);
}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/

int XrdOssArc::Stat(const char *path, struct stat *Stat,
                    int opts, XrdOucEnv *envP)
{
   TraceInfo("Stat", 0);
   char buff[MAXPATHLEN];
   int rc;

// Prepare to process the archive/backup request
//
   XrdOssArcCompose dsInfo(path, envP, rc, false);

// Make sure all went well
//
   if (rc)
      {if (rc != EDOM)
          {Elog.Emsg("Stat", rc, "locate", path);
           return -rc;
          }
       return wrapPI.Stat(path, Stat, opts, envP);
      }

// If this is a stat for an archive, we can do this here
//
   if (dsInfo.didType == dsInfo.isARC)
      {if ((rc = dsInfo.ArcPath(buff, sizeof(buff), true))) return -rc;
       if (stat(buff, Stat))
          {rc = errno;
           DEBUG("Stat archive "<<buff<<" failed; "<<XrdSysE2T(rc));
           return -rc;
          }
       return XrdOssOK;
      }

// The person want to stat a particular file in an archive. The most sensible
// way to do this is to ask the DM system for than information.
//
   if ((rc = dsInfo.Stat(dsInfo.flScope.c_str(), dsInfo.flName.c_str(), Stat)))
      {DEBUG("Stat file "<<dsInfo.flScope.c_str()<<':'<<dsInfo.flName.c_str()
             <<" failed; "<<XrdSysE2T(rc));
       return -rc;
      }
   return XrdOssOK;
}

/******************************************************************************/
/*                              T r u n c a t e                               */
/******************************************************************************/

int XrdOssArc::Truncate(const char *path, unsigned long long size,
                        XrdOucEnv *envP)
{

// Truncate is only valid for non-archive paths
//
   if (XrdOssArcCompose::isMine(path))
      {Elog.Emsg("Truncate", EROFS, "truncate", path);
       return -EROFS;
      }

// Pass this through
//
   return wrapPI.Truncate(path, size, envP);
}

/******************************************************************************/
/*                                U n l i n k                                 */
/******************************************************************************/
  
int XrdOssArc::Unlink(const char* path, int Opts, XrdOucEnv* envP)
{

// Unlink is only valid for non-archive paths
//
   if (XrdOssArcCompose::isMine(path))
      {Elog.Emsg("Rename", EROFS, "remove", path);
       return -EROFS;
      }

// Pass this through
//
   return wrapPI.Unlink(path, Opts, envP);
}
