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

#include <iostream>
#include <fcntl.h>
#include <vector>
#include "json.h"
#include "XrdPfcPrint.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucArgs.hh"
#include "XrdSys/XrdSysTrace.hh"
#include "XrdOfs/XrdOfsConfigPI.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdPfcInfo.hh"
#include "XrdOss/XrdOss.hh"

using namespace XrdPfc;

Print::Print(XrdOss* oss, bool v, bool j, const char* path) : m_oss(oss), m_verbose(v), m_json(j), m_ossUser("nobody")
{
   if (isInfoFile(path))
   {
       if (m_json) printFileJson(std::string(path));
       else        printFile(std::string(path));
   }
   else
   {
       XrdOssDF* dh = m_oss->newDir(m_ossUser);
       if ( dh->Opendir(path, m_env)  >= 0 )
       {
          printDir(dh, path, m_json);
       }
   }
}

bool Print::isInfoFile(const char* path)
{
   if (strncmp(&path[strlen(path)-6], ".cinfo", 6))
   {
      printf("%s is not cinfo file.\n\n", path);
      return false;
   }
   return true;
}


void Print::printFileJson(const std::string& path)
{
   printf("FILE: %s\n", path.c_str());
   struct json_object *jobj;
   jobj = json_object_new_object();

   // Filename
   json_object_object_add(jobj, "file", json_object_new_string(path.c_str()));

   XrdOssDF* fh = m_oss->newFile(m_ossUser);
   fh->Open((path).c_str(),O_RDONLY, 0600, m_env);

   XrdSysTrace tr("XrdPfcPrint"); tr.What = 2;
   Info cfi(&tr);

   if ( ! cfi.Read(fh, path.c_str()))
   {
      json_object_put(jobj); // Delete the json object
      return;
   }

   int cntd = 0;
   for (int i = 0; i < cfi.GetNBlocks(); ++i)
   {
      if (cfi.TestBitWritten(i)) cntd++;
   }
   const Info::Store& store = cfi.RefStoredData();
   char  timeBuff[128];
   strftime(timeBuff, 128, "%c", localtime(&store.m_creationTime));

   json_object_object_add(jobj, "version", json_object_new_int(cfi.GetVersion()));
   json_object_object_add(jobj, "created", json_object_new_string(timeBuff));
   json_object_object_add(jobj, "cksum", json_object_new_string(cfi.GetCkSumStateAsText()));

   if (cfi.HasNoCkSumTime()) {
      strftime(timeBuff,  128, "%c", localtime(&store.m_noCkSumTime));
      json_object_object_add(jobj, "no-cksum-time", json_object_new_string(timeBuff));
   }

   //json_object_object_add(jobj, "version", json_object_new_string( ));
   json_object_object_add(jobj, "file_size", json_object_new_int(cfi.GetFileSize() >> 10));
   json_object_object_add(jobj, "buffer_size", json_object_new_int(cfi.GetBufferSize() >> 10));
   json_object_object_add(jobj, "n_blocks", json_object_new_int(cfi.GetNBlocks()));
   json_object_object_add(jobj, "state_complete", json_object_new_string((cntd < cfi.GetNBlocks()) ? "incomplete" : "complete"));
   json_object_object_add(jobj, "state_percentage", json_object_new_double(100.0 * cntd / cfi.GetNBlocks()));

   // verbose needed in json?
   if (m_verbose)
   {
      struct json_object *block_array;
      block_array = json_object_new_array();
      for (int i = 0; i < cfi.GetNBlocks(); ++i)
      {
         json_object_array_add(block_array, json_object_new_int(cfi.TestBitWritten(i) ? 1 : 0));
      }
      json_object_object_add(jobj, "block_array", json_object_get(block_array));
      json_object_put(block_array); // Delete the json object
   }

   json_object_object_add(jobj, "n_acc_total", json_object_new_int(store.m_accessCnt));

   int idx = 1;
   const std::vector<Info::AStat> &astats = cfi.RefAStats();
   struct json_object *access_array;
   access_array = json_object_new_array();
   for (std::vector<Info::AStat>::const_iterator it = astats.begin(); it != astats.end(); ++it)
   {
      const int MM = 128;
      char s[MM];
      struct json_object *obj1;
      obj1 = json_object_new_object();

      strftime(s, MM, "%y%m%d:%H%M%S", localtime(&(it->AttachTime)));
      std::string at = s;
      strftime(s, MM, "%y%m%d:%H%M%S", localtime(&(it->DetachTime)));
      std::string dt = it->DetachTime > 0 ? s : "------:------";
      {
         int hours   = it->Duration/3600;
         int min     = (it->Duration - hours * 3600)/60;
         int sec     = it->Duration % 60;
         snprintf(s, MM, "%d:%02d:%02d", hours, min, sec);
      }
      std::string dur = s;
      json_object_object_add(obj1, "record", json_object_new_int(idx++));
      json_object_object_add(obj1, "attach", json_object_new_string(at.c_str()));
      json_object_object_add(obj1, "detach", json_object_new_string(dt.c_str()));
      json_object_object_add(obj1, "duration", json_object_new_string(dur.c_str()));
      json_object_object_add(obj1, "n_ios", json_object_new_int(it->NumIos));
      json_object_object_add(obj1, "n_mrg", json_object_new_int(it->NumMerged));
      json_object_object_add(obj1, "B_hit[kB]", json_object_new_int(it->BytesHit >> 10));
      json_object_object_add(obj1, "B_miss[kB]", json_object_new_int(it->BytesMissed >> 10));
      json_object_object_add(obj1, "B_bypass[kB]", json_object_new_int(it->BytesBypassed >> 10));
      json_object_array_add(access_array, json_object_get(obj1));
      json_object_put(obj1); // Delete the json object
   }
   json_object_object_add(jobj, "accesses", access_array);

   printf(json_object_to_json_string(jobj));
   printf("\n");
   delete fh;
   json_object_put(access_array); // Delete the json object
   json_object_put(jobj); // Delete the json object
}


void Print::printFile(const std::string& path)
{
   printf("FILE: %s\n", path.c_str());
   XrdOssDF* fh = m_oss->newFile(m_ossUser);
   fh->Open((path).c_str(),O_RDONLY, 0600, m_env);

   XrdSysTrace tr("XrdPfcPrint"); tr.What = 2;
   Info cfi(&tr);

   if ( ! cfi.Read(fh, path.c_str()))
   {
      return;
   }

   int cntd = 0;
   for (int i = 0; i < cfi.GetNBlocks(); ++i)
   {
      if (cfi.TestBitWritten(i)) cntd++;
   }
   const Info::Store& store = cfi.RefStoredData();
   char  timeBuff[128];
   strftime(timeBuff, 128, "%c", localtime(&store.m_creationTime));

   printf("version %d, created %s; cksum %s",  cfi.GetVersion(), timeBuff, cfi.GetCkSumStateAsText());
   if (cfi.HasNoCkSumTime()) {
      strftime(timeBuff,  128, "%c", localtime(&store.m_noCkSumTime));
      printf(", no-cksum-time %s\n", timeBuff);
   } else printf("\n");

   printf("file_size %lld kB, buffer_size %lld kB, n_blocks %d, n_downloaded %d, state %scomplete [%.3f%%]\n",
          cfi.GetFileSize() >> 10, cfi.GetBufferSize() >> 10, cfi.GetNBlocks(), cntd,
          (cntd < cfi.GetNBlocks()) ? "in" : "", 100.0 * cntd / cfi.GetNBlocks());

   if (m_verbose)
   {
      int8_t n_db = 0;
      { int x = cfi.GetNBlocks(); while (x)
        {
           x /= 10; ++n_db;
        }
      }
      static const char *nums = "0123456789";
      printf("printing %d blocks:\n", cfi.GetNBlocks());
      printf("%*s  %10d%10d%10d%10d%10d%10d\n", n_db, "", 1, 2, 3, 4, 5, 6);
      printf("%*s %s%s%s%s%s%s0123", n_db, "", nums, nums, nums, nums, nums, nums);
      for (int i = 0; i < cfi.GetNBlocks(); ++i)
      {
         if (i % 64 == 0)
            printf("\n%*d ", n_db, i);
         printf("%c", cfi.TestBitWritten(i) ? 'x' : '.');
      }
      printf("\n");
   }

   printf("Access records (N_acc_total=%llu):\n"
          "%-6s %-13s  %-13s  %-12s %-5s %-5s %12s %12s %12s\n",
          (unsigned long long) store.m_accessCnt,
          "Record", "Attach", "Detach", "Duration", "N_ios", "N_mrg", "B_hit[kB]", "B_miss[kB]", "B_bypass[kB]");

   int idx = 1;
   const std::vector<Info::AStat> &astats = cfi.RefAStats();
   for (std::vector<Info::AStat>::const_iterator it = astats.begin(); it != astats.end(); ++it)
   {
      const int MM = 128;
      char s[MM];

      strftime(s, MM, "%y%m%d:%H%M%S", localtime(&(it->AttachTime)));
      std::string at = s;
      strftime(s, MM, "%y%m%d:%H%M%S", localtime(&(it->DetachTime)));
      std::string dt = it->DetachTime > 0 ? s : "------:------";
      {
         int hours   = it->Duration/3600;
         int min     = (it->Duration - hours * 3600)/60;
         int sec     = it->Duration % 60;
         snprintf(s, MM, "%d:%02d:%02d", hours, min, sec);
      }
      std::string dur = s;

      printf("%-6d %-13s  %-13s  %-12s %5d %5d %12lld %12lld %12lld\n", idx++,
             at.c_str(), dt.c_str(), dur.c_str(), it->NumIos, it->NumMerged,
             it->BytesHit >> 10, it->BytesMissed >> 10, it->BytesBypassed >> 10);
   }

   delete fh;
}

void Print::printDir(XrdOssDF* iOssDF, const std::string& path, bool m_json)
{
   // printf("---------> print dir %s \n", path.c_str());

   const int MM = 1024;
   char  buff[MM];
   int   rdr;
   bool  first = true;

   while ((rdr = iOssDF->Readdir(&buff[0], MM)) >= 0)
   {
      if (strncmp("..", &buff[0], 2) && strncmp(".", &buff[0], 1))
      {
         if (strlen(buff) == 0)
         {
            break; // end of readdir
         }
         std::string np = path + "/" + std::string(&buff[0]);
         if (isInfoFile(buff))
         {
           if (first) first = false;
           else       printf("\n");
           if (m_json) printFileJson(np);
           else        printFile(np);
         }
         else
         {
            XrdOssDF* dh = m_oss->newDir(m_ossUser);
            if (dh->Opendir(np.c_str(), m_env) >= 0)
            {
               printDir(dh, np, m_json);
            }
            delete dh; dh = 0;
         }
      }
   }
}


//------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
   static const char* usage = "Usage: pfc_print [-c config_file] [-v] [-j] path\n\n";
   bool verbose = false;
   bool json = false;
   const char* cfgn = 0;

   XrdOucEnv myEnv;

   XrdSysLogger log;
   XrdSysError err(&log);

   XrdOucStream Config(&err, getenv("XRDINSTANCE"), &myEnv, "=====> ");
   XrdOucArgs   Spec(&err, "xrdpfc_print: ", "",
                     "verbose",      1, "v",
                     "config",       1, "c",
                     "json",         1, "j",
                     (const char *) 0);

   Spec.Set(argc-1, &argv[1]);
   char theOpt;

   while ((theOpt = Spec.getopt()) != (char)-1)
   {
      switch (theOpt)
      {
      case 'c':
      {
         cfgn = Spec.getarg();
         int fd = open(cfgn, O_RDONLY, 0);
         Config.Attach(fd);
         break;
      }
      case 'v':
      {
         verbose = true;
         break;
      }
      case 'j':
      {
         json = true;
         break;
      }
      default:
      {
         printf("%s", usage);
         exit(1);
      }
      }
   }

   // suppress oss init messages
   int efs = open("/dev/null",O_RDWR, 0);
   XrdSysLogger ossLog(efs);
   XrdSysError ossErr(&ossLog, "print");
   XrdOss *oss;
   XrdOfsConfigPI *ofsCfg = XrdOfsConfigPI::New(cfgn,&Config,&ossErr);
   bool ossSucc = ofsCfg->Load(XrdOfsConfigPI::theOssLib);
   if ( ! ossSucc)
   {
      printf("can't load oss\n");
      exit(1);
   }
   ofsCfg->Plugin(oss);

   const char* path;
   while ((path = Spec.getarg()))
   {
      if ( ! path)
      {
         printf("%s", usage);
         exit(1);
      }

      // append oss.localroot if path starts with 'root://'
      if ( ! strncmp(&path[0], "root:/", 6))
      {
         if (Config.FDNum() < 0)
         {
            printf("Configuration file not specified.\n");
            exit(1);
         }
         char *var;
         while((var = Config.GetFirstWord()))
         {
            // printf("var %s \n", var);
            if ( ! strncmp(var,"oss.localroot", strlen("oss.localroot")))
            {
               std::string tmp = Config.GetWord();
               tmp += &path[6];
               // printf("Absolute path %s \n", tmp.c_str());
               XrdPfc::Print p(oss, verbose, json, tmp.c_str());
            }
         }
      }
      else
      {
         XrdPfc::Print p(oss, verbose, json, path);
      }
   }

   return 0;
}
