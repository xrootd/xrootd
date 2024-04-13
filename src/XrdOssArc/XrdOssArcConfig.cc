/******************************************************************************/
/*                                                                            */
/*                    X r d O s s A r c C o n f i g . c c                     */
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

#include <string>

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "Xrd/XrdScheduler.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOssArc/XrdOssArc.hh"
#include "XrdOssArc/XrdOssArcConfig.hh"
#include "XrdOssArc/XrdOssArcTrace.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucGatherConf.hh"
#include "XrdOuc/XrdOucProg.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysError.hh"

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/
  
namespace XrdOssArcGlobals
{
extern XrdOss*       ossP;

extern XrdScheduler* schedP;
  
extern XrdSysError   Elog;

extern XrdSysTrace   ArcTrace;
}
using namespace XrdOssArcGlobals;

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdOssArcConfig::XrdOssArcConfig()
{
// Establish the defaults 
   getManPath  = strdup("XrdOssArc_Manifest");
   getManEOL  = "===";
   ArchiverPath= strdup("XrdOssArc_Archiver");
   MssComPath  = strdup("XrdOssArc_MssCom");
   MssComCmd   = 0;
   MssComRoot  = 0;
   arcvPathLFN = strdup("/archive/");
   arcvPathLEN = strlen(arcvPathLFN);
   bkupPathLFN = strdup("/backup/");
   bkupPathLEN = strlen(bkupPathLFN);
   stagePath   = strdup("/tmp/stage");
   tapePath    = strdup("/TapeBuffer");
   utilsPath   = strdup("/usr/local/etc");

   maxStage    = 30;
   wtpStage    = 30;
   arFName     = strdup("Archive.zip");
   arfSfx      = strdup(".zip");
   arfSfxLen   = 4;
   mySep       = '#';

   if (getenv("XRDOSSARC_DEBUG") || getenv("XRDDEBUG"))
      ArcTrace.What |= TRACE_Debug;
}

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/

bool  XrdOssArcConfig::Configure(const char* cfn,  const char* parms,
                                 XrdOucEnv* envP)
{
   TraceInfo("Configure", 0);
   bool NoGo = false;

// Read and process the config file
//
   if (!ConfigXeq(cfn, parms, envP)) return false;

// Set the external debug flag as needed
//
   if (ArcTrace.What & TRACE_Debug)
      XrdOucEnv::Export("XRDOSSARC_DEBUG", "1");

// Verify the archive path is usable
//
   if (!Usable(arcvPathLFN, "archive path")) NoGo = true;

// Verify that the tape buffer is usable
//
   if (!Usable(tapePath, "tape buffer path", false)) NoGo = true;

// Create the program to be used to get the manifest. This is a thread safe
// program so only one instance is needed
//
   getManProg = new XrdOucProg(&Elog);
   ConfigPath(&getManPath, utilsPath);
   if (getManProg->Setup(getManPath)) NoGo = true;

// Create the program to be used to create the archive
//
   ArchiverProg = new XrdOucProg(&Elog);
   ConfigPath(&ArchiverPath, utilsPath);
   if (ArchiverProg->Setup(ArchiverPath)) NoGo = true;

// Create program to communicate with the MSS
//
   MssComProg = new XrdOucProg(&Elog);
   ConfigPath(&MssComPath, utilsPath);
   if (MssComProg->Setup(MssComPath)) NoGo = true;

// Export envars that the programs needs
//
   if (MssComCmd)
      {XrdOucEnv::Export("XRDOSSARC_MSSCMD",  MssComCmd);
       DEBUG("Exporting XRDOSSARC_MSSCMD="<<MssComCmd);
      }
   if (MssComRoot)
      {XrdOucEnv::Export("XRDOSSARC_MSSROOT", MssComRoot);
       DEBUG("exporting XRDOSSARC_MSSROOT="<<MssComRoot);
      } else MssComRoot = strdup(""); 

// All done
//
   return !NoGo;
}

/******************************************************************************/
/* Private:                   C o n f i g P a t h                             */
/******************************************************************************/

void XrdOssArcConfig::ConfigPath(char** pDest, const char* pRoot)
{
// If pDest starts with a slash then we use it as is; otherwise qualify it
//
   if ((*pDest)[0] != '/') 
       {std::string tmp(pRoot);
       if (tmp.back() != '/') tmp += '/';
       tmp += *pDest;
       free(*pDest);
       *pDest = strdup(tmp.c_str());
      }
}       
  
/******************************************************************************/
/* Private:                   C o n f i g P r o c                             */
/******************************************************************************/

bool XrdOssArcConfig::ConfigProc(const char* drctv)
{  

// Process each directive
//
        if (!strcmp(drctv, "msscmd"))
           return xqGrab("msscmd", MssComCmd, Conf->LastLine());
   else if (!strcmp(drctv, "paths"))  return xqPaths();
   else if (!strcmp(drctv, "stage"))  return xqStage();
   else if (!strcmp(drctv, "trace"))  return xqTrace();
   else if (!strcmp(drctv, "utils"))  return xqUtils();
   else Conf->MsgfW("ignoring unknown directive '%s'", drctv);

   return true;
}

/******************************************************************************/
/* Private:                    C o n f i g X e q                              */
/******************************************************************************/


bool  XrdOssArcConfig::ConfigXeq(const char* cfName, const char* parms, 
                                 XrdOucEnv*  envP)
{
   XrdOucGatherConf Cfile("ossarc.", &Elog);
   char *token;
   int rc;
   bool NoGo = false;

// Get all relevant config options. Ignore the parms.
//
   if ((rc = Cfile.Gather(cfName, XrdOucGatherConf::full_lines)) <= 0)
      return (rc < 0 ? false : true);

   Cfile.Tabs(0);

// We point to the tokenizer to avod passing it around everywhere
//
   Conf = &Cfile;

// Process each directive
//
   while(Cfile.GetLine())
        {if ((token = Cfile.GetToken()))
            {if (!strncmp(token, "ossarc.", 7)) token += 7;
             NoGo |= !ConfigProc(token);
            }
        }   
      
// All done
//
   return !NoGo;
}

/******************************************************************************/
/*                          G e n L o c a l P a t h                           */
/******************************************************************************/
  
int XrdOssArcConfig::GenLocalPath(const char* dsn, char* buff, int bSZ)
{
   int rc;
  
// Generate the pfn treating the dsn as the lfn
//
   if ((rc = ossP->Lfn2Pfn(dsn, buff, bSZ)))
      {Elog.Emsg("Archive", rc, "generate local path for", dsn);
       return rc;
      }
   return 0;
}

/******************************************************************************/
/*                           G e n T a p e P a t h                            */
/******************************************************************************/

// Typical Path: <tapePath>/<dsn>

int XrdOssArcConfig::GenTapePath(const char* dsn, char* buff, int bSZ,
                                 bool addafn)
{
   int n;

// Generate the tape path
//
   if (addafn) n = snprintf(buff, bSZ, "%s/%s/%s", tapePath, dsn, arFName);
      else     n = snprintf(buff, bSZ, "%s/%s",    tapePath, dsn);
   if (n >= bSZ)
      {const char* eTxt = (addafn ? "generate tape archive file path for"
                                  : "generate tape directory path for");
       Elog.Emsg("Archive", ENAMETOOLONG, eTxt, dsn);
       return ENAMETOOLONG;
      }
   return 0;
}
  
/******************************************************************************/
/* Private:                      M i s s A r g                                */
/******************************************************************************/

bool XrdOssArcConfig::MissArg(const char* what)
{
   Conf->MsgfE("%s not specified.", what);
   return false;
}
  
/******************************************************************************/
/*                                U s a b l e                                 */
/******************************************************************************/
  
bool XrdOssArcConfig::Usable(const char* path, const char* what, bool useOss)
{
   struct stat Stat;
   int rc;

   if (useOss) rc = ossP->Stat(path, &Stat);
      else if ((rc = stat(path, &Stat))) rc = errno;

   if (rc)
      {char buff[256];
       snprintf(buff, sizeof(buff), "use %s", what);
       Elog.Emsg("Config", rc, buff, path);
       return false;
      }

// Verify that it is a directory
//
   if (!S_ISDIR(Stat.st_mode))
      {Elog.Emsg("Config", what, path, "is not a directory!");
       return false;
      }

// Verify that it is writable
//
   if ( ((Stat.st_mode & S_IWOTH) == 0) 
   &&   ((Stat.st_mode & S_IWUSR) != 0  && (Stat.st_uid != geteuid()))
   &&   ((Stat.st_mode & S_IWGRP) != 0  && (Stat.st_uid != getegid())))
      {Elog.Emsg("Config", what, path, "is not writable!");
       rc = 1;
      }

// Verify that it is searchable
//
   if ( ( (Stat.st_mode & S_IXOTH) == 0)
   &&   ( (Stat.st_mode & S_IXUSR) != 0  && (Stat.st_uid != geteuid()))
   &&   ( (Stat.st_mode & S_IXGRP) != 0  && (Stat.st_uid != getegid())))
      {Elog.Emsg("Config", what, path, "is not searchable!");
       rc = 1;
      }

// All done
//
   return rc == 0;
}

/******************************************************************************/
/* Private:                       x q G r a b                                 */
/******************************************************************************/
  
bool XrdOssArcConfig::xqGrab(const char* what, char*& theDest,
                            const char* theLine)
{
   const char* tP;

// Get all text after the direcive keyword in the last line
//
   if ((tP = index(theLine, ' '))) while(*tP == ' ') tP++;
   if (!tP || !(*tP))
      {Conf->MsgfE("%s argument not specified!", what);
       return false;
      }

// Replace the new argument with the old one
//
   if (theDest) free(theDest);
   theDest = strdup(tP);
   return true;
}

/******************************************************************************/
/* Private:                      x q P a t h s                                */
/******************************************************************************/
/*  
   paths [backing <path>] [mssfs <path>] [staging <path>] [utils <path>]
*/

bool XrdOssArcConfig::xqPaths()
{
   char** pDest = 0;
   char *token, *ploc;

// Process all options
//
   while((token = Conf->GetToken()))
        {     if (!strcmp("backing",  token)) pDest = &tapePath;
         else if (!strcmp("mssfs",    token)) pDest = &MssComRoot;
         else if (!strcmp("staging",  token)) pDest = &stagePath;
         else if (!strcmp("utils",    token)) pDest = &utilsPath;
         else {Conf->MsgfE("Unknown path type '%s'; ignored.", token);
               if (!Conf->GetToken()) break;
               continue;
              }
         if (!(ploc = Conf->GetToken()))
            {Conf->MsgfE("%s path not specified!", token);
             return false;
            }

         // Make sure path starts with a slash
         //
         if (*ploc != '/')
            {Conf->MsgfE("%s path is not absolute!", token);
             return false;
            }

         if (*pDest) free(*pDest);
         *pDest = strdup(ploc);
        }

// Make sure we have at least one argument
//
   if (!pDest) {Conf->MsgfE("no 'path' arguments specified!"); return false;}

// All done
//
   return true;
}

/******************************************************************************/
/* Private:                      x q S t a g e                                */
/******************************************************************************/
/*  
   stage [max <num>] [poll <sec>]
*/

bool XrdOssArcConfig::xqStage()
{
   static const int minStg = 1, maxStg = 100, minPoll = 5, maxPoll = 100;
   char* val;
   int   num, rc;

// Get the first token (there must be at least one)
//
   if (!(val = Conf->GetToken()))
      {Conf->MsgfE("No stage parameters specified");
       return false;
      }

// Now process all of them
//
   while(val)
        {     if (!strcmp(val, "max"))
                 {if (!(val = Conf->GetToken())) return MissArg("'max' value");
                  rc = XrdOuca2x::a2i(Elog, "stage max value", val, &num,
                                      minStg, maxStg);
                  if (rc) {Conf->EchoLine(); return false;}
                  maxStage = num;
                 }
         else if (!strcmp(val, "poll"))
                 {if (!(val = Conf->GetToken())) return MissArg("'poll' value");
                  rc = XrdOuca2x::a2tm(Elog, "stage poll value", val, &num,
                                       minPoll, maxPoll);
                  if (rc) {Conf->EchoLine(); return false;}
                  wtpStage = num;
                 }
         else {Conf->MsgE("unknown option -", val);
               return false;
              }
        } while((val = Conf->GetToken()));

// All done
//
   return true;
}

/******************************************************************************/
/* Private:                      x q T r a c e                                */
/******************************************************************************/

bool XrdOssArcConfig::xqTrace()
{
   char *val;
   struct traceopts {const char *opname; unsigned int opval;} tropts[] =
      {
       {"all",      TRACE_All},
       {"off",      TRACE_None},
       {"none",     TRACE_None},
       {"debug",    TRACE_Debug}
      };
   int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

   if (!(val = Conf->GetToken()))
      {Conf->MsgE("no trace options specified");
       return false;
      }
   while (val)
        {if (!strcmp(val, "off")) trval = 0;
            else {if ((neg = (val[0] == '-' && val[1]))) val++;
                  for (i = 0; i < numopts; i++)
                      {if (!strcmp(val, tropts[i].opname))
                          {if (neg)
                              if (tropts[i].opval) trval &= ~tropts[i].opval;
                                 else trval = TRACE_All;
                              else if (tropts[i].opval) trval |= tropts[i].opval;
                                      else trval = TRACE_None;
                           break;
                          }
                      }
                  if (i >= numopts)
                     Conf->MsgfW("ignoring invalid trace option '%s'", val);
                 }
         val = Conf->GetToken();
        }
   ArcTrace.What = trval;
   return true;
}

/******************************************************************************/
/* Private:                      x q U t i l s                                */
/******************************************************************************/
/*  
   utils [archiver <path>] [manifest <path>] [msscom <path>]
*/

bool XrdOssArcConfig::xqUtils()
{
   char** uDest = 0;
   char *token, *uloc;

// Process all options
//
   while((token = Conf->GetToken()))
        {     if (!strcmp("archiver", token)) uDest = &ArchiverPath;
         else if (!strcmp("manifest", token)) uDest = &getManPath;
         else if (!strcmp("msscom",   token)) uDest = &MssComPath;
         else {Conf->MsgfW("Unknown util '%s'; ignored.", token);
               if (!Conf->GetToken()) break;
               continue;
              }
         if (!(uloc = Conf->GetToken()))
            {Conf->MsgfE("utils %s value not specified!", token);
             return false;
            }
         if (*uDest) free(*uDest);
         *uDest = strdup(uloc);
        }

// Make sure we have at least one argument
//
   if (!uDest) {Conf->MsgE("no 'utils' arguments specified!"); return false;}

// All done
//
   return true;
}
