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
#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcDataset.hh"
#include "XrdOssArc/XrdOssArcRecompose.hh"
#include "XrdOssArc/XrdOssArcStage.hh"
#include "XrdOssArc/XrdOssArcZipFile.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"

#include "XrdOuc/XrdOucEnv.hh"

#include "XrdSec/XrdSecEntity.hh"

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
/*                                C r e a t e                                 */
/******************************************************************************/
  
int XrdOssArc::Create(const char* tid, const char* path, mode_t mode,
                      XrdOucEnv& env, int opts)
{
   int rc;
   XrdOssArcRecompose dsInfo(path, rc, true);

// Make sure all went well
//
   if (rc)
      {if (rc != EDOM)
          {Elog.Emsg("create", rc, "create dataset from", path);
           return -rc;
          }
       return wrapPI.Create(tid, path, mode, env, opts);
      }


// Indicate a creation event has occurred. This may create a dataset entry
// if this is the first reference to the dataset.
//
   if ((rc = XrdOssArcDataset::Create(tid, dsInfo))) return rc;

// We now need to create the file.
//
   char buff[MAXPATHLEN], *crPath = dsInfo.Compose(buff, sizeof(buff));

   if (!crPath) return -ENAMETOOLONG;
   opts |= XRDOSS_mkpath;
   return wrapPI.Create(tid, crPath, mode, env, opts);
}
  
/******************************************************************************/
/*                                U n l i n k                                 */
/******************************************************************************/
  
int XrdOssArc::Unlink(const char* path, int Opts, XrdOucEnv* envP)
{
   const char *tid = 0;
   int rc;
   XrdOssArcRecompose dsInfo(path, rc, true);

// Make sure all went well
//
   if (rc)
      {if (rc != EDOM)
          {Elog.Emsg("Unlink", rc, "unlink", path);
           return -rc;
          }
       return wrapPI.Unlink(path, Opts, envP);
      }

// Passthrough the unlink and return it there was an error
//
   char buff[MAXPATHLEN], *rmPath = dsInfo.Compose(buff, sizeof(buff));

   if (!rmPath) return -ENAMETOOLONG;
   if ((rc = wrapPI.Unlink(rmPath, Opts, envP))) return rc;

// Obtain the trace identifier
//
   if (envP)
      {const XrdSecEntity* secent = envP->secEnv();
       if (secent) tid = secent->tident;
      }

// Process this in case it refers to one of our pending files
//
   XrdOssArcDataset::Unlink(tid, dsInfo);
   return rc;
}
