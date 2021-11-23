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
#include "XrdPfcPrint.hh"
#include "XrdPfcInfo.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucArgs.hh"
#include "XrdOuc/XrdOucJson.hh"
#include "XrdSys/XrdSysTrace.hh"
#include "XrdOfs/XrdOfsConfigPI.hh"
#include "XrdSys/XrdSysLogger.hh"
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
       delete dh;
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

   nlohmann::json jobj = {
      { "file",             path },
      { "version",          cfi.GetVersion() },
      { "created",          timeBuff },
      { "cksum",            cfi.GetCkSumStateAsText() },
      { "file_size",        cfi.GetFileSize() >> 10 },
      { "buffer_size",      cfi.GetBufferSize() >> 10 },
      { "n_blocks",         cfi.GetNBlocks() },
      { "state_complete",   cntd < cfi.GetNBlocks() ? "incomplete" : "complete" },
      { "state_percentage", 100.0 * cntd / cfi.GetNBlocks() },
      { "n_acc_total",      store.m_accessCnt }
   };

   if (cfi.HasNoCkSumTime()) {
      strftime(timeBuff,  128, "%c", localtime(&store.m_noCkSumTime));
      jobj["no-cksum-time"] = timeBuff;
   }

   // verbose needed in json?
   if (m_verbose)
   {
      nlohmann::json jarr = nlohmann::json::array();
      for (int i = 0; i < cfi.GetNBlocks(); ++i)
      {
         jarr.push_back(cfi.TestBitWritten(i) ? 1 : 0);
      }
      jobj["block_array"] = jarr;
   }

   int idx = 1;
   const std::vector<Info::AStat> &astats = cfi.RefAStats();
   nlohmann::json acc_arr = nlohmann::json::array();
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

      nlohmann::json acc = {
         { "record",       idx++ },
         { "attach",       at },
         { "detach",       dt },
         { "duration",     dur },
         { "n_ios",        it->NumIos },
         { "n_mrg",        it->NumMerged },
         { "B_hit[kB]",    it->BytesHit >> 10 },
         { "B_miss[kB]",   it->BytesMissed >> 10 },
         { "B_bypass[kB]", it->BytesBypassed >> 10 }
      };
      acc_arr.push_back(acc);
   }
   jobj["accesses"] = acc_arr;

   std::cout << jobj.dump() << "\n";

   delete fh;
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
