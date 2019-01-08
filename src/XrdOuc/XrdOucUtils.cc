/******************************************************************************/
/*                                                                            */
/*                        X r d O u c U t i l s . c c                         */
/*                                                                            */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <stdio.h>

#ifdef WIN32
#include <direct.h>
#include "XrdSys/XrdWin32.hh"
#else
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdOuc/XrdOucStream.hh"
  
/******************************************************************************/
/*                              e n d s W i t h                               */
/******************************************************************************/
  
bool XrdOucUtils::endsWith(const char *text, const char *ending, int endlen)
{
   int tlen = strlen(text);

   return (tlen >= endlen && !strcmp(text+(tlen-endlen), ending));
}

/******************************************************************************/
/*                                 e T e x t                                  */
/******************************************************************************/
  
// eText() returns the text associated with the error, making the first
// character in the text lower case. The text buffer pointer is returned.

char *XrdOucUtils::eText(int rc, char *eBuff, int eBlen, int AsIs)
{
   const char *etP;

// Get error text
//
   if (!(etP = strerror(rc)) || !(*etP)) etP = "reason unknown";

// Copy the text and lower case the first letter
//
   strlcpy(eBuff, etP, eBlen);
   if (!AsIs) *eBuff = tolower(*eBuff);

// All done
//
   return eBuff;
}

/******************************************************************************/
/*                                  d o I f                                   */
/******************************************************************************/
  
// doIf() parses "if [<hostlist>] [<conds>]"
// conds: <cond1> | <cond2> | <cond3>
// cond1: defined <varlist> [&& <conds>]
// cond2: exec <pgmlist> [&& <cond3>]
// cond3: named <namelist>

// Returning 1 if true (i.e., this machine is one of the named hosts in hostlist 
// and is running one of the programs pgmlist and named by one of the names in 
// namelist).
// Return -1 (negative truth) if an error occured.
// Otherwise, returns false (0). Some combination of hostlist, pgm, and 
// namelist, must be specified.

int XrdOucUtils::doIf(XrdSysError *eDest, XrdOucStream &Config,
                      const char *what,  const char *hname,
                      const char *nname, const char *pname)
{
   static const char *brk[] = {"defined", "exec", "named", 0};
   XrdOucEnv *theEnv = 0;
   char *val;
   int hostok, isDef;

// Make sure that at least one thing appears after the if
//
   if (!(val = Config.GetWord()))
      {if (eDest) eDest->Emsg("Config","Host name missing after 'if' in", what);
       return -1;
      }

// Check if we are one of the listed hosts
//
   if (!is1of(val, brk))
      {do {hostok = XrdNetUtils::Match(hname, val);
           val = Config.GetWord();
          } while(!hostok && val && !is1of(val, brk));
      if (hostok)
         { while(val && !is1of(val, brk)) val = Config.GetWord();
           // No more directives
           if (!val) return 1;
         } else return 0;
      }

// Check if this is a defined test
//
   while(!strcmp(val, "defined"))
      {if (!(val = Config.GetWord()) || *val != '?')
          {if (eDest)
             {eDest->Emsg("Config","'?var' missing after 'defined' in",what);}
           return -1;
          }
       // Get environment if we have none
       //
          if (!theEnv && (theEnv = Config.SetEnv(0))) Config.SetEnv(theEnv);
          if (!theEnv && *(val+1) != '~') return 0;

       // Check if any listed variable is defined.
       //
       isDef = 0;
       while(val && *val == '?')
            {if (*(val+1) == '~' ? getenv(val+2) : theEnv->Get(val+1)) isDef=1;
             val = Config.GetWord();
            }
       if (!val || !isDef) return isDef;
       if (strcmp(val, "&&"))
          {if (eDest)
             {eDest->Emsg("Config",val,"is invalid for defined test in",what);}
           return -1;
          } else {
           if (!(val = Config.GetWord()))
              {if (eDest)
                  {eDest->Emsg("Config","missing keyword after '&&' in",what);}
               return -1;
              }
          }
       if (!is1of(val, brk))
          {if (eDest)
             {eDest->Emsg("Config",val,"is invalid after '&&' in",what);}
           return -1;
          }
      }

// Check if we need to compare program names (we are here only if we either
// passed the hostlist test or there was no hostlist present)
//
   if (!strcmp(val, "exec"))
      {if (!(val = Config.GetWord()) || !strcmp(val, "&&"))
          {if (eDest)
             {eDest->Emsg("Config","Program name missing after 'if exec' in",what);}
           return -1;
          }

       // Check if we are one of the programs.
       //
       if (!pname) return 0;
       while(val && strcmp(val, pname))
            if (!strcmp(val, "&&")) return 0;
               else  val = Config.GetWord();
       if (!val) return 0;
       while(val && strcmp(val, "&&")) val = Config.GetWord();
       if (!val) return 1;

       if (!(val = Config.GetWord()))
          {if (eDest)
             {eDest->Emsg("Config","Keyword missing after '&&' in",what);}
           return -1;
          }
       if (strcmp(val, "named"))
          {if (eDest)
             {eDest->Emsg("Config",val,"is invalid after '&&' in",what);}
           return -1;
          }
      }

// Check if we need to compare net names (we are here only if we either
// passed the hostlist test or there was no hostlist present)
//
   if (!(val = Config.GetWord()))
      {if (eDest)
         {eDest->Emsg("Config","Instance name missing after 'if named' in", what);}
       return -1;
      }

// Check if we are one of the names
//
   if (!nname) return 0;
   while(val && strcmp(val, nname)) val = Config.GetWord();

// All done
//
   return (val != 0);
}

/******************************************************************************/
/*                              f m t B y t e s                               */
/******************************************************************************/
  
int XrdOucUtils::fmtBytes(long long val, char *buff, int bsz)
{
   static const long long Kval = 1024LL;
   static const long long Mval = 1024LL*1024LL;
   static const long long Gval = 1024LL*1024LL*1024LL;
   static const long long Tval = 1024LL*1024LL*1024LL*1024LL;
   char sName = ' ';
   int resid;

// Get correct scaling
//
        if (val < 1024)  return snprintf(buff, bsz, "%lld", val);
        if (val < Mval) {val = val*10/Kval; sName = 'K';}
   else if (val < Gval) {val = val*10/Mval; sName = 'M';}
   else if (val < Tval) {val = val*10/Gval; sName = 'G';}
   else                 {val = val*10/Tval; sName = 'T';}
   resid = val%10LL; val = val/10LL;

// Format it
//
   return snprintf(buff, bsz, "%lld.%d%c", val, resid, sName);
}

/******************************************************************************/
/*                               g e n P a t h                                */
/******************************************************************************/

char *XrdOucUtils::genPath(const char *p_path, const char *inst, 
                           const char *s_path)
{
   char buff[2048];
   int i = strlcpy(buff, p_path, sizeof(buff));

   if (buff[i-1] != '/') {buff[i++] = '/'; buff[i] = '\0';}
   if (inst) {strcpy(buff+i, inst); strcat(buff, "/");}
   if (s_path) strcat(buff, s_path);

   i = strlen(buff);
   if (buff[i-1] != '/') {buff[i++] = '/'; buff[i] = '\0';}

   return strdup(buff);
}

/******************************************************************************/
  
int XrdOucUtils::genPath(char *buff, int blen, const char *path, const char *psfx)
{
    int i, j;

    i = strlen(path);
    j = (psfx ? strlen(psfx) : 0);
    if (i+j+3 > blen) return -ENAMETOOLONG;

     strcpy(buff, path);
     if (psfx)
        {if (buff[i-1] != '/') buff[i++] = '/';
         strcpy(&buff[i], psfx);
         if (psfx[j-1] != '/') strcat(buff, "/");
        }
    return 0;
}

/******************************************************************************/
/*                                G r o u p s                                 */
/******************************************************************************/
  
int XrdOucUtils::GroupName(gid_t gID, char *gName, int gNsz)
{
   static const int maxgBsz = 256*1024;
   static const int addGsz  = 4096;
   struct group  *gEnt, gStruct;
   char gBuff[1024], *gBp = gBuff;
   int glen, gBsz = sizeof(gBuff), aOK = 1;
   int retVal = 0;

// Get the the group struct. If we don't have a large enough buffer, get a
// larger one and try again up to the maximum buffer we will tolerate.
//
   while(( retVal = getgrgid_r(gID, &gStruct, gBp, gBsz, &gEnt) ) == ERANGE)
        {if (gBsz >= maxgBsz) {aOK = 0; break;}
         if (gBsz >  addGsz) free(gBp);
         gBsz += addGsz;
         if (!(gBp = (char *)malloc(gBsz))) {aOK = 0; break;}
        }

// Return a group name if all went well
//
   if (aOK && retVal == 0 && gEnt != NULL)
      {glen = strlen(gEnt->gr_name);
       if (glen >= gNsz) glen = 0;
          else strcpy(gName, gEnt->gr_name);
      } else glen = 0;

// Free any allocated buffer and return result
//
   if (gBsz >  addGsz && gBp) free(gBp);
   return glen;
}

/******************************************************************************/
/*                                 I d e n t                                  */
/******************************************************************************/

char *XrdOucUtils::Ident(long long  &mySID, char *iBuff, int iBlen,
                         const char *iHost, const char *iProg,
                         const char *iName, int Port)
{
   const char *sP;
   char sName[64], uName[256];
   long long urSID;
   int  myPid;

// Generate our server ID
//
   myPid   = static_cast<int>(getpid());
   urSID   = static_cast<long long>(myPid)<<16ll | Port;
   sprintf(sName, "%lld", urSID);
   if (!(sP = getenv("XRDSITE"))) sP = "";

// Get our username
//
   if (UserName(getuid(), uName, sizeof(uName)))
      sprintf(uName, "%d", static_cast<int>(getuid()));

// Create identification record
//
   snprintf(iBuff, iBlen, "%s.%d:%s@%s\n&pgm=%s&inst=%s&port=%d&site=%s",
                          uName, myPid, sName, iHost, iProg, iName, Port, sP);

// Return a copy of the sid
//
   h2nll(urSID, mySID);
   return strdup(sName);
}
  
/******************************************************************************/
/*                              I n s t N a m e                               */
/******************************************************************************/
  
const char *XrdOucUtils::InstName(int TranOpt)
{
   const char *iName = getenv("XRDNAME");

// If tran is zero, return what we have
//
   if (!TranOpt) return iName;

// If trans is positive then make sure iName has a value. Otherwise, make sure
// iName has no value if it's actually "anon".
//
   if (TranOpt > 0) {if (!iName || !*iName) iName = "anon";}
      else if (iName && !strcmp(iName, "anon")) iName = 0;
   return iName;
}
/******************************************************************************/
  
const char *XrdOucUtils::InstName(const char *name, int Fillit)
{ return (Fillit ? name && *name                        ? name : "anon"
                 : name && strcmp(name,"anon") && *name ? name :     0);
}
  
/******************************************************************************/
/*                                 i s 1 o f                                  */
/******************************************************************************/
  
int XrdOucUtils::is1of(char *val, const char **clist)
{
   int i = 0;
   while(clist[i]) if (!strcmp(val, clist[i])) return 1;
                      else i++;
   return 0;
}

/******************************************************************************/
/*                                 i s F W D                                  */
/******************************************************************************/
  
int XrdOucUtils::isFWD(const char *path, int *port, char *hBuff, int hBLen,
                       bool pTrim)
{
   const char *hName, *hNend, *hPort, *hPend, *hP = path;
   char *eP;
   int n;

   if (*path == '/') hP++;  // Note: It's assumed an objectid if no slash
   if (*hP   == 'x') hP++;
   if (strncmp("root:/", hP, 6)) return 0;
   if (hBuff == 0 || hBLen <= 0) return (hP - path) + 6;
   hP += 6;

   if (!XrdNetUtils::Parse(hP, &hName, &hNend, &hPort, &hPend)) return 0;
   if (*hNend == ']') hNend++;
      else {if (!(*hNend) && !(hNend = index(hName, '/'))) return 0;
            if (!(*hPend)) hPend = hNend;
           }

   if (pTrim || !(*hPort)) n = hNend - hP;
      else n = hPend - hP;
   if (n >= hBLen) return 0;
   strncpy(hBuff, hP, n);
   hBuff[n] = 0;

   if (port)
      {if (*hNend != ':') *port = 0;
          else {*port = strtol(hPort, &eP, 10);
                if (*port < 0 || *port > 65535 || eP != hPend) return 0;
               }
      }

   return hPend-path;
}
  
/******************************************************************************/
/*                                  L o g 2                                   */
/******************************************************************************/

// Based on an algorithm produced by Todd Lehman. However, this one returns 0
// when passed 0 (which is invalid). The caller must check the validity of
// the input prior to calling Log2(). Essentially, the algorithm subtracts
// away progressively smaller squares in the sequence
// { 0 <= k <= 5: 2^(2^k) } = { 2**32, 2**16, 2**8 2**4 2**2, 2**1 } =
//                          = { 4294967296, 65536, 256, 16, 4, 2 }
// and sums the exponents k of the subtracted values. It is generally the
// fastest way to compute log2 for a wide range of possible input values.

int XrdOucUtils::Log2(unsigned long long n)
{
  int i = 0;

  #define SHFT(k) if (n >= (1ULL << k)) { i += k; n >>= k; }

  SHFT(32); SHFT(16); SHFT(8); SHFT(4); SHFT(2); SHFT(1); return i;

  #undef SHFT
}
  
/******************************************************************************/
/*                                 L o g 1 0                                  */
/******************************************************************************/

int XrdOucUtils::Log10(unsigned long long n)
{
  int i = 0;

  #define SHFT(k, m) if (n >= m) { i += k; n /= m; }

  SHFT(16,10000000000000000ULL); SHFT(8,100000000ULL); 
  SHFT(4,10000ULL);              SHFT(2,100ULL);       SHFT(1,10ULL);
  return i;

  #undef SHFT
}
  
/******************************************************************************/
/*                              m a k e H o m e                               */
/******************************************************************************/
  
void XrdOucUtils::makeHome(XrdSysError &eDest, const char *inst)
{
   char buff[1024];

   if (!inst || !getcwd(buff, sizeof(buff))) return;

   strcat(buff, "/"); strcat(buff, inst);
   if (MAKEDIR(buff, pathMode) && errno != EEXIST)
      {eDest.Emsg("Config", errno, "create home directory", buff);
       return;
      }

   if (chdir(buff) < 0)
      eDest.Emsg("Config", errno, "chdir to home directory", buff);
}

/******************************************************************************/
  
bool XrdOucUtils::makeHome(XrdSysError &eDest, const char *inst,
                                               const char *path, mode_t mode)
{
   char cwDir[1024];
   const char *slash = "", *slash2 = "";
   int n, rc;

// Provide backward compatability for instance name qualification
//

   if (!path || !(n = strlen(path)))
      {if (inst) makeHome(eDest, inst);
       return true;
      }

// Augment the path with instance name, if need be
//
   if (path[n-1] != '/') slash = "/";
   if (!inst || !(n = strlen(inst))) inst = "";
      else slash2 = "/";
    n = snprintf(cwDir, sizeof(cwDir), "%s%s%s%s", path, slash, inst, slash2);
    if (n >= (int)sizeof(cwDir))
       {eDest.Emsg("Config", ENAMETOOLONG, "create home directory", cwDir);
        return false;
       }

// Create the path if it doesn't exist
//
   if ((rc = makePath(cwDir, mode)))
      {eDest.Emsg("Config", rc, "create home directory", cwDir);
       return false;
      }

// Switch to this directory
//
   if (chdir(cwDir) < 0)
      {eDest.Emsg("Config", errno, "chdir to home directory", cwDir);
       return false;
      }

// All done
//
   return true;
}

/******************************************************************************/
/*                              m a k e P a t h                               */
/******************************************************************************/
  
int XrdOucUtils::makePath(char *path, mode_t mode)
{
    char *next_path = path+1;
    struct stat buf;

// Typically, the path exists. So, do a quick check before launching into it
//
   if (!stat(path, &buf)) return 0;

// Start creating directories starting with the root
//
   while((next_path = index(next_path, int('/'))))
        {*next_path = '\0';
         if (MAKEDIR(path, mode))
            if (errno != EEXIST) return -errno;
         *next_path = '/';
         next_path = next_path+1;
        }

// All done
//
   return 0;
}
 
/******************************************************************************/
/*                             p a r s e H o m e                              */
/******************************************************************************/
  
char *XrdOucUtils::parseHome(XrdSysError &eDest, XrdOucStream &Config, int &mode)
{
   char *pval, *val, *HomePath = 0;

// Get the path
//
   pval = Config.GetWord();
   if (!pval || !pval[0])
      {eDest.Emsg("Config", "home path not specified"); return 0;}

// Make sure it's an absolute path
//
   if (*pval != '/')
      {eDest.Emsg("Config", "home path not absolute"); return 0;}

// Record the path
//
   HomePath = strdup(pval);

// Get the optional access rights
//
   mode = S_IRWXU;
   if ((val = Config.GetWord()) && val[0])
      {if (!strcmp("group", val)) mode |= (S_IRGRP | S_IXGRP);
          else {eDest.Emsg("Config", "invalid home path modifier -", val);
                free(HomePath);
                return 0;
               }
      }
   return HomePath;
}

/******************************************************************************/
/*                                R e L i n k                                 */
/******************************************************************************/

int XrdOucUtils::ReLink(const char *path, const char *target, mode_t mode)
{
   const mode_t AMode = S_IRWXU|S_IRWXG; // 770
   char pbuff[MAXPATHLEN+64];
   int n;

// Copy the path
//
   n = strlen(path);
   if (n >= (int)sizeof(pbuff)) return ENAMETOOLONG;
   strcpy(pbuff, path);

// Unlink the target, make the path, and create the symlink
//
   unlink(path);
   makePath(pbuff, (mode ? mode : AMode));
   if (symlink(target, path)) return errno;
   return 0;
}

/******************************************************************************/
/*                              s u b L o g f n                               */
/******************************************************************************/
  
char *XrdOucUtils::subLogfn(XrdSysError &eDest, const char *inst, char *logfn)
{
   const mode_t lfm = S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH;
   char buff[2048], *sp;
   int rc;

   if (!inst || !*inst) return logfn;
   if (!(sp = rindex(logfn, '/'))) strcpy(buff, "./");
      else {*sp = '\0'; strcpy(buff, logfn); strcat(buff, "/");}

   strcat(buff, inst); strcat(buff, "/");

   if ((rc = XrdOucUtils::makePath(buff, lfm)))
      {eDest.Emsg("Config", rc, "create log file path", buff);
       return 0;
      }

   if (sp) {*sp = '/'; strcat(buff, sp+1);}
      else strcat(buff, logfn);

   free(logfn);
   return strdup(buff);
}

/******************************************************************************/
/*                               t o L o w e r                                */
/******************************************************************************/

void XrdOucUtils::toLower(char *str)
{
// Change each character to lower case
//
   while(*str) {*str = tolower(*str); str++;}
}
  
/******************************************************************************/
/*                                 T o k e n                                  */
/******************************************************************************/

int XrdOucUtils::Token(const char **str, char delim, char *buff, int bsz)
{
   const char *eP, *bP = *str;
   int aLen, mLen;

// Trim off the delimeters. Return zero if nothing left.
//
   while(*bP && *bP == delim) bP++;
   if (*bP == 0) {*buff = 0; return 0;}

// Find the next delimiter
//
   eP = bP;
   while(*eP && *eP != delim) eP++;

// If we ended at a null, make sure next call will return zero
//
   if (*eP == 0) *str = eP;
      else       *str = eP+1;

// Calculate length and make sure we don't overrun the buffer
//
   aLen = eP-bP;
   if (aLen >= bsz) mLen = bsz-1;
      else          mLen = aLen;

// Copy token into buffer and end with null byte
//
   strncpy(buff, bP, mLen);
   buff[mLen] = 0;

// Return actual length
//
   return aLen;
}

/******************************************************************************/
/*                            U n d e r c o v e r                             */
/******************************************************************************/
#ifdef WIN32
void XrdOucUtils::Undercover(XrdSysError &, int, int *)
{
}
#else
void XrdOucUtils::Undercover(XrdSysError &eDest, int noLog, int *pipeFD)
{
   static const int maxFiles = 256;
   pid_t mypid;
   int myfd, logFD = eDest.baseFD();

// Issue warning if there is no logfile attached
//
   if (noLog) eDest.Emsg("Config", "Warning! No log file specified; "
                                   "backgrounding disables all logging!");

// Fork so that we are not tied to a shell
//
   if ((mypid = fork()) < 0)
      {eDest.Emsg("Config", errno, "fork process 1 for backgrounding");
       return;
      }
   else if (mypid)
   {
      // we have been given a pair of pipe descriptors to be able to read the
      // status of the child process
      if( pipeFD )
      {
         int status = 1;
         close( pipeFD[1] );
         // read will wait untill the status is communicated by the
         // child process, if the child process dies before being able
         // to comunicate the status then read will see EOF
         if( read( pipeFD[0], &status, sizeof(status) ) != sizeof(status) )
            _exit(1);
         _exit(status);
      }
      // no pipes given, return success
      else _exit(0);
   }

   if( pipeFD )
      close( pipeFD[0] );

// Become the process group leader
//
   if (setsid() < 0)
      {eDest.Emsg("Config", errno, "doing setsid() for backgrounding");
       return;
      }

// Fork to that we are cannot get a controlling terminal
//
   if ((mypid = fork()) < 0)
      {eDest.Emsg("Config", errno, "fork process 2 for backgrounding");
       return;
      }
      else if (mypid) _exit(0);

// Switch stdin, stdout, and stderr to /dev/null (we can't use /dev/console
// unless we are root which is unlikely).
//
   if ((myfd = open("/dev/null", O_RDWR)) < 0)
      {eDest.Emsg("Config", errno, "open /dev/null for backgrounding");
       return;
      }
   dup2(myfd, 0); dup2(myfd, 1); dup2(myfd, 2); dup2(myfd, logFD);

// Close any open file descriptors left open by the parent process
// but the communication pipe and the logger's shadow file descriptor.
//
  for (myfd = 3; myfd < maxFiles; myfd++)
      if( (!pipeFD || myfd != pipeFD[1]) && myfd != logFD ) close(myfd);
}

/******************************************************************************/
/*                              U s e r N a m e                               */
/******************************************************************************/
  
int XrdOucUtils::UserName(uid_t uID, char *uName, int uNsz)
{
   struct passwd *pEnt, pStruct;
   char pBuff[1024];
   int rc;

// Try to obtain the username. We use this form to make sure we are using
// the standards conforming version (compilation error otherwise).
//
   rc = getpwuid_r(uID, &pStruct, pBuff, sizeof(pBuff), &pEnt);
   if (rc)    return rc;
   if (!pEnt) return ESRCH;

// Return length of username or zero if it is too big
//
   if (uNsz <= (int)strlen(pEnt->pw_name)) return ENAMETOOLONG;
   strcpy(uName, pEnt->pw_name);
   return 0;
}

/******************************************************************************/
/*                               P i d F i l e                                */
/******************************************************************************/
  
bool XrdOucUtils::PidFile(XrdSysError &eDest, const char *path)
{
   char buff[32];
   int  fd;

   if( (fd = open( path, O_WRONLY|O_CREAT|O_TRUNC, 0644 )) < 0 )
   {
      eDest.Emsg( "Config", errno, "create pidfile" );
      return false;
   }

   if( write( fd, buff, snprintf( buff, sizeof(buff), "%d",
                                  static_cast<int>(getpid()) ) ) < 0 )
   {
      eDest.Emsg( "Config", errno, "write to pidfile" );
      close(fd);
      return false;
   }

   close(fd);
   return true;
}
#endif

