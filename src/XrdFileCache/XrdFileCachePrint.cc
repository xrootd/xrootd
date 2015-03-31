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

static const char* usage = "Usage: pfc_print [-c config_file] [-v] path\n\n";

using namespace XrdFileCache;

void printFile(XrdOssDF* fh, bool verbose) 
{
   Info cfi;
   long long off = cfi.Read(fh);

   std::vector<Info::AStat> statv;
   statv.resize(cfi.GetAccessCnt());
   for (int i = 0; i <cfi.GetAccessCnt(); ++i )
   {
      off += fh->Read(&statv[1], off, sizeof(Info::AStat));
   }

   int cntd = 0;
   for (int i = 0; i < cfi.GetSizeInBits(); ++i) if (cfi.TestBit(i)) cntd++;


   printf("version == %d, bufferSize %lld nBlocks %d nDownlaoded %d %s\n",cfi.GetVersion(), cfi.GetBufferSize(), cfi.GetSizeInBits() , cntd, (cfi.GetSizeInBits() == cntd) ? " complete" :"");

   if (verbose) {
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
      printf("[%s], bytesDisk=%lld, bytesRAM=%lld, bytesMissed=%lld\n", s, a.BytesDisk, a.BytesRam, a.BytesMissed);
   }
}

int main(int argc, char *argv[])
{
    bool verbose = false;
    const char* infoFP = "/data1/store/user/alja/data.root.cinfo";
    const char* cfgn = 0;
  
    XrdOucEnv myEnv;
    int efs = open("/dev/null",O_RDWR, 0);
    XrdSysLogger log(efs);
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

    infoFP = Spec.getarg();
    if (!infoFP) {
       printf("%s", usage);
       exit(1);
    }

    XrdOss *oss; 
    XrdOfsConfigPI *ofsCfg = XrdOfsConfigPI::New(cfgn,&Config,&err);
    bool ossSucc = ofsCfg->Load(XrdOfsConfigPI::theOssLib);
    if (!ossSucc) {
       printf("can't load oss\n");
       exit(1);
    }
    ofsCfg->Plugin(oss);

    XrdOssDF* fh = oss->newFile("user");
    fh->Open(infoFP, O_RDWR, 0644, myEnv);
    printFile(fh, verbose);
    fh->Close();
}


