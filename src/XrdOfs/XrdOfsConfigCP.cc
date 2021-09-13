/******************************************************************************/
/*                                                                            */
/*                     X r d O f s C o n f i g C P . c c                      */
/*                                                                            */
/* (c) 2020 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <cstdio>
#include <cstring>

#include "XrdOfs/XrdOfsChkPnt.hh"
#include "XrdOfs/XrdOfsConfigCP.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucNSWalk.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
extern XrdOss     *XrdOfsOss;
extern XrdSysError OfsEroute;

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/
  
char      *XrdOfsConfigCP::Path      = 0;
long long  XrdOfsConfigCP::MaxSZ     = 1024*1024;
int        XrdOfsConfigCP::MaxVZ     = 16;
bool       XrdOfsConfigCP::cprErrNA  = true;
bool       XrdOfsConfigCP::Enabled   = true;
bool       XrdOfsConfigCP::isProxy   = false;
bool       XrdOfsConfigCP::EnForce   = false;

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/

bool XrdOfsConfigCP::Init()
{
   const int AMode = S_IRWXU|S_IRGRP|S_IXGRP;
   const char *endMsg = "completed.";
   char *aPath;
   int rc;
   bool autoDis = false;

// If we are not enabled then do nothing
//
   if (!Enabled || isProxy) return true;

// Print warm-up message
//
   OfsEroute.Say("++++++ Checkpoint initialization started.");

// If there is no path then generate the default path
//
   if (Path)
      {aPath = XrdOucUtils::genPath(Path, XrdOucUtils::InstName(-1), "chkpnt/");
       free(Path);
       Path = aPath;
      } else {
       if (!(aPath = getenv("XRDADMINPATH")))
          {OfsEroute.Emsg("Config",
                          "Unable to determine adminpath for chkpnt files.");
           return false;
          }
       Path = XrdOucUtils::genPath(aPath, (char *)0, "chkpnt/");
      }

// Make sure directory path exists
//
   if ((rc = XrdOucUtils::makePath(Path, AMode)))
      {OfsEroute.Emsg("Config", rc, "create path for", Path);
       return false;
      }

// We generally prohibit placing the checkpoint directory in /tmp unless
// "enable" has been specified.
//
   if (!strncmp(Path, "/tmp/", 5))
      {if (EnForce) OfsEroute.Say("Config warning: rooting the checkpoint "
                                  "directory in '/tmp' is ill-advised!");
          else autoDis = true;
      }

// Prepare to list contents of the directory
//
   XrdOucNSWalk nsWalk(&OfsEroute, Path, 0, XrdOucNSWalk::retFile);
   XrdOucNSWalk::NSEnt *nsX, *nsP = nsWalk.Index(rc);
   if (rc)
      {OfsEroute.Emsg("Config", rc, "list CKP path", Path);
       return false;
      }

// Process all files
//
   Stats stats;
   while((nsX = nsP))
        {Recover(nsX->Path, stats);
         nsP = nsP->Next;
         delete nsX;
        }

// Print message if we found anything
//
   if (stats.numFiles)
      {char mBuff[256];
       snprintf(mBuff, sizeof(mBuff),
               "%d of %d checkpoints restored, %d failed, and %d skipped.",
               stats.numRecov, stats.numFiles, stats.numError, stats.numSkipd);
       OfsEroute.Say("Config ", mBuff);
       if (stats.numUnres)
          {snprintf(mBuff, sizeof(mBuff), "%d", stats.numUnres);
           OfsEroute.Say("Config warning: ", mBuff, " unresolved checkpoint "
                         "restore failures found!");
           endMsg = "requires attention!";
          }
      }

// Check if we need to disable checkpoint processing at this point
//
   if (autoDis)
      {OfsEroute.Say("Config warning: checkpoints disabled because the "
                     "checkpoint directory is rooted in '/tmp'!");
       Enabled = false;
      }

// Print final mesage
//
   OfsEroute.Say("++++++ Checkpoint initialization ", endMsg);
   return true;
}
  
/******************************************************************************/
/*                                 P a r s e                                  */
/******************************************************************************/

/* Function: Parse

   Purpose:  To parse the directive: chkpnt [disable|enable]
                                            [cprerr <opt>]
                                            [maxsz <sz>] [path <path>]

             disable Disables checkpointing.
             enable  Enables  checkpointing.
             <opt>   Checkpoint restore error option:
                     makero  - make the source file read/only and
                               rename checkpoint file.
                     stopio  - make file neither readable nor writable and
                               rename checkpoint file.
             <sz>    the maximum size of a checkpoint. The minimum and default
                     value is 1m.
             <path>  path to where checkpoint files are to be placed.
                     The default is <adminpath>/.ofs/chkpnt
*/
  
bool XrdOfsConfigCP::Parse(XrdOucStream &Config)
{
   static const int minSZ = 1024*1024;
   char *val;

// Get the size
//
   if (!(val = Config.GetWord()) || !val[0])
      {OfsEroute.Emsg("Config", "chkpnt parameters not specified");
       return false;
      }

// Process options
//
do{     if (!strcmp(val, "disable")) Enabled = EnForce = false;
   else if (!strcmp(val, "enable"))  Enabled = EnForce = true;
   else if (!strcmp(val, "cprerr"))
           {     if (!strcmp(val, "makero")) cprErrNA = false;
            else if (!strcmp(val, "stopio")) cprErrNA = true;
            else {OfsEroute.Emsg("Config","invalid chkpnt cperr option -",val);
                  return false;
                 }
           }
   else if (!strcmp(val, "maxsz"))
           {if (!(val = Config.GetWord()) || !val[0])
               {OfsEroute.Emsg("Config", "chkpnt maxsz value not specified");
                return false;
               }
            if (XrdOuca2x::a2sz(OfsEroute,"chkpnt maxsz", val, &MaxSZ, minSZ))
               return false;
           }
   else if (!strcmp(val, "path"))
           {if (!(val = Config.GetWord()) || !val[0])
               {OfsEroute.Emsg("Config", "chkpnt path value not specified");
                return false;
               }
            if (*val != '/')
               {OfsEroute.Emsg("Config", "chkpnt path is not absolute");
                return false;
               }
            if (Path) free(Path);
            int n = strlen(val);
            if (val[n-1] == '/') Path = strdup(val);
               else {XrdOucString pstr(n+1);
                     pstr = val; pstr.append('/');
                     Path = strdup(pstr.c_str());
                    }
           }
   else {OfsEroute.Emsg("Config", "invalid chkpnt parameter -", val);
         return false;
        }

  } while((val =  Config.GetWord()));

// All done
//
   return true;
}

/******************************************************************************/
/*                               R e c o v e r                                */
/******************************************************************************/
  
void XrdOfsConfigCP::Recover(const char *ckpPath, struct Stats &stats)
{
   const char *sfx = rindex(ckpPath, '.');

// Count of files we have seen
//
   stats.numFiles++;

// Check if this is an unresolved error
//
   if ( sfx && !strcmp(sfx, ".ckperr"))
      {char *fName = XrdOfsCPFile::Target(ckpPath);
       OfsEroute.Say("Config warning: unresolved checkpoint error in '",
                     ckpPath, "' for file '", fName, "'!");
       free(fName);
       stats.numUnres++;
       return;
      }

// Make sure this file end with '.ckp')
//
   if (!sfx || strcmp(sfx, ".ckp"))
      {OfsEroute.Say("Config warning: unrecognized checkpoint file '",
                     ckpPath, "' skipped!");
       stats.numSkipd++;
       return;
      }

// Get a new oss file, create a checkpoint object and try to restore the file
//
   XrdOssDF *ossFP = XrdOfsOss->newFile("checkpoint");
   XrdOfsChkPnt chkPnt(*ossFP, 0, ckpPath);
   if (chkPnt.Restore()) stats.numError++;
      else stats.numRecov++;
   delete ossFP;
}
