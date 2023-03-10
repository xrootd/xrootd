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

#include <cctype>
#include <grp.h>
#include <cstdio>
#include <list>
#include <vector>

#ifdef WIN32
#include <direct.h>
#include "XrdSys/XrdWin32.hh"
#else
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include <map>
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucSHA3.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlatform.hh"
#include "XrdSys/XrdSysPthread.hh"

#ifndef ENODATA
#define ENODATA ENOATTR
#endif

/******************************************************************************/
/*                         L o c a l   M e t h o d s                          */
/******************************************************************************/
  
namespace
{
struct idInfo
{      time_t  Expr;
       char   *Name;

       idInfo(const char *name, time_t keep)
             : Expr(time(0)+keep), Name(strdup(name)) {}
      ~idInfo() {free(Name);}
};

typedef std::map<unsigned int, struct idInfo*> idMap_t;

idMap_t     gidMap;
idMap_t     uidMap;
XrdSysMutex idMutex;

void AddID(idMap_t &idMap, unsigned int id, const char *name, time_t keepT)
{
   std::pair<idMap_t::iterator,bool> ret;
   idInfo *infoP = new idInfo(name, keepT);

   idMutex.Lock();
   ret = idMap.insert(std::pair<unsigned int, struct idInfo*>(id, infoP));
   if (ret.second == false) delete infoP;
   idMutex.UnLock();
}

int LookUp(idMap_t &idMap, unsigned int id, char *buff, int blen)
{
   idMap_t::iterator it;
   int luRet = 0;

   idMutex.Lock();
   it = idMap.find(id);
   if (it != idMap.end())
      {if (it->second->Expr <= time(0))
          {delete it->second;
           idMap.erase(it);
          } else {
           if (blen > 0) luRet = snprintf(buff, blen, "%s", it->second->Name);
          }
      }
   idMutex.UnLock();
   return luRet;
}
}
  
/******************************************************************************/
/*                               a r g L i s t                                */
/******************************************************************************/

int XrdOucUtils::argList(char *args, char **argV, int argC)
{
   char *aP = args;
   int j;

// Construct the argv array based on passed command line.
//
for (j = 0; j < argC; j++)
    {while(*aP == ' ') aP++;
     if (!(*aP)) break;

     if (*aP == '"' || *aP == '\'')
        {argV[j] = aP+1;
         aP = index(aP+1, *aP);
         if (!aP || (*(aP+1) != ' ' && *(aP+1)))
            {if (!j) argV[0] = 0; return -EINVAL;}
         *aP++ = '\0';
        } else {
         argV[j] = aP;
         if ((aP = index(aP+1, ' '))) *aP++ = '\0';
            else {j++; break;}
        }

    }

// Make sure we did not overflow the vector
//
   if (j > argC-1) return -E2BIG;

// End list with a null pointer and return the actual number of arguments
//
   argV[j] = 0;
   return j;
}
  
/******************************************************************************/
/*                               b i n 2 h e x                                */
/******************************************************************************/
  
char *XrdOucUtils::bin2hex(char *inbuff, int dlen, char *buff, int blen,
                           bool sep)
{
    static char hv[] = "0123456789abcdef";
    char *outbuff = buff;
    for (int i = 0; i < dlen && blen > 2; i++) {
        *outbuff++ = hv[(inbuff[i] >> 4) & 0x0f];
        *outbuff++ = hv[ inbuff[i]       & 0x0f];
        blen -= 2;
        if (sep && blen > 1 && ((i & 0x03) == 0x03 || i+1 == dlen))
           {*outbuff++ = ' '; blen--;}
        }
     *outbuff = '\0';
     return buff;
}

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
  
// eText() returns the text associated with the error.
// The text buffer pointer is returned.

char *XrdOucUtils::eText(int rc, char *eBuff, int eBlen)
{
   const char *etP;

// Get error text
//
   etP = XrdSysE2T(rc);

// Copy the text and lower case the first letter
//
   strlcpy(eBuff, etP, eBlen);

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
// Return -1 (negative truth) if an error occurred.
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
/*                               f i n d P g m                                */
/******************************************************************************/

bool XrdOucUtils::findPgm(const char *pgm, XrdOucString& path)
{
   struct stat Stat;

// Check if only executable bit needs to be checked
//
   if (*pgm == '/')
      {if (stat(pgm, &Stat) || !(Stat.st_mode & S_IXOTH)) return false;
       path = pgm;
       return true;
      }

// Make sure we have the paths to check
//
   const char *pEnv = getenv("PATH");
   if (!pEnv) return false;

// Setup to find th executable
//
   XrdOucString prog, pList(pEnv);
   int from = 0;;
   prog += '/'; prog += pgm;

// Find it!
//
   while((from = pList.tokenize(path, from, ':')) != -1)
        {path += prog;
         if (!stat(path.c_str(), &Stat) && Stat.st_mode & S_IXOTH) return true;
        }
   return false;
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
/*                               g e t F i l e                                */
/******************************************************************************/

char *XrdOucUtils::getFile(const char *path, int &rc, int maxsz, bool notempty)
{
   struct stat Stat;
   struct fdHelper
         {int fd = -1;
              fdHelper() {}
             ~fdHelper() {if (fd >= 0) close(fd);}
         } file;
   char *buff;
   int   flen;

// Preset RC
//
   rc = 0;

// Open the file in read mode
//
   if ((file.fd = open(path, O_RDONLY)) < 0) {rc = errno; return 0;}

// Get the size of the file
//
   if (fstat(file.fd, &Stat)) {rc = errno; return 0;}

// Check if the size exceeds the maximum allowed
//
   if (Stat.st_size > maxsz) {rc = EFBIG; return 0;}

// Make sure the file is not empty if empty files are disallowed
//
   if (Stat.st_size == 0 && notempty) {rc = ENODATA; return 0;}

// Allocate a buffer
//
   if ((buff = (char *)malloc(Stat.st_size+1)) == 0)
      {rc = errno; return 0;}

// Read the contents of the file into the buffer
//
   if (Stat.st_size)
      {if ((flen = read(file.fd, buff, Stat.st_size)) < 0)
          {rc = errno; free(buff); return 0;}
      } else flen = 0;

// Add null byte. recall the buffer is bigger by one byte
//
   buff[flen] = 0;

// Return the size aand the buffer
//
   rc = flen;
   return buff;
}

/******************************************************************************/
/*                                g e t G I D                                 */
/******************************************************************************/
  
bool XrdOucUtils::getGID(const char *gName, gid_t &gID)
{
   struct group Grp, *result;
   char buff[65536];

   getgrnam_r(gName, &Grp, buff, sizeof(buff), &result);
   if (!result) return false;

   gID = Grp.gr_gid;
   return true;
}

/******************************************************************************/
/*                                g e t U I D                                 */
/******************************************************************************/
  
bool XrdOucUtils::getUID(const char *uName, uid_t &uID, gid_t *gID)
{
   struct passwd pwd, *result;
   char buff[16384];

   getpwnam_r(uName, &pwd, buff, sizeof(buff), &result);
   if (!result) return false;

   uID = pwd.pw_uid;
   if (gID) *gID = pwd.pw_gid;

   return true;
}
  
/******************************************************************************/
/*                               G i d N a m e                                */
/******************************************************************************/
  
int XrdOucUtils::GidName(gid_t gID, char *gName, int gNsz, time_t keepT)
{
   static const int maxgBsz = 256*1024;
   static const int addGsz  = 4096;
   struct group  *gEnt, gStruct;
   char gBuff[1024], *gBp = gBuff;
   int glen = 0, gBsz = sizeof(gBuff), aOK = 1;
   int n, retVal = 0;

// Get ID from cache, if allowed
//
   if (keepT)
      {int n = LookUp(gidMap, static_cast<unsigned int>(gID),gName,gNsz);
       if (n > 0) return (n < gNsz ? n : 0);
      }

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
      {if (keepT)
          AddID(gidMap, static_cast<unsigned int>(gID), gEnt->gr_name, keepT);
       glen = strlen(gEnt->gr_name);
       if (glen >= gNsz) glen = 0;
          else strcpy(gName, gEnt->gr_name);
      } else {
       n = snprintf(gName, gNsz, "%ud", static_cast<unsigned int>(gID));
       if (n >= gNsz) glen = 0;
      }

// Free any allocated buffer and return result
//
   if (gBsz >  addGsz && gBp) free(gBp);
   return glen;
}

/******************************************************************************/
/*                             G r o u p N a m e                              */
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
/*                                i 2 b s t r                                 */
/******************************************************************************/

const char *XrdOucUtils::i2bstr(char *buff, int blen, int val, bool pad)
{
   char zo[2] = {'0', '1'};

   if (blen < 2) return "";

   buff[--blen] = 0;
   if (!val) buff[blen--] = '0';
      else while(val && blen >= 0)
                {buff[blen--] = zo[val & 0x01];
                 val >>= 1;
                }

   if (blen >= 0 && pad) while(blen >= 0) buff[blen--] = '0';

   return &buff[blen+1];
}
  
/******************************************************************************/
/*                                 I d e n t                                  */
/******************************************************************************/

namespace
{
long long genSID(char *&urSID, const char *iHost, int         iPort,
                               const char *iName, const char *iProg)
{
   static const XrdOucSHA3::MDLen mdLen = XrdOucSHA3::SHA3_512;
   static const uint32_t fpOffs = 2, fpSize = 6;  // 48 bit finger print

   const char *iSite = getenv("XRDSITE");
   unsigned char mDigest[mdLen];
   XrdOucString  myID;
   union {uint64_t mdLL; unsigned char mdUC[8];}; // Works for fpSize only!

// Construct our unique identification
//
   if (iSite) myID  = iSite;
   myID += iHost;
   myID += iPort;
   if (iName) myID += iName;
   myID += iProg;

// Generate a SHA3 digest of this string.
//
   memset(mDigest, 0, sizeof(mDigest));
   XrdOucSHA3::Calc(myID.c_str(), myID.length(), mDigest, mdLen);

// Generate a CRC32C of the same string
//
   uint32_t crc32c = XrdOucCRC::Calc32C(myID.c_str(), myID.length());

// We need a 48-bit fingerprint that has a very low probability of collision.
// We accomplish this by convoluting the CRC32C checksum with the SHA3 checksum.
//
   uint64_t fpPos = crc32c % (((uint32_t)mdLen) - fpSize);
   mdLL = 0;
   memcpy(mdUC+fpOffs, mDigest+fpPos, fpSize);
   long long fpVal = static_cast<long long>(ntohll(mdLL));

// Generate the character version of our fingerprint and return the binary one.
//
   char fpBuff[64];
   snprintf(fpBuff, sizeof(fpBuff), "%lld", fpVal);
   urSID = strdup(fpBuff);
   return fpVal;
}
}

char *XrdOucUtils::Ident(long long  &mySID, char *iBuff, int iBlen,
                         const char *iHost, const char *iProg,
                         const char *iName, int iPort)
{
   static char *theSIN;
   static long long theSID = genSID(theSIN, iHost, iPort, iName, iProg);
   const char *sP = getenv("XRDSITE");
   char uName[256];
   int myPid   = static_cast<int>(getpid());

// Get our username
//
   if (UserName(getuid(), uName, sizeof(uName)))
      sprintf(uName, "%d", static_cast<int>(getuid()));

// Create identification record
//
   snprintf(iBuff,iBlen,"%s.%d:%s@%s\n&site=%s&port=%d&inst=%s&pgm=%s",
            uName, myPid, theSIN, iHost, (sP ? sP : ""), iPort, iName, iProg);

// Return a copy of the sid key
//
   h2nll(theSID, mySID);
   return strdup(theSIN);
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
   char buff[2048];

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
   char cwDir[2048];
   const char *slash = "", *slash2 = "";
   int n, rc;

// Provide backward compatibility for instance name qualification
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
   if ((rc = makePath(cwDir, mode, true)))
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
  
int XrdOucUtils::makePath(char *path, mode_t mode, bool reset)
{
    char *next_path = path+1;
    struct stat buf;
    bool dochmod = false; // The 1st component stays as is

// Typically, the path exists. So, do a quick check before launching into it
//
   if (!reset && !stat(path, &buf)) return 0;

// Start creating directories starting with the root
//
   while((next_path = index(next_path, int('/'))))
        {*next_path = '\0';
         if (MAKEDIR(path, mode))
            if (errno != EEXIST) return -errno;
         if (dochmod) CHMOD(path, mode);
         dochmod = reset;
         *next_path = '/';
         next_path = next_path+1;
        }

// All done
//
   return 0;
}
 
/******************************************************************************/
/*                             m o d e 2 m a s k                              */
/******************************************************************************/

bool XrdOucUtils::mode2mask(const char *mode, mode_t &mask)
{
   mode_t mval[3] = {0}, mbit[3] = {0x04, 0x02, 0x01};
   const char *mok = "rwx";
   char mlet;

// Accept octal mode
//
   if (isdigit(*mode))
      {char *eP;
       mask = strtol(mode, &eP, 8);
       return *eP == 0;
      }

// Make sure we have the correct number of characters
//
   int n = strlen(mode);
   if (!n || n > 9 || n/3*3 != n) return false;

// Convert groups of three
//
   int k = 0;
   do {for (int i = 0; i < 3; i++)
           {mlet = *mode++;
            if (mlet != '-')
               {if (mlet != mok[i]) return false;
                mval[k] |= mbit[i]; 
               }
           } 
       } while(++k < 3 && *mode);

// Combine the modes and return success
//
   mask = mval[0]<<6 | mval[1]<<3 | mval[2];
   return true;
}
  
/******************************************************************************/
/*                              p a r s e L i b                               */
/******************************************************************************/
  
bool XrdOucUtils::parseLib(XrdSysError &eDest, XrdOucStream &Config,
                           const char *libName, char *&libPath, char **libParm)
{
    char *val, parms[2048];

// Get the next token
//
   val = Config.GetWord();

// We do not support stacking as the caller does not support stacking
//
   if (val && !strcmp("++", val))
      {eDest.Say("Config warning: stacked plugins are not supported in "
                 "this context; directive ignored!");
       return true;
      }

// Now skip over any options
//
   while(val && *val && *val == '+') val = Config.GetWord();

// Check if we actually have a path
//
   if (!val || !val[0])
      {eDest.Emsg("Config", libName, "not specified"); return false;}

// Record the path
//
   if (libPath) free(libPath);
   libPath = strdup(val);

// Handle optional parameter
//
   if (!libParm) return true;
   if (*libParm) free(*libParm);
   *libParm = 0;

// Record any parms
//
   *parms = 0;
   if (!Config.GetRest(parms, sizeof(parms)))
      {eDest.Emsg("Config", libName, "parameters too long"); return false;}
   if (*parms) *libParm = strdup(parms);
   return true;
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
   const mode_t AMode = S_IRWXU;  // Only us as a default
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
/*                              S a n i t i z e                               */
/******************************************************************************/

void XrdOucUtils::Sanitize(char *str, char subc)
{

// Sanitize string according to POSIX.1-2008 stanadard using only the
// Portable Filename Character Set: a-z A-Z 0-9 ._- with 1st char not being -
//
   if (*str)
      {if (*str == '-') *str = subc;
          else if (*str == ' ') *str = subc;
       char *blank = rindex(str, ' ');
       if (blank) while(*blank == ' ') *blank-- = 0;
       while(*str)
            {if (!isalnum(*str) && index("_-.", *str) == 0) *str = subc;
             str++;
            }
      }
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
/*                               U i d N a m e                                */
/******************************************************************************/
  
int XrdOucUtils::UidName(uid_t uID, char *uName, int uNsz, time_t keepT)
{
   struct passwd *pEnt, pStruct;
   char pBuff[1024];
   int n, rc;

// Get ID from cache, if allowed
//
   if (keepT)
      {int n = LookUp(uidMap, static_cast<unsigned int>(uID),uName,uNsz);
       if (n > 0) return (n < uNsz ? n : 0);
      }

// Try to obtain the username. We use this form to make sure we are using
// the standards conforming version (compilation error otherwise).
//
   rc = getpwuid_r(uID, &pStruct, pBuff, sizeof(pBuff), &pEnt);
   if (rc || !pEnt)
      {n = snprintf(uName, uNsz, "%ud", static_cast<unsigned int>(uID));
       return (n >= uNsz ? 0 : n);
      }

// Add entry to the cache if need be
//
   if (keepT)
      AddID(uidMap, static_cast<unsigned int>(uID), pEnt->pw_name, keepT);

// Return length of username or zero if it is too big
//
   n = strlen(pEnt->pw_name);
   if (uNsz <= (int)strlen(pEnt->pw_name)) return 0;
   strcpy(uName, pEnt->pw_name);
   return n;
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
/*                               V a l P a t h                                */
/******************************************************************************/

const char *XrdOucUtils::ValPath(const char *path, mode_t allow, bool isdir)
{
   static const mode_t mMask = S_IRWXU | S_IRWXG | S_IRWXO;
   struct stat buf;

// Check if this really exists
//
   if (stat(path, &buf))
      {if (errno == ENOENT) return "does not exist.";
       return XrdSysE2T(errno);
      }

// Verify that this is the correct type of file
//
   if (isdir)
      {if (!S_ISDIR(buf.st_mode)) return "is not a directory.";
      } else {
       if (!S_ISREG(buf.st_mode)) return "is not a file.";
      }

// Verify that the does not have excessive privileges
//
   if ((buf.st_mode & mMask) & ~allow) return "has excessive access rights.";

// All went well
//
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
/******************************************************************************/
/*                               getModificationTime                          */
/******************************************************************************/
int XrdOucUtils::getModificationTime(const char *path, time_t &modificationTime) {
    struct stat buf;
    int statRet = ::stat(path,&buf);
    if(!statRet) {
        modificationTime = buf.st_mtime;
    }
    return statRet;
}

void XrdOucUtils::trim(std::string &str) {
    // Trim leading non-letters
    while( str.size() && !isgraph(str[0]) ) str.erase(str.begin());

    // Trim trailing non-letters

    while( str.size() && !isgraph(str[str.size()-1]) )
        str.resize (str.size () - 1);
}
#endif

