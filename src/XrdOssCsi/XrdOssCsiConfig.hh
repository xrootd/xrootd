#ifndef _XRDOSSCSICONFIG_H
#define _XRDOSSCSICONFIG_H
/******************************************************************************/
/*                                                                            */
/*                   X r d O s s C s i C o n f i g . h h                      */
/*                                                                            */
/* (C) Copyright 2021 CERN.                                                   */
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

#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSys/XrdSysLogger.hh"

#include <string>

class TagPath
{
public:

   TagPath() : prefix_("/.xrdt"), suffix_(".xrdt") { calcPrefixElements(); }
   ~TagPath() { }

   // path may be full path (relative or absolute) or if we're using empty prefix_
   // the path can also be a single name entry, found during a directory listing
   // will also match the prefix_ directory
   bool isTagFile(const char *path)
   {
      if (!path || !*path) return false;
      std::string s(path);
      simplePath(s);
      // if prefix_ set, the test is to match if "path" is equal to or starts with prefix_
      // we do not require matching suffix_, this will also match the prefix_ directory
      if (!prefix_.empty())
      {
         if (s.find(prefix_) == 0)
         {
            if (prefix_.length() == s.length()) return true;
            if (s[prefix_.length()] == '/') return true;
         }
         return false;
      }
      // prefix_ not set, test is if "path" ends with suffix_
      const size_t haystack = s.length();
      const size_t needle = suffix_.length();
      if (haystack >= needle && s.substr(haystack-needle, std::string::npos) == suffix_) return true;
      return false;
   }

   int SetPrefix(XrdSysError &Eroute, const std::string &v)
   { 
      if (!v.empty() && v[0] != '/')
      {
         Eroute.Emsg("Config","prefix must be empty or start with /");
         return 1;
      }
      prefix_ = v;
      calcPrefixElements();
      return XrdOssOK;
   }

   bool hasPrefix() { return !prefix_.empty(); }

   //
   // to convert a data directory to corresponding tag directory
   std::string makeBaseDirname(const char *path)
   {
      if (!path || !*path || prefix_.empty()) return std::string();

      std::string p(path);
      bool wasabs = false;
      simplePath(p,&wasabs);
      std::string ret = (wasabs) ? prefix_ : prefix_.substr(1);
      if (p.length()>1) ret += p;
      return ret;
   }

   //
   // test if path is the directory containing the
   // base directory of the tags: only expected to
   // be called if prefix_ is not empty.
   bool matchPrefixDir(const char *path)
   {
      if (!path || !*path || prefix_.empty()) return false;

      std::string p(path);
      simplePath(p);
      if (prefixstart_ == p) return true;
      return false;
   }

   // the name (not path) of directory containing the tags
   // only called if prefix_ not empty and matchPrefixDir is true
   std::string getPrefixName()
   {
      return prefixend_;
   }

   // take datafile name at path and convert it to filename of tagfile
   std::string makeTagFilename(const char *path)
   {
      if (!path || !*path) return std::string();
      std::string p(path);
      bool wasabs = false;
      simplePath(p,&wasabs);
      if (wasabs) return prefix_ + p + suffix_;
      return prefix_.substr(1) + p + suffix_;
   }

   // user specified prefix; empty or otherwise always has a leading '/'
   // is the location of a directory under which all tag files are stored
   std::string prefix_;

private:
   void calcPrefixElements()
   {
      prefixstart_.clear();
      prefixend_.clear();
      if (prefix_.empty()) return;
      simplePath(prefix_);
      const size_t idx = prefix_.rfind("/");
      prefixstart_ = prefix_.substr(0,idx);
      if (prefixstart_.empty()) prefixstart_ = std::string("/");
      prefixend_ = prefix_.substr(idx+1,std::string::npos);
   }

   // remove double slashes, trailing slashes and ensure a leading
   // slash.
   //
   // The input/output 'str' parameter is the path to be modified.
   // The output 'ls' parameter, if non-null, is set to indicate if
   // the input path had a leading slash.
   void simplePath(std::string &str, bool *ls=nullptr)
   {
      // replace double slashes with single
      size_t i=0;
      do {
         i = str.find("//", i);
         if (i == std::string::npos) break;
         str.erase(i, 1);      
      } while (!str.empty());

      // remove trailing /
      if (str.length()>1 && str[str.length()-1] == '/')
      {
         str.erase( str.end()-1 );
      }

      if (ls) *ls=(str.length() ? true : false);

      if (str.length() && str[0] != '/')
      {
         str = "/" + str;
         if (ls) *ls = false;
      }
   }

   std::string prefixstart_;
   std::string prefixend_;
   const std::string suffix_;
};

class XrdOssCsiConfig
{
public:

  XrdOssCsiConfig() : fillFileHole_(true), xrdtSpaceName_("public"), allowMissingTags_(true), disablePgExtend_(false), disableLooseWrite_(false) { }
  ~XrdOssCsiConfig() { }

  int Init(XrdSysError &, const char *, const char *, XrdOucEnv *);

  bool fillFileHole() const { return fillFileHole_; }

  std::string xrdtSpaceName() const { return xrdtSpaceName_; }

  bool allowMissingTags() const { return allowMissingTags_; }

  bool disablePgExtend() const { return disablePgExtend_; }

  bool disableLooseWrite() const { return disableLooseWrite_; }

  TagPath tagParam_;

private:
  int readConfig(XrdSysError &, const char *);

  int ConfigXeq(char *, XrdOucStream &, XrdSysError &);

  int xtrace(XrdOucStream &, XrdSysError &);

  bool fillFileHole_;
  std::string xrdtSpaceName_;
  bool allowMissingTags_;
  bool disablePgExtend_;
  bool disableLooseWrite_;
};

#endif
