/******************************************************************************/
/*                                                                            */
/*                        X r d F r m A d m i n . c c                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <fcntl.h>
#include <grp.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "XrdCks/XrdCksManager.hh"
#include "XrdFrc/XrdFrcProxy.hh"
#include "XrdFrc/XrdFrcTrace.hh"
#include "XrdFrc/XrdFrcUtils.hh"
#include "XrdFrm/XrdFrmAdmin.hh"
#include "XrdFrm/XrdFrmConfig.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucArgs.hh"
#include "XrdOuc/XrdOucExport.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSys/XrdSysTimer.hh"

using namespace XrdFrc;
using namespace XrdFrm;

/******************************************************************************/
/*                                 A u d i t                                  */
/******************************************************************************/

const char *XrdFrmAdmin::AuditHelp = 

"audit [opts] {names ldir | space name[:pdir] | usage [name]}\n\n"

"opts: -fix -f[orce] -m[igratable] -p[urgeable] -r[ecursive]";

int XrdFrmAdmin::Audit()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ",    "",
                                "fix",         3, "f",
                                "force",       1, "F",
                                "migratable",  1, "m",
                                "purgeable",   1, "p",
                                "recursive",   1, "r",
                                (const char *)0);

   static const char *Reqs[] = {"type", 0};

// Parse the request
//
   if (!Parse("audit ", Spec, Reqs)) return 1;
   Opt.Args[1] = Spec.getarg();

// Fan out based on the function
//
           if (!strcmp(Opt.Args[0], "usage")) return AuditUsage();
   if (!Opt.Args[1])  Emsg("audit target not specified.");
      else if (!strcmp(Opt.Args[0], "names")) return AuditNames();
      else if (!strcmp(Opt.Args[0], "space")) return AuditSpace();
      else    Emsg("Unknown audit type - ", Opt.Args[0]);

// Nothing we understand
//
   return 4;
}

/******************************************************************************/
/*                                C h k s u m                                 */
/******************************************************************************/

const char *XrdFrmAdmin::ChksumHelp =

"chksum [opts] {calc | ls | set <csval> | unset | ver[ify] <csval>} fn\n\n"

"opts: -f[orce] -pfn -t[ype] <cstype> -v[erbose]";

int XrdFrmAdmin::Chksum()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ",    "",
                                "force",       1, "f",
                                "pfn",         3, "p",
                                "type",        1, "t:",
                                "verbose",     1, "v", (const char *)0);

   static const char *Reqs[] = {"function", "target", 0};
   const char *csName;
   char pfnbuf[MAXPATHLEN], *Pfn, *Lfn, *csFunc;
   int rc = 0;

// Check if this is even supported
//
   memset(&CksData, 0, sizeof(CksData));
   if (!Config.CksMan || !(CksData.Length = Config.CksMan->Size()))
      {Emsg("Checksum support has not been configured!"); return 8;}

// Parse the request
//
   if (!Parse("chksum ", Spec, Reqs)) return 1;
   csFunc = Opt.Args[0];
   csName = CksData.Name;
   if (!*CksData.Name) {Opt.All = 1; CksData.Set(Config.CksMan->Name());}

// Check first for set or verify
//
   if (!strcmp (csFunc, "set") || Abbrev(csFunc, "verify", 3))
      {int n = strlen(Opt.Args[1]);
       if (n != CksData.Length*2 || !CksData.Set(Opt.Args[1], n))
          {Emsg("Invalid ", csName, " checksum value - ", Opt.Args[1]);
           return 4;
          }
       if (!(Opt.Args[1] = Spec.getarg()))
          {Emsg("chksum target not specified."); return 4;}
      }

// Convert the lfn to a pfn if it has not been converted already
//
   Pfn = Lfn = Opt.Args[1];
   if (Opt.MPType != 'p')
      {if (!Config.LocalPath(Opt.Args[1], pfnbuf, sizeof(pfnbuf))) return 4;
       Pfn = pfnbuf;
      }

// Process the request
//
        if (!strcmp(csFunc, "calc"))
           {if (Opt.Force || Config.CksMan->Get(Pfn, CksData) <= 0)
               rc = Config.CksMan->Calc(Pfn, CksData, 1);
            if (rc >= 0) {ChksumPrint(Lfn, rc); return 0;}
           }

   else if (!strcmp("ls", csFunc))
           {if (!(rc = ChksumList(Lfn, Pfn))) return 0;}

   else if (!strcmp(csFunc, "set"))
           {if (!(rc = Config.CksMan->Set(Pfn, CksData))) return 0;}

   else if (!strcmp(csFunc, "unset"))
           {if (!(rc = Config.CksMan->Del(Pfn, CksData))) return 0;}

   else if (Abbrev(csFunc, "verify", 3))
           {if ((rc = Config.CksMan->Ver(Pfn, CksData)) > 0)
               {Say.Say(CksData.Name, " checksums match for ", Lfn); return 0;}
            if (!rc)
               {Say.Say(CksData.Name, " checksums differ for ",Lfn); return 1;}
           }

   else {Emsg("Unknown chksum function - ", csFunc); return 4;}

// Determine name of the problem
//
        if (rc == -EDOM)    Emsg("Invalid ", csName, " checksum length.");
   else if (rc == -ENOTSUP) Emsg(csName, " checksums are not supported.");
   else if (rc == -ESRCH)   Emsg(csName, " checksum not set for ", Lfn);
   else if (rc == -ESTALE)  Emsg(csName, " checksum no longer valid for ", Lfn);
   else                     Emsg(-rc, csFunc, " ", csName, " checksum");

// Return failure
//
   return 4;
}

/******************************************************************************/
/*                            C h k s u m L i s t                             */
/******************************************************************************/
  
int XrdFrmAdmin::ChksumList(const char *Lfn, const char *Pfn)
{
   char Buff[256], *csBP;
   XrdOucTokenizer csList(Buff);
   int rc = 0;

// If all checksums wanted, then check if we have any
//
   if (Opt.All)
      {if (!(csBP = Config.CksMan->List(Pfn, Buff, sizeof(Buff))))
          {Say.Say("No checksums exist for ", Lfn); return 0;}
       csList.GetLine();
      }

// Print one or all checksums, as needed
//
   do {if (Opt.All)
          {if ((csBP = csList.GetToken())) CksData.Set(csBP);
              else break;
          }
       if ((rc = Config.CksMan->Get(Pfn, CksData)) <= 0
       && rc != -ESTALE && rc != -ENOTSUP) return rc;
       ChksumPrint(Lfn, rc);
      } while(Opt.All);

// All done
//
   return 0;
}
  
/******************************************************************************/
/*                           C h k s u m P r i n t                            */
/******************************************************************************/
  
void XrdFrmAdmin::ChksumPrint(const char *Lfn, int rc)
{
   char csBuff[XrdCksData::ValuSize*2+8], Buff[64];
   static const int bSize = sizeof(Buff)-XrdCksData::NameSize;

// Insert the checksum and name in the buffer
//
   *Buff = ' ';
   strcpy(Buff+1, CksData.Name);

// Format long display, if so wanted
//
   if (Opt.Verbose)
      {time_t csTime = CksData.fmTime + CksData.csTime;
       strftime(Buff+strlen(Buff), bSize, " %D %T ", localtime(&csTime));
      }

// Grab the checksum value
//
        if (rc == -ENOTSUP) strcpy(csBuff, "Unsupported!");
   else if (rc < 0 )        strcpy(csBuff, "Invalid!");
   else CksData.Get(csBuff, sizeof(csBuff));

// Print result
//
   Say.Say(csBuff, Buff, (Opt.Verbose ? Lfn : 0));
}

/******************************************************************************/
/*                                  F i n d                                   */
/******************************************************************************/

const char *XrdFrmAdmin::FindHelp = "find [-r[ecursive]] what ldir [ldir [...]]\n\n"

"what: fail[files] | mmap[ped] | nochksum <type> | nolk[files] | pin[ned] | unmig[rated]";

int XrdFrmAdmin::Find()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ",    "",
                                "recursive",   1, "r", (const char *)0);

   static const char *Reqs[] = {"type", "target", 0};

// Parse the request
//
   if (!Parse("find ", Spec, Reqs)) return 1;

// Process the correct find
//
        if (Abbrev(Opt.Args[0], "failfiles", 4)) return FindFail(Spec);
   else if (Abbrev(Opt.Args[0], "mmapped",   4)) return FindMmap(Spec);
   else if (Abbrev(Opt.Args[0], "nocs",      4)) return FindNocs(Spec);
   else if (Abbrev(Opt.Args[0], "nochksum",  8)) return FindNocs(Spec);
   else if (Abbrev(Opt.Args[0], "nolkfiles", 4)) return FindNolk(Spec);
   else if (Abbrev(Opt.Args[0], "pinned",    3)) return FindPins(Spec);
   else if (Abbrev(Opt.Args[0], "unmigrated",4)) return FindUnmi(Spec);

// Nothing we understand
//
   Emsg("Unknown find type - ", Opt.Args[0]);
   return 4;
}

/******************************************************************************/
/*                                  H e l p                                   */
/******************************************************************************/

const char *XrdFrmAdmin::HelpHelp =
"[help] {audit | chksum | exit | f[ind] | makelf | mark | mmap | mv | pin "
         "| q[uery] | quit | reloc | rm} ...";
  
int XrdFrmAdmin::Help()
{
   static struct CmdInfo {const char *Name;
                                int   minL;
                                int   maxL;
                          const char *Help;
                         }
                 CmdTab[] = {{"audit",  5, 5, AuditHelp },
                             {"chksum", 6, 6, ChksumHelp },
                             {"find",   1, 4, FindHelp  },
                             {"makelf", 6, 6, MakeLFHelp},
                             {"mark",   4, 4, MarkHelp  },
                             {"mmap",   4, 4, MmapHelp  },
                             {"mv",     2, 2, MvHelp    },
                             {"pin",    3, 3, PinHelp   },
                             {"query",  1, 5, QueryHelp },
                             {"reloc",  5, 5, RelocHelp },
                             {"rm",     2, 2, RemoveHelp}
                            };
   static int CmdNum = sizeof(CmdTab)/sizeof(struct CmdInfo);
   const char *theHelp = HelpHelp;
   char *Cmd;
   int   i, n;

// Get the next argument (array or string)
//
   if (!ArgS) Cmd = ArgV[0];
      else {XrdOucTokenizer Tokens(ArgS);
            if ((Cmd = Tokens.GetLine())) Cmd = Tokens.GetToken();
           }

// Try to give the correct help
//
   if (Cmd)
      {n = strlen(Cmd);
       for (i = 0; i < CmdNum; i++)
           if (n <= CmdTab[i].maxL && n >= CmdTab[i].minL
           && !strncmp(CmdTab[i].Name, Cmd, n)) break;
       if (i < CmdNum) {Msg("Usage: ", CmdTab[i].Help); return 0;}
      }
   Emsg(0, "Usage: ", theHelp);
   return 0;
}

/******************************************************************************/
/*                                M a k e L F                                 */
/******************************************************************************/

const char *XrdFrmAdmin::MakeLFHelp = "makelf [opts] lspec [lspec [...]]\n\n"

"opts: -m[igratable] -o[wner] [usr][:[grp]] -p[urgeable] "
      "-r[ecursive]\n\n"

"lspec: lfn | ldir[*]";

int XrdFrmAdmin::MakeLF()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ",    "",
                                "migratable",  1, "m",
                                "owner",       1, "o:",
                                "purgeable",   1, "p",
                                "recursive",   1, "r",
                                (const char *)0);

   static const char *Reqs[] = {"lfn", 0};

   char *lfn, buff[80], Resp;
   int ok = 1;

// Parse the request
//
   if (!Parse("makelf ", Spec, Reqs)) return 1;

// Process all of the files
//
   numFiles = 0;
   lfn = Opt.Args[0];
   if (!Opt.MPType) Opt.MPType = 'm';
   do {Opt.All = VerifyAll(lfn);
       if ((Resp = VerifyMP("makelf", lfn)) == 'y') ok = mkLock(lfn);
      } while(Resp != 'a' && ok && (lfn = Spec.getarg()));

// All done
//
   if (Resp == 'a' || !ok) Msg("makelf aborted!");
   sprintf(buff, "%d lock file%s made.", numFiles, (numFiles == 1 ? "" : "s"));
   Msg(buff);
   return 0;
}

/******************************************************************************/
/*                                  M a r k                                   */
/******************************************************************************/

const char *XrdFrmAdmin::MarkHelp = "mark [opts] lspec [lspec [...]]\n\n"

"opts: -f[orce] -m[igratable] -p[urgeable] -r[ecursive]\n\n"

"lspec: lfn | ldir[/*]";

int XrdFrmAdmin::Mark()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ",    "",
                                "force",       1, "F",
                                "migratable",  1, "m",
                                "purgeable",   1, "p",
                                "recursive",   1, "r",
                                (const char *)0);

   static const char *Reqs[] = {"lfn", 0};

   char *lfn, buff[80], Resp;
   int ok = 1;

// Parse the request
//
   if (!Parse("mark ", Spec, Reqs)) return 1;

// Process all of the files
//
   numFiles = 0;
   lfn = Opt.Args[0];
   if (!Opt.MPType) Opt.MPType = 'm';
   do {Opt.All = VerifyAll(lfn);
       if ((Resp = VerifyMP("mark", lfn)) == 'y') ok = mkMark(lfn);
      } while(Resp != 'a' && ok && (lfn = Spec.getarg()));

// All done
//
   if (Resp == 'a' || !ok) Msg("mark aborted!");
   sprintf(buff, "%d file%s marked %s.", numFiles, (numFiles == 1 ? "" : "s"),
                 (Opt.MPType == 'm' ? "migratable" : "purgeable"));
   Msg(buff);
   return 0;
}

/******************************************************************************/
/*                                  M m a p                                   */
/******************************************************************************/

const char *XrdFrmAdmin::MmapHelp = "mmap [opts] lspec [lspec [...]]\n\n"

"opts: -k[eep] -l[ock] -o[ff] -r[ecursive]\n\n"

"lspec: lfn | ldir[/*]";

int XrdFrmAdmin::Mmap()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ",    "",
                                "keep",        1, "K",
                                "lock",        1, "f",
                                "off",         1, "l",
                                "recursive",   1, "r",
                                (const char *)0);

   static const char *Reqs[] = {"lfn", 0};

   char *lfn, itbuff[80], Resp;
   int ok = 1;

// Parse the request
//
   if (!Parse("pin ", Spec, Reqs)) return 1;

// Process all of the files
//
   numFiles = 0;
   lfn = Opt.Args[0];
   Opt.MPType = 'p';
   do {Opt.All = VerifyAll(lfn);
       if ((Resp = VerifyMP("mmap", lfn)) == 'y') ok = mkMmap(lfn);
      } while(Resp != 'a' && ok && (lfn = Spec.getarg()));

// All done
//
   if (Resp == 'a' || !ok) Msg("mmap aborted!");
   sprintf(itbuff,"%d mmap%s processed.",numFiles,(numFiles==1?"":"s"));
   Msg(itbuff);
   return 0;
}

/******************************************************************************/
/*                                    M v                                     */
/******************************************************************************/

const char *XrdFrmAdmin::MvHelp = "mv oldlfn newlfn";

int XrdFrmAdmin::Mv()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ", "", (const char *)0);

   static const char *Reqs[] = {"oldlfn", "newlfn", 0};

   int rc;

// Parse the request and do it
//
   if (!Parse("mv ", Spec, Reqs)) return 1;

// Simply invoke the reloc function in the underlying FS
//
   if ((rc = Config.ossFS->Rename(Opt.Args[0], Opt.Args[1])))
      Emsg(-rc, "rename ", Opt.Args[0]);
      else Msg(Opt.Args[0], " renamed to ", Opt.Args[1]);
   return rc != 0;
}

/******************************************************************************/
/*                                   P i n                                    */
/******************************************************************************/

const char *XrdFrmAdmin::PinHelp = "pin [opts] lspec [lspec [...]]\n\n"

"opts: -k[eep] <time> -r[ecursive]\n\n"

"time: [+]<n>[d|h|m|s] | mm/dd/[yy]yy | forever\n\n"

"lspec: lfn | ldir[/*]";

int XrdFrmAdmin::Pin()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ",    "",
                                "keep",        1, "k:",
                                "recursive",   1, "r",
                                (const char *)0);

   static const char *Reqs[] = {"lfn", 0};

   const char *Act;
   char *lfn, itbuff[80], Resp;
   int ok = 1;

// Parse the request
//
   if (!Parse("pin ", Spec, Reqs)) return 1;

// Process all of the files
//
   numFiles = 0;
   lfn = Opt.Args[0];
   Opt.MPType = 'p';
   do {Opt.All = VerifyAll(lfn);
       if ((Resp = VerifyMP("pin", lfn)) == 'y') ok = mkPin(lfn);
      } while(Resp != 'a' && ok && (lfn = Spec.getarg()));

// All done
//
   Act = (Opt.KeepTime || Opt.ktAlways ? "" : "un");
   if (Resp == 'a' || !ok) Msg("pin aborted!");
   sprintf(itbuff,"%d %spin%s processed.",numFiles,Act,(numFiles==1?"":"s"));
   Msg(itbuff);
   return 0;
}

/******************************************************************************/
/*                                 Q u e r y                                  */
/******************************************************************************/

const char *XrdFrmAdmin::QueryHelp = "\n"
           "query pfn lspec [lspec [...]]\n"
           "query rfn lspec [lspec [...]]\n"
           "query space [[-r[ecursive]] lspec [...]]\n"
           "query usage [name]\n"
           "query xfrq  [name] [vars]\n\n"

           "lspec: lfn | ldir[*]";

int XrdFrmAdmin::Query()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ", "", (const char *)0);

   static const char *Reqs[] = {"type", 0};
   static struct CmdInfo {const char *Name;
                          int (XrdFrmAdmin::*Method)(XrdOucArgs &Spec);
                         }
                 CmdTab[] = {{"pfn",    &XrdFrmAdmin::QueryPfn},
                             {"rfn",    &XrdFrmAdmin::QueryRfn},
                             {"space",  &XrdFrmAdmin::QuerySpace},
                             {"usage",  &XrdFrmAdmin::QueryUsage},
                             {"xfrq",   &XrdFrmAdmin::QueryXfrQ}
                            };
   static int CmdNum = sizeof(CmdTab)/sizeof(struct CmdInfo);

   int i;

// Parse the request
//
   if (!Parse("query ", Spec, Reqs)) return 1;

// Find the command
//
   for (i = 0; i < CmdNum; i++)
       if (!strcmp(CmdTab[i].Name, Opt.Args[0])) break;

// See if we found the command
//
   if (i >= CmdNum)
      {Emsg("Invalid query type - ", Opt.Args[0]);
       return 1;
      }

// Perform required function
//
   return (*this.*CmdTab[i].Method)(Spec);
}

/******************************************************************************/
/*                                 R e l o c                                  */
/******************************************************************************/

const char *XrdFrmAdmin::RelocHelp = "reloc lfn {cgroup[:path]}";

int XrdFrmAdmin::Reloc()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ", "", (const char *)0);

   static const char *Reqs[] = {"lfn", "target", 0};

   int rc;

// Parse the request and do it
//
   if (!Parse("reloc ", Spec, Reqs)) return 1;

// Simply invoke the reloc function in the underlying FS
//
   if ((rc = Config.ossFS->Reloc("admin", Opt.Args[0], Opt.Args[1])))
      Emsg(-rc, "reloc ", Opt.Args[0]);
      else Msg(Opt.Args[0], " relocated to space ", Opt.Args[1]);
   return rc != 0;
}

/******************************************************************************/
/*                                R e m o v e                                 */
/******************************************************************************/

const char *XrdFrmAdmin::RemoveHelp = "rm [opts] lspec [lspec [...]]\n\n"

"opts: -e[cho] -f[orce] -n[otify] -r[ecursive]\n\n"

"lspec: lfn | ldir[*]";

int XrdFrmAdmin::Remove()
{
   static XrdOucArgs Spec(&Say, "frm_admin: ",    "",
                                "echo",        1, "E",
                                "force",       1, "F",
                                "recursive",   1, "r",
                          (const char *)0);

   static const char *Reqs[] = {"lfn", 0};

   const char *Txt = "";
   char buff[80];
   int rc = 0, aOK = 1;

// Parse the request
//
   if (!Parse("rm ", Spec, Reqs)) return 1;

// Do some initialization
//
   numDirs = numFiles = numProb = 0;

// Preform action
//
   do {Opt.All = VerifyAll(Opt.Args[0]);
       if ((rc = Unlink(Opt.Args[0])) < 0) aOK = 0;
      } while(rc && (Opt.Args[0] = Spec.getarg()));

   if (!rc) {Txt = "rm aborted; only "; finalRC = 4;}
      else if (numProb || !aOK) {Txt = "rm incomplete; only "; finalRC = 4;}

// Compose message
//
   sprintf(buff, "%s%d %s and %d %s deleted.", Txt,
          numFiles, (numFiles != 1 ? "files"       : "file"),
          numDirs,  (numDirs  != 1 ? "directories" : "directory"));
   Msg(buff);
   return 0;
}

/******************************************************************************/
/*                               s e t A r g s                                */
/******************************************************************************/

void XrdFrmAdmin::setArgs(int argc, char **argv)
{
   ArgC = argc; ArgV = argv; ArgS = 0;
}


void XrdFrmAdmin::setArgs(char *args)
{
   ArgC = 0;    ArgV = 0;    ArgS = args;
}

/******************************************************************************/
/*                               x e q A r g s                                */
/******************************************************************************/
  
int XrdFrmAdmin::xeqArgs(char *Cmd)
{
   static struct CmdInfo {const char *Name;
                          int         minLen;
                          int         maxLen;
                          int        (XrdFrmAdmin::*Method)();
                         }
                 CmdTab[] = {{"audit",  5, 5, &XrdFrmAdmin::Audit},
                             {"chksum", 6, 6, &XrdFrmAdmin::Chksum},
                             {"convert",7, 7, &XrdFrmAdmin::Convert},
                             {"exit",   4, 4, &XrdFrmAdmin::Quit},
                             {"find",   1, 4, &XrdFrmAdmin::Find},
                             {"help",   1, 4, &XrdFrmAdmin::Help},
                             {"makelf", 6, 6, &XrdFrmAdmin::MakeLF},
                             {"mark",   1, 4, &XrdFrmAdmin::Mark},
                             {"mmap",   1, 4, &XrdFrmAdmin::Mmap},
                             {"mv",     2, 2, &XrdFrmAdmin::Mv},
                             {"pin",    3, 3, &XrdFrmAdmin::Pin},
                             {"query",  1, 5, &XrdFrmAdmin::Query},
                             {"quit",   4, 4, &XrdFrmAdmin::Quit},
                             {"reloc",  5, 5, &XrdFrmAdmin::Reloc},
                             {"rm",     2, 2, &XrdFrmAdmin::Remove}
                            };
   static int CmdNum = sizeof(CmdTab)/sizeof(struct CmdInfo);

   int i, n = strlen(Cmd);

// Find the command
//
   for (i = 0; i < CmdNum; i++)
       if (n >= CmdTab[i].minLen && n <= CmdTab[i].maxLen
       &&  !strncmp(CmdTab[i].Name, Cmd, n)) break;

// See if we found the command
//
   if (i >= CmdNum)
      {Emsg("Invalid command - ", Cmd);
       return 1;
      }

// Perform required function
//
   return (*this.*CmdTab[i].Method)();
}

/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/*                                A b b r e v                                 */
/******************************************************************************/

int XrdFrmAdmin::Abbrev(const char *Spec, const char *Word, int minLen)
{
   int n = strlen(Spec);
   if (n > int(strlen(Word)) || n < minLen) return 0;
   return !strncmp(Spec, Word, n);
}
  
/******************************************************************************/
/*                           C o n f i g P r o x y                            */
/******************************************************************************/
  
void XrdFrmAdmin::ConfigProxy()
{
   static struct {const char *qFN; int qID;} qVec[] =
                 {{"getfQ.0", XrdFrcProxy::opGet},
                  {"migrQ.0", XrdFrcProxy::opMig},
                  {"pstgQ.0", XrdFrcProxy::opStg},
                  {"putfQ.0", XrdFrcProxy::opPut},
                  {0, 0}};
   struct stat Stat;
   char qBuff[1032], *qBase;
   int i, qTypes = 0;

// If we've been here before, return
//
   if (frmProxy || frmProxz) return;

// Construct the directory where the queue files reside
//
   strcpy(qBuff, Config.QPath);
   qBase = XrdFrcUtils::makeQDir(qBuff, -1);
   strcpy(qBuff, qBase); free(qBase); qBase = qBuff+strlen(qBuff);

// Since routines will create queue files we want to only look at queue files
// that actually exist. While may be none.
//
   for (i = 0; qVec[i].qFN; i++)
       {strcpy(qBase, qVec[i].qFN);
        if (!stat(qBuff, &Stat)) qTypes |= qVec[i].qID;
       }

// Check if we actually found any queues create them, otherwise complain.
//
   if (qTypes)
      {frmProxy = new XrdFrcProxy(Say.logger(),Config.myInst,Trace.What != 0);
       frmProxz = frmProxy->Init(qTypes, 0, -1, Config.QPath);
      } else {
       *qBase = 0; frmProxz = 1;
       Emsg("No transfer queues found in ", qBuff);
      }
}
  
/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/
  
void XrdFrmAdmin::Emsg(const char *tx1, const char *tx2, const char *tx3,
                       const char *tx4, const char *tx5)
{
     Say.Say("frm_admin: ", tx1, tx2, tx3, tx4, tx5);
     finalRC = 4;
}

void XrdFrmAdmin::Emsg(int ec, const char *tx2, const char *tx3,
                               const char *tx4, const char *tx5)
{
   char buff[128];

   if (!ec) Say.Say(tx2, tx3, tx4, tx5);
      else {strcpy(buff+2, strerror(ec));
            if (strncmp(buff+2, "Unknown", 7)) buff[2] = tolower(buff[2]);
               else sprintf(buff+2, "error %d", ec);
            buff[0] = ';'; buff[1] = ' ';
            Say.Say("frm_admin: Unable to ", tx2, tx3, tx4, tx5, buff);
           }
   finalRC = 4;
}

/******************************************************************************/
/*                                   M s g                                    */
/******************************************************************************/
  
void XrdFrmAdmin::Msg(const char *tx1, const char *tx2, const char *tx3,
                      const char *tx4, const char *tx5)
{
     Say.Say(tx1, tx2, tx3, tx4, tx5);
}

/******************************************************************************/
/*                                 P a r s e                                  */
/******************************************************************************/
  
int XrdFrmAdmin::Parse(const char *What, XrdOucArgs &Spec, const char **Reqs)
{
   static const int MaxArgs = sizeof(Opt.Args)/sizeof(char *);
   char theOpt;
   int i;

// Clear the option area
//
   memset(&Opt, 0, sizeof(Opt));
   Opt.Uid = static_cast<uid_t>(-1); Opt.Gid = static_cast<gid_t>(-1);

// Set the Arguments
//
   if (ArgS) Spec.Set(ArgS);
      else   Spec.Set(ArgC, ArgV);

// Now process all the options
//
   while((theOpt = Spec.getopt()) != (char)-1)
        {switch(theOpt)
               {case 'A': Opt.All     = 1; break;
                case 'e': Opt.Erase   = 1; break;
                case 'E': Opt.Echo    = 1; break;
                case 'f': Opt.Fix     = 1; break;
                case 'F': Opt.Force   = 1; break;
                case 'k': Opt.Keep    = 1;
                          if (!ParseKeep(What, Spec.argval)) return 0;
                          break;
                case 'K': Opt.Keep    = 1; break;  // No argument
                case 'l': Opt.Local   = 1; break;
                case 'm': Opt.MPType  ='m';break;
                case 'o': if (!ParseOwner(What, Spec.argval)) return 0;
                          break;
                case 'p': Opt.MPType  ='p';break;
                case 'r': Opt.Recurse = 1; break;
                case 't': if (!ParseType(What, Spec.argval)) return 0;
                          break;
                case 'v': Opt.Verbose = 1; break;
                case '?': return 0;
                default:  Emsg("Internal error mapping options!");
                          return 0;
               }
        }

// Check if we need additional arguments (up to three)
//
   for (i = 0; i < MaxArgs && Reqs[i]; i++)
        if (!(Opt.Args[i] = Spec.getarg()))
           {Emsg(What, Reqs[i], " not specified."); return 0;}

// All done
//
   return 1;
}

/******************************************************************************/
/*                             P a r s e K e e p                              */
/******************************************************************************/
  
int XrdFrmAdmin::ParseKeep(const char *What, const char *kTime)
{
   struct tm myTM;
   char *eP;
   int  theSec;

// Initialize the values
//
   Opt.ktAlways = 0;
   Opt.KeepTime = 0;
   Opt.ktIdle   = 0;

// Check for forever and unused
//
   if (!strcmp(kTime, "forever")) {Opt.ktAlways = 1; return 1;}

// if no slashes then this is number of days
//
   if (!index(kTime, '/'))
      {if (*kTime == '+') {Opt.ktIdle = 1; kTime++;}
       if (XrdOuca2x::a2tm(Say,"keep time", kTime, &theSec)) return 0;
       if (Opt.ktIdle || !theSec) {Opt.KeepTime = theSec; Opt.ktIdle = 1;}
          else Opt.KeepTime = static_cast<time_t>(theSec)+time(0);
       return 1;
      }

// Do a date conversion
//
   eP = strptime(kTime, "%D", &myTM);
   if (*eP) {Emsg("Invalid ", What, "keep date - ", kTime); return 0;}
   Opt.KeepTime = mktime(&myTM);
   return 1;
}

/******************************************************************************/
/*                            P a r s e O w n e r                             */
/******************************************************************************/
  
int XrdFrmAdmin::ParseOwner(const char *What, char *Uname)
{
   struct group  *grP;
   struct passwd *pwP;
   char  *Gname = 0;
   int    Gnum, Unum;

// Set defaults
//
   Opt.Uid = Config.myUid;
   Opt.Gid = Config.myGid;

// Separate the uid from the gid
//
   if (*Uname == ':') {Gname = Uname+1; Uname = 0;}
      else if ((Gname = index(Uname, ':'))) *Gname++ = '\0';
   if (Gname && *Gname == '\0') Gname = 0;

// Process username
//
   if (Uname)
      {if (*Uname >= '0' && *Uname <= '9')
          {if (XrdOuca2x::a2i(Say,"uid",Uname, &Unum)) return 0;
           Opt.Uid = Unum;
          }
          else {if (!(pwP = getpwnam(Uname)))
                   {Emsg("Invalid user name - ", Uname); return 0;}
                Opt.Uid = pwP->pw_uid; Opt.Gid = pwP->pw_gid;
               }
      }

// Process groupname
//
   if (Gname)
      {if (*Gname >= '0' && *Gname <= '9')
          {if (XrdOuca2x::a2i(Say, "gid", Gname, &Gnum))  return 0;
           Opt.Gid = Gnum;
          }
          else {if (!(grP = getgrnam(Gname)))
                   {Emsg("Invalid group name - ", Gname); return 0;}
                Opt.Gid = grP->gr_gid;
               }
      }

// All done
//
   return 1;
}

/******************************************************************************/
/*                            P a r s e S p a c e                             */
/******************************************************************************/
  
XrdOucTList *XrdFrmAdmin::ParseSpace(char *Space, char **Path)
{
   XrdOucTList *pP;

// Check if we should process all paths in the space or just one
//
   if ((*Path = index(Space, ':'))) {**Path = '\0'; (*Path)++;}

// Find the proper space entry
//
   if (!(pP = Config.Space(Space, *Path))) Emsg(Space, " space not found.");
      else if (!(pP->text))
              {Emsg(Space, " space does not contain ", *Path); pP = 0;}
   return pP;
}

/******************************************************************************/
/*                             P a r s e T y p e                              */
/******************************************************************************/
  
int XrdFrmAdmin::ParseType(const char *What, char *Type)
{

// Set checksum type and check for all
//
   if (!CksData.Set(Type)) {Emsg("Invalid type - ", Type); return 0;}
   return 1;
}

/******************************************************************************/
/*                             V e r i f y A l l                              */
/******************************************************************************/
  
int XrdFrmAdmin::VerifyAll(char *path)
{
   char *Slash = rindex(path, '/');

   if (!Slash || strcmp(Slash, "/*")) return 0;
   *Slash = '\0';
   return 1;
}

/******************************************************************************/
/*                              V e r i f y M P                               */
/******************************************************************************/
  
char XrdFrmAdmin::VerifyMP(const char *func, const char *path)
{
   unsigned long long Popts = 0;
   const char *msg = 0;
   int rc;

// Get the export attributes for this path
//
   if ((rc = Config.ossFS->StatXP(path, Popts)))
      {Emsg(rc, func, " ", path); return 0;}

// Resolve attributes to the options in effect
//
        if (Opt.MPType == 'm')
           {if (!(Popts & XRDEXP_MIG))   msg = " is not migratable";}
   else if (Opt.MPType == 'p')
           {if (!(Popts & XRDEXP_STAGE)) msg = " is not stageable"; }
   else if (Popts & XRDEXP_MIG)   Opt.MPType = 'm';
   else if (Popts & XRDEXP_STAGE) Opt.MPType = 'p';

   if (msg && !Opt.Force)
      return XrdFrcUtils::Ask('n', path, msg, "; continue?");
   return 'y';
}
