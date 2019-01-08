//----------------------------------------------------------------------------------
// Copyright (c) 2015 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman
//----------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------------------

#include "XrdFileCacheDecision.hh"
#include "XrdSys/XrdSysError.hh"

#include <vector>
#include <fcntl.h>
#include <stdio.h>
#include <fnmatch.h>

class BlacklistDecision : public XrdFileCache::Decision
{
//----------------------------------------------------------------------------
//! A decision library that allows all files to be cached except for a blacklist
//----------------------------------------------------------------------------

public:
virtual bool Decide(const std::string & lfn, XrdOss &) const
{
   //m_log.Emsg("BlacklistDecide", "Deciding whether to cache file", url.c_str());
   for (std::vector<std::string>::const_iterator it = m_blacklist.begin(); it != m_blacklist.end(); it++)
   {
      if (! fnmatch(it->c_str(), lfn.c_str(), FNM_PATHNAME))
      {
         //m_log.Emsg("BlacklistDecide", "Not caching file as it matches blacklist entry", it->c_str());
         return false;
      }
   }
   //m_log.Emsg("BlacklistDecide", "Caching file", fname);
   return true;
}

BlacklistDecision(XrdSysError &log)
   : m_log(log)
{}

virtual bool ConfigDecision(const char * parms)
{
   if (! parms || ! parms[0] || (strlen(parms) == 0))
   {
      m_log.Emsg("ConfigDecision", "Blacklist file not specified.");
      return false;
   }
   m_log.Emsg("ConfigDecision", "Using blacklist", parms);
   FILE * fp = fopen(parms, "r");
   if (fp == 0)
   {
      m_log.Emsg("ConfigDecision", errno, "Failed to open blacklist:", parms);
      return false;
   }

   char line[4096];
   while(fgets(line, sizeof(line), fp))
   {
      char *trimmed = line;
      while (trimmed[0] && isspace(trimmed[0])) {trimmed++; }
      if (trimmed[0] == 0) {continue; }
      size_t filelen = strlen(trimmed);
      if (trimmed[filelen-1] == '\n') {trimmed[filelen-1] = '\0'; }
      m_blacklist.push_back(trimmed);
   }
   if (! feof(fp))
   {
      m_log.Emsg("ConfigDecision", errno, "Failed to parse blacklist");
   }
   fclose(fp);
   for (std::vector<std::string>::const_iterator it = m_blacklist.begin(); it!=m_blacklist.end(); it++)
   {
      m_log.Emsg("ConfigDecision", "Cache is blacklisting paths matching", it->c_str());
   }
   return true;
}

private:
std::vector<std::string> m_blacklist;
XrdSysError &m_log;
};

/******************************************************************************/
/*                          XrdFileCacheGetDecision                           */
/******************************************************************************/

// Return a decision object to use.
extern "C"
{
XrdFileCache::Decision *XrdFileCacheGetDecision(XrdSysError &err)
{
   return new BlacklistDecision(err);
}
}

