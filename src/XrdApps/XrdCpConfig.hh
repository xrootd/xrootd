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

       defVar      *intDefs;       // -> -DI settings
       defVar      *strDefs;       // -> -DS settings
       const char  *dstOpq;        // -> -OD setting (dest opaque)
       const char  *srcOpq;        // -> -OS setting (src  opaque)
       const char  *Pgm;           // -> Program name
        long long   xRate;         // -xrate value in bytes/sec   (0 if not set)
             int    Parallel;      // Number of simultaneous copy ops (1 to 4)
             char  *pHost;         // -> SOCKS4 proxy hname       (0 if none)
             int    pPort;         //    SOCKS4 proxy port
             int    OpSpec;        // Bit mask of set options     (see Doxxxx)
             int    Dlvl;          // Debug level                 (0 to 3)
             int    nSrcs;         // Number of sources wanted    (dflt 1)
             int    nStrm;         // Number of streams wanted    (dflt 1)
             int    Retry;         // Max times to retry connects (<0->use dflt)
             int    Verbose;       // True if --verbose specified
             int    CksLen;        // Binary length of checksum, if any

             int    numFiles;      // Number of source files
       long long    totBytes;      // Total number of bytes for local files

XrdCksData          CksData;       // Checksum information
XrdCks             *CksMan;        // -> Checksum manager
XrdCksCalc         *CksObj;        // -> Cks computation object   (0 if no cks)
const char         *CksVal;        // -> Cks argument (0 if none)

XrdCpFile          *srcFile;       // List of source files
XrdCpFile          *dstFile;       // The destination for the copy

static XrdSysError *Log;           // -> Error message object

static const int    OpCksum    =  'C';        // -adler -MD5 legacy -> DoCksrc
static const int    DoCksrc    =  0x00000001; // --cksum <type>:source
static const int    DoCksum    =  0x00000002; // --cksum <type>
static const int    DoCkprt    =  0x00000004; // --cksum <type>:print

static const int    OpCoerce   =  'F';
static const int    DoCoerce   =  0x00000008; // -F | --coerce

static const int    OpDebug    =  'd';
static const int    DoDebug    =  0x00000010; // -d | --debug <val>

static const int    OpForce    =  'f';
static const int    DoForce    =  0x00000020; // -f | --force

static const int    OpHelp     =  'h';
static const int    DoHelp     =  0x00000040; // -h | --help

static const int    OpIfile    =  'I';
static const int    DoIfile    =  0x00100000; // -I | --infiles

static const int    OpLicense  =  'H';        // -H | --license

//atic const int    DoXyzzy    =  0x00000080; // Reserved

static const int    OpNoPbar   =  'N';
static const int    DoNoPbar   =  0x00000100; // -N | --nopbar | -np {legacy}

static const int    OpPosc     =  'P';
static const int    DoPosc     =  0x00000200; // -P | --posc

static const int    OpProxy    =  'D';
static const int    DoProxy    =  0x00000400; // -D | --proxy

static const int    OpRecurse  =  'r';
static const int    OpRecursv  =  'R';
static const int    DoRecurse  =  0x00000800; // -r | --recursive | -R {legacy}

static const int    OpRetry    =  't';
static const int    DoRetry    =  0x00001000; // -t | --retry

static const int    OpServer   =  0x03;
static const int    DoServer   =  0x00002000; //      --server

static const int    OpSilent   =  's';
static const int    DoSilent   =  0x00004000; // -s | --silent

static const int    OpSources  =  'y';
static const int    DoSources  =  0x00008000; // -y | --sources

static const int    OpStreams  =  'S';
static const int    DoStreams  =  0x00010000; // -S | --streams

static const int    OpTpc      =  'T';
static const int    DoTpc      =  0x00020000; // -T | --tpc {first | only}
static const int    DoTpcOnly  =  0x00100000; // -T | --tpc          only

static const int    OpVerbose  =  'v';
static const int    DoVerbose  =  0x00040000; // -v | --verbose

static const int    OpVersion  =  'V';        // -V | --version

static const int    OpXrate    =  'X';
static const int    DoXrate    =  0x00080000; // -X | --xrate

static const int    OpParallel =  0x04;
static const int    DoParallel =  0x00200000; //      --parallel

static const int    OpDynaSrc  =  'Z';
static const int    DoDynaSrc  =  0x00200000; //      --dynamic-src

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
inline       int    Want(int What) {return (OpSpec & What) != 0;}

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
