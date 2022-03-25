/******************************************************************************/
/*                                                                            */
/*                            X r d P r e p . c c                             */
/*                                                                            */
/* (c) 2021 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <iostream>
#include <vector>
#include <string>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdCl/XrdClFileSystem.hh"

using namespace XrdCl;
  
/******************************************************************************/
/*                         L o c a l   D e f i n e s                          */
/******************************************************************************/

#define EMSG(x) std::cerr <<"xrdprep: "<<x<<std::endl
  
/******************************************************************************/
/*                                G e t N u m                                 */
/******************************************************************************/
  
namespace
{
bool GetNum(const char *emsg, const char *item, int *val, int minv, int maxv=-1)
{
    char *eP;

    if (!item || !*item)
       {EMSG(emsg<<" value not specified"); return false;}

    errno = 0;
    *val  = strtol(item, &eP, 10);
    if (errno || *eP)
       {EMSG(emsg<<" '"<<item<<"' is not a number");
        return false;
       }

    if (*val < minv) 
       {EMSG(emsg<<" may not be less than "<<minv); return false;}
    if (maxv >= 0 && *val > maxv)
       {EMSG(emsg<<" may not be greater than "<<maxv); return false;}

    return true;
}
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
namespace
{
void Usage(int rc)
{
std::cerr<<"\nUsage: xrdprep [opts1] [prepare] host[:port] [path [...]]\n";
std::cerr<<"\n       xrdprep [opts2] {cancel | query} host[:port] handle [path [...]]\n";
std::cerr<<"\nOpts1: [-E] [-p prty] [-s] [-S] [-w] [opts2]\n";
std::cerr<<"\nOpts2: [-d n] [-f fn]" <<std::endl;
exit(rc);
}
}
  
/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char **argv)
{
   extern char *optarg;
   extern int  optind, opterr;

   static const int MaxPathLen = MAXPATHLEN+1;
   static const PrepareFlags::Flags mPrep = PrepareFlags::Cancel |
                PrepareFlags::Colocate    | PrepareFlags::Stage |
                PrepareFlags::Fresh       | PrepareFlags::WriteMode;

   std::vector<std::string> fList;
   FILE *Stream = 0;
   const char *msgArgs[] = {"execute", "prepare"};
   char Target[512];
   char *inFile = 0;
   PrepareFlags::Flags Opts = PrepareFlags::None;
   int rc, Prty = 0, Debug = 0;
   char c, lastOpt = 0;
   bool isQuery = false, needHandle = false;

// See simple help is needed
//
   if (argc <= 1) Usage(0);

// Process the options
//
   opterr = 0;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c = getopt(argc,argv,"d:Ef:p:sStw")) && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'd': if (!GetNum("debug level", optarg, &Debug, 0, 5)) exit(1);
                 break;
       case 'E': lastOpt = c;
                 Opts |= PrepareFlags::Evict;
                 break;
       case 'f': inFile = optarg;
                 break;
       case 'p': lastOpt = c;
                 if (!GetNum("priority", optarg, &Prty, 0, 3)) exit(1);
                 break;
       case 's': lastOpt = c; Opts |= PrepareFlags::Stage;
                 break;
       case 'S': lastOpt = c; Opts |=(PrepareFlags::Stage|PrepareFlags::Colocate);
                 break;
       case 't': lastOpt = c; Opts |= PrepareFlags::Fresh;
                 break;
       case 'w': lastOpt = c; Opts |= PrepareFlags::WriteMode;
                 break;
       default:  EMSG("Invalid option '-"<<argv[optind-1]<<"'");
                 Usage(1);
       }
     }

// The next argument is either a keyword or the hostname
//
   while(optind < argc)
      {     if (!strcmp(argv[optind], "cancel")) Opts = PrepareFlags::Cancel;
       else if (!strcmp(argv[optind], "query"))  isQuery = true;
       else if (!strcmp(argv[optind], "prepare")){optind++; break;}
       else break;
       if (lastOpt)
          {EMSG('-'<<lastOpt<<"' option is invalid for '"<<argv[optind]<<"'");
           Usage(1);
          }
       needHandle = true;
       msgArgs[0] = argv[optind++];
       break;
      }

// Make sure a host has been specified
//
   if (optind >= argc || !isalnum(*argv[optind]))
      {EMSG("target host name not specified");
       Usage(1);
      }

// Grab the host name or address
//
   strcpy(Target, "root://");
   strcat(Target, argv[optind]);
   optind++;

// If we need a handle then make sure we have one
//
   if (needHandle)
      {if (optind >= argc || *argv[optind] == '/')
          {EMSG(msgArgs[0]<<" prepare request handle not specified");
           Usage(1);
          }
      }

// Pre-process any command line paths at this point
//
   std::string strArg;
   int totArgLen = 0;
   for (int i = optind; i < argc; i++)
      {strArg = argv[i];
       totArgLen += strArg.size() + 1;
       fList.push_back(strArg);
      }

// If an infile was specified, make sure we can open it
//
   if (inFile)
      {if (!(Stream = fopen(inFile, "r")))
          {EMSG("Unable to open "<<inFile<<"; "<<XrdSysE2T(errno));
           exit(4);
          }
       char *sP, fBuff[MaxPathLen];
       do {if (!(sP = fgets(fBuff, MaxPathLen, Stream))) break;
           while(*sP && *sP == ' ') sP++;
           if (*sP && *sP != '\n')
              {strArg = sP;
               if (strArg.size() && strArg.back() == '\n') strArg.pop_back();
               while(strArg.size() && strArg.back() == ' ') strArg.pop_back();
               totArgLen += strArg.size() + 1;
               fList.push_back(strArg);
              }
          } while(!feof(Stream) && !ferror(Stream));
       if ((rc = ferror(Stream)))
          {EMSG("unable to read "<<inFile<<"; "<<XrdSysE2T(rc));
           exit(4);
          }
       fclose(Stream);
      }

// If this is a prepare request then we need at least one path
//
   if (!needHandle && fList.size() == 0)
      {EMSG("No files specified for 'prepare'");
       Usage(1);
      }

// Cleanup options if eviction is wanted
//
   if (Opts & PrepareFlags::Evict) Opts &= ~mPrep;

// Establish debugging level
//
  if (Debug > 0)
     {const char *dbg[] = {"Info","Warning","Error","Debug","Dump"};
      if (Debug > 5) Debug = 5;
      XrdOucEnv::Export("XRD_LOGLEVEL", dbg[Debug-1]);
     }

// Get an instance of the file system
//
   FileSystem Admin(Target);

// Issue the relevant operation
//
   Buffer *response = 0;
   XRootDStatus st;
   if (!isQuery) st = Admin.Prepare(fList, Opts, uint8_t(Prty), response);
      else {Buffer qryArgs(totArgLen);
            char *bP = qryArgs.GetBuffer();
            for (int i = 0; i < (int)fList.size(); i++)
                {strcpy(bP, fList[i].c_str());
                 bP += fList[i].size();
                 *bP++ = '\n';
                }
            *(bP-1) = 0;
            st = Admin.Query(QueryCode::Prepare, qryArgs, response);
           }

// Check if all went well
//
   if (!st.IsOK())
      {std::string estr = st.ToStr();
       const char *einfo, *etxt = estr.c_str();
       if (!(einfo = rindex(etxt, ']'))) einfo = etxt;
          else {einfo++;
                while(*einfo && *einfo == ' ') einfo++;
               }
       EMSG("Unable to "<<msgArgs[0]<<' '<<msgArgs[1]<<"; "
                        <<(char)tolower(*einfo)<<einfo+1);
       exit(8);
      }

// Display the result
//
   std::string rstr = response->ToString();
   const char *xx = rstr.c_str();
   if (*xx) std::cout << xx << std::endl;
   delete response;

// All done
//
   exit(0);
}
