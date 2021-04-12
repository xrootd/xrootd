/******************************************************************************/
/*                                                                            */
/*                        X r d O s s C s i . c c                             */
/*                                                                            */
/* (C) Copyright 2020 CERN.                                                   */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* In applying this licence, CERN does not waive the privileges and           */
/* immunities granted to it by virtue of its status as an Intergovernmental   */
/* Organization or submit itself to any jurisdiction.                         */
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

#include "XrdOssCsiTrace.hh"
#include "XrdOssCsi.hh"
#include "XrdOssCsiConfig.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysPageSize.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdVersion.hh"

#include <string>
#include <memory>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <assert.h>

XrdVERSIONINFO(XrdOssAddStorageSystem2,XrdOssCsi)

XrdSysError OssCsiEroute(0, "osscsi_");
XrdOucTrace OssCsiTrace(&OssCsiEroute);

XrdScheduler *XrdOssCsi::Sched_;

int XrdOssCsiDir::Opendir(const char *path, XrdOucEnv &env)
{
   if (config_.tagParam_.isTagFile(path)) return -ENOENT;

   skipsuffix_ = !config_.tagParam_.hasPrefix();
   if (!skipsuffix_)
   {
      skipprefix_ = config_.tagParam_.matchPrefixDir(path);
      if (skipprefix_)
      {
         skipprefixname_ = config_.tagParam_.getPrefixName();
      }
   }
   return successor_->Opendir(path, env);
}

// skip tag files in directory listing
int XrdOssCsiDir::Readdir(char *buff, int blen)
{
   int ret;
   do
   {
      ret = successor_->Readdir(buff, blen);
      if (ret<0) return ret;
      if (skipsuffix_)
      {
         if (config_.tagParam_.isTagFile(buff)) continue;
      }
      else if (skipprefix_)
      {
         if (skipprefixname_ == buff) continue;
      }
      break;
   } while(1);
   return ret;
}

XrdOssDF *XrdOssCsi::newDir(const char *tident)
{
   // tident starting with '*' is a special case to bypass OssCsi
   if (tident && *tident == '*')
   {
      return successor_->newDir(tident);
   }

   return (XrdOssDF *)new XrdOssCsiDir(successor_, tident, config_);
}

XrdOssDF *XrdOssCsi::newFile(const char *tident)
{
   // tident starting with '*' is a special case to bypass OssCsi
   if (tident && *tident == '*')
   {
      return successor_->newFile(tident);
   }

   return (XrdOssDF *)new XrdOssCsiFile(successor_, tident, config_);
}

int XrdOssCsi::Init(XrdSysLogger *lP, const char *cP, const char *params, XrdOucEnv *env)
{
   OssCsiEroute.logger(lP);

   int cret = config_.Init(OssCsiEroute, cP, params, env);
   if (cret != XrdOssOK)
   {
      return cret;
   }

   if ( ! env ||
        ! (Sched_ = (XrdScheduler*) env->GetPtr("XrdScheduler*")))
   {
      Sched_ = new XrdScheduler;
      Sched_->Start();
   }

   return XrdOssOK;
}

int XrdOssCsi::Unlink(const char *path, int Opts, XrdOucEnv *eP)
{
   if (config_.tagParam_.isTagFile(path)) return -ENOENT;

   // get mapinfo entries for file
   std::shared_ptr<XrdOssCsiFile::puMapItem_t> pmi;
   {
      const std::string tpath = config_.tagParam_.makeTagFilename(path);
      XrdOssCsiFile::mapTake(tpath, pmi);
   }

   int utret = 0;

   XrdSysMutexHelper lck(pmi->mtx);
   pmi->dpath = path;
   if (!pmi->unlinked)
   {
      const int uret = successor_->Unlink(path, Opts, eP);
      if (uret != XrdOssOK)
      {
         XrdOssCsiFile::mapRelease(pmi,&lck);
         return uret;
      }

      utret = successor_->Unlink(pmi->tpath.c_str(), Opts, eP);
   }

   pmi->unlinked = true;
   XrdOssCsiFile::mapRelease(pmi,&lck);

   return (utret == -ENOENT) ? 0 : utret;
}

int XrdOssCsi::Rename(const char *oldname, const char *newname,
                      XrdOucEnv  *old_env, XrdOucEnv  *new_env)
{
   if (config_.tagParam_.isTagFile(oldname) || config_.tagParam_.isTagFile(newname)) return -ENOENT;

   const std::string inew = config_.tagParam_.makeTagFilename(newname);
   const std::string iold = config_.tagParam_.makeTagFilename(oldname);

   // get mapinfo entries for both old and possibly existing newfile
   std::shared_ptr<XrdOssCsiFile::puMapItem_t> newpmi,pmi;
   XrdOssCsiFile::mapTake(inew, newpmi);
   XrdOssCsiFile::mapTake(iold   , pmi);

   // rename to self, do nothing
   if (newpmi == pmi)
   {
      XrdOssCsiFile::mapRelease(pmi);
      XrdOssCsiFile::mapRelease(newpmi);
      return 0;
   }

   // take in consistent order
   XrdSysMutexHelper lck(NULL), lck2(NULL);
   if (newpmi > pmi)
   {
     lck.Lock(&newpmi->mtx);
     lck2.Lock(&pmi->mtx);
   }
   else
   {
     lck2.Lock(&pmi->mtx);
     lck.Lock(&newpmi->mtx);
   }

   if (pmi->unlinked || newpmi->unlinked)
   {
      // something overwrote the source or target file since we checked
      XrdOssCsiFile::mapRelease(pmi,&lck2);
      XrdOssCsiFile::mapRelease(newpmi,&lck);
      return Rename(oldname, newname, old_env, new_env);
   }

   const int sret = successor_->Rename(oldname, newname, old_env, new_env);
   if (sret<0)
   {
      XrdOssCsiFile::mapRelease(pmi,&lck2);
      XrdOssCsiFile::mapRelease(newpmi,&lck);
      return sret;
   }

   int mkdret = XrdOssOK;
   {
      std::string base = inew;
      const size_t idx = base.rfind("/");
      base = base.substr(0,idx);
      if (!base.empty())
      {
         const int AMode = S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH; // 775
         mkdret = successor_->Mkdir(base.c_str(), AMode, 1, new_env);
      }
   }

   if (mkdret != XrdOssOK && mkdret != -EEXIST)
   {
      (void) successor_->Rename(newname, oldname, new_env, old_env);
      XrdOssCsiFile::mapRelease(pmi,&lck2);
      XrdOssCsiFile::mapRelease(newpmi,&lck);
      return mkdret;
   }

   const int iret = successor_->Rename(iold.c_str(), inew.c_str(), old_env, new_env);
   if (iret<0)
   {
      if (iret == -ENOENT)
      {
         // old tag did not exist, make sure there is no new tag
         (void) successor_->Unlink(inew.c_str(), 0, new_env);
      }
      else
      {
         (void) successor_->Rename(newname, oldname, new_env, old_env);
         XrdOssCsiFile::mapRelease(pmi,&lck2);
         XrdOssCsiFile::mapRelease(newpmi,&lck);
         return iret;
      }
   }

   if (newpmi)
   {
      newpmi->unlinked = true;
   }

   {
      XrdSysMutexHelper lck3(XrdOssCsiFile::pumtx_);
      auto mapidx_new = XrdOssCsiFile::pumap_.find(inew);
      if (mapidx_new != XrdOssCsiFile::pumap_.end()) XrdOssCsiFile::pumap_.erase(mapidx_new);

      auto mapidx = XrdOssCsiFile::pumap_.find(iold);
      assert(mapidx != XrdOssCsiFile::pumap_.end());

      XrdOssCsiFile::pumap_.erase(mapidx);
      XrdOssCsiFile::pumap_.insert(std::make_pair(inew, pmi));
      pmi->dpath = newname;
      pmi->tpath = inew;
   }
         
   XrdOssCsiFile::mapRelease(pmi,&lck2);
   XrdOssCsiFile::mapRelease(newpmi,&lck);

   return XrdOssOK;
}

int XrdOssCsi::Truncate(const char *path, unsigned long long size, XrdOucEnv *envP)
{
   if (config_.tagParam_.isTagFile(path)) return -ENOENT;

   std::unique_ptr<XrdOssDF> fp(newFile("xrdt"));
   XrdOucEnv   myEnv;
   int ret = fp->Open(path, O_RDWR, 0, envP ? *envP : myEnv);
   if (ret != XrdOssOK)
   {
      return ret;
   }
   ret = fp->Ftruncate(size);
   if (ret != XrdOssOK)
   {
      return ret;
   }
   long long retsz=0;
   fp->Close(&retsz);
   return XrdOssOK;
}

int XrdOssCsi::Reloc(const char *tident, const char *path,
                     const char *cgName, const char *anchor)
{
   if (config_.tagParam_.isTagFile(path)) return -ENOENT;
   return successor_->Reloc(tident, path, cgName, anchor);
}

int XrdOssCsi::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv *envP)
{
   if (config_.tagParam_.isTagFile(path)) return -EACCES;
   return successor_->Mkdir(path, mode, mkpath, envP);
}

int XrdOssCsi::Create(const char *tident, const char *path, mode_t access_mode,
                      XrdOucEnv &env, int Opts)
{
   // tident starting with '*' is a special case to bypass OssCsi
   if (tident && *tident == '*')
   {
      return successor_->Create(tident, path, access_mode, env, Opts);
   }

   if (config_.tagParam_.isTagFile(path)) return -EACCES;

   // get mapinfo entries for file
   std::shared_ptr<XrdOssCsiFile::puMapItem_t> pmi;
   {
      const std::string tpath = config_.tagParam_.makeTagFilename(path);
      XrdOssCsiFile::mapTake(tpath, pmi);
   }

   XrdSysMutexHelper lck(pmi->mtx);
   if (pmi->unlinked)
   {
      XrdOssCsiFile::mapRelease(pmi,&lck);
      return Create(tident, path, access_mode, env, Opts);
   }

   const bool isTrunc = ((Opts>>8)&O_TRUNC) ? true : false;
   const bool isExcl = ((Opts&XRDOSS_new) || ((Opts>>8)&O_EXCL)) ? true : false;

   if (isTrunc && pmi->pages)
   {
      // truncate of already open file at open() not supported
      XrdOssCsiFile::mapRelease(pmi, &lck);
      return -EDEADLK;
   }

   // create file: require it not to exist (unless we're truncating) so that
   // we can tell if we have a zero length file without stat in more cases

   const int exflags = isTrunc ? 0 : ((O_EXCL<<8)|XRDOSS_new);

   int ret = successor_->Create(tident, path, access_mode, env, Opts | exflags);
   if (ret == XrdOssOK || ret == -EEXIST)
   {
      // success from trunc/exclusive create means the file must now be zero length
      bool zlen = (ret == XrdOssOK) ? true : false;
      struct stat sbuf;
      if (!zlen && successor_->Stat(path, &sbuf, 0, &env) == XrdOssOK)
      {
         // had to check file size
         if (sbuf.st_size == 0)
         {
            zlen = true;
         }
      }
      
      // If datafile is zero length try to make empty tag file
      if (zlen)
      {
         const std::string tpath = config_.tagParam_.makeTagFilename(path);
         const int flags = O_RDWR|O_CREAT|O_TRUNC;
         const int cropts = XRDOSS_mkpath;

         std::unique_ptr<XrdOucEnv> tagEnv = tagOpenEnv(config_, env);

         ret = successor_->Create(tident, tpath.c_str(), 0666, *tagEnv, (flags<<8)|cropts);
      }
   }

   XrdOssCsiFile::mapRelease(pmi, &lck);

   // may not need to return EEXIST
   return (ret==-EEXIST && !isExcl) ? XrdOssOK : ret;
}

int XrdOssCsi::Chmod(const char *path, mode_t mode, XrdOucEnv *envP)
{
   if (config_.tagParam_.isTagFile(path)) return -ENOENT;
   return successor_->Chmod(path, mode, envP);
}

int XrdOssCsi::Remdir(const char *path, int Opts, XrdOucEnv *eP)
{
   if (config_.tagParam_.isTagFile(path)) return -ENOENT;
   const int ret = successor_->Remdir(path, Opts, eP);
   if (ret != XrdOssOK || !config_.tagParam_.hasPrefix()) return ret;

   // try to remove the corresponding directory under the tagfile directory.
   // ignore errors

   const std::string tpath = config_.tagParam_.makeBaseDirname(path);
   (void) successor_->Remdir(tpath.c_str(), Opts, eP);
   return XrdOssOK;
}

int XrdOssCsi::Stat(const char *path, struct stat *buff, int opts,
                    XrdOucEnv  *EnvP)
{
   if (config_.tagParam_.isTagFile(path)) return -ENOENT;
   return successor_->Stat(path, buff, opts, EnvP);
}

int XrdOssCsi::StatPF(const char *path, struct stat *buff, int opts)
{
   if (config_.tagParam_.isTagFile(path)) return -ENOENT;
   if (!(opts & XrdOss::PF_dStat)) return successor_->StatPF(path, buff, opts);

   buff->st_rdev = 0;
   const int pfret = successor_->StatPF(path, buff, opts);
   if (pfret != XrdOssOK)
   {
      return pfret;
   }

   std::unique_ptr<XrdOssCsiFile> fp((XrdOssCsiFile*)newFile("xrdt"));
   XrdOucEnv   myEnv;
   const int oret = fp->Open(path, O_RDONLY, 0, myEnv);
   if (oret != XrdOssOK)
   {
      return oret;
   }
   const int vs = fp->VerificationStatus();

   long long retsz=0;
   fp->Close(&retsz);

   buff->st_rdev &= ~(XrdOss::PF_csVer | XrdOss::PF_csVun);
   buff->st_rdev |= static_cast<dev_t>(vs);
   return XrdOssOK;
}

int XrdOssCsi::StatXA(const char *path, char *buff, int &blen,
                         XrdOucEnv *envP)
{
   if (config_.tagParam_.isTagFile(path)) return -ENOENT;
   return successor_->StatXA(path, buff, blen, envP);
}


XrdOss *XrdOssAddStorageSystem2(XrdOss       *curr_oss,
                                XrdSysLogger *Logger,
                                const char   *config_fn,
                                const char   *parms,
                                XrdOucEnv    *envP)
{
   XrdOssCsi *myOss = new XrdOssCsi(curr_oss);
   if (myOss->Init(Logger, config_fn, parms, envP) != XrdOssOK)
   {
      delete myOss;
      return NULL;
   }
   return (XrdOss*)myOss;
}

std::unique_ptr<XrdOucEnv> XrdOssCsi::tagOpenEnv(const XrdOssCsiConfig &config, XrdOucEnv &env)
{
   // for tagfile open, start with copy of datafile environment
   int infolen;
   const char *info = env.Env(infolen);
   std::unique_ptr<XrdOucEnv> newEnv(new XrdOucEnv(info, infolen, env.secEnv()));

   // give space name for tag files
   newEnv->Put("oss.cgroup", config.xrdtSpaceName().c_str());

   char *tmp;
   long long cgSize=0;
   if ((tmp = env.Get("oss.asize")) && XrdOuca2x::a2sz(OssCsiEroute,"invalid asize",tmp,&cgSize,0))
   {
      cgSize=0;
   }

   if (cgSize>0)
   {
      char size_str[32];
      sprintf(size_str, "%lld", 20+4*((cgSize+XrdSys::PageSize-1)/XrdSys::PageSize));
      newEnv->Put("oss.asize",  size_str);
   }
   else
   {
      newEnv->Put("oss.asize",  "0");
   }

   return newEnv;
}
