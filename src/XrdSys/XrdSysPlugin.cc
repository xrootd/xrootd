/******************************************************************************/
/*                                                                            */
/*                       X r d S y s P l u g i n . c c                        */
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

// Bypass Solaris ELF madness
//
#ifdef __solaris__
#include <sys/isa_defs.h>
#if defined(_ILP32) && (_FILE_OFFSET_BITS != 32)
#undef  _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#undef  _LARGEFILE_SOURCE
#endif
#endif

#ifndef WIN32
#include <dlfcn.h>
#if !defined(__APPLE__) && !defined(__CYGWIN__)
#include <link.h>
#endif
#include <stdio.h>
#include <strings.h>
#include <sys/types.h>
#include <errno.h>
#else
#include "XrdSys/XrdWin32.hh"
#endif
  
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdVersion.hh"
#include "XrdVersionPlugin.hh"
 
/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

struct XrdSysPlugin::PLlist *XrdSysPlugin::plList = 0;

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdSysPlugin::~XrdSysPlugin()
{
   if (libHandle) dlclose(libHandle);
   if (libPath)   free(libPath);
}

/******************************************************************************/
/* Private:                   b a d V e r s i o n                             */
/******************************************************************************/
  
XrdSysPlugin::cvResult XrdSysPlugin::badVersion(XrdVersionInfo &urInfo,
                                                char mmv, int majv, int minv)
{
   const char *path;
   char buff1[512], buff2[128];

   if (minv > 99) minv = 99;
   snprintf(buff1, sizeof(buff1), "version %s is incompatible with %s "
                                  "(must be %c= %d.%d.x)",
                                   myInfo->vStr, urInfo.vStr, mmv, majv, minv);

   path = msgSuffix(" in ", buff2, sizeof(buff2));

   Inform(buff1, buff2, path, 0, 0, 1);

   return cvBad;
}
  
/******************************************************************************/
/* Private:                   c h k V e r s i o n                             */
/******************************************************************************/
  
XrdSysPlugin::cvResult XrdSysPlugin::chkVersion(XrdVersionInfo &urInfo,
                                                const char     *pname,
                                                void           *lHandle)
{
   static XrdVersionPlugin vInfo[] = {XrdVERSIONPLUGINRULES};
   static XrdVersionPlugin vNote[] = {XrdVERSIONPLUGINMAXIMS};
   XrdVersionPlugin *vinP;
   char buff[1024], vName[256];
   void *vP;
   int  i, n=0, pMajor, vMajor, pMinor, vMinor;

// If no version information supplied, skip version check
//
   if (!myInfo) return cvNone;

// Check if we need to check the version here
//
   i = 0;
   while(vInfo[i].pName && strcmp(vInfo[i].pName, pname)) i++;

// If we didn't find it in the rules table then try to match the maxims
//
   if (!vInfo[i].pName)
      {i = 0; n = strlen(pname);
       while(vNote[i].pName)
            {if ((vNote[i].vPfxLen + vNote[i].vSfxLen <= n)
             &&  !strncmp(vNote[i].pName, pname, vNote[i].vPfxLen)
             &&  !strncmp(vNote[i].pName+vNote[i].vPfxLen,
                   pname + n - vNote[i].vSfxLen, vNote[i].vSfxLen)) break;
             i++;
            }
             vinP = &vNote[i];
      } else vinP = &vInfo[i];

   if (!(vinP->pName)) return cvNone;
   if ( vinP->vProcess == XrdVERSIONPLUGIN_DoNotChk) return cvDirty;

// Construct the version entry point
//
   if (!n) n = strlen(pname);
   if (n+sizeof(XrdVERSIONINFOSFX) > sizeof(vName))
      return libMsg("Unable to generate version name for", "%s in ", pname);
   strcpy(vName, pname); strcpy(vName+n, XrdVERSIONINFOSFX);

// Find the version number
//
   if (!(vP = dlsym(lHandle, vName)))
      {if (vinP->vProcess != XrdVERSIONPLUGIN_Required) return cvMissing;
       return libMsg(dlerror()," required version information for %s in ",pname);
      }

// Extract the version number from the plugin and do a quick check. We use
// memcpy to avoid instances where the symbol is wrongly defined. Make sure
// the version string ends with a null by copying one less byte than need be.
// The caller provided a struct that is gauranteed to end with nulls.
//
   memcpy(&urInfo, vP, sizeof(XrdVersionInfo)-1);

// If version numbers are identical then we are done
//
   if (myInfo->vNum == urInfo.vNum)
      if (myInfo->vNum != XrdVNUMUNK
      ||  !strcmp(myInfo->vStr + (myInfo->vOpt & 0x0f)+1,
                  urInfo. vStr + (urInfo. vOpt & 0x0f)+1)) return cvClean;

// If the caller or plugin is unreleased, just issue a warning.
//
   if (myInfo->vNum == XrdVNUMUNK || urInfo.vNum == XrdVNUMUNK)
      {if (eDest)
          {char mBuff[128];
           sprintf(buff, "%s%s is using %s%s version",
                   (myInfo->vNum == XrdVNUMUNK ? "unreleased ":""),myInfo->vStr,
                   (urInfo.vNum  == XrdVNUMUNK ? "unreleased ":""),urInfo.vStr);
           msgSuffix(" in ", mBuff, sizeof(mBuff));
           Inform(buff, mBuff, libPath);
          }
       return cvDirty;
      }

// Extract version numbers
//
   vMajor = XrdMajorVNUM(myInfo->vNum);
   vMinor = XrdMinorVNUM(myInfo->vNum);
   pMajor = XrdMajorVNUM(urInfo. vNum);
   pMinor = XrdMinorVNUM(urInfo. vNum);

// The major version must always be compatible
//
   if ((vinP->vMajLow >= 0 && pMajor <  vinP->vMajLow)
   ||  (vinP->vMajLow <  0 && pMajor != vMajor))
      return badVersion(urInfo, '>', vinP->vMajLow, vinP->vMinLow);

// The major version may not be greater than our versin
//
   if (pMajor > vMajor) return badVersion(urInfo, '<', vMajor, vMinor);

// If we do not need to check minor versions then we are done
//
   if (vinP->vMinLow > 99) return cvClean;

// In no case can the plug-in mnor version be greater than our version
//
   if (pMajor == vMajor && pMinor > vMinor)
      return badVersion(urInfo, '<', vMajor, vMinor);

// Verify compatible minor versions
//
   if ((vinP->vMinLow >= 0 && pMinor >= vinP->vMinLow)
   ||  (vinP->vMinLow <  0 && pMinor == vMinor)) return cvClean;

// Incompatible versions
//
   return badVersion(urInfo, '>', vinP->vMajLow, vinP->vMinLow);
}

/******************************************************************************/
/* Private:                      D L F l a g s                                */
/******************************************************************************/
  
int XrdSysPlugin::DLflags()
{
#if    defined(__APPLE__)
       return RTLD_FIRST;
#elif  defined(__linux__)
       return RTLD_NOW;
#else
       return RTLD_NOW;
#endif
}

/******************************************************************************/
/* Private:                         F i n d                                   */
/******************************************************************************/
  
void *XrdSysPlugin::Find(const char *libpath)
{
   struct PLlist *plP = plList;

// Find the library in the preload list
//
   while(plP && strcmp(libpath, plP->libPath)) plP = plP->next;

// Return result
//
  return (plP ? plP->libHandle : 0);
}

/******************************************************************************/
/*                             g e t P l u g i n                              */
/******************************************************************************/

void *XrdSysPlugin::getPlugin(const char *pname, int optional)
{
   return getPlugin(pname, optional, false);
}

void *XrdSysPlugin::getPlugin(const char *pname, int optional, bool global)
{
   XrdVERSIONINFODEF(urInfo, unknown, XrdVNUMUNK, "");
   void *ep, *myHandle;
   cvResult cvRC;
   int flags;

// If no path is given then we want to just search the executable. This is easy
// for some platforms and more difficult for others. So, we do the best we can.
//
   if (libPath) flags = DLflags();
      else {    flags = RTLD_NOW;
#ifndef WIN32
                flags|= global ? RTLD_GLOBAL : RTLD_LOCAL;
#else
            if (global && eDest) eDest->Emsg("getPlugin",
               "request for global symbols unsupported under Windows - ignored");
#endif
      }

// Check if we should use the preload list
//
   if (!(myHandle = libHandle) && plList) myHandle = Find(libPath);

// Open whatever it is we need to open
//
   if (!myHandle)
      {if ((myHandle = dlopen(libPath, flags))) libHandle = myHandle;
          else {libMsg(dlerror(), " loading "); return 0;}
      }

// Get the symbol. In the environment we have defined, null values are not
// allowed and we will issue an error.
//
   if (!(ep = dlsym(myHandle, pname)))
      {if (!optional) libMsg(dlerror(), " plugin %s in ", pname);
       return 0;
      }

// Check if we need to verify version compatability
//
   if ((cvRC = chkVersion(urInfo, pname, myHandle)) == cvBad) return 0;

// Print the loaded version unless message is suppressed or not needed
//
   if (libPath && optional < 2 && msgCnt
   &&  (cvRC == cvClean || cvRC == cvMissing))
      {char buff[128];
       msgSuffix(" from ", buff, sizeof(buff));
       msgCnt--;
            if (cvRC == cvClean)
               {const char *wTxt=(urInfo.vNum == XrdVNUMUNK ? "unreleased ":0);
                Inform("loaded ", wTxt, urInfo.vStr, buff, libPath);
               }
       else if (cvRC == cvMissing)
               {Inform("loaded unversioned ", pname, buff, libPath);}
      }

// All done
//
   return ep;
}

/******************************************************************************/
/* Private:                       I n f o r m                                 */
/******************************************************************************/

void XrdSysPlugin::Inform(const char *txt1, const char *txt2, const char *txt3,
                          const char *txt4, const char *txt5, int noHush)
{
   const char *eTxt[] = {"Plugin ",txt1, txt2, txt3, txt4, txt5, 0};
   char *bP;
   int n, i, bL;

// Check if we should hush this messages (largely for client-side usage)
//
   if (!noHush && getenv("XRDPIHUSH")) return;

// If we have a messaging object, use that
//
   if (eDest)
      {char buff[2048];
       i = 1; bP = buff; bL = sizeof(buff);
       while(bL > 1 && eTxt[i])
            {n = snprintf(bP, bL, "%s", eTxt[i]);
             bP += n; bL -= n; i++;
            }
       eDest->Say("Plugin ", buff);
       return;
      }

// If we have a buffer, set message in the buffer
//
   if ((bP = eBuff))
      {i = 0; bL = eBLen;
       while(bL > 1 && eTxt[i])
            {n = snprintf(bP, bL, "%s", eTxt[i]);
             bP += n; bL -= n; i++;
            }
      }
}
  
/******************************************************************************/
/* Private:                       l i b M s g                                 */
/******************************************************************************/
  
XrdSysPlugin::cvResult XrdSysPlugin::libMsg(const char *txt1, const char *txt2,
                                            const char *mSym)
{
   static const char fndg[] = "Finding";
   static const int  flen   = sizeof("Finding");
   const char *path;
   char mBuff[512], nBuff[512];

// Check if this is a lookup or open issue. Trim message for the common case.
//
        if (mSym)
           {if (!txt1 || strstr(txt1, "undefined"))
               {txt1 = "Unable to find ";
                snprintf(nBuff, sizeof(nBuff), txt2, mSym);
               } else {
                strcpy(nBuff, fndg);
                snprintf(nBuff+flen-1,sizeof(nBuff)-flen,txt2,mSym);
               }
            txt2 = nBuff;
           }
   else if (!txt1) txt1 = "Unknown system error!";
   else if (strstr(txt1, "No such file")) txt1 = "No such file or directory";
   else txt2 = " ";

// Spit out the message
//
   path = msgSuffix(txt2, mBuff, sizeof(mBuff));
   Inform(txt1, mBuff, path, 0, 0, 1);
   return cvBad;
}

/******************************************************************************/
/* Private:                    m s g S u f f i x                              */
/******************************************************************************/

const char *XrdSysPlugin::msgSuffix(const char *Word, char *buff, int bsz)
{
   if (libPath) snprintf(buff, bsz,"%s%s ", Word, libName);
      else      snprintf(buff, bsz,"%sexecutable image", Word);
   return (libPath ? libPath : "");
}
  
/******************************************************************************/
/*                               P r e l o a d                                */
/******************************************************************************/
  
bool XrdSysPlugin::Preload(const char *path,  char *ebuff, int eblen)
{
   struct PLlist *plP;
   void *myHandle;

// First see if this is already in the preload list
//
   if (Find(path)) return true;

// Try to open the library
//
   if (!(myHandle = dlopen(path, DLflags())))
      {if (ebuff && eblen > 0)
          {const char *dlMsg = dlerror();
           snprintf(ebuff, eblen, "Plugin unable to load %s; %s", path,
                                  (dlMsg ? dlMsg : "unknown system error"));
          }
       return false;
      }

// Add the library handle
//
   plP = new PLlist;
   plP->libHandle = myHandle;
   plP->libPath   = strdup(path);
   plP->next      = plList;
   plList         = plP;

// All done
//
   return true;
}

/******************************************************************************/
/*                                V e r C m p                                 */
/******************************************************************************/
  
bool XrdSysPlugin::VerCmp(XrdVersionInfo &vInfo1,
                          XrdVersionInfo &vInfo2, bool noMsg)
{
   const char *mTxt;
   char v1buff[128], v2buff[128];
   int unRel;

// Do a quick return if the version need not be checked or are equal
//
   if (vInfo1.vNum <= 0 || vInfo1.vNum == vInfo2.vNum) return true;

// As it works out, many times two modules wind up in different shared
// libraries. For consistency we require that both major.minor version be the
// same unless either is unreleased (i.e. test). Issue warning if need be.
//
   mTxt = (vInfo1.vNum == XrdVNUMUNK ? "unreleased " : "");
   sprintf(v1buff, " %sversion %s", mTxt, vInfo1.vStr);
   unRel  = *mTxt;

   mTxt = (vInfo2.vNum == XrdVNUMUNK ? "unreleased " : "");
   sprintf(v2buff, " %sversion %s", mTxt, vInfo2.vStr);
   unRel |= *mTxt;

   if (unRel || vInfo1.vNum/100 == vInfo2.vNum/100) mTxt = "";
      else mTxt = " which is incompatible!";

   if (!noMsg)
      cerr <<"Plugin: " <<v1buff <<" is using " <<v2buff <<mTxt <<endl;

   return (*mTxt == 0);
}
