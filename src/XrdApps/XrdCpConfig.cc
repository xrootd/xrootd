/******************************************************************************/
/*                                                                            */
/*                        X r d C p C o n f i g . c c                         */
/*                                                                            */
/* (c) 2011 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
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
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/
  
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "XrdVersion.hh"
#include "XrdApps/XrdCpConfig.hh"
#include "XrdApps/XrdCpFile.hh"
#include "XrdCks/XrdCksCalc.hh"
#include "XrdCks/XrdCksManager.hh"
#include "XrdOuc/XrdOucStream.hh"
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
  
const char   *XrdCpConfig::opLetters = ":C:d:D:fFhHI:NPrRsS:t:T:vVX:y:Z";

struct option XrdCpConfig::opVec[] =         // For getopt_long()
     {
      {OPT_TYPE "cksum",     1, 0, XrdCpConfig::OpCksum},
      {OPT_TYPE "debug",     1, 0, XrdCpConfig::OpDebug},
      {OPT_TYPE "dynamic-src",0,0, XrdCpConfig::OpDynaSrc},
      {OPT_TYPE "coerce",    0, 0, XrdCpConfig::OpCoerce},
      {OPT_TYPE "force",     0, 0, XrdCpConfig::OpForce},
      {OPT_TYPE "help",      0, 0, XrdCpConfig::OpHelp},
      {OPT_TYPE "infiles",   1, 0, XrdCpConfig::OpIfile},
      {OPT_TYPE "license",   0, 0, XrdCpConfig::OpLicense},
      {OPT_TYPE "nopbar",    0, 0, XrdCpConfig::OpNoPbar},
      {OPT_TYPE "posc",      0, 0, XrdCpConfig::OpPosc},
      {OPT_TYPE "proxy",     1, 0, XrdCpConfig::OpProxy},
      {OPT_TYPE "recursive", 0, 0, XrdCpConfig::OpRecurse},
      {OPT_TYPE "retry",     1, 0, XrdCpConfig::OpRetry},
      {OPT_TYPE "server",    0, 0, XrdCpConfig::OpServer},
      {OPT_TYPE "silent",    0, 0, XrdCpConfig::OpSilent},
      {OPT_TYPE "sources",   1, 0, XrdCpConfig::OpSources},
      {OPT_TYPE "streams",   1, 0, XrdCpConfig::OpStreams},
      {OPT_TYPE "tpc",       1, 0, XrdCpConfig::OpTpc},
      {OPT_TYPE "verbose",   0, 0, XrdCpConfig::OpVerbose},
      {OPT_TYPE "version",   0, 0, XrdCpConfig::OpVersion},
      {OPT_TYPE "xrate",     1, 0, XrdCpConfig::OpXrate},
      {OPT_TYPE "parallel",  1, 0, XrdCpConfig::OpParallel},
      {0,                    0, 0, 0}
     };

/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/
  
XrdCpConfig::XrdCpConfig(const char *pgm)
{
   if ((PName = rindex(pgm, '/'))) PName++;
      else PName = pgm;
   XrdCpFile::SetMsgPfx(PName);
   intDefs  = 0;
   intDend  = 0;
   strDefs  = 0;
   strDend  = 0;
   dstOpq   = 0;
   srcOpq   = 0;
   pHost    = 0;
   pPort    = 0;
   xRate    = 0;
   Parallel = 1;
   OpSpec   = 0;
   Dlvl     = 0;
   nSrcs    = 1;
   nStrm    = 1;
   Retry    =-1;
   Verbose  = 0;
   numFiles = 0;
   totBytes = 0;
   CksLen   = 0;
   CksMan   = 0;
   CksObj   = 0;
   CksVal   = 0;
   srcFile  = 0;
   dstFile  = 0;
   inFile   = 0;
   parmVal  = 0;
   parmCnt  = 0;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/
  
XrdCpConfig::~XrdCpConfig()
{
   XrdCpFile *pNow;
   defVar    *dP;

   if (inFile)  free(inFile);
   if (pHost)   free(pHost);
   if (parmVal) free(parmVal);
   if (CksMan)  delete CksMan;
   if (dstFile) delete dstFile;

   while((pNow = pFile)) {pFile = pFile->Next; delete pNow;}

   while((dP = intDefs)) {intDefs = dP->Next;  delete dP;}
   while((dP = strDefs)) {strDefs = dP->Next;  delete dP;}

}
  
/******************************************************************************/
/*                                C o n f i g                                 */
/******************************************************************************/

void XrdCpConfig::Config(int aCnt, char **aVec, int opts)
{
   extern char *optarg;
   extern int   optind, opterr;
   static int pgmSet = 0;
   char Buff[128], *Path, opC;
   XrdCpFile    pBase;
   int i, rc;

// Allocate a parameter vector
//
   if (parmVal) free(parmVal);
   parmVal = (char **)malloc(aCnt*sizeof(char *));

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
do{while(optind < Argc && Legacy(optind)) {}
   if ((opC = getopt_long(Argc, Argv, opLetters, opVec, &i)) != (char)-1)
      switch(opC)
         {case OpCksum:    defCks(optarg);
                           break;
          case OpDebug:    OpSpec |= DoDebug;
                           if (!a2i(optarg, &Dlvl, 0, 3)) Usage(22);
                           break;
          case OpDynaSrc:  OpSpec |= DoDynaSrc;
                           break;
          case OpForce:    OpSpec |= DoForce;
                           break;
          case OpHelp:     Usage(0);
                           break;
          case OpIfile:    if (inFile) free(inFile);
                           inFile = strdup(optarg);
                           OpSpec |= DoIfile;
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
                           if (!a2i(optarg, &Retry, 0, -1)) Usage(22);
                           break;
          case OpServer:   OpSpec |= DoServer|DoSilent|DoNoPbar|DoForce;
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
                           if (!strcmp("only",  optarg)) OpSpec|= DoTpcOnly;
                              else if (strcmp("first", optarg))
                                      {optind--;
                                       UMSG("Invalid option, '" <<OpName()
                                            <<' ' <<optarg <<"' ");
                                      }
                           break;
          case OpVerbose:  OpSpec |= DoVerbose;
                           Verbose = 1;
                           break;
          case OpVersion:  cerr <<XrdVERSION <<endl; exit(0);
                           break;
          case OpXrate:    OpSpec |= DoXrate;
                           if (!a2z(optarg, &xRate, 10*1024LL, -1)) Usage(22);
                           break;
          case OpParallel: OpSpec |= DoParallel;
                           if (!a2i(optarg, &Parallel, 1, 4)) Usage(22);
                           break;
          case ':':        UMSG("'" <<OpName() <<"' argument missing.");
                           break;
          case '?':        if (!Legacy(optind-1))
                              UMSG("Invalid option, '" <<OpName() <<"'.");
                           break;
          default:         UMSG("Internal error processing '" <<OpName() <<"'.");
                           break;
         }
  } while(opC != (char)-1 && optind < Argc);

// Make sure we have the right number of files
//
   if (inFile) {if (!parmCnt     ) UMSG("Destination not specified.");}
      else {    if (!parmCnt     ) UMSG("No files specified.");
                if ( parmCnt == 1) UMSG("Destination not specified.");
           }

// Check for conflicts wit third party copy
//
   if (OpSpec & DoTpc &&  nSrcs > 1)
      UMSG("Third party copy requires a single source.");

// Turn off verbose if we are in server mode
//
   if (OpSpec & DoServer)
      {OpSpec &= ~DoVerbose;
       Verbose = 0;
      }

// Process the destination first as it is special
//
   dstFile = new XrdCpFile(parmVal[--parmCnt], rc);
   if (rc) FMSG("Invalid url, '" <<dstFile->Path <<"'.", 22);

// Do a protocol check
//
   if (dstFile->Protocol != XrdCpFile::isFile
   &&  dstFile->Protocol != XrdCpFile::isStdIO
   &&  dstFile->Protocol != XrdCpFile::isXroot)
      {FMSG(dstFile->ProtName <<"file protocol is not supported.", 22)}

// Resolve this file if it is a local file
//
   isLcl = (dstFile->Protocol == XrdCpFile::isFile)
         | (dstFile->Protocol == XrdCpFile::isStdIO);
   if (isLcl && (rc = dstFile->Resolve()))
      {if (rc != ENOENT || (Argc - optind - 1) > 1 || OpSpec & DoRecurse)
          FMSG(strerror(rc) <<" processing " <<dstFile->Path, 2);
      }

// Now pick up all the source files from the command line
//
   pLast = &pBase;
   for (i = 0; i < parmCnt; i++) ProcFile(parmVal[i]);

// If an input file list was specified, process it as well
//
   if (inFile)
      {XrdOucStream inList(Log);
       char *fname;
       int inFD = open(inFile, O_RDONLY);
       if (inFD < 0) FMSG(strerror(errno) <<" opening infiles " <<inFile, 2);
       inList.Attach(inFD);
       while((fname = inList.GetLine())) if (*fname) ProcFile(fname);
      }

// Check if we have any sources or too many sources
//
   if (!numFiles) UMSG("Source not specified.");
   if (Opts & opt1Src && numFiles > 1)
      FMSG("Only a single source is allowed.", 2);
   srcFile = pBase.Next;

// Check if we have an appropriate destination
//
   if (dstFile->Protocol == XrdCpFile::isFile && (numFiles > 1 
   ||  (OpSpec & DoRecurse && srcFile->Protocol != XrdCpFile::isFile)))
      FMSG("Destination is neither remote nor a directory.", 2);

// Do the dumb check
//
   if (isLcl && Opts & optNoLclCp)
      FMSG("All files are local; use 'cp' instead!", 1);

// Check for checksum spec conflicts
//
   if (OpSpec & DoCksum)
      {if (CksData.Length && numFiles > 1)
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
                      numFiles--;
                      if ((rc = pFile->Extend(&pLast, numFiles, totBytes)))
                         FMSG(strerror(rc) <<" indexing " <<Path, 2);
                      if (pFile->Next)
                         {pLast->Next = pPrev->Next;
                          pPrev->Next = pFile->Next;
                         }
                      delete pFile;
                     }
            }
       if (!(srcFile = pBase.Next))
          FMSG("No regular files found to copy!", 2);
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
   static XrdVERSIONINFODEF(myVer, xrdcp, XrdVNUMBER, XrdVERSION);
   const char *Colon = index(opval, ':');
   char  csName[XrdCksData::NameSize];
   int   n;

// Initialize the checksum manager if we have not done so already
//
   if (!CksMan)
      {CksMan = new XrdCksManager(Log, 0, myVer, true);
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
   OpSpec &= ~(DoCkprt | DoCksrc | DoCksum);

// Check for any additional arguments
//
   if (Colon)
      {Colon++;
       if (!(*Colon)) UMSG(CksData.Name <<" argument missing after ':'.");
       if (!strcmp(Colon, "print")) OpSpec |= (DoCkprt | DoCksum);
          else if (!strcmp(Colon, "source")) OpSpec |= (DoCkprt | DoCksrc);
          else {n = strlen(Colon);
                if (n != CksLen*2 || !CksData.Set(Colon, n))
                   UMSG("Invalid " <<CksData.Name <<" value '" <<Colon <<"'.");
                OpSpec |= DoCksum;
               }
      } else OpSpec |= DoCksum;

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
       if (!strDend) strDefs = strDend = dP;
          else {strDend->Next = dP; strDend = dP;}
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

    for (i = 0; i < sizeof(sfx)/sizeof(sfx[0]) - 1 && inval >= 1024; i++) 
        inval = inval/1024;

    snprintf(Buff, Blen, "%lld%s", inval, sfx[i]);
    return Buff;
}

/******************************************************************************/
/* Private:                       L e g a c y                                 */
/******************************************************************************/

int XrdCpConfig::Legacy(int oIndex)
{
   extern int optind;
   char *oArg;
   int   rc;

// if (!Argv[oIndex]) return 0;

   while(oIndex < Argc && (*Argv[oIndex] != '-' || *(Argv[oIndex]+1) == '\0'))
        parmVal[parmCnt++] = Argv[oIndex++];
   if (oIndex >= Argc) return 0;

   if (oIndex+1 >= Argc || *Argv[oIndex+1] == '-') oArg = 0;
      else oArg = Argv[oIndex+1];
   if (!(rc = Legacy(Argv[oIndex], oArg))) return 0;
   optind = oIndex + rc;

   return 1;
}

/******************************************************************************/
  
int XrdCpConfig::Legacy(const char *theOp, const char *theArg)
{
   if (!strcmp(theOp, "-adler")) return defCks("adler32:source");

   if (!strncmp(theOp, "-DI", 3) || !strncmp(theOp, "-DS", 3))
      return defOpt(theOp, theArg);

   if (!strcmp(theOp, "-extreme") || !strcmp(theOp, "-x"))
      {if (nSrcs <= 1) {nSrcs = dfltSrcs; OpSpec |= DoSources;}
       return 1;
      }

   if (!strcmp(theOp, "-np")) {OpSpec |= DoNoPbar; return 1;}

   if (!strcmp(theOp, "-md5")) return defCks("md5:source");

   if (!strncmp(theOp,"-OD",3) || !strncmp(theOp,"-OS",3)) return defOpq(theOp);

   if (!strcmp(theOp, "-version")) {cerr <<XrdVERSION <<endl; exit(0);}

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

   if (!optopt || optopt == '-' || *(Argv[optind-1]+1) == '-')
      return Argv[optind-1];
   oName[1] = optopt;
   return oName;
}

/******************************************************************************/
/*                              p r o c F i l e                               */
/******************************************************************************/
  
void XrdCpConfig::ProcFile(const char *fname)
{
   int rc;

// Chain in this file in the input list
//
   pLast->Next = pFile = new XrdCpFile(fname, rc);
   if (rc) FMSG("Invalid url, '" <<fname <<"'.", 22);

// For local files, make sure it exists and get its size
//
   if (pFile->Protocol == XrdCpFile::isFile && (rc = pFile->Resolve()))
      FMSG(strerror(rc) <<" processing " <<pFile->Path, 2);

// Process file based on type (local or remote)
//
         if (pFile->Protocol == XrdCpFile::isFile) totBytes += pFile->fSize;
    else if (pFile->Protocol == XrdCpFile::isDir)
            {if (!(OpSpec & DoRecurse))
                FMSG(pFile->Path <<" is a directory.", 2);
            }
    else if (pFile->Protocol == XrdCpFile::isStdIO)
            {if (Opts & optNoStdIn)
                FMSG("Using stdin as a source is disallowed.", 22);
             if (numFiles)
                FMSG("Multiple sources disallowed with stdin.", 22);
            }
    else if (pFile->Protocol != XrdCpFile::isXroot)
            {FMSG(pFile->ProtName <<" file protocol is not supported.", 22)}
    else if (OpSpec & DoRecurse && !(Opts & optRmtRec))
            {FMSG("Recursive copy from a remote host is not supported.",22)}
    else isLcl = 0;

// Update last pointer and we are done if this is stdin
//
   numFiles++;
   pLast = pFile;
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
   "Options: [--cksum <args>] [--debug <lvl>] [--coerce] [--dynamic-src]\n"
   "         [--force] [--help] [--infiles <fn>] [--license] [--nopbar]\n"
   "         [--posc] [--proxy <host>:<port>] [--recursive] [--retry <n>]\n"
   "         [--server] [--silent] [--sources <n>] [--streams <n>]\n"
   "         [--tpc {first|only}] [--verbose] [--version] [--xrate <rate>]\n"
   "         [--parallel <n>]";

   static const char *Syntax2= "\n"
   "<src>:   [[x]root://<host>[:<port>]/]<path> | -";

   static const char *Syntay2= "\n"
   "<src>:   [[x]root://<host>[:<port>]/]<path>";

   static const char *Syntax3= "\n"
   "<dest>:  [[x]root://<host>[:<port>]/]<path> | -";

   static const char *Detail = "\n"
   "-C | --cksum <args> verifies the checksum at the destination as provided\n"
   "                    by the source server or locally computed. The args are\n"
   "                    {adler32 | crc32 | md5}[:{<value>|print|source}]\n"
   "                    If the hex value of the checksum is given, it is used.\n"
   "                    Otherwise, the server's checksum is used for remote files\n"
   "                    and computed for local files. Specifying print merely\n"
   "                    prints the checksum but does not verify it.\n"
   "-d | --debug <lvl>  sets the debug level: 0 off, 1 low, 2 medium, 3 high\n"
   "-Z | --dynamic-src  file size may change during the copy\n"
   "-F | --coerce       coerces the copy by ignoring file locking semantics\n"
   "-f | --force        replaces any existing output file\n"
   "-h | --help         prints this information\n"
   "-H | --license      prints license terms and conditions\n"
   "-I | --infiles      specifies the file that contains a list of input files\n"
   "-N | --nopbar       does not print the progress bar\n"
   "-P | --posc         enables persist on successful close semantics\n"
   "-D | --proxy        uses the specified SOCKS4 proxy connection\n"
   "-r | --recursive    recursively copies all source files\n"
   "-t | --retry <n>    maximum number of times to retry rejected connections\n"
   "     --server       runs in a server environment with added operations\n"
   "-s | --silent       produces no output other than error messages\n"
   "-y | --sources <n>  uses up to the number of sources specified in parallel\n"
   "-S | --streams <n>  copies using the specified number of TCP connections\n"
   "-T | --tpc          uses third party copy mode between the src and dest.\n"
   "                    Both the src and dest must allow tpc mode. Argument\n"
   "                    'first' tries tpc and if it fails, does a normal copy;\n"
   "                    while 'only' fails the copy unless tpc succeeds.\n"
   "-v | --verbose      produces more information about the copy\n"
   "-V | --version      prints the version number\n"
   "-X | --xrate <rate> limits the transfer to the specified rate. You can\n"
   "                    suffix the value with 'k', 'm', or 'g'\n"
   "     --parallel <n> number of copy jobs to be run simultaneously\n\n"
   "Legacy options:     [-adler] [-DI<var> <val>] [-DS<var> <val>] [-np]\n"
   "                    [-md5] [-OD<cgi>] [-OS<cgi>] [-version] [-x]";

   cerr <<(Opts & opt1Src    ? Syntax1 : Syntax)  <<Options;
   cerr <<(Opts & optNoStdIn ? Syntay2 : Syntax2) <<Syntax3 <<endl;
   if (!rc) cerr <<Detail <<endl;
   exit(rc);
}
