#ifndef __XRDCPCONFIG_HH__
#define __XRDCPCONFIG_HH__
/******************************************************************************/
/*                                                                            */
/*                        X r d C p C o n f i g . h h                         */
/*                                                                            */
/* (c) 2012 by the Board of Trustees of the Leland Stanford, Jr., University  */
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

#include "XrdCks/XrdCksData.hh"

#include <cctype>
#include <cstdint>

#include <string>
#include <vector>

struct option;
class  XrdCks;
class  XrdCksCalc;
class  XrdCpFile;
class  XrdSysError;
  
class XrdCpConfig
{
public:

struct defVar
      {        defVar     *Next;    // -> Next such definition, 0 if no more
               const char *vName;   // -> Variable name
       union  {const char *strVal;  // -> String  value if in strDefs
               int         intVal;  //    Integer value if in intDefs
              };
               defVar(const char *vn, const char *vl)
                     : Next(0), vName(vn), strVal(vl) {}
               defVar(const char *vn, int         vl)
                     : Next(0), vName(vn), intVal(vl) {}
      };

       defVar      *intDefs;         // -> -DI settings
       defVar      *strDefs;         // -> -DS settings
       const char  *dstOpq;          // -> -OD setting (dest opaque)
       const char  *srcOpq;          // -> -OS setting (src  opaque)
       const char  *Pgm;             // -> Program name
        long long   xRate;           // -> xrate value in bytes/sec   (0 if not set)
        long long   xRateThreashold; // -> xrate threshold value in bytes/sec (0 if not set)
             int    Parallel;       // Number of simultaneous copy ops (1 to 4)
             char  *pHost;          // -> SOCKS4 proxy hname       (0 if none)
             int    pPort;          //    SOCKS4 proxy port
        long long   OpSpec;         // Bit mask of set options     (see Doxxxx)
             int    Dlvl;           // Debug level                 (0 to 3)
             int    nSrcs;          // Number of sources wanted    (dflt 1)
             int    nStrm;          // Number of streams wanted    (dflt 1)
             int    Retry;          // Max times to retry failed copy job
        std::string RetryPolicy;    // retry policy (to force or to continue)
             int    Verbose;        // True if --verbose specified
             int    CksLen;         // Binary length of checksum, if any

             int    numFiles;       // Number of source files
       long long    totBytes;       // Total number of bytes for local files

XrdCksData          CksData;        // Checksum information
XrdCks             *CksMan;         // -> Checksum manager
XrdCksCalc         *CksObj;         // -> Cks computation object   (0 if no cks)
const char         *CksVal;         // -> Cks argument (0 if none)

XrdCpFile          *srcFile;        // List of source files
XrdCpFile          *dstFile;        // The destination for the copy

char               *zipFile;        // The file name if the URL points to a ZIP archive

static XrdSysError *Log;            // -> Error message object

std::vector<std::string> AddCksVal; // -> Additional checksum argument

static const uint64_t    OpCksum        =  'C';  // -adler -MD5 legacy -> DoCksrc
static const uint64_t    DoCksrc        =  0x0000000000000001LL; // --cksum <type>:source
static const uint64_t    DoCksum        =  0x0000000000000002LL; // --cksum <type>
static const uint64_t    DoCkprt        =  0x0000000000000004LL; // --cksum <type>:print

static const uint64_t    OpCoerce       =  'F';
static const uint64_t    DoCoerce       =  0x0000000000000008LL; // -F | --coerce

static const uint64_t    OpDebug        =  'd';
static const uint64_t    DoDebug        =  0x0000000000000010LL; // -d | --debug <val>

static const uint64_t    OpForce        =  'f';
static const uint64_t    DoForce        =  0x0000000000000020LL; // -f | --force

static const uint64_t    OpHelp         =  'h';
static const uint64_t    DoHelp         =  0x0000000000000040LL; // -h | --help

static const uint64_t    OpIfile        =  'I';
static const uint64_t    DoIfile        =  0x0000000000000080LL; // -I | --infiles

static const uint64_t    OpLicense      =  'H';                  // -H | --license

static const uint64_t    OpNoPbar       =  'N';        // -N | --nopbar | -np {legacy}
static const uint64_t    DoNoPbar       =  0x0000000000000100LL;

static const uint64_t    OpPosc         =  'P';
static const uint64_t    DoPosc         =  0x0000000000000200LL; // -P | --posc

static const uint64_t    OpProxy        =  'D';
static const uint64_t    DoProxy        =  0x0000000000000400LL; // -D | --proxy

static const uint64_t    OpRecurse      =  'r';
static const uint64_t    OpRecursv      =  'R';        // -r | --recursive | -R {legacy}
static const uint64_t    DoRecurse      =  0x0000000000000800LL;

static const uint64_t    OpRetry        =  't';
static const uint64_t    DoRetry        =  0x0000000000001000LL; // -t | --retry

static const uint64_t    OpServer       =  0x03;
static const uint64_t    DoServer       =  0x0000000000002000LL; //      --server

static const uint64_t    OpSilent       =  's';
static const uint64_t    DoSilent       =  0x0000000000004000LL; // -s | --silent

static const uint64_t    OpSources      =  'y';
static const uint64_t    DoSources      =  0x0000000000008000LL; // -y | --sources

static const uint64_t    OpStreams      =  'S';
static const uint64_t    DoStreams      =  0x0000000000010000LL; // -S | --streams

static const uint64_t    OpTpc          =  'T'; // -T | --tpc [delegate] {first | only}
static const uint64_t    DoTpc          =  0x0000000000020000LL; // --tpc {first | only}
static const uint64_t    DoTpcOnly      =  0x0000000000100000LL; // --tpc          only
static const uint64_t    DoTpcDlgt      =  0x0000000000800000LL; // --tpc delegate ...

static const uint64_t    OpVerbose      =  'v';
static const uint64_t    DoVerbose      =  0x0000000000040000LL; // -v | --verbose

static const uint64_t    OpVersion      =  'V';                  // -V | --version

static const uint64_t    OpXrate        =  'X';
static const uint64_t    DoXrate        =  0x0000000000080000LL; // -X | --xrate

static const uint64_t    OpParallel     =  0x04;
static const uint64_t    DoParallel     =  0x0000000000200000LL; //      --parallel

static const uint64_t    OpDynaSrc      =  'Z';
static const uint64_t    DoDynaSrc      =  0x0000000000400000LL; //      --dynamic-src

//     const uint64_t    DoTpcDlgt      =  0x0000000000800000LL; // Marker for bit used

static const uint64_t    OpZip          =  'z';
static const uint64_t    DoZip          =  0x0000000001000000LL; // -z | --zip

static const uint64_t    OpTlsNoData    =  'E';
static const uint64_t    DoTlsNoData    =  0x0000000002000000LL; // -E | --tlsnodata

static const uint64_t    OpNoTlsOK      =  0x05;
static const uint64_t    DoNoTlsOK      =  0x0000000004000000LL; //      --notlsok

static const uint64_t    OpTlsMLF       =  0x06;
static const uint64_t    DoTlsMLF       =  0x0000000008000000LL; //      --tlsmetalink

static const uint64_t    OpPath         =  'p';
static const uint64_t    DoPath         =  0x0000000010000000LL; // -p | --path

static const uint64_t    OpXAttr        =  0x07;
static const uint64_t    DoXAttr        =  0x0000000020000000LL; // --xattr

static const uint64_t    OpZipMtlnCksum = 0x08;
static const uint64_t    DoZipMtlnCksum = 0x0000000040000000LL; // --zip-mtln-cksum

static const uint64_t    OpRmOnBadCksum = 0x09;
static const uint64_t    DoRmOnBadCksum = 0x0000000080000000LL; // --rm-bad-cksum

static const uint64_t    OpContinue     = 0x10;
static const uint64_t    DoContinue     = 0x0000000100000000LL; // --continue

static const uint64_t    OpXrateThreashold = 0x11;
static const uint64_t    DoXrateThreashold = 0x0000000200000000LL; // --xrate-threashold

static const uint64_t    OpRetryPolicy     = 0x12;
static const uint64_t    DoRetryPolicy     = 0x0000000400000000LL; // --retry-policy

static const uint64_t    OpZipAppend       = 0x13;
static const uint64_t    DoZipAppend       = 0x0000000800000000LL; // --zip-append

// Flag to allow the use of HTTP (and HTTPS) as source and destination
// protocols. If specified, the XrdClHttp client plugin must be available
// for the transfer operations to succeed.
static const int OpAllowHttp = 'A';
static const int DoAllowHttp = 0x2000000; // --allow-http

// Call Config with the parameters passed to main() to fill out this object. If
// the method returns then no errors have been found. Otherwise, it exits.
// The following options may be passed (largely to support legacy stuff):
//
static const int    opt1Src     = 0x00000001; // Only one source is allowed
static const int    optNoXtnd   = 0x00000002; // Do not index source directories
static const int    optRmtRec   = 0x00000004; // Allow remote recursive copy
static const int    optNoStdIn  = 0x00000008; // Disallow '-' as src for stdin
static const int    optNoLclCp  = 0x00000010; // Disallow local/local copy

             void   Config(int argc, char **argv, int Opts=0);

// Method to check for setting
//
inline       int    Want(uint64_t What) {return (OpSpec & What) != 0;}

                    XrdCpConfig(const char *pgname);
                   ~XrdCpConfig();

private:
             int    a2i(const char *item, int *val, int minv, int maxv=-1);
             int    a2l(const char *item, long long *val,
                        long long minv, long long maxv=-1);
             int    a2t(const char *item, int *val, int minv, int maxv=-1);
             int    a2x(const char *Val, char *Buff, int Vlen);
             int    a2z(const char *item, long long *val,
                                          long long minv, long long maxv=-1);
             int    defCks(const char *opval);
             int    defOpq(const char *theOp);
             int    defOpt(const char *theOp, const char *theArg);
             void   defPxy(const char *opval);
       const char  *Human(long long Val, char *Buff, int Blen);
             int    Legacy(int oIndex);
             int    Legacy(const char *theOp, const char *theArg);
             void   License();
       const char  *OpName();
             void   ProcFile(const char *fname);
             void   Usage(int rc=0);

      static void   toLower( char cstr[] )
      {
        for( int i = 0; cstr[i]; ++i )
          cstr[i] = tolower( cstr[i] );
      }


       const char  *PName;
             int    Opts;
             int    Argc;
             char **Argv;
           defVar  *intDend;
           defVar  *strDend;

static const  char   *opLetters;
static struct option  opVec[];

static const int    dfltSrcs   = 12;

       XrdCpFile   *pFile;
       XrdCpFile   *pLast;
       XrdCpFile   *pPrev;
       char        *inFile;
       char       **parmVal;
       int          parmCnt;
       int          isLcl;
};
#endif
