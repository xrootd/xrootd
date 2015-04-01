#include <iostream>
#include <fcntl.h>
#include <vector>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucArgs.hh"
#include "XrdOfs/XrdOfsConfigPI.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdFileCacheInfo.hh"
#include "XrdOss/XrdOss.hh"


namespace XrdFileCache {
class Print {
public:
   Print(XrdOss* oss, bool v, const char* path):m_oss(oss), m_verbose(v), m_ossUser("nobody"){
      // check if file ends with .cinfo
      if (isInfoFile(path)) {
         printFile(std::string(path));
      }
      else {
         XrdOssDF* dh = m_oss->newDir(m_ossUser);
         if ( dh->Opendir(path, m_env)  >= 0 ) {
            printDir(dh, path);
         }
      }

   }
   ~Print(){};

private:
   XrdOss* m_oss;
   bool    m_verbose;
   const char* m_ossUser;
XrdOucEnv m_env;

   bool isInfoFile(const char* path) {
      if (strncmp(&path[strlen(path)-6], ".cinfo", 6))
         return false;
      return true;
   }


   void printFile(const std::string& path) 
   { 
      printf("printing %s ...\n", path.c_str());
      XrdOssDF* fh = m_oss->newFile(m_ossUser);
      fh->Open((path).c_str(),O_RDONLY, 0600, m_env);
      Info cfi;
      long long off = cfi.Read(fh);

      std::vector<Info::AStat> statv;

      printf("Numaccess %d \n", cfi.GetAccessCnt());
      for (int i = 0; i <cfi.GetAccessCnt(); ++i ) {
         Info::AStat a;
         fh->Read(&a, off , sizeof(Info::AStat));
         statv.push_back(a);
      }

      int cntd = 0;
      for (int i = 0; i < cfi.GetSizeInBits(); ++i) if (cfi.TestBit(i)) cntd++;


      printf("version == %d, bufferSize %lld nBlocks %d nDownlaoded %d %s\n",cfi.GetVersion(), cfi.GetBufferSize(), cfi.GetSizeInBits() , cntd, (cfi.GetSizeInBits() == cntd) ? " complete" :"");

      if (m_verbose) {
         printf("printing %d blocks: \n", cfi.GetSizeInBits());
         for (int i = 0; i < cfi.GetSizeInBits(); ++i)
            printf("%c ", cfi.TestBit(i) ? 'x':'.');
         printf("\n");
      }

      for (int i=0; i < cfi.GetAccessCnt(); ++i)
      {
         printf("access %d >> ", i);
         Info::AStat& a = statv[i];
         char s[1000];
         struct tm * p = localtime(&a.DetachTime);
         strftime(s, 1000, "%c", p);
         printf("[%s], bytesDisk=%lld, bytesRAM=%lld, bytesMissed=%lld\n", "test", a.BytesDisk, a.BytesRam, a.BytesMissed);
      }

      delete fh;
      printf("\n");
   }

   void printDir(XrdOssDF* iOssDF, const std::string& path) 
   {
      // printf("---------> print dir %s \n", path.c_str());
      char buff[256];
      int rdr;
      while ( (rdr = iOssDF->Readdir(&buff[0], 256)) >= 0)
      {
         if (strncmp("..", &buff[0], 2) && strncmp(".", &buff[0], 1)) {

            if (strlen(buff) == 0) {
               break; // end of readdir
            }
            std::string np = path + "/" + std::string(&buff[0]);
            if (isInfoFile(buff))
            {
               printFile(np);
            }
            else {
               XrdOssDF* dh = m_oss->newDir(m_ossUser);
               if (dh->Opendir(np.c_str(), m_env) >= 0) {
                  printDir(dh, np);
               }
               delete dh; dh = 0;
            }
         }
      }
   }

};


   }



//______________________________________________________________________________


int main(int argc, char *argv[])
{ 

   static const char* usage = "Usage: pfc_print [-c config_file] [-v] path\n\n";


   bool verbose = false;
   const char* cfgn = 0;
  
   XrdOucEnv myEnv;
   int efs = open("/dev/null",O_RDWR, 0); XrdSysLogger log(efs);

   XrdSysError err(&log, "print");
   XrdOucStream Config(&err, getenv("XRDINSTANCE"), &myEnv, "=====> ");
   XrdOucArgs Spec(&err, "pfc_print: ",    "", 
                   "verbose",        1, "v",
                   "config",       1, "c",
                   (const char *)0);


   Spec.Set(argc-1, &argv[1]);
   char theOpt;

   while((theOpt = Spec.getopt()) != (char)-1)
   {
      switch(theOpt)
      {
         case 'c': {
            cfgn = Spec.getarg();
            // if (cfgn) printf("config ... %s\n", cfgn);
            break;
         }
         case 'v': {
            // printf("set verbose argval  !\n");
            verbose = true;
            break;
         }
         default: {
            // printf("invalid option %c \n", theOpt);
            printf("%s", usage);
            exit(1);
         }
      }
   }

   XrdOss *oss; 
   XrdOfsConfigPI *ofsCfg = XrdOfsConfigPI::New(cfgn,&Config,&err);
   bool ossSucc = ofsCfg->Load(XrdOfsConfigPI::theOssLib);
   if (!ossSucc) {
      printf("can't load oss\n");
      exit(1);
   }
   ofsCfg->Plugin(oss);


   const char* path = Spec.getarg();
   if (!path) {
      printf("%s", usage);
      exit(1);
   }
   std::cerr << "START !!!!\n";

   XrdFileCache::Print p(oss, verbose, path);
}


