//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel
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

#include "XrdFileCacheInfo.hh"
#include "XrdFileCache.hh"
#include "XrdFileCacheTrace.hh"

#include "XrdOfs/XrdOfsConfigPI.hh"
#include "XrdOss/XrdOss.hh"
#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucArgs.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysFallocate.hh"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <vector>
#include <sys/time.h>

using namespace XrdFileCache;

//______________________________________________________________________________

const int MAX_ACCESSES = 20;

const long long ONE_MB = 1024ll * 1024;
const long long ONE_GB = 1024ll * 1024 * 1024;

void Cache::ExecuteCommandUrl(const std::string& command_url)
{
   static const char *top_epfx = "ExecuteCommandUrl ";

   struct SplitParser
   {
      char       *str;
      const char *delim;
      char       *state;
      bool        first;

      SplitParser(const std::string &s, const char *d) :
         str(strdup(s.c_str())), delim(d), state(0), first(true)
      {}
      ~SplitParser() { free(str); }

      std::string get_token()
      {
         if (first) { first = false; return strtok_r(str, delim, &state); }
         else       { return strtok_r(0, delim, &state); }
      }
      std::string get_reminder()
      {
         if (first) { return str; }
         else       { *(state - 1) = delim[0]; return state - 1; }
      }
      int fill_argv(std::vector<char*> &argv)
      {
         if (!first) return 0;
         int dcnt = 0; { char *p = str; while (*p) { if (*(p++) == delim[0]) ++dcnt; } }
         argv.reserve(dcnt + 1);
         int argc = 0;
         char *i = strtok_r(str, delim, &state);
         while (i)
         {
            ++argc;
            argv.push_back(i);
            // printf("  arg %d : '%s'\n", argc, i);
            i = strtok_r(0, delim, &state);
         }
         return argc;
      }
   };

   SplitParser cp(command_url, "/");

   std::string token = cp.get_token();

   if (token != "xrdpfc_command")
   {
      TRACE(Error, top_epfx << "First token is NOT xrdpfc_command.");
      return;
   }

   // Get the command
   token = cp.get_token();


   //================================================================
   // create_file
   //================================================================

   if (token == "create_file")
   {
      static const char* err_prefix = "ExecuteCommandUrl: /xrdpfc_command/create_file: ";
      static const char* usage =
         "Usage: create_file [-s filesize] [-b blocksize] [-t access_time] [-d access_duration]\n"
         " Notes:\n"
         " . Default filesize=1G, blocksize=<as configured>, access_time=-10, access_duration=10.\n"
         " . -t and -d can be given multiple times to record several accesses.\n"
         " . Negative arguments given to -t are interpreted as relative to now.\n";

      const Configuration &conf = m_configuration;

      token = cp.get_token();

      TRACE(Debug, err_prefix << "Entered with argument string '" << token <<"'.");

      std::vector<char*> argv;
      SplitParser ap(token, " ");
      int argc = ap.fill_argv(argv);
      
      long long   file_size    = ONE_GB;
      long long   block_size   = conf.m_bufferSize;
      int         access_time    [MAX_ACCESSES];
      int         access_duration[MAX_ACCESSES];
      int         at_count = 0, ad_count = 0;
      XrdOucArgs  Spec(&m_log, err_prefix, "hvs:b:t:d:",
                       "help",         1, "h",
                       "verbose",      1, "v",
                       "size",         1, "s",
                       "blocksize",    1, "b",
                       "time",         1, "t",
                       "duration",     1, "d",
                       (const char *) 0);

      time_t time_now = time(0);

      Spec.Set(argc, &argv[0]);
      char theOpt;

      while ((theOpt = Spec.getopt()) != (char) -1)
      {
         switch (theOpt)
         {
            case 'h': {
               m_log.Say(err_prefix, " -- printing help, no action will be taken\n", usage);
               return;
            }
            case 's': {
               if (XrdOuca2x::a2sz(m_log, "Error getting filesize", Spec.getarg(),
                                   &file_size, 0ll, 32 * ONE_GB))
                  return;
               break;
            }
            case 'b': {
               if (XrdOuca2x::a2sz(m_log, "Error getting blocksize", Spec.getarg(),
                                   &block_size, 0ll, 64 * ONE_MB))
                  return;
               break;
            }
            case 't': {
               if (XrdOuca2x::a2i(m_log, "Error getting access time", Spec.getarg(),
                                  &access_time[at_count++], INT_MIN, INT_MAX))
                  return;
               break;
            }
            case 'd': {
               if (XrdOuca2x::a2i(m_log, "Error getting access duration", Spec.getarg(),
                                  &access_duration[ad_count++], 0, 24 * 3600))
                  return;
               break;
            }
            default: {
               TRACE(Error, err_prefix << "Unhandled command argument.");
               return;
            }
         }
      }
      if (Spec.getarg())
      {
         TRACE(Error, err_prefix << "Options must take up all the arguments.");
         return;
      }

      if (at_count < 1) access_time    [at_count++] = time_now - 10;
      if (ad_count < 1) access_duration[ad_count++] = 10;

      if (at_count != ad_count)
      {
         TRACE(Error, err_prefix << "Options -t and -d must be given the same number of times.");
         return;
      }

      std::string file_path (cp.get_reminder());
      std::string cinfo_path(file_path + Info::m_infoExtension);

      TRACE(Debug, err_prefix << "Command arguments parsed successfully. Proceeding to create file " << file_path);

      // Check if cinfo exists ... bail out if it does.
      {
         struct stat infoStat;
         if (GetOss()->Stat(cinfo_path.c_str(), &infoStat) == XrdOssOK)
         {
            TRACE(Error, err_prefix << "cinfo file alreay exists for '" << file_path << "'. Refusing to overwrite.");
            return;
         }
      }

      TRACE(Debug, err_prefix << "Command arguments parsed successfully, proceeding to execution.");

      {
         const char          *myUser = conf.m_username.c_str();
         XrdOucEnv            myEnv;

         // Create the data file.

         char size_str[32]; sprintf(size_str, "%lld", file_size);
         myEnv.Put("oss.asize",  size_str);
         myEnv.Put("oss.cgroup", conf.m_data_space.c_str());
         int cret;
         if ((cret = GetOss()->Create(myUser, file_path.c_str(), 0600, myEnv, XRDOSS_mkpath)) != XrdOssOK)
         {
            TRACE(Error, err_prefix << "Create failed for data file " << file_path << ", cret=" << cret << ERRNO_AND_ERRSTR);
            return;
         }

         XrdOssDF *myFile = GetOss()->newFile(myUser);
         if (myFile->Open(file_path.c_str(), O_RDWR, 0600, myEnv) != XrdOssOK)
         {
            TRACE(Error, err_prefix << "Open failed for data file " << file_path << ERRNO_AND_ERRSTR);
            delete myFile;
            return;
         }

         // Create the info file.

         myEnv.Put("oss.asize", "64k"); // TODO: Calculate? Get it from configuration? Do not know length of access lists ...
         myEnv.Put("oss.cgroup", conf.m_meta_space.c_str());
         if (GetOss()->Create(myUser, cinfo_path.c_str(), 0600, myEnv, XRDOSS_mkpath) != XrdOssOK)
         {
            TRACE(Error, err_prefix << "Create failed for info file " << cinfo_path << ERRNO_AND_ERRSTR);
            myFile->Close(); delete myFile;
            return;
         }

         XrdOssDF *myInfoFile = GetOss()->newFile(myUser);
         if (myInfoFile->Open(cinfo_path.c_str(), O_RDWR, 0600, myEnv) != XrdOssOK)
         {
            TRACE(Error, err_prefix << "Open failed for info file " << cinfo_path << ERRNO_AND_ERRSTR);
            delete myInfoFile;
            myFile->Close(); delete myFile;
            return;
         }

         // Allocate space for the data file.

         if (posix_fallocate(myFile->getFD(), 0, file_size))
         {
            TRACE(Error, err_prefix << "posix_fallocate failed for data file " << file_path << ERRNO_AND_ERRSTR);
         }

         // Fill up cinfo.

         Info myInfo(m_trace, false);
         myInfo.SetBufferSize(block_size);
         myInfo.SetFileSize(file_size);
         myInfo.SetAllBitsSynced();

         for (int i = 0; i < at_count; ++i)
         {
            time_t att_time = access_time[i] >= 0 ? access_time[i] : time_now + access_time[i];

            myInfo.WriteIOStatSingle(file_size, att_time, att_time + access_duration[i]);
         }

         myInfo.Write(myInfoFile);

         myInfoFile->Close(); delete myInfoFile;
         myFile->Close();     delete myFile;

         TRACE(Info, err_prefix << "Created file '" << file_path << "', size=" << (file_size>>20) << "MB.");

         {
            XrdSysCondVarHelper lock(&m_writeQ.condVar);

            m_writeQ.writes_between_purges += file_size;
         }
      }

      return;
   }

   //================================================================
   // unknown command
   //================================================================

   else
   {
      TRACE(Error, top_epfx << "Unknown or empty command '" << token << "'");
   }
}


//==============================================================================
// Example python script to use /xrdpfc_command/
//==============================================================================
/*
from XRootD import client
from XRootD.client.flags import OpenFlags

import sys
import time

#-------------------------------------------------------------------------------

port = int( sys.argv[1] );

g_srv = "root://localhost:%d/" % port
g_com = "/xrdpfc_command/create_file/"
g_dir = "/store/user/matevz/"

#-------------------------------------------------------------------------------

def xxsend(args, file) :

  url = g_srv + g_com + args + g_dir + file
  print "Opening ", url

  with client.File() as f:
    status, response = f.open(url, OpenFlags.READ)

    print '%r' % status
    print '%r' % response

#-------------------------------------------------------------------------------

pfx1 = "AAAA"
pfx2 = "BBBB"

for i in range(1, 1024 + 1):

  atime = -10000 + i

  xxsend("-s 4g -t %d -d 10" % atime,
         "%s-%04d" % (pfx1, i))

  time.sleep(0.01)


for i in range(1, 512 + 1):

  atime = -5000 + i

  xxsend("-s 4g -t %d -d 10" % atime,
         "%s-%04d" % (pfx2, i))

  time.sleep(0.01)
 */
