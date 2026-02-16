/******************************************************************************/
/*                                                                            */
/*                   X r d O s s A r c C o m p o s e . c c                    */
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "XrdOssArc/XrdOssArcCompose.hh"
#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"

#include "XrdOuc/XrdOucECMsg.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"

#include "XrdSys/XrdSysE2T.hh"

#ifndef ENOANO
#define ENOANO 555
#endif

/******************************************************************************/
/*                        S t a t i c   O b j e c t s                         */
/******************************************************************************/

namespace XrdOssArcGlobals
{
extern XrdOssArcConfig Config;
extern XrdSysError     Elog;
extern thread_local XrdOucECMsg ecMsg;
}
using namespace XrdOssArcGlobals;

/******************************************************************************/

int   XrdOssArcCompose::minLenDSN = 4;
int   XrdOssArcCompose::minLenFN  = 4;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOssArcCompose::XrdOssArcCompose(const char *path, XrdOucEnv *env,
                                   int& rc, bool isW, bool optfn)
{
   TraceInfo("Compose",0);

// By conventioon, the env["ossarc.fn"] holds the file did and the path is
// the dataset did both represented as paths. We only support two path
// prefixes: 1) the archive prefix, and 2) the backup prefix. These are
// removed but we track which one it was as they have different semantics.
//

// Determine the path type being referenced
//
        if (!strncmp(Config.arcvPathLFN, path, Config.arcvPathLEN))
           {didType = isARC;
            path += Config.arcvPathLEN;
           }
   else if (!strncmp(Config.arcvPathLFN, path, Config.arcvPathLEN-1)
        &&   strlen(path) < (size_t)Config.arcvPathLEN)
           {didType = isARC;
            path += Config.arcvPathLEN-1;
           }
   else if (!strncmp(Config.bkupPathLFN, path, Config.bkupPathLEN))
           {didType = isBKP;
            path += Config.bkupPathLEN;
            optfn = false;  // ossarc.fn is required for /backup
           }
   else {rc = EDOM; return;}  // This will forward the request

// Get the dataset name and if there is no ENV, we can return as this is
// only a construction for datasets.
//
   if ((rc = getDSN(path))) return;
   if (!env) {rc = 0; return;}

// Prepare for full construction (note: we know we have an env pointer)
//
   const char* theFN = 0;
   bool fl2arc = false;
   bool fscpDS = false;

// This is our domain and that domain is strictly read/only
//
   if (isW) {rc = EROFS; return;}

// Make sure we have an env file name to work with. This must come from the CGI
// if /backup and from either the CGI path for /archive. We support optional
// path specification to allow recursive copies of all dataset archive files.
// Noe that we ignore the CGI if specified via the path.
//
   if (didType == isARC && isArcFile(path))
      {const char* fn = rindex(path, '/');
       if (fn && fn != path)
          {theFN = fn + 1;
           dsName.erase(dsName.find_last_of("/"));
          }
      }

   if (!theFN && !(theFN = env->Get("ossarc.fn")))
      {if (optfn) rc = 0;
          else {rc = EINVAL;
                ecMsg.Msg("Compose","CGI ossarc.fn=<target_fname> not specified");
               }
       return;
      }

// If the archive if the target then env filename may refer to a specific
// archive or be a file that is to be used to determine the target archive.
// Otherwise, it should refer to a single file backups so an arc file suffix
// is disallowed as it makes to sense.
//
   if (isArcFile(theFN))
      {if (didType == isBKP)
          {rc = EINVAL;
           ecMsg.Msg("Compose","Backup filename cannot refer to an archive");
           return;
          }
       arName = theFN;
      } else fl2arc = true;


// Pick apart the filename if this is a backup reference or an indirect
// archive reference as we will need the scope and file.
//
   if ((didType == isBKP) || fl2arc)
      {const char* colon = index(theFN, ':');
       if (!colon)
          {flScope = dsScope;
           flName  = theFN;
           fscpDS  = true;  // For tracing purposes
          } else {
           if (*theFN == ':')
              {rc = EINVAL;
               ecMsg.Msg("Compose","File scope not specified though implied");
               return;
              }
           flScope.assign(theFN, colon - theFN);
           flName = colon+1;
          }
      if ((int)flName.length() < minLenDSN)
         {rc = EINVAL;
          ecMsg.Msg("Compose", "Dataset name is too short");
          return;
         }
      }

// Do some debugging
//
   const char* atName[] = {"Archive", "Backup"};

   DEBUG("Type="<<atName[didType]<<" f2a="<<fl2arc<<" fscpDF="<<fscpDS
        <<" dsScope="<<dsScope<<" dsName="<<dsName
        <<" flScope="<<flScope<<" flName="<<flName<<" arName="<<arName);

// Set the archive name if we need to
//
   rc = (fl2arc ? SetarName() : 0);
}

/******************************************************************************/
/*                             A r c M e m b e r                              */
/******************************************************************************/

int XrdOssArcCompose::ArcMember(char* buff, int blen)
{
   if (snprintf(buff, blen, "%s:%s", flScope.c_str(), flName.c_str()) >= blen)
      {std::string fn = flScope + ":" + flName;
       Elog.Emsg("Compose", ENAMETOOLONG, "generate archive member name "
                            "in dataset", dsName.c_str());
       ecMsg.Msg("Compose", "Archive member name", fn.c_str(), "is too long");
       return ENAMETOOLONG;
      }
    return 0;
}

/******************************************************************************/
/*                               A r c P a t h                                */
/******************************************************************************/

int XrdOssArcCompose::ArcPath(char* buff, int blen, bool addafn)
{
   int n;

// The path to the archive is <tape_path>/<scope>/<dataset_did>/<arc_fname>
//
   if (addafn) n = snprintf(buff, blen, "%s/%s/%s/%s", Config.tapePath,
                            dsScope.c_str(), dsName.c_str(), arName.c_str());
      else n = snprintf(buff, blen, "%s/%s/%s", Config.tapePath,
                                    dsScope.c_str(), dsName.c_str());

// Verify that we did not truncate the path
//
   if (n >= blen)
      {std::string dsn = dsScope + ":" + dsName;
       Elog.Emsg("Compose", ENAMETOOLONG, "generate archive path for dataset",
                                          dsn.c_str());
       return ENAMETOOLONG;
      }

// All done
//
   return 0;
}

/******************************************************************************/
/*                               D S N 2 D i r                                */
/******************************************************************************/

std::string XrdOssArcCompose::DSN2Dir(const char* dsn)
{
   std::string retdir(dsn);
   int n = retdir.length();

   for (int i = 0; i < n; i++) if (retdir[i] == '/') retdir[i] = '%';

   return retdir;
}

/******************************************************************************/
/*                               D i r 2 D S N                                */
/******************************************************************************/

std::string XrdOssArcCompose::Dir2DSN(const char* dir)
{
   std::string retdsn(dir);
   int n = retdsn.length();

   for (int i = 0; i < n; i++) if (retdsn[i] == '%') retdsn[i] = '/';

   return retdsn;
}

/******************************************************************************/
/* Private:                       g e t D S N                                 */
/******************************************************************************/

int XrdOssArcCompose::getDSN(const char *path)
{

// At this point we must have a dataset name containing a scope name
//
   const char* colon = index(path, ':');
   if (!colon || *path == ':')
      {ecMsg.Msg("Compose", "Dataset scope not specified");
       return EINVAL;
      }
    dsScope.assign(path, colon - path);

// Verify that the dataset name is long enough
//
   if ((int)strlen(colon+1) < minLenDSN)
      {ecMsg.Msg("Compose", "dataset name is too short");
       return EINVAL;
      }

// The dataset name must not end with the arc file suffix
//
// if (isArcFile(colon+1))
//    {ecMsg.Msg("Compose", "Dataset name cannot refer to an archive");
//     return EINVAL;
//    }

// Assign the dataset name
//
   dsName = colon+1;
   return 0;
}

/******************************************************************************/
/*                             i s A r c F i l e                              */
/******************************************************************************/

bool XrdOssArcCompose::isArcFile(const char *path)
{
   int n = strlen(path);

// Is it's too short, it cannot end with ".zip" (or what it the suffix is)
//
   if (n <= Config.arfSfxLen) return false;

// Verify the ending
//
   return !strcmp(Config.arfSfx, path+n-Config.arfSfxLen);
}

/******************************************************************************/
/*                             i s A r c P a t h                              */
/******************************************************************************/

bool XrdOssArcCompose::isArcPath(const char *path)
{
   return  !strncmp(Config.arcvPathLFN, path, Config.arcvPathLEN);
}

/******************************************************************************/
/*                             i s B k p P a t h                              */
/******************************************************************************/

bool XrdOssArcCompose::isBkpPath(const char *path)
{
   return  !strncmp(Config.bkupPathLFN, path, Config.bkupPathLEN);
}

/******************************************************************************/
/*                                i s M i n e                                 */
/******************************************************************************/

bool XrdOssArcCompose::isMine(const char *path)
{
   return  !strncmp(Config.arcvPathLFN, path, Config.arcvPathLEN) ||
           !strncmp(Config.bkupPathLFN, path, Config.bkupPathLEN);
}

/******************************************************************************/
/* Private:                   S e t a r N a m e                              */
/******************************************************************************/

int XrdOssArcCompose::SetarName()
{
  TraceInfo("SetarName", 0);

// Which is applicable to any type of object as it only wants to know
// which archive file contains a particular dataset file. So, construct
// the argument list to ask.
//
   std::string fName = flScope + (std::string)":" + flName;
   const char* argV[] = {"which", Config.arFName, fName.c_str(),
                         dsScope.c_str(), dsName.c_str()};
   int argC = sizeof(argV)/sizeof(char*);
   int rc;

// Do some tracing
//
    DEBUG("Running "<<Config.BkpUtilName<<' '<<argV[0]<<' '
                    <<argV[1]<<' '<<argV[2]<<' '<<argV[3]<<' '<<argV[4]);

// Execute command, it should return a single line of output
//
   XrdOucStream cmdSup;
   bool aOK = false;

   if (!(rc = Config.BkpUtilProg->Run(&cmdSup, argV, argC)))
      {char* resp;
       if (cmdSup.GetLine() && (resp = cmdSup.GetToken()) && *resp)
          {arName = resp;
           aOK = *resp != '!';
           DEBUG(resp<<" holds "<<dsScope<<':'<<dsName<<'['<<fName<<"]")
          }
       rc = Config.BkpUtilProg->RunDone(cmdSup);
      }

// Check if we have a result, that implies all went well enough
//
   if (aOK) return 0;

// Diagnose the error
//
   if (arName == "!ENOENT") rc = ENOENT;
      else if (arName == "!ENOANO") rc = ENOANO;
              else if (!rc) rc = EPROTO;

   fName += ';';
   std::string dName =  dsScope +':'+ dsName;

   ecMsg.Msg("Compose", "Unable to determine name of the dataset",
              dName.c_str(), "archive for", fName.c_str(),
              (rc == ENOANO ? "dataset not backed up" : XrdSysE2T(rc)));
   return rc;
}

/******************************************************************************/
/*                                  S t a t                                   */
/******************************************************************************/

int XrdOssArcCompose::Stat(const char* Scope, const char* Name,
                           struct stat* Stat)
{
   TraceInfo("Stat", 0);

// Setup argument list for a stat call
//
   const char* argV[] = {"stat", "cgi", Scope, Name};
   int argC = sizeof(argV)/sizeof(char*);
   int rc, rc2 = 0;

// Do some tracing
//
    DEBUG("Running "<<Config.BkpUtilName<<' '<<argV[0]<<' '
                    <<argV[1]<<' '<<argV[2]<<' '<<argV[3]);

// Execute command, it should return a single line of output
//
   XrdOucStream cmdSup;
   bool aOK = false;

   if (!(rc = Config.BkpUtilProg->Run(&cmdSup, argV, argC)))
      {char* resp;
       if (cmdSup.GetLine() && (resp = cmdSup.GetToken()) && *resp)
          {if (*resp != '!')
              {if (!(rc = StatDecode(*Stat, resp))) aOK = true;
                  else if (!strcmp("!ENOENT", resp)) rc = ENOENT;
                          else rc = EINVAL;
              }
          }
       rc2 = Config.BkpUtilProg->RunDone(cmdSup);
      }

// Check if we have a result, that implies all went well enough
//
   if (aOK) return 0;
   if (rc)  return rc;
   if (rc2) return rc2;
   return EPROTO;
}

/******************************************************************************/
/* Private:                   S t a t D e c o d e                             */
/******************************************************************************/

int XrdOssArcCompose::StatDecode(struct stat& Stat, const char* resp)
{
   TraceInfo("StatDecode", 0);
   XrdOucEnv env(resp);
   char* infoP;
   long long val;
   int n;

   memset(&Stat, 0, sizeof(struct stat));

   if (StatGet("uid", env, val)) Stat.st_uid = int(val);
      else return EINVAL;

   if (StatGet("gid", env, val)) Stat.st_gid = int(val);
      else return EINVAL;

   if (StatGet("size", env, val)) Stat.st_size = val;
      else return EINVAL;

   if (StatGet("atime", env, val)) Stat.st_atime = time_t(val);
      else return EINVAL;
   if (StatGet("mtime", env, val)) Stat.st_mtime = time_t(val);
      else return EINVAL;
   if (StatGet("ctime", env, val)) Stat.st_ctime = time_t(val);
      else return EINVAL;

// Set the mode
//
   if (!(infoP = env.Get("mode")))
      {DEBUG("Missing 'mode' in stat resp '"<<env.Env(n)<<"'");
       return EINVAL;
      }

// Set the correct mode
//
   Stat.st_mode = S_IRUSR | S_IRGRP;
   if (!strcmp(infoP, "FILE")) Stat.st_mode |= S_IFREG;
      else Stat.st_mode = S_IFDIR | S_IXUSR | S_IXGRP;

// All done
//
   return 0;
}

/******************************************************************************/
/* Private:                     S t a t G e t                                */
/******************************************************************************/

bool XrdOssArcCompose::StatGet(const char* var, XrdOucEnv& env, long long& val)
{
   TraceInfo("StatGet", 0);
   const char* varval;
   char* endval;
   int n;

// Fetch the value
//
   if (!(varval = env.Get(var)))
      {DEBUG("Missing '"<<var<<"' in stat resp '"<<env.Env(n)<<"'");
       return false;
      }

// Convert it to a long long
//
   val = strtoll(varval, &endval, 10);
   if (*endval == 0) return true;

// Document failure
//
   DEBUG("Invalid '"<<var<<'='<<varval<<"' in stat resp '"<<env.Env(n)<<"'");
   return false;
}
