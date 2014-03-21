/******************************************************************************/
/*                                                                            */
/*                       X r d D i g C o n f i g . c c                        */
/*                                                                            */
/* (C) 2013 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*               DE-AC02-76-SFO0515 with the Deprtment of Energy              */
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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdNet/XrdNetAddr.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"

#include "XrdDig/XrdDigAuth.hh"
#include "XrdDig/XrdDigConfig.hh"

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define TS_Xeq(x,m)    if (!strcmp(x,var)) return m(cFile);

/******************************************************************************/
/*                 S t a t i c   G l o b a l   O b j e c t s                  */
/******************************************************************************/
  
namespace XrdDig
{
   extern XrdSysError *eDest;

   extern XrdDigAuth   Auth;

          XrdDigConfig Config;
};

using namespace XrdDig;

namespace
{
   struct pTV {const char  *pfx;
               XrdDigAuthEnt::aType aType;
               const char   pfxlen;
                     char   isOK;
              } pTab[] = {{"conf", XrdDigAuthEnt::aConf, 4, 0},
                          {"core", XrdDigAuthEnt::aCore, 4, 0},
                          {"logs", XrdDigAuthEnt::aLogs, 4, 0},
                          {"proc", XrdDigAuthEnt::aProc, 4, 0}
                         };
   static const int pNum = 4;

   struct stat rootStat;
};

/******************************************************************************/
/*                             C o n f i g u r e                              */
/******************************************************************************/
  
bool XrdDigConfig::Configure(const char *cFN, const char *parms)
{
/*
  Function: Establish default values using configuration parameters.

  Input:    None.

  Output:   true upon success or false otherwise.
*/
   char buff[4096], *afile, *var;
   XrdOucTokenizer cParms(buff);
   struct stat Stat;
   int  n;
   bool isOK = true;

// Get the adminpath (this better succeed).
//
   if (!(var = getenv("XRDADMINPATH")) || (n = strlen(var)) >= MAXPATHLEN)
      {eDest->Emsg("Config", "Unable to deterine adminpath!");
       return false;
      }

// Create a template for file remapping
//
   strcpy(buff, var);
   if (buff[n-1] != '/') {buff[n] = '/'; n++;}
   strcpy(buff+n, ".xrd/=/%s");
   fnTmplt = strdup(buff);

// Make sure that conf/etc no longer exists as a previous start may have
// exported something that we no longer wish to export.
//
   if (snprintf(buff, sizeof(buff), fnTmplt, "conf/etc") < (int)sizeof(buff))
      Empty(buff);

// Pake sure there are parameters here
//
   if(!parms || !*parms)
     {eDest->Emsg("Config", "DigFS parameters not specified.");
      return false;
     }

// Copy the parms as they will be altered and attach it to the tokenizer
//
   n = strlen(parms);
   if (n >= (int)sizeof(buff))
     {eDest->Emsg("Config", "DigFS parm string is too long.");
      return false;
     }
   strcpy(buff, parms);

// First token is the authfile
//
   cParms.GetLine();
   if (!(afile = cParms.GetToken()) || !afile[0])
      {eDest->Emsg("Config", "DigFS authfile not specified.");
       return false;
      }

// If we have a config file, process it now
//
   if (cFN && *cFN) isOK = ConfigProc(cFN);

// Config authorization. The config may have failed but we want to generate
// all of the rror messages in one go.
//
   if (!Auth.Configure(afile)) isOK = false;

// Setup locate response
//
   SetLocResp();

// Get a valid stat structure for the root directory
//
   stat("/", &rootStat);

// Validate base entries
//
   for (n = 0; n < pNum; n++)
       {sprintf(buff, fnTmplt, pTab[n].pfx);
        pTab[n].isOK = stat(buff, &Stat) == 0;
       }

// All done
//
   return isOK;
}

/******************************************************************************/
/*                             G e n A c c e s s                              */
/******************************************************************************/
  
int XrdDigConfig::GenAccess(const XrdSecEntity *client,
                            const char         *aList[],
                            int                 aMax
                           )
{
   bool aOK[XrdDigAuthEnt::aNum], hasAcc = false;
   int  i, n = 0;

// Validate aMax
//
   if (aMax < 1) return -1;

// Get access right for this client
//
   Auth.Authorize(client, XrdDigAuthEnt::aNum, aOK);

// Return entries that are allowed
//
   for (i = (int)sizeof(aOK)-1; i >= 0 && n < aMax; i--)
       {hasAcc |= aOK[i];
        if (aOK[i] && pTab[i].isOK) aList[n++] = pTab[i].pfx;
       }

// Return permission denied if no access allowed
//
   if (!hasAcc) return -1;

// Return something if we had an error setting up as empty dirs cause problems.
//
   if (!n) {aList[0] = "."; n = 1;}
   return n;
}

/******************************************************************************/
/*                               G e n P a t h                                */
/******************************************************************************/
  
char *XrdDigConfig::GenPath(int &rc, const XrdSecEntity *client,
                                     const char         *opname,
                                     const char         *fname,
                                     XrdDigConfig::pType lfnType)

{
   char path[2048];
   int i, n;

// First we better have a client object
//
   if (!client) {rc = EPERM; return 0;}

// Translate the fname to the right file type
//
   for (i = 0; i < pNum; i++)
       {if (!strncmp(pTab[i].pfx, fname, pTab[i].pfxlen)
        &&  (*(fname+pTab[i].pfxlen) == '/' || !*(fname+pTab[i].pfxlen))) break;
       }

// Make sure we found a valid entry
//
   if (i >= pNum || !pTab[i].isOK) {rc = ENOENT; return 0;}

// Authorize this access
//
   if (!Auth.Authorize(client, pTab[i].aType))
      {if (lfnType == isFile && logRej) Audit(client, "denied", opname, fname);
       rc = EACCES;
       return 0;
      }

// If the entry is being suffixed and it's proc, make sure we are not trying
// to gain access to something outside of the proc directory tree
//
   if (pTab[i].aType == XrdDigAuthEnt::aProc && (rc = ValProc(fname)))
      {if (logRej && rc == EPERM) Audit(client, "denied", opname, fname);
       return 0;
      }

// Log this access if so wanted
//
   if (lfnType == isFile && logAcc) Audit(client, "allowed", opname, fname);

// Construct the name to be returned
//
   i = (lfnType == isDir ? 1 : 0);
   n = snprintf(path, sizeof(path), fnTmplt, fname);
   if (n >= (int)sizeof(path)-1) {rc = ENAMETOOLONG; return 0;}

// Attach a trailing slash if there is none if this is a directory
//
   if (lfnType == isDir && path[n-1] != '/') {path[n] = '/'; path[n+1] = 0;}

// Return the composite name
//
   rc = 0;
   return strdup(path);
}

/******************************************************************************/
/*                            G e t L o c R e s p                             */
/******************************************************************************/
  
void XrdDigConfig::GetLocResp(XrdOucErrInfo &eInfo, bool nameok)
{

// Return desired value
//
        if (nameok)
           eInfo.setErrInfo(locRlenHP, locRespHP);
   else if (eInfo.getUCap() & XrdOucEI::uIPv4)
           eInfo.setErrInfo(locRlenV4, locRespV4);
   else    eInfo.setErrInfo(locRlenV6, locRespV6);
}

/******************************************************************************/
/*                              S t a t R o o t                               */
/******************************************************************************/
  
void XrdDigConfig::StatRoot(struct stat *sP)
{
   memcpy(sP, &rootStat, sizeof(struct stat));
}

/******************************************************************************/
/*                     p r i v a t e   f u n c t i o n s                      */
/******************************************************************************/
/******************************************************************************/
/* Private:                      A d d P a t h                                */
/******************************************************************************/
  
const char *XrdDigConfig::AddPath(XrdDigConfig::pType sType, const char *src,
                          const char *tpd, const char *tfn)
{
   struct stat Stat;
   char *pP, tBuff[MAXPATHLEN], pBuff[MAXPATHLEN], nBuff[MAXNAMELEN];
   int fd, rc;

// Make sure the source path is absolute and is readable and is of proper type
//
   if (*src != '/' || *(src+1) == 0)   return "not absolute path";
   if ((fd = open(src, O_RDONLY)) < 0) return strerror(errno);
   rc = (fstat(fd, &Stat) ? errno : 0); close(fd);
   if (rc) return strerror(rc);
   switch(sType)
         {case isFile: if (!S_ISREG(Stat.st_mode)) return "not a file";
                       break;
          case isDir:  if (!S_ISDIR(Stat.st_mode)) return "not a directory";
                       break;
          default:     break;
         }

// If no target name specified it becomes the last components of src
//
   if (!tfn)
      {const char *tbeg = (strncmp(src, "/etc/", 5) ? src+1 : src+5);
       tfn = src;
       while(strlen(tbeg) >= sizeof(nBuff)/2 && (tfn = index(tbeg,'/')))
            tbeg = tfn + 1;
       if (!tfn) tfn = rindex(src, '/')+1;
          else {strcpy(nBuff, tbeg); tfn = pP = nBuff;
                while((pP = index(pP, '/'))) *pP++ = '.';
               }
      }
   if (!(*tfn)) return "invalid derived target name";

// Construct the target path
//
   if (snprintf(tBuff, sizeof(tBuff), "%s%s", tpd, tfn) > (int)sizeof(tBuff))
      return "target name too long";
   if (snprintf(pBuff, sizeof(pBuff), fnTmplt, tBuff)   > (int)sizeof(pBuff))
      return "target path too long";

// Create the link and return
//
   if ((rc = XrdOucUtils::ReLink(pBuff, src))) return strerror(rc);
   return 0;
}

/******************************************************************************/
/* Private:                        A u d i t                                  */
/******************************************************************************/
  
void XrdDigConfig::Audit(const XrdSecEntity *client, const char *what,
                         const char         *opn,    const char *trg)
{
    const char *name = (client->name ? client->name : "anon");
    char hbuff[512], buff[1024];

// Get the hostname
//
   client->addrInfo->Format(hbuff, sizeof(hbuff), XrdNetAddrInfo::fmtName,
                                                  XrdNetAddrInfo::noPort);

// Format the message and print it
//
   snprintf(buff, sizeof(buff), "%s@%s %s", name, hbuff, what);
   eDest->Emsg(opn, client->tident, buff, trg);
}

/******************************************************************************/
/*                            C o n f i g P r o c                             */
/******************************************************************************/
  
bool XrdDigConfig::ConfigProc(const char *ConfigFN)
{
  char *var;
  int  cfgFD, retc, NoGo = 0;
  XrdOucEnv    myEnv;
  XrdOucStream cFile(eDest, getenv("XRDINSTANCE"), &myEnv, "=====> ");

// Try to open the configuration file.
//
   if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      {eDest->Emsg("Config", errno, "open config file", ConfigFN);
       return 1;
      }
   cFile.Attach(cfgFD);

// Now start reading records until eof.
//
   while((var = cFile.GetMyFirstWord()))
        {if (!strncmp(var, "dig.", 4))
            if (!ConfigXeq(var+4, cFile)) {cFile.Echo(); NoGo = 1;}
        }

// Now check if any errors occured during file i/o
//
   if ((retc = cFile.LastError()))
      NoGo = eDest->Emsg("Config", retc, "read config file", ConfigFN);
   cFile.Close();

// Return final return code
//
   return !NoGo;
}

/******************************************************************************/
/* Private:                    C o n f i g X e q                              */
/******************************************************************************/

bool XrdDigConfig::ConfigXeq(char *var, XrdOucStream &cFile)
{

// Process items. for either a local or a remote configuration
//
   TS_Xeq("addconf",       xacf);
   TS_Xeq("log",           xlog);
   return true;
}

/******************************************************************************/
/* Private:                        E m p t y                                  */
/******************************************************************************/
  
void XrdDigConfig::Empty(const char *path)
{
   DIR *dh;
   struct dirent *dP;
   char pBuff[MAXPATHLEN+8], *pB;
   int n, bLeft;

// Copy the path. We will need it for deletions. This should never fail.
//
   if ((n = snprintf(pBuff,sizeof(pBuff),"%s/",path)) >= (int)sizeof(pBuff))
      return;
   bLeft = sizeof(pBuff) - n - 1;
   pB = pBuff + n;

// Open the directory
//
   if (!(dh = opendir(path))) return;

// Delete each entry in this directory (no need to be thread safe here)
//
   while((dP = readdir(dh)))
        {if (bLeft > (int)strlen(dP->d_name))
            {strcpy(pB, dP->d_name);
             unlink(pBuff);
            }
        }

// Now remove the actual directory
//
   rmdir(path);
}

/******************************************************************************/
/* Private:                   S e t L o c R e s p                             */
/******************************************************************************/
  
void XrdDigConfig::SetLocResp()
{
   static const int fmtopts = XrdNetAddr::old6Map4;
   XrdNetAddr myAddr(0);
   char *pP, buff[512], *bp = buff+2;
   int myPort, bsz = sizeof(buff)-2;

// Obtain port number we will be using. Note that the constructor must occur
// after the port number is known (i.e., this cannot be a global static).
//
   myPort = (pP = getenv("XRDPORT")) ? strtol(pP, (char **)NULL, 10) : 0;
   strcpy(buff, "Sr");

// Establish hostname locate response
//
   myAddr.Port(myPort);
   myAddr.Format(bp, bsz, XrdNetAddr::fmtName);
   locRespHP = strdup(buff); locRlenHP = strlen(buff)+1;

// Extablish IPV6 locate response
//
   myAddr.Format(bp, bsz, XrdNetAddr::fmtAdv6, fmtopts);
   locRespV6 = locRespV4 = strdup(buff); locRlenV6 = locRlenV4 = strlen(buff)+1;

// If we are truly IPv6 then see if we also have an IPv4 address
//
   if (myAddr.isIPType(XrdNetAddrInfo::IPv6) && !myAddr.isMapped())
      {XrdNetAddr *iP;
       int iN;
       if (!XrdNetUtils::GetAddrs(myAddr.Name(""), &iP, iN,
                                  XrdNetUtils::onlyIPv4, 0) && iN)
          {iP[0].Port(myPort);
           iP[0].Format(bp, bsz, XrdNetAddr::fmtAdv6, fmtopts);
           locRespV4 = strdup(buff); locRlenV4 = strlen(buff)+1;
           delete [] iP;
          }
       }
}

/******************************************************************************/
/*                               V a l P r o c                                */
/******************************************************************************/
  
int XrdDigConfig::ValProc(const char *path)
{
   struct stat Stat;
   char *Slash, cpath[1040], ppath[1040];
   int n;

// Copy the path so we can modify it and make sure it ends with a slash
//
   n = snprintf(ppath, sizeof(ppath), "%s", path);
   if (n >= (int)sizeof(ppath)-2) return ENAMETOOLONG;
   if (ppath[n-1] != '/') {ppath[n] = '/'; ppath[n+1] = 0;}

// We accept proc/x/y where y and any other path suffix is not a symlink
//
   if (!(Slash = index(ppath,   '/')) || !(Slash = index(Slash+1, '/'))
   ||  !(Slash = index(Slash+1, '/'))) return 0;

// Now check each component
//
   while(Slash)
        {*Slash = 0;
         n = snprintf(cpath, sizeof(cpath), fnTmplt, ppath);
         if (n >= (int)sizeof(cpath)) return ENAMETOOLONG;
         if (lstat(cpath, &Stat)) return errno;
         if (!S_ISDIR(Stat.st_mode) && !S_ISREG(Stat.st_mode)) return EPERM;
         *Slash = '/';
         Slash = index(Slash+1, '/');
        }

// It's OK to use proc
//
   return 0;
}

/******************************************************************************/
/* Private:                         x a c f                                   */
/******************************************************************************/
  
/* Function: xacf


   Purpose:  Parse the directive: addconf <path> [<fname>]

             <path>  path to a configuration file
             <fname> the file name it is to have in "/=/conf/etc/"

   Output: true upon success or false upon failure.
*/

bool XrdDigConfig::xacf(XrdOucStream &cFile)
{
   const char *eTxt;
   char *val, src[MAXPATHLEN+8];

// Check out first token
//
   if (!(val = cFile.GetWord()) || !val[0])
      {eDest->Emsg("Config", "addconf path not specified."); return false;}

// Copy the path
//
   if (strlen(val) >= sizeof(src))
      {eDest->Emsg("Config", "addconf path is too long."); return false;}
   strcpy(src, val);

// Check if we have a special filename here
//
   if (!(val = cFile.GetWord()) || !val[0]) val = 0;
      else {if (index(val, '/'))
               {eDest->Emsg("Config", "invalid addconf fname -", val);
                return false;
               }
           }

// Now add the file path
//
   if ((eTxt = AddPath(isFile, src, "conf/etc/", val)))
      {char eBuff[256];
       snprintf(eBuff, sizeof(eBuff), "- %s", eTxt);
       eDest->Emsg("Config", "Unable to addconf" , src, eBuff);
       return false;
      }

// All done
//
   return true;
}

/******************************************************************************/
/* Private:                         x l o g                                   */
/******************************************************************************/
  
/* Function: xlog


   Purpose:  Parse the directive: log [grant] [deny] | none

             grant  log   successful access to information
             deny   log unsuccessful access to information
             none   do not log anything

   Output: true upon success or false upon failure.
*/

bool XrdDigConfig::xlog(XrdOucStream &cFile)
{
   char *val;

// Check out first token
//
   if (!(val = cFile.GetWord()) || !val[0])
      {eDest->Emsg("Config", "log parameter not specified"); return false;}

// Check for appropriate words
//
   logAcc = logRej = false;
   do {     if (!strcmp("grant", val)) logAcc = true;
       else if (!strcmp("deny",  val)) logRej = true;
       else if (!strcmp("none",  val)) logRej = logAcc = false;
       else {eDest->Emsg("Config","invalid log option -",val); return false;}
       val = cFile.GetWord();
      } while(val && *val);

// All done
//
   return true;
}
