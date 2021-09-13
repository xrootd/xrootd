/******************************************************************************/
/*                                                                            */
/*                           X r d S h M a p . c c                            */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <string>
#include <cstring>
#include <cstdio>
#include <zlib.h>

#include "XrdSsi/XrdSsiShMap.hh"

using namespace std;

/* Gentoo removed OF from their copy of zconf.h but we need it here.
   See https://bugs.gentoo.org/show_bug.cgi?id=383179 for the sad history.
   This patch modelled after https://trac.osgeo.org/gdal/changeset/24622
*/
#ifndef OF
#define OF(args) args
#endif

/******************************************************************************/
/*                          U n i t   G l o b a l s                           */
/******************************************************************************/
  
namespace
{
   XrdSsi::ShMap<int>  *theMap = 0;
   XrdSsi::ShMap_Parms  rParms(XrdSsi::ShMap_Parms::ForResize);
   XrdSsi::ShMap_Parms  sParms;
   char                *keyPfx = strdup("key");
   const char          *hashN = "c32";
   XrdSsi::ShMap_Hash_t hashF = 0;
   const char          *path = "/tmp/shmap.sms";
   const char          *MeMe = "shmap: ";
   char                *uLine = 0;
   int                  tmo = -1;
   int                  xRC   = 0;
}

/******************************************************************************/
/*                               D e f i n e s                                */
/******************************************************************************/
  
#define FMSG(x) xRC|=1,cerr <<MeMe <<x <<endl
#define EMSG(x) xRC|=2,cerr <<MeMe <<x <<endl
#define SAY(x)  cerr <<MeMe <<x <<endl
#define UMSG(x) xRC=1,cerr <<MeMe <<"Unable to " <<x <<"; " <<strerror(errno) <<endl

#define IFCMD1(x)   if (!strcmp(x, cmd))
#define IFCMD2(x,y) if (!strcmp(x, cmd) || !strcmp(y, cmd))

/******************************************************************************/
/*                   C o m m a n d   E x p l a n a t i o n                    */
/******************************************************************************/

namespace
{
struct theHelp
      {const char     *cmd;
       const char     *alt;
       const char    **dtl;
       int             cln;
       int             dsz;

       theHelp(const char *cP, const char *aP, const char **dP, int dS)
              :cmd(cP), alt(aP), dtl(dP), cln(strlen(dP[0])),
               dsz(dS/sizeof(char *)) {}
      ~theHelp() {}
     };

const char *CLeHelp[] =
{"-e",
 "    Echoes input file read from the file specified with the -i option.",
};

const char *CLhHelp[] =
{"-h {a32|c32|x32}",
 "    Specifies the key hashing function. 'a32' uses adler, 'c32' uses crc, and",
 "    'x32' uses a simple xor function. The default is c32."
};

const char *CLiHelp[] =
{"-i <path>",
 "    Reads input newline separated commands from the file identified by <path>."
};

const char *CLpHelp[] =
{"-p <path>",
 "    Specifies the file name, <path>, of the shared memory segment.",
 "    The default is '/tmp/shmap.sms'."
};

const char *CLtHelp[] =
{"-t <atmo>",
 "    Specifies the attach timeout in seconds. The default is -1 which causes",
 "    attach to wait until the shared memory segment is exported."
};

const char *addHelp[] =
{"add <key> <val>",
 "    Adds key <key> with an integer value of <val> to the map. The key must",
 "    not be in the map. Use the rep command to replace it."
};

const char *attHelp[] =
{"att[ach] {r|w}",
 "    Attach a shared memory identified by the -p command line option.",
 "    The 'r' argument attaches it as read/only while 'w' attaches it read/write."
};

const char *creHelp[] =
{"cr[eate] {[m][s][r][u][=]}",
 "    Create a shared memory identified by the -p command line option.",
 "    Specify 'm' to enable multiple writers or 's' for a single writer.",
 "    Specify 'r' to enable space reuse or 'u' to disallow space reuse.",
 "    Specify '=' for defaults (su). See the setmax command for size options.",
 "    The map is not visible to other processes until 'export' is executed."
};

const char *dleHelp[] =
{"del[ete] <key>",
 "    Delete <key> from the map."
};

const char *detHelp[] =
{"det[ach]",
 "    Detach the shared memory segment."
};

const char *exiHelp[] =
{"ex[ists] <key>",
 "    Try to find <key> in the map and indicate whether or not it exists."
};

const char *xitHelp[] =
{"{exit | quit}",
 "    Exits; a non-zero return code may indicate an unexpected result."
};

const char *xplHelp[] =
{"explain {<cmd> | all}",
 "    Explain the specified command, <cmd>, or everything."
};

const char *expHelp[] =
{"exp[ort]",
 "    Make a newly created map visible to other processes."
};

const char *getHelp[] =
{"get <key>",
 "    Fetch the key <key> from the map and display its associated value."
};

const char *hasHelp[] =
{"hash <key>",
 "    Print the hash value of <key> using the default or current -h setting."
};

const char *hlpHelp[] =
{"{help | ?}",
 "    Display a synopsis of the xrdshmap command."
};

const char *infHelp[] =
{"info",
 "    Display information about the attached map."
};

const char *kysHelp[] =
{"keys",
 "    Enumerate the map displaying each key and its associated value."
};

const char *kpxHelp[] =
{"keypfx",
 "    Set the key prefix to use with the 'load', 'unload', 'verify', and 'verdel'",
 "    commands. The default is 'key'. See the commands for details.'"
};

const char *lodHelp[] =
{"load <numkeys>",
 "    Add <numkeys> to the map. Each key is formed as <keypfx><n> where",
 "    0 <= n < <numkeys> (e.g. key0, key1, etc). The key value is set to n.'"
};

const char *qmxHelp[] =
{"qmax",
 "    Display the size defaults for creating or resizing a map. See 'setmax'."
};

const char *repHelp[] =
{"rep[lace] <key> <val>",
 "    Replace key <key> with an integer value of <val> in the map. If the key",
 "    exists, its previous value is displayed."
};

const char *rszHelp[] =
{"res[ize] {[m][s][r][u][=]}",
 "    Resize a shared memory identified by the -p command line option.",
 "    Specify 'm' to enable multiple writers or 's' for a single writer.",
 "    Specify 'r' to enable space reuse or 'u' to disallow space reuse.",
 "    Specify '=' to use the existing values in the map. This compresses the",
 "    map as much as possible. The map must have been exported. See the setmax",
 "    command for more size options."
};

const char *smxHelp[] =
{"setmax [cr[eate] | res[ize]] {index | keylen | keys | mode} <val>",
 "    Set the creation or resizing values for a subsequent create or resize.",
 "    To only set the create value specify 'cr' or 'res' for just resizing.",
 "    If neither is specified, the values are set for both commands. Specify",
 "    index    number of hash table entries. For rsz, 0 uses the map's value.",
 "    keylen   maximum key length in bytes.  For rsz, 0 uses the map's value.",
 "    keys     maximum number of keys.       For rsz, 0 uses the map's value.",
 "    mode     the creation mode for create. The resize command ignores this.",
 "    <val> is the integer value to use. Use qmax to display the values."
};

const char *susHelp[] =
{"sus[pend]",
 "    Stop execution and wait for a carriage return. This is useful when testing",
 "    interactively with file or command line input. Also, see 'wait'."
};

const char *synHelp[] =
{"sync {all | off | on | now | <qsz>}",
 "    Set synchronization parameters between shared memory and its file.",
 "    all   turn sync on; pages are written synchronously in the foreground.",
 "    off   turn sync off (initial setting) and let the kernel do it whenever.",
 "    on    turn sync on; pages are written asynchronously in the background.",
 "    now   write back any modified pages but don't change any sync settings.",
 "    <qsz> specifies the maximum number of changed pages before a sync occurs.",
 "          The setting must be on or all for the sync to actually occur."
};

const char *unlHelp[] =
{"unload <numkeys>",
 "    Delete <numkeys> from the map. Each key is formed as <keypsz><n> where",
 "    0 <= n < <numkeys> (e.g. key0, key1, etc)."
};

const char *verHelp[] =
{"ver[ify] <numkeys>",
 "    Verify that <numkeys> have the expected value in the map. Each key is",
 "    formed as <keypsz><n> where 0 <= n < <numkeys> (e.g. key0, key1, etc).",
 "    The key's value must equal n.'"
};

const char *vdlHelp[] =
{"verdel <numkeys>",
 "    Perform an unload/verify operation for <numkeys>. Each deleted key must",
 "    have the expected value (see unload and verify)."
};

const char *wwtHelp[] =
{"wait <sec>",
 "    Pause the program for <sec> seconds. This is useful when testing in the",
 "    background. Also, see the suspend command."
};

theHelp helpInfo[] =
{theHelp("-e",  0,        CLeHelp, sizeof(CLeHelp)),
 theHelp("-h",  0,        CLhHelp, sizeof(CLhHelp)),
 theHelp("-i",  0,        CLiHelp, sizeof(CLiHelp)),
 theHelp("-p",  0,        CLpHelp, sizeof(CLpHelp)),
 theHelp("-t",  0,        CLtHelp, sizeof(CLtHelp)),
 theHelp("add", 0,        addHelp, sizeof(addHelp)),
 theHelp("att", "attach", attHelp, sizeof(attHelp)),
 theHelp("cr",  "create", creHelp, sizeof(creHelp)),
 theHelp("del", "delete", dleHelp, sizeof(dleHelp)),
 theHelp("det", "detach", detHelp, sizeof(detHelp)),
 theHelp("ex",  "exists", exiHelp, sizeof(exiHelp)),
 theHelp("exit", "quit",  xitHelp, sizeof(xitHelp)),
 theHelp("explain",0,     xplHelp, sizeof(xplHelp)),
 theHelp("exp",  "export",expHelp, sizeof(expHelp)),
 theHelp("get",    0,     getHelp, sizeof(getHelp)),
 theHelp("hash",   0,     hasHelp, sizeof(hasHelp)),
 theHelp("help","?",      hlpHelp, sizeof(hlpHelp)),
 theHelp("info",   0,     infHelp, sizeof(infHelp)),
 theHelp("keys",   0,     kysHelp, sizeof(kysHelp)),
 theHelp("keypfx", 0,     kpxHelp, sizeof(kpxHelp)),
 theHelp("load",   0,     lodHelp, sizeof(lodHelp)),
 theHelp("qmax",   0,     qmxHelp, sizeof(qmxHelp)),
 theHelp("rep", "replace",repHelp, sizeof(repHelp)),
 theHelp("res", "resize", rszHelp, sizeof(rszHelp)),
 theHelp("setmax", 0,     smxHelp, sizeof(smxHelp)),
 theHelp("sus", "suspend",susHelp, sizeof(susHelp)),
 theHelp("sync",   0,     synHelp, sizeof(synHelp)),
 theHelp("unload", 0,     unlHelp, sizeof(unlHelp)),
 theHelp("ver", "verify", verHelp, sizeof(verHelp)),
 theHelp("verdel", 0,     vdlHelp, sizeof(vdlHelp)),
 theHelp("wt", "wait",    wwtHelp, sizeof(wwtHelp)),
};
}
  
/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/

namespace
{
void Usage()
{
   const char *lbeg = "command: ";
   const char *lcon = "         ";
   int ulen = 0, i, n = sizeof(helpInfo)/sizeof(theHelp), plen = strlen(lbeg);
   int zlen = 0, zpos;

// Find where the commands start
//
   for (i = 0; i < n; i++) if (*helpInfo[i].cmd != '-') break;

// Count up characters and llocate a buffer
//
   for (int j = i; j < n; j++) ulen += helpInfo[j].cln + plen + 3;
   uLine = new char[ulen+(plen*((ulen+80)/80))];

// Copy over all of the commands
//
   strcpy(uLine, lbeg); zlen = zpos = plen;
   for (int j = i; j < n; j++)
       {if (zlen + helpInfo[j].cln > 78)
           {uLine[zpos-1] = '\n';
            strcpy(&uLine[zpos], lcon);
            zlen = plen; zpos += plen;
           }
        strcpy(&uLine[zpos], helpInfo[j].dtl[0]);
        zpos += helpInfo[j].cln; strcpy(&uLine[zpos], " | ");
        zpos += 3;
        zlen += helpInfo[j].cln + 3;
       }

// Finish off the line
//
   uLine[zpos-3] = 0;
}

/******************************************************************************/

int Usage(int rc, bool terse=true)
{

cerr <<"Usage:   xrdshmap [options] [command [command [...]]]\n\n";
cerr <<"options: [-e] [-h {a32|c32|x32}] [-i <file>] [-p <path>] [-t <atmo>]\n\n";

   if (terse) return rc;

   if (!uLine) Usage();
   cerr <<uLine <<endl;

return rc;
}
}

/******************************************************************************/
/*                               E x p l a i n                                */
/******************************************************************************/

namespace
{
void Explain(const char *what)
{
   int i, n = sizeof(helpInfo)/sizeof(theHelp);

   if (!strcmp(what, "all")) i = 0;
      else {for (i = 0; i < n; i++)
                {if (!strcmp(what, helpInfo[i].cmd)
                 || (helpInfo[i].alt && !strcmp(what, helpInfo[i].alt)))
                    {n = i+1; break;}
                }
            if (i >= n)
               {cerr <<MeMe <<"No explanation for " <<what <<"; try help!" <<endl;
                return;
               }
           }

   for (int k = i; k < n; k++)
       {if (k > i) cerr <<'\n' <<endl;
        for (int j = 0; j < helpInfo[k].dsz; j++)
            {cerr <<helpInfo[k].dtl[j] <<endl;}
       }
   return;
}
}
  
/******************************************************************************/
/*                                 D o A 3 2                                  */
/******************************************************************************/
  
int DoA32(const char *buff)
{
   ZEXTERN uLong ZEXPORT adler32 OF((uLong adler, const Bytef *buf, uInt len));
   uLong adler = adler32(0L, Z_NULL, 0);

// Check for ID request now
//
   if (!buff) {int myID; memcpy(&myID, "c32 ", sizeof(int)); return myID;}

// Compute hash
//
   int blen = strlen(buff);
   adler = adler32(adler, (const Bytef *)buff, blen);
   int a32 = static_cast<int>(adler);
// cerr <<"Z a32 sz=" <<sizeof(adler) <<" val=" <<hex <<adler <<dec <<endl;
// cerr <<"S a32 sz=" <<sizeof(a32)   <<" val=" <<hex <<a32   <<dec <<endl;
   return a32;
}

/******************************************************************************/
/*                                 D o C 3 2                                  */
/******************************************************************************/
  
int DoC32(const char *buff)
{
   ZEXTERN uLong ZEXPORT crc32 OF((uLong crc, const Bytef *buf, uInt len));

// Check for ID request now
//
   if (!buff) {int myID; memcpy(&myID, "c32 ", sizeof(int)); return myID;}

// Compute hash
//
   uLong crc   = crc32(0L, Z_NULL, 0);
   int blen = strlen(buff);
   crc   = crc32(crc,   (const Bytef *)buff, blen);
   int c32 = static_cast<int>(crc);
// cerr <<"Z c32 sz=" <<sizeof(crc) <<" val=" <<hex <<crc <<dec <<endl;
// cerr <<"S c32 sz=" <<sizeof(c32) <<" val=" <<hex <<c32   <<dec <<endl;
   return c32;
}

/******************************************************************************/
/*                                 D o X 3 2                                  */
/******************************************************************************/
  
int DoX32(const char *key)
{  int j, hval, kval, klen;
   int *lp;

// Check for ID request now
//
   if (!key) {int myID; memcpy(&myID, "c32 ", sizeof(int)); return myID;}

// If name is shorter than the hash length, use the name.
//
   klen = strlen(key);
   if (klen <= (int)sizeof(int))
      {hval = 0;
       memcpy(&hval, key, klen);
       return hval;
      }

// Compute the int words in the name and develop starting hash.
//
   hval = klen;
   j = klen % sizeof(int); klen /= sizeof(int);
   if (j) 
      {memcpy(&kval, key, sizeof(int));
       hval ^= kval;
      }
   lp = (int *)&key[j];

// Compute and return the full hash.
//
   while(klen--)
        {memcpy(&kval, lp++, sizeof(int));
         hval ^= kval;
        }
   return (hval ? hval : 1);
}

/******************************************************************************/
/*                                D o D u m p                                 */
/******************************************************************************/
  
namespace
{
void DoDump(bool getdata)
{
   void *myJar = 0;
   char *kbuff;
   int  *dataP, num = 0;;

   while(theMap->Enumerate(myJar, kbuff, dataP))
        {cout <<kbuff <<" = " <<*dataP <<endl;
         num++;
        }

   if (errno == ENOENT) cerr <<num <<" entries displayed." <<endl;
      else EMSG("Unable to fully enumerate keys; " <<strerror(errno));
}
}

/******************************************************************************/
/*                                D o I n f o                                 */
/******************************************************************************/
  
namespace
{
void DoInfo()
{
   const char *vname[] = {"flockro", "flockrw",  "indexsz",  "indexused",
                          "keys",    "keysfree", "maxkeylen",
                          "multw",   "reuse",    "typesz", 0};
   int n, i = 0;
   char iBuff[256];

// Get the atomics being used
//
   n = theMap->Info("atomics", iBuff, sizeof(iBuff));
   if (n < 0)
      {UMSG("get info for atomics");
       if (errno == ENOTCONN) return;
      }
   cout <<"atomics = " <<iBuff <<endl;

// Get the hash name.
//
   n = theMap->Info("hash", iBuff, sizeof(iBuff));
   if (n < 0)
      {UMSG("get info for hash");
       if (errno == ENOTCONN) return;
      }
   cout <<"hash = " <<iBuff <<endl;

// Get the implementation
//
   n = theMap->Info("impl", iBuff, sizeof(iBuff));
   if (n < 0)
      {UMSG("get info for implementation");
       if (errno == ENOTCONN) return;
      }
   cout <<"impl = " <<iBuff <<endl;

// Print the rest of them
//
   while(vname[i])
        {n = theMap->Info(vname[i]);
         if (n >= 0) cout <<vname[i] <<" = " <<n <<endl;
            else UMSG("get info for " <<vname[i]);
         i++;
        }

// Get the type name
//
   n = theMap->Info("type", iBuff, sizeof(iBuff));
   if (n < 0)
      {UMSG("get info for the type");
       if (errno == ENOTCONN) return;
      }
   cout <<"type = " <<iBuff <<endl;
}
}

/******************************************************************************/
/*                                 D o L U V                                  */
/******************************************************************************/

namespace
{
enum isLUV {isAdd = 0, isUnload, isUnver, isVer};

bool DoLUV(int numkeys, isLUV what)
{
   const char *endtxt = (what == isUnver ? " and unloaded." : ".");
   char key[256];
   int kval, numOK = 0;

   for (int k = 0; k < numkeys; k++)
       {sprintf(key, "%s%d", keyPfx, k);
              if (what == isAdd)
                 {if (!(theMap->Add(key, k)))
                     {UMSG("add key " <<key);
                      return false;
                     }
                 }
         else if (what == isUnload)
                 {if (!theMap->Del(key))
                     {UMSG("ver key " <<key);
                      return false;
                     }
                 }
         else    {if ((what == isVer ? theMap->Get(key,  kval)
                                     : theMap->Del(key, &kval)))
                     {if (k == kval) numOK++;
                         else EMSG("Key " <<key <<" has incorrect value " <<kval);
                      continue;
                     }
                  if (errno != ENOENT)
                     {UMSG("ver key " <<key);
                      return false;
                     }
                 }
       }

        if (what == isAdd)    SAY(numkeys <<" keys added.");
   else if (what == isUnload) SAY(numkeys <<" keys unloaded.");
   else SAY(numOK <<" out of " <<numkeys <<" keys verified" <<endtxt);
   return true;
}
}
  
/******************************************************************************/
/*                                 T o k e n                                  */
/******************************************************************************/
  
namespace
{
   char               **Argv  = 0;
   int                  Argc  = 0;
   int                  Apos  = 0;
   istream             *inStream;
   std::string          inLine;
   char                *line   = 0;
   const char          *prompt = 0;
   bool                 echo   = false;
  
char *Token(const char *what1, const char *what2=0, bool cont=true)
{
   char *token;
   int   n;

// If using argv then return a token or exit.
//
   if (Argv)
      {if (Apos >= Argc)
          {if (cont) return 0;
           exit(xRC);
          }
       return Argv[Apos++];
      }

// Get input from a file or the terminal
//
do{if (!line || !cont)
      {if (line) {delete [] line; line = 0;}
       if (prompt)
          {cerr <<what1;
           if (what2) cerr <<' ' <<what2 <<": ";
              else    cerr <<": ";
          }
       std::getline(*inStream, inLine, '\n');
       if (!(n = inLine.length()))
          {if (prompt|| !inStream->eof()) continue;
              else return 0;
          }
       line = new char[n+1];
       strcpy(line, inLine.c_str());
       if (echo) cerr <<MeMe <<line <<endl;
       token = strtok(line, " ");
      } else token = strtok(0, " ");

// Return the token if we have one
//
   if (token) return token;

// Check if we should complain
//
   if (cont) cerr <<what1 <<(what2 ? what2 : "") <<" not specified." <<endl;
   delete [] line; line = 0;
   if (cont) return 0;
  } while(true);
}
}

/******************************************************************************/
/*                                   X e q                                    */
/******************************************************************************/

namespace
{
void Xeq()
{
   char *cmd, *token, *key, *val, *xval, *theOp, c;
   int kval, numkeys, pval;

// Process commands
//
while(true)
     {if (!(cmd = Token("shmap", 0, false))) return;

      IFCMD1("add")
           {if (!(key = Token("key to add")))    continue;
            if (!(val = Token("add key value"))) continue;
            kval = atoi(val);
                 if (theMap->Add(key, kval)) SAY("key " <<key <<" added!");
            else if (errno == EEXIST) FMSG("key " <<key <<" already exists!");
            else UMSG("add key " <<key);
            continue;
           }

      IFCMD2("att", "attach")
           {XrdSsi::ShMap_Access  acc = XrdSsi::ReadOnly;
            if (!(theOp = Token("attach argument"))) continue;
            while(*theOp)
                 {switch(*theOp)
                        {case 'r': acc = XrdSsi::ReadOnly;
                                   break;
                         case 'w': acc = XrdSsi::ReadWrite;
                                   break;
                         default:  EMSG("Unknown attach option - " <<*theOp);
                                   theOp++;
                                   continue;
                        }
                       theOp++;
                 }
            if (!(theMap->Attach(path, acc, tmo))) UMSG("attach map");
               else SAY("attach OK");
            continue;
           }

      IFCMD2("cr", "create")
           {int attOpts = 0;
            if (!(theOp = Token("create argument"))) continue;
            while(*theOp)
                 {switch(*theOp)
                        {case 'm': attOpts |= XrdSsi::ShMap_Parms::MultW;
                                   break;
                         case 's': attOpts |= XrdSsi::ShMap_Parms::noMultW;
                                   break;
                         case 'r': attOpts |= XrdSsi::ShMap_Parms::ReUse;
                                   break;
                         case 'u': attOpts |= XrdSsi::ShMap_Parms::noReUse;
                                   break;
                         case '=': attOpts = 0;
                                   break;
                         default:  EMSG("Unknown create option - " <<*theOp);
                                   theOp++;
                                   continue;
                        }
                       theOp++;
                 }
            sParms.options = attOpts;
            if (!(theMap->Create(path, sParms))) UMSG("create map");
               else SAY("create OK");
            continue;
           }

      IFCMD2("del", "delete")
           {if (!(key = Token("key to delete"))) continue;
                 if (theMap->Del(key, &pval))
                    SAY("key " <<key <<" deleted; pval=" <<pval);
            else if (errno == ENOENT) FMSG("key " <<key <<" not found!");
            else UMSG("delete key " <<key);
            continue;
           }

      IFCMD2("det", "detach")
           {theMap->Detach();
            SAY("detach done!");
            continue;
           }

      IFCMD2("ex", "exists")
           {if (!(key = Token("key to check"))) continue;
                 if (theMap->Exists(key)) SAY("key " <<key <<" exists!");
            else if (errno == ENOENT) SAY("key " <<key <<" does not exist!");
            else UMSG("check key " <<key);
            continue;
           }

      IFCMD1("explain")
           {if (!(key = Token("command to explain"))) continue;
            Explain(key);
            continue;
           }

      IFCMD2("exp", "export")
           {if (theMap->Export()) SAY("export OK");
               else UMSG("export map");
            continue;
           }

      IFCMD1("get")
           {if (!(key = Token("key to get"))) continue;
                 if (theMap->Get(key, pval)) SAY("key " <<key <<" = " <<pval);
            else if (errno == ENOENT) FMSG("key " <<key <<" not found!");
            else UMSG("get key " <<key);
            continue;
           }

      IFCMD1("hash")
           {if (!(key = Token("key to hash"))) continue;
            if (!hashF) kval = DoC32(key);
               else     kval = hashF(key);
            cout <<hashN <<' ' <<hex <<kval <<dec <<' ' <<key <<endl;
            continue;
           }

      IFCMD2("help", "?")
           {Usage(0, false);
            continue;
           }

      IFCMD1("info")    {DoInfo();      continue;}

      IFCMD1("keys")    {DoDump(false); continue;}

      IFCMD1("keypfx")
           {if (!(key = Token("keypfx key"))) continue;
            if (keyPfx) free(keyPfx);
            keyPfx = strdup(key);
            continue;
           }

      IFCMD1("load")
           {if (!(val = Token("load count"))) continue;
            numkeys = strtol(val, &xval, 10);
            if (numkeys <= 0 || *xval)
               EMSG("number of keys to load is invalid.");
               else DoLUV(numkeys, isAdd);
            continue;
           }

      IFCMD1("qmax")
            {cerr <<"setmax create index  " <<sParms.indexSize <<endl;
             cerr <<"setmax create keylen " <<sParms.maxKeyLen <<endl;
             cerr <<"setmax create keys   " <<sParms.maxKeys   <<endl;
             cerr <<"setmax create mode   " <<oct <<sParms.mode <<dec <<endl;
             cerr <<"setmax resize index  " <<rParms.indexSize <<endl;
             cerr <<"setmax resize keylen " <<rParms.maxKeyLen <<endl;
             cerr <<"setmax resize keys   " <<rParms.maxKeys   <<endl;
             continue;
            }

      IFCMD2("quit", "exit") break;

      IFCMD2("rep", "replace")
           {if (!(key = Token("key to replace")))    continue;
            if (!(val = Token("replace key value"))) continue;
            kval = atoi(val); pval = -1;
            if (!(theMap->Rep(key, kval, &pval))) UMSG("rep key " <<key);
               else {if (errno == EEXIST)
                        SAY("key " <<key <<" replaced; pval=" <<pval);
                        else SAY("key " <<key <<" added.");
                    }
            continue;
           }

      IFCMD2("res", "resize")
           {XrdSsi::ShMap_Parms *xParms = &rParms;
            int   rszOpts               = 0;
            if (!(theOp = Token("resize argument"))) continue;
            while(*theOp)
                 {switch(*theOp)
                        {case 'm': rszOpts |= XrdSsi::ShMap_Parms::MultW;
                                   break;
                         case 's': rszOpts |= XrdSsi::ShMap_Parms::noMultW;
                                   break;
                         case 'r': rszOpts |= XrdSsi::ShMap_Parms::ReUse;
                                   break;
                         case 'u': rszOpts |= XrdSsi::ShMap_Parms::noReUse;
                                   break;
                         case '=': xParms = 0;
                                   break;
                         default:  EMSG("Unknown resize option - " <<*theOp);
                                   theOp++;
                                   continue;
                        }
                       theOp++;
                 }
            rParms.options = rszOpts;
            if (!(theMap->Resize(xParms))) UMSG("resize map");
               else SAY("resize OK");
            continue;
           }

      IFCMD1("setmax")
            {XrdSsi::ShMap_Parms *rP = &rParms;
             XrdSsi::ShMap_Parms *sP = &sParms;
             int *dst1, *dst2, base = 10;
             if (!(cmd = Token("setmax argument"))) continue;
                  IFCMD2("create","cr" ) {rP=sP; token = Token(cmd,"argument");}
             else IFCMD2("resize","res") {sP=rP; token = Token(cmd,"argument");}
             else token = cmd;
             if (!token) continue;
                  if (!strcmp("index", token))
                     {dst1 = &(rP->indexSize); dst2 = &(sP->indexSize);}
             else if (!strcmp("keylen",  token))
                     {dst1 = &(rP->maxKeyLen); dst2 = &(sP->maxKeyLen);}
             else if (!strcmp("keys",    token))
                     {dst1 = &(rP->maxKeys);   dst2 = &(sP->maxKeys);}
             else if (!strcmp("mode",    token))
                     {dst1 = &(rP->mode);      dst2 = &(sP->mode);   base = 8;}
             else {EMSG("Invalid setmax argument - " <<token); continue;}
             if (!(val = Token(token,"value"))) continue;
             kval = strtol(val, &key, base);
             if (kval < 0 || *key)
                {EMSG("Invalid " <<token <<" value = " <<val); continue;}
             *dst1 = kval; *dst2 = kval;
             continue;
            }

      IFCMD2("sus", "suspend")
           {cerr <<"Hit enter to continue:"; cin.getline(&c,1); continue;}

      IFCMD1("sync")
           {XrdSsi::SyncOpt sopt;
            if (!(key = Token("sync argument"))) continue;
                 if (!strcmp(key, "off")) sopt = XrdSsi::SyncOff;
            else if (!strcmp(key, "on") ) sopt = XrdSsi::SyncOn;
            else if (!strcmp(key, "all")) sopt = XrdSsi::SyncAll;
            else if (!strcmp(key, "now")) sopt = XrdSsi::SyncNow;
            else if (*key >= '0' && *key <= '9')
                    {kval = strtol(key, &xval, 10);
                     if (kval < 0 || *xval)
                        {EMSG("invalid queue size - " <<key); continue;}
                      sopt = XrdSsi::SyncQSz;
                    }
            else {EMSG(key <<" is an invalid sync argument."); continue;}
            if (theMap->Sync(sopt, kval)) SAY("sync OK");
               else UMSG("sync map");
            continue;
           }

      IFCMD1("unload")
           {if (!(val = Token("unload count"))) continue;
            numkeys = strtol(val, &xval, 10);
            if (numkeys <= 0 || xval)
               EMSG("number of keys to unload is invalid.");
               else DoLUV(numkeys, isUnload);
            continue;
           }

      IFCMD2("ver", "verify")
           {if (!(val = Token("verify count"))) continue;
            numkeys = strtol(val, &xval, 10);
            if (numkeys <= 0 || *xval)
               EMSG("number of keys to verify is invalid.");
               else DoLUV(numkeys, isVer);
            continue;
           }

      IFCMD1("verdel")
           {if (!(val = Token("verdel count"))) continue;
            numkeys = strtol(val, &xval, 10);
            if (numkeys <= 0 || *xval)
               EMSG("number of keys to ver and del is invalid.");
               else DoLUV(numkeys, isUnver);
            continue;
           }

      IFCMD1("wait")
           {if (!(val = Token("seconds to wait"))) continue;
            kval = strtol(val, &xval, 10);
            if (kval <= 0 || *xval) EMSG("seconds to wait is invalid.");
               else sleep(kval);
            continue;
           }

      EMSG("Unknown command - " <<cmd);
     }

// Add done
//
   exit(xRC);
}
}
  
/******************************************************************************/
/*                                  m a i n                                   */
/******************************************************************************/
  
int main(int argc, char **argv)
{
   extern char *optarg;
   extern int optind, opterr;
   ifstream inFile;
   char *inpath = 0, c;

// Process options
//
   opterr = 0;
   if (argc > 1 && '-' == *argv[1]) 
      while ((c = getopt(argc,argv,":eh:i:p:t:"))
             && ((unsigned char)c != 0xff))
     { switch(c)
       {
       case 'e': echo = true;
                 break;
       case 'h':      if (!strcmp(optarg,"a32")) {hashF = DoA32; hashN = "a32";}
                 else if (!strcmp(optarg,"c32")) {hashF = DoC32; hashN = "c32";}
                 else if (!strcmp(optarg,"x32")) {hashF = DoX32; hashN = "x32";}
                 else {EMSG(optarg <<" hash is not supported.");
                       exit(Usage(1));
                      }
                 break;
       case 'i': inpath  = optarg;
                 break;
       case 'p': path    = optarg;
                 break;
       case 't': tmo     = atoi(optarg);
                 break;
       case ':': EMSG('-' <<char(optopt) <<" parameter not specified.");
                 exit(Usage(1));
                 break;
       case '?': EMSG('-' <<char(optopt) <<" is not an option.");
                 exit(Usage(1));
                 break;
       default:  break;
       }
     }

// Check if we are using the arglist or are getting interactive input
//
        if (inpath)
           {if (optind < argc)
               cerr <<MeMe <<"Warning, -i supersedes arguments!" <<endl;
            inFile.open(inpath, std::ifstream::in);
            if (!inFile.good()) {UMSG("open " <<inpath); exit(8);}
            inStream = &inFile;
           }
   else if (optind < argc) {Argv = argv; Argc = argc; Apos = optind;}
   else {inStream = &cin; prompt = MeMe;}

// Allocate a map
//
   theMap = new XrdSsi::ShMap<int>("int", hashF);

// Process command stream
//
   Xeq();
   return xRC;
}
