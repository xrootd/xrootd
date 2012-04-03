/******************************************************************************/
/*                                                                            */
/*                        X r d C p C o n f i g . c c                         */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/
  
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "XrdVersion.hh"
#include "XrdApps/XrdCpConfig.hh"
#include "XrdApps/XrdCpFile.hh"
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCks/XrdCksManager.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysHeaders.hh"
#include "XrdSys/XrdSysLogger.hh"

using namespace std;

/******************************************************************************/
/*                         D e f i n e   M a c r o s                          */
/******************************************************************************/
  
#define EMSG(x) cerr <<PName <<": " <<x <<endl
  
#define FMSG(x,y) {EMSG(x);exit(y);}

#define UMSG(x) {EMSG(x);Usage(22);}

#define ZMSG(x) {EMSG(x);return 0;}

// Bypass stupid issue with stupid solaris for missdefining 'struct opt'.
//
#ifdef __solaris__
#define OPT_TYPE (char *)
#else
#define OPT_TYPE
#endif

/******************************************************************************/
/*                        S t a t i c   M e m b e r s                         */
/******************************************************************************/

namespace XrdCpConfiguration
{
static XrdSysLogger Logger;
static XrdSysError  eDest(&Logger, "");
};

XrdSysError  *XrdCpConfig::Log = &XrdCpConfiguration::eDest;
  
const char   *XrdCpConfig::opLetters = ":C:d:D:fFhHPrRsS:t:TvVy:";

struct option XrdCpConfig::opVec[] =         // For getopt_long()
     {
      {OPT_TYPE "cksum",     1, 0, XrdCpConfig::OpCksum},
      {OPT_TYPE "debug",     1, 0, XrdCpConfig::OpDebug},
      {OPT_TYPE "coerce",    0, 0, XrdCpConfig::OpCoerce},
      {OPT_TYPE "force",     0, 0, XrdCpConfig::OpForce},
      {OPT_TYPE "help",      0, 0, XrdCpConfig::OpHelp},
      {OPT_TYPE "nopbar",    0, 0, XrdCpConfig::OpNoPbar},
      {OPT_TYPE "posc",      0, 0, XrdCpConfig::OpPosc},
      {OPT_TYPE "proxy",     0, 0, XrdCpConfig::OpProxy},
      {OPT_TYPE "recursive", 0, 0, XrdCpConfig::OpRecurse},
      {OPT_TYPE "retry",     1, 0, XrdCpConfig::OpRetry},
      {OPT_TYPE "server",    0, 0, XrdCpConfig::OpServer},
      {OPT_TYPE "silent",    0, 0, XrdCpConfig::OpSilent},
      {OPT_TYPE "sources",   0, 0, XrdCpConfig::OpSources},
      {OPT_TYPE "streams",   0, 0, XrdCpConfig::OpStreams},
      {OPT_TYPE "tpc",       2, 0, XrdCpConfig::OpTpc},
      {OPT_TYPE "verbose",   0, 0, XrdCpConfig::OpVerbose},
      {OPT_TYPE "version",   0, 0, XrdCpConfig::OpVersion},
      {OPT_TYPE "xrate",     1, 0, XrdCpConfig::OpXrate},
      {0,                    0, 0, 0}
     };

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCpConfig::XrdCpConfig(const char *pgm)
{
   PName    = pgm;
   intDefs  = 0;
   intDend  = 0;
   strDefs  = 0;
   strDend  = 0;
   dstOpq   = 0;
   srcOpq   = 0;
   pHost    = 0;
   pPort    = 0;
   xRate    = 0;
   OpSpec   = 0;
   Dlvl     = 0;
   nSrcs    = 1;
   nStrm    = 1;
   Retry    = 0;
   Verbose  = 0;
   numFiles = 0;
   totBytes = 0;
   CksLen   = 0;
   CksMan   = 0;
   CksObj   = 0;
   CksVal   = 0;
   srcFile  = 0;
   dstFile  = 0;
}

/******************************************************************************/
/*                                C o n f i g                                 */
/******************************************************************************/

void XrdCpConfig::Config(int aCnt, char **aVec, int opts)
{
   extern char *optarg;
   extern int   optind, opterr, optopt;
   static int pgmSet = 0;
   XrdCpFile    pBase, *pFile, *pLast, *pPrev;
   char Buff[128], *Path, opC;
   int i, isLcl, rc;

// Preset handling options
//
   Argv   = aVec;
   Argc   = aCnt;
   Opts   = opts;
   opterr = 0;
   optind = 1;
   opC    = 0;

// Set name of executable for error messages
//
   if (!pgmSet)
      {char *Slash = rindex(aVec[0], '/');
       pgmSet = 1;
       Pgm = (Slash ? Slash+1 : aVec[0]);
       Log->SetPrefix(Pgm);
      }

// Process legacy options first before atempting normal options
//
do{while(optind < Argc && Legacy()) {}
   if ((opC = getopt_long(Argc, Argv, opLetters, opVec, &i)) >= 0)
      switch(opC)
         {case OpCksum:    defCks(optarg);
                           break;
          case OpDebug:    OpSpec |= DoDebug;
                           if (!a2i(optarg, &Dlvl, 0, 3)) Usage(22);
                           break;
          case OpForce:    OpSpec |= DoForce;
                           break;
          case OpHelp:     Usage(0);
                           break;
          case OpLicense:  License();
                           break;
          case OpNoPbar:   OpSpec |= DoNoPbar;
                           break;
          case OpPosc:     OpSpec |= DoPosc;
                           break;
          case OpProxy:    OpSpec |= DoProxy;
                           defPxy(optarg);
                           break;
          case OpRecurse:  OpSpec |= DoRecurse;
                           break;
          case OpRecursv:  OpSpec |= DoRecurse;
                           break;
          case OpRetry:    OpSpec |= DoRetry;
                           if (!a2i(optarg, &Retry, 0, 0)) Usage(22);
                           break;
          case OpServer:   OpSpec |= DoServer;
                           break;
          case OpSilent:   OpSpec |= DoSilent|DoNoPbar;
                           break;
          case OpSources:  OpSpec |= DoSources;
                           if (!a2i(optarg, &nSrcs, 1, 32)) Usage(22);
                           break;
          case OpStreams:  OpSpec |= DoStreams;
                           if (!a2i(optarg, &nStrm, 1, 15)) Usage(22);
                           break;
          case OpTpc:      OpSpec |= DoTpc;
                           break;
          case OpVerbose:  OpSpec |= DoVerbose;
                           Verbose = 1;
                           break;
          case OpVersion:  cerr <<XrdVSTRING <<endl; exit(0);
                           break;
          case OpXrate:    OpSpec |= DoXrate;
                           if (!a2l(optarg, &xRate, 10*1024LL, 0)) Usage(22);
                           break;
          case ':':        UMSG("'" <<OpName() <<"' argument missing.");
                           break;
          case '?':        if (!Legacy())
                              UMSG("Invalid option, '" <<OpName() <<"'.");
                           break;
          default:         UMSG("Internal error processing '" <<OpName() <<"'.");
                           break;
         }
  } while(opC >= 0 && optind < Argc);

// Make sure we have the right number of files
//
   if (optind     >= Argc) UMSG("No files specified.");
   if ((optind+1) >= Argc) UMSG("Destination not specified.");
   if (Argc - optind > 2 && Opts & opt1Src)
      UMSG("Only a single source is allowed");

// Check for conflicts wit third party copy
//
   if (OpSpec & DoTpc &&  nSrcs > 1)
      UMSG("Third party copy requires a single source.");

// Process the destination first as it is special
//
   dstFile = new XrdCpFile(Argv[Argc-1], rc);
   if (rc) FMSG("Invalid url, '" <<dstFile->Path <<"'.", 22);

// Do a protocol check
//
   if (dstFile->Protocol != XrdCpFile::isFile
   &&  dstFile->Protocol != XrdCpFile::isXroot)
      {FMSG(dstFile->ProtName <<"file protocol is not supported.", 22)}

// Resolve this file if it is a local file
//
   isLcl = (dstFile->Protocol == XrdCpFile::isFile);
   if (isLcl && (rc = dstFile->Resolve()))
      {if (rc != ENOENT || (Argc - optind - 1) > 1 || OpSpec & DoRecurse)
          FMSG(strerror(rc) <<" processing " <<dstFile->Path, 2);
      }

// Now pick up all the source files
//
   pLast = &pBase;
   for (i = optind; i < Argc-1; i++)
       {pLast->Next = pFile = new XrdCpFile(Argv[i], rc);
        if (rc) FMSG("Invalid url, '" <<dstFile->Path <<"'.", 22);
        if (pFile->Protocol == XrdCpFile::isFile && (rc = pFile->Resolve()))
           FMSG(strerror(rc) <<" processing " <<pFile->Path, 2);
             if (pFile->Protocol == XrdCpFile::isFile)
                {totBytes += pFile->fSize; numFiles++;}
        else if (pFile->Protocol == XrdCpFile::isDir)
                {if (!(OpSpec & DoRecurse))
                    FMSG(pFile->Path <<" is a directory.", 2);
                }
        else if (pFile->Protocol != XrdCpFile::isXroot)
                {FMSG(pFile->ProtName <<" file protocol is not supported.", 22)}
        else if (OpSpec & DoRecurse)
                {FMSG("Recursive copy from a remote host is not supported.",22)}
        else {isLcl = 0; numFiles++;}
        pLast = pFile;
       }
   srcFile = pBase.Next;

// Do the dumb check
//
   if (isLcl) FMSG("All files are local; use 'cp' instead!", 1);

// Check for checksum spec conflicts
//
   if (OpSpec & DoCksum)
      {if (CksData.Length && numFiles > 2)
          FMSG("Checksum with fixed value requires a single input file.", 2);
       if (OpSpec & DoRecurse)
          FMSG("Checksum with fixed value conflicts with '--recursive'.", 2);
      }

// Now extend all local sources if recursive is in effect
//
   if (OpSpec & DoRecurse && !(Opts & optNoXtnd))
      {pPrev = &pBase; pBase.Next = srcFile;
       while((pFile = pPrev->Next))
            {if (pFile->Protocol != XrdCpFile::isDir)  pPrev = pFile;
                else {Path = pFile->Path;
                      pPrev->Next = pFile->Next;
                      if (Verbose) EMSG("Indexing files in " <<Path);
                      if ((rc = pFile->Extend(&pLast, numFiles, totBytes)))
                         FMSG(strerror(rc) <<" indexing " <<Path, 2);
                      if (pFile->Next)
                         {pLast->Next = pPrev->Next;
                          pPrev->Next = pFile->Next;
                         }
                      delete pFile;
                     }
            }
       srcFile = pBase.Next;
       if (Verbose) EMSG("Copying " <<Human(totBytes, Buff, sizeof(Buff))
                         <<" from " <<numFiles
                         <<(numFiles != 1 ? " files." : " file."));
      }
}
  
/******************************************************************************/
/*                       P r i v a t e   M e t h o d s                        */
/******************************************************************************/
/******************************************************************************/
/* Private:                          a 2 i                                    */
/******************************************************************************/
  
int XrdCpConfig::a2i(const char *item, int *val, int minv, int maxv)
{
    char *eP;

// Convert the numeric argument
//
    errno = 0;
    *val  = strtol(item, &eP, 10);
    if (errno || *eP) ZMSG("'" <<OpName() <<"' argument is not a number.");

// Impose min/max limits
//
    if (*val < minv)
       ZMSG("'" <<OpName() <<"' argument must be >= " <<minv <<'.');
    if (maxv >= 0 && *val > maxv)
       ZMSG("'" <<OpName() <<"' argument must be <= " <<maxv <<'.');
    return 1;
}
/******************************************************************************/
/* Private:                          a 2 l                                    */
/******************************************************************************/
  
int XrdCpConfig::a2l(const char *item, long long *val,
                                       long long minv, long long maxv)
{
    char *eP;

// Convert the numeric argument
//
    errno = 0;
    *val  = strtoll(item, &eP, 10);
    if (errno || *eP) ZMSG("'" <<OpName() <<"' argument is not a number.");

// Impose min/max limits
//
    if (*val < minv)
       ZMSG("'" <<OpName() <<"' argument must be >= " <<minv <<'.');
    if (maxv >= 0 && *val > maxv)
       ZMSG("'" <<OpName() <<"' argument must be <= " <<maxv <<'.');
    return 1;
}

/******************************************************************************/
/* Private:                          a 2 t                                    */
/******************************************************************************/
  
int XrdCpConfig::a2t(const char *item, int *val, int minv, int maxv)
{   int qmult;
    char *eP, *fP = (char *)item + strlen(item) - 1;

// Get scaling
//
         if (*fP == 's' || *fP == 'S') qmult = 1;
    else if (*fP == 'm' || *fP == 'M') qmult = 60;
    else if (*fP == 'h' || *fP == 'H') qmult = 60*60;
    else if (*fP == 'd' || *fP == 'D') qmult = 60*60*24;
    else                              {qmult = 1; fP++;}

// Convert the value
//
    errno = 0;
    *val  = strtoll(item, &eP, 10) * qmult;
    if (errno || eP != fP)
       ZMSG("'" <<OpName() <<"' argument is not a valid time.");

// Impose min/max limits
//
    if (*val < minv) 
       ZMSG("'" <<OpName() <<"' argument must be >= " <<minv <<'.');
    if (maxv >= 0 && *val > maxv)
       ZMSG("'" <<OpName() <<"' argument must be <= " <<maxv <<'.');
    return 1;
}

/******************************************************************************/
/* Private:                          a 2 x                                    */
/******************************************************************************/
  
int XrdCpConfig::a2x(const char *Val, char *Buff, int Vlen)
{
   int n, i = 0, Odd = 0;
   if (Vlen & 0x01) return 0;
   while(Vlen--)
        {     if (*Val >= '0' && *Val <= '9') n = *Val-48;
         else if (*Val >= 'a' && *Val <= 'f') n = *Val-87;
         else if (*Val >= 'A' && *Val <= 'F') n = *Val-55;
         else return 0;
         if (Odd) Buff[i++] |= n;
            else  Buff[i  ]  = n << 4;
         Val++; Odd = ~Odd;
        }
   return 1;
}

/******************************************************************************/
/* Private:                          a 2 z                                    */
/******************************************************************************/
  
int XrdCpConfig::a2z(const char *item, long long *val,
                                       long long minv, long long maxv)
{   long long qmult;
    char *eP, *fP = (char *)item + strlen(item) - 1;

// Get scaling
//
         if (*fP == 'k' || *fP == 'K') qmult = 1024LL;
    else if (*fP == 'm' || *fP == 'M') qmult = 1024LL*1024LL;
    else if (*fP == 'g' || *fP == 'G') qmult = 1024LL*1024LL*1024LL;
    else if (*fP == 't' || *fP == 'T') qmult = 1024LL*1024LL*1024LL*1024LL;
    else                              {qmult = 1; fP++;}

// Convert the value
//
    errno = 0;
    *val  = strtoll(item, &eP, 10) * qmult;
    if (errno || eP != fP)
       ZMSG("'" <<OpName() <<"' argument is not a valid time.");

// Impose min/max limits
//
    if (*val < minv) 
       ZMSG("'" <<OpName() <<"' argument must be >= " <<minv <<'.');
    if (maxv >= 0 && *val > maxv)
       ZMSG("'" <<OpName() <<"' argument must be <= " <<maxv <<'.');
    return 1;
}

/******************************************************************************/
/* Private:                       d e f C k s                                 */
/******************************************************************************/
  
int XrdCpConfig::defCks(const char *opval)
{
   const char *Colon = index(opval, ':');
   char  csName[XrdCksData::NameSize];
   int csSize, n;

// Initialize the checksum manager if we have not done so already
//
   if (!CksMan)
      {CksMan = new XrdCksManager(Log);
       if (!(CksMan->Init("")))
          {delete CksMan; CksMan = 0;
           FMSG("Unable to initialize checksum processing.", 13);
          }
      }

// Copy out the checksum name
//
   n = (Colon ? Colon - opval : strlen(opval));
   if (n >= XrdCksData::NameSize)
      UMSG("Invalid checksum type, '" <<opval <<"'.");
   strncpy(csName, opval, n); csName[n] = 0;

// Get a checksum object for this checksum
//
   if (CksObj) {delete CksObj; CksObj = 0;}
   if (!CksData.Set(csName) || !(CksObj = CksMan->Object(CksData.Name)))
      UMSG("Invalid checksum type, '" <<csName <<"'.");
   CksObj->Type(CksLen);

// Reset checksum information
//
   CksData.Length = 0;
   OpSpec &= ~DoCkprt;
   OpSpec |=  DoCksum;

// Check for any additional arguments
//
   if (Colon)
      {Colon++;
       if (!(*Colon)) UMSG(CksData.Name <<" argument missing after ':'.");
       if (!strcmp(Colon, "print")) OpSpec |= DoCkprt;
          else {n = strlen(Colon);
                if (n != CksLen*2 || !CksData.Set(Colon, n))
                   UMSG("Invalid " <<CksData.Name <<" value '" <<Colon <<"'.");
               }
      }

// All done
//
   CksVal = opval;
   return 1;
}

/******************************************************************************/
/* Private:                       d e f O p q                                 */
/******************************************************************************/
  
int XrdCpConfig::defOpq(const char *theOp)
{
   const char *oVal = theOp+3;

// Make sure opaque information was specified
//
   if (!(*oVal)) UMSG("'" <<theOp <<"' opaque data not specified.");

// Set proper opaque data
//
   if (*(theOp+2) == 'S') srcOpq = oVal;
      else                dstOpq = oVal;

// All done
//
   return 1;
}

/******************************************************************************/
/* Private:                       d e f O p t                                 */
/******************************************************************************/
  
int XrdCpConfig::defOpt(const char *theOp, const char *theArg)
{
   defVar      *dP;
          int   opval, isInt = (*(theOp+2) == 'I');
   const  char *vName = theOp+3;
          char *eP;

// Make sure define variable name specified
//
   if (!(*vName)) UMSG("'" <<theOp <<"' variable not specified.");

// Make sure we have a value
//
   if (!theArg) UMSG("'" <<theOp <<"' argument not specified.");

// For integer arguments convert the value
//
   if (isInt)
      {errno = 0;
       opval  = strtol(theArg, &eP, 10);
       if (errno || *eP) UMSG("'" <<theOp <<"' argument is not a number.");
       dP = new defVar(vName, opval);
       if (!intDend) intDefs = intDend = dP;
          else {intDend->Next = dP; intDend = dP;}
     } else {
       dP = new defVar(vName, theArg);
       if (!intDend) intDefs = intDend = dP;
          else {intDend->Next = dP; intDend = dP;}
     }

// Convert the argument
//
   return 2;
}

/******************************************************************************/
/* Private:                       d e f P x y                                 */
/******************************************************************************/
  
void XrdCpConfig::defPxy(const char *opval)
{
   const char *Colon = index(opval, ':');
   char *eP;
   int n;

// Make sure the host was specified
//
   if (Colon == opval) UMSG("Proxy host not specified.");

// Make sure the port was specified
//
   if (!Colon || !(*(Colon+1))) UMSG("Proxy port not specified.");

// Make sure the port is a valid number that is not too big
//
    errno = 0;
    pPort = strtol(Colon+1, &eP, 10);
    if (errno || *eP || pPort < 1 || pPort > 65535)
       UMSG("Invalid proxy port, '" <<opval <<"'.");

// Copy out the proxy host
//
   if (pHost) free(pHost);
   n = Colon - opval + 1;
   pHost = (char *)malloc(n);
   strncpy(pHost, opval, n-1);
   pHost[n-1] = 0;
}


/******************************************************************************/
/*                                 H u m a n                                  */
/******************************************************************************/
  
const char *XrdCpConfig::Human(long long inval, char *Buff, int Blen)
{
    static const char *sfx[] = {" bytes", "KB", "MB", "GB", "TB", "PB"};
    unsigned int i;

    for (i = 0; i < sizeof(sfx)-1 && inval >= 1024; i++) inval = inval/1024;

    snprintf(Buff, Blen, "%lld%s", inval, sfx[i]);
    return Buff;
}

/******************************************************************************/
/* Private:                       L e g a c y                                 */
/******************************************************************************/

int XrdCpConfig::Legacy()
{
   extern int optind;
   char *oArg;
   int   rc;

   if (!Argv[optind]) return 0;

   if (optind+1 >= Argc || *Argv[optind+1] == '-') oArg = 0;
      else oArg = Argv[optind+1];
   if (!(rc = Legacy(Argv[optind], oArg))) return 0;
   optind += rc;
}

/******************************************************************************/
  
int XrdCpConfig::Legacy(const char *theOp, const char *theArg)
{
   if (!strcmp(theOp, "-adler")) return defCks("adler32:print");

   if (!strncmp(theOp, "-DI", 3) || !strncmp(theOp, "-DS", 3))
      return defOpt(theOp, theArg);

   if (!strcmp(theOp, "-extreme") || !strcmp(theOp, "-x"))
      {if (nSrcs <= 1) nSrcs = dfltSrcs;
       return 1;
      }

   if (!strcmp(theOp, "-np")) {OpSpec |= DoNoPbar; return 1;}

   if (!strcmp(theOp, "-md5")) return defCks("md5:print");

   if (!strncmp(theOp,"-OD",3) || !strncmp(theOp,"-OS",3)) return defOpq(theOp);

   if (!strcmp(theOp, "-version")) {cerr <<XrdVSTRING <<endl; exit(0);}

   if (!strcmp(theOp, "-force"))
      FMSG("-force is no longer supported; use --retry instead!",22);

   return 0;
}

/******************************************************************************/
/* Private:                      L i c e n s e                                */
/******************************************************************************/
  
void XrdCpConfig::License()
{
   const char *theLicense =
#include "../../LICENSE"
;

   cerr <<theLicense;
   exit(0);
}

/******************************************************************************/
/* Private:                       O p N a m e                                 */
/******************************************************************************/
  
const char *XrdCpConfig::OpName()
{
   extern int optind, optopt;
   static char oName[4] = {'-', 0, 0, 0};

   if (optopt == '-') return Argv[optind-1];
   oName[1] = optopt;
   return oName;
}

/******************************************************************************/
/*                                 U s a g e                                  */
/******************************************************************************/
  
void XrdCpConfig::Usage(int rc)
{
   static const char *Syntax = "\n"
   "Usage:   xrdcp [<options>] <src> [<src> [. . .]] <dest>\n";

   static const char *Syntax1= "\n"
   "Usage:   xrdcp [<options>] <src> <dest>\n";

   static const char *Options= "\n"
   "Options: [--cksum <args>] [--debug <lvl>] [--coerce] [--force] [--help]\n"
   "         [--license] [--nopbar] [--posc] [--proxy <host>:<port] [--recursive]\n"
   "         [--retry <time>] [--server] [--silent] [--sources <n>]\n"
   "         [--streams <n>] [--tpc] [--verbose] [--version] [--xrate <rate>]\n"
   "<src>:   [[x]root://<host>[:<port>]/]<path>\n"
   "<dest>:  [[x]root://<host>[:<port>]/]<path> | -";

   static const char *Detail = "\n"
   "-C | --cksum <args> verifies the checksum at the destination as provided\n"
   "                    by the source server or locally computed. The args are\n"
   "                    {adler32 | crc32 | md5}[:{<value>|print}]\n"
   "                    If the hex value of the checksum is given, it is used.\n"
   "                    Otherwise, the server's checksum is used for remote files\n"
   "                    and computed for local files. Specifying print merely\n"
   "                    prints the checksum but does not verify it.\n"
   "-d | --debug <lvl>  sets the debug level: 0 off, 1 low, 2 medium, 3 high\n"
   "-F | --coerce       coerces the copy by ignoring file locking semantics\n"
   "-f | --force        replaces any existing output file\n"
   "-h | --help         prints this information\n"
   "-H | --license      prints license terms and conditions\n"
   "-N | --nopbar       does not print the progress bar\n"
   "-P | --posc         enables persist on successful close semantics\n"
   "-D | --proxy        uses the specified SOCKS4 proxy connection\n"
   "-r | --recursive    recursively copies all local source files\n"
   "     --server       runs in a server environment with added operations\n"
   "-s | --silent       produces no output other than error messages\n"
   "-y | --sources <n>  uses up to the number of sources specified in parallel\n"
   "-S | --streams <n>  copies using the specified number of TCP connections\n"
   "-T | --tpc          uses third party copy mode between the src and dest.\n"
   "                    The copy fails unless src and dest allow tpc mode.\n"
   "-v | --verbose      produces more information about the copy\n"
   "-V | --version      prints the version number\n"
   "-X | --xrate <rate> limits the transfer to the specified rate. You can\n"
   "                    suffix the value with 'k', 'm', or 'g'\n\n"
   "Legacy options:     [-adler] [-DI<var> <val>] [-DS<var> <val>] [-np]\n"
   "                    [-md5] [-OD<cgi>] [-OS<cgi>] [-version] [-x]";

   cerr <<(Opts & opt1Src ? Syntax1 : Syntax) <<Options  <<endl;
   if (!rc) cerr <<Detail <<endl;
   exit(rc);
}
