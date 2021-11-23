#ifndef __FRMADMIN__HH
#define __FRMADMIN__HH
/******************************************************************************/
/*                                                                            */
/*                        X r d F r m A d m i n . h h                         */
/*                                                                            */
/* (c) 2009 by the Board of Trustees of the Leland Stanford, Jr., University  */
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
  
#include <cstdlib>
#include <sys/types.h>

#include "XrdCks/XrdCksData.hh"
#include "XrdOuc/XrdOucNSWalk.hh"

class  XrdFrcProxy;
class  XrdFrmFileset;
class  XrdOucArgs;
class  XrdOucTList;

class XrdFrmAdmin
{
public:

int  Audit();

int  Chksum();

int  Copy();

int  Create();

int  Find();

int  Help();

int  MakeLF();

int  Mark();

int  Mmap();

int  Mv();

int  Pin();

int  Query();

int  Quit() {exit(finalRC); return 0;}

int  Reloc();

int  Remove();

int  Rename();

void setArgs(int argc, char **argv);

void setArgs(char *argv);

int  xeqArgs(char *Cmd);

     XrdFrmAdmin() : frmProxy(0), frmProxz(0), finalRC(0) {}
    ~XrdFrmAdmin() {}

private:
int  AuditNameNB(XrdFrmFileset *sP);
int  AuditNameNF(XrdFrmFileset *sP);
int  AuditNameNL(XrdFrmFileset *sP);
int  AuditNames();
int  AuditNameXA(XrdFrmFileset *sP);
int  AuditRemove(XrdFrmFileset *sP);
int  AuditSpace();
int  AuditSpaceAX(const char *Path);
int  AuditSpaceAXDB(const char *Path);
int  AuditSpaceAXDC(const char *Path, XrdOucNSWalk::NSEnt *nP);
int  AuditSpaceAXDL(int dorm, const char *Path, const char *Dest);
int  AuditSpaceXA(const char *Space, const char *Path);
int  AuditSpaceXA(XrdFrmFileset *sP);
int  AuditUsage();
int  AuditUsage(char *Space);
int  AuditUsageAX(const char *Path);
int  AuditUsageXA(const char *Path, const char *Space);
int  isXA(XrdOucNSWalk::NSEnt *nP);

int  ChksumList( const char *Lfn, const char *Pfn);
void ChksumPrint(const char *Lfn, int rc);

int  FindFail(XrdOucArgs &Spec);
int  FindMmap(XrdOucArgs &Spec);
int  FindNocs(XrdOucArgs &Spec);
int  FindPins(XrdOucArgs &Spec);
int  FindPins(XrdFrmFileset *sP);
int  FindUnmi(XrdOucArgs &Spec);

int  Abbrev(const char *Spec, const char *Word, int minLen);

void ConfigProxy();

void Emsg(const char *tx1, const char *tx2=0, const char *tx3=0,
                           const char *tx4=0, const char *tx5=0);
void Emsg(int Enum,        const char *tx2=0, const char *tx3=0,
                           const char *tx4=0, const char *tx5=0);
void Msg(const char *tx1,  const char *tx2=0, const char *tx3=0,
                           const char *tx4=0, const char *tx5=0);

int          Parse(const char *What, XrdOucArgs &Spec, const char **Reqs);
int          ParseKeep(const char *What, const char *kTime);
int          ParseOwner(const char *What, char *Uname);
XrdOucTList *ParseSpace(char *Space, char **Path);
int          ParseType(const char *What, char *Type);

char ckAttr(int What, const char *Lfn, char *Pfn, int Pfnsz);
int  mkLock(const char *Lfn);
int  mkFile(int What, const char *Path, const char *Data=0, int Dlen=0);
int  mkMark(const char *Lfn);
int  mkMmap(const char *Lfn);
int  mkPin(const char *Lfn);
char mkStat(int What, const char *Lfn, char *Pfn, int Pfnsz);

// For mkFile and mkStat the following options may be passed via What
//
static const int isPFN= 0x0001; // Filename is actual physical name
static const int mkLF = 0x0002; // Make lock file or copy attribute
static const int mkMF = 0x0004; // Make mmap file or mmap attribute
static const int mkPF = 0x0008; // Make pin  file or pin  attribute

int  QueryPfn(XrdOucArgs &Spec);
int  QueryRfn(XrdOucArgs &Spec);
int  QuerySpace(XrdOucArgs &Spec);
int  QuerySpace(const char *Pfn, char *Lnk=0, int Lsz=0);
int  QueryUsage(XrdOucArgs &Spec);
int  QueryXfrQ(XrdOucArgs &Spec);

int  Reloc(char *srcLfn, char *Space);
int  RelocCP(const char *srcpfn, const char *trgpfn, off_t srcSz);
int  RelocWR(const char *outFn,  int oFD, char *Buff, size_t BLen, off_t Boff);

int  Unlink(const char *Path);
int  UnlinkDir(const char *Path, const char *lclPath);
int  UnlinkDir(XrdOucNSWalk::NSEnt *&nP, XrdOucNSWalk::NSEnt *&dP);
int  UnlinkFile(const char *lclPath);

int  VerifyAll(char *path);
char VerifyMP(const char *func, const char *path);

static const char *AuditHelp;
static const char *ChksumHelp;
static const char *FindHelp;
static const char *HelpHelp;
static const char *MakeLFHelp; // runOld
static const char *MarkHelp;
static const char *MmapHelp;
static const char *MvHelp;
static const char *PinHelp;
static const char *QueryHelp;
static const char *RelocHelp;
static const char *RemoveHelp;

// Frm agent/proxy control
//
XrdFrcProxy *frmProxy;
int          frmProxz;

// Command control
//
char    **ArgV;
char     *ArgS;
int       ArgC;

// The following are common variables for audit functions
//
long long numBytes;
long long numBLost;
int       numDirs;
int       numFiles;
int       numProb;
int       numFix;
int       finalRC;

// Checksum control area
//
XrdCksData     CksData;

// Options from the command
//
struct {char   All;
        char   Echo;
        char   Erase;
        char   Fix;
        char   Force;
        char   Keep;
        char   ktAlways;
        char   ktIdle;
        char   Local;
        char   MPType;
        char   Recurse;
        char   Verbose;
        char  *Args[2];
        uid_t  Uid;
        gid_t  Gid;
        time_t KeepTime;
       } Opt;
};
namespace XrdFrm
{
extern XrdFrmAdmin Admin;
}
#endif
