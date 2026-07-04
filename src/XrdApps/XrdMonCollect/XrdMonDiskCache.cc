/******************************************************************************/
/*                                                                            */
/*                   X r d M o n D i s k C a c h e . c c                      */
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
/******************************************************************************/

#include "XrdApps/XrdMonCollect/XrdMonDiskCache.hh"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
bool endsWith(const std::string& s, const std::string& suffix)
{
   return s.size() >= suffix.size() &&
          s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}
}

bool XrdMonDiskCache::Init(std::string& err)
{
   if (mkdir(dir.c_str(), 0750) != 0 && errno != EEXIST)
      {err = "cannot create cache dir '" + dir + "': " + std::strerror(errno);
       return false;}

   DIR* d = opendir(dir.c_str());
   if (!d)
      {err = "cannot open cache dir '" + dir + "': " + std::strerror(errno);
       return false;}

// Collect completed cache files (oldest first by name); remove stale partials.
//
   std::vector<std::string> names;
   for (struct dirent* e; (e = readdir(d)); )
      {std::string n = e->d_name;
       if (endsWith(n, ".tmp"))  {unlink(path(n).c_str()); continue;}
       if (endsWith(n, suffix))  names.push_back(n);
      }
   closedir(d);
   std::sort(names.begin(), names.end());

   std::uint64_t b = 0;
   for (auto& n : names)
      {struct stat st;
       if (stat(path(n).c_str(), &st) == 0)
          {pending.push_back(n); b += (std::uint64_t)st.st_size;}
      }
   files = pending.size();
   bytes = b;
   return true;
}

// Evict the oldest pending body to make room under the byte cap.
//
void XrdMonDiskCache::dropOldest()
{
   const std::string p = path(pending.front());
   struct stat st;
   std::uint64_t sz = (stat(p.c_str(), &st) == 0) ? (std::uint64_t)st.st_size : 0;
   unlink(p.c_str());
   pending.pop_front();
   files = pending.size();
   if (bytes >= sz) bytes -= sz;
   ++dropped;
}

bool XrdMonDiskCache::Store(const std::string& body, std::string& err)
{
   while (maxBytes && !pending.empty() && bytes + body.size() > maxBytes)
      dropOldest();

   using namespace std::chrono;
   std::uint64_t ms = (std::uint64_t)duration_cast<milliseconds>(
                         system_clock::now().time_since_epoch()).count();
   char name[64];
   std::snprintf(name, sizeof(name), "%013llu-%06llu%s",
                 (unsigned long long)ms, (unsigned long long)seq++,
                 suffix.c_str());

   std::string tmp = path(name) + ".tmp";
   {std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) {err = "cannot create '" + tmp + "'"; return false;}
    f.write(body.data(), (std::streamsize)body.size());
    f.flush();
    if (!f) {err = "cannot write '" + tmp + "'";
             f.close(); unlink(tmp.c_str()); return false;}
   }
   if (rename(tmp.c_str(), path(name).c_str()) != 0)
      {err = std::string("cannot rename cache file: ") + std::strerror(errno);
       unlink(tmp.c_str()); return false;}

   pending.push_back(name);
   files = pending.size();
   bytes += body.size();
   ++stored;
   return true;
}

int XrdMonDiskCache::ReplayOldest(
        const std::function<bool(const std::string&)>& cb, std::string& err)
{
   if (pending.empty()) return 0;
   const std::string name = pending.front();
   const std::string p    = path(name);

   std::ifstream f(p, std::ios::binary);
   if (!f)
      {// File vanished or is unreadable: drop it from the backlog.
       err = "cannot read cache file '" + p + "'; dropping";
       struct stat st;
       std::uint64_t sz = (stat(p.c_str(), &st) == 0) ? (std::uint64_t)st.st_size : 0;
       unlink(p.c_str());
       pending.pop_front();
       files = pending.size();
       if (bytes >= sz) bytes -= sz;
       return 1;
      }

   std::stringstream ss;
   ss << f.rdbuf();
   std::string body = ss.str();
   f.close();

   if (!cb(body)) return -1;             // sink still down: keep the file

   unlink(p.c_str());
   pending.pop_front();
   files = pending.size();
   std::uint64_t sz = body.size();
   if (bytes >= sz) bytes -= sz;
   ++replayed;
   return 1;
}
