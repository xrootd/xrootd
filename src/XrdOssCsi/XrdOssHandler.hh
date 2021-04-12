#ifndef _XRDOSSHANDLER_H
#define _XRDOSSHANDLER_H
/******************************************************************************/
/*                                                                            */
/*                   X r d O s s H a n d l e r . h h                          */
/*                                                                            */
/* (C) Copyright 2020 CERN.                                                   */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* In applying this licence, CERN does not waive the privileges and           */
/* immunities granted to it by virtue of its status as an Intergovernmental   */
/* Organization or submit itself to any jurisdiction.                         */
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

#include "XrdOss/XrdOss.hh"

// XrdOssHandler
//
// Chain-of-responsibility pattern
//

class XrdOssDFHandler : public XrdOssDF {
public:
                // Directory oriented methods
virtual int     Opendir(const char *dir_path, XrdOucEnv &Env) /* override */ { return successor_->Opendir(dir_path, Env); }
virtual int     Readdir(char *buff, int blen) /* override */               { return successor_->Readdir(buff, blen); }
virtual int     StatRet(struct stat *buff) /* override */             { return successor_->StatRet(buff); }

                // File oriented methods
virtual int     Fchmod(mode_t Mode) /* override */                     { return successor_->Fchmod(Mode); }
virtual void    Flush() /* override */                            { successor_->Flush(); }
virtual int     Fstat(struct stat *buff) /* override */               { return successor_->Fstat(buff); }
virtual int     Fsync() /* override */                            { return successor_->Fsync(); }
virtual int     Fsync(XrdSfsAio *aiop) /* override */                 { return successor_->Fsync(aiop); }
virtual int     Ftruncate(unsigned long long flen) /* override */      { return successor_->Ftruncate(flen); }
virtual int     getFD() /* override */                            { return successor_->getFD(); }
virtual off_t   getMmap(void **addr) /* override */                   { return successor_->getMmap(addr); }
virtual int     isCompressed(char *cxidp=0) /* override */        { return successor_->isCompressed(cxidp); }
virtual int     Open(const char *path, int Oflag, mode_t Mode, XrdOucEnv &Env) /* override */ { return successor_->Open(path, Oflag, Mode, Env); }
virtual ssize_t pgRead (void *buffer, off_t offset, size_t rdlen, uint32_t *csvec, uint64_t opts) /* override */ { return successor_->pgRead(buffer, offset, rdlen, csvec, opts); }
virtual int     pgRead (XrdSfsAio *aioparm, uint64_t opts) /* override */ { return successor_->pgRead(aioparm, opts); }
virtual ssize_t pgWrite(void *buffer, off_t offset, size_t wrlen, uint32_t *csvec, uint64_t opts) /* override */ { return successor_->pgWrite(buffer, offset, wrlen, csvec, opts); }
virtual int     pgWrite(XrdSfsAio *aioparm, uint64_t opts) /* override */ { return successor_->pgWrite(aioparm, opts); }
virtual ssize_t Read(off_t offset, size_t blen) /* override */                { return successor_->Read(offset, blen); }
virtual ssize_t Read(void *buff, off_t offset, size_t blen) /* override */        { return successor_->Read(buff, offset, blen); }
virtual int     Read(XrdSfsAio *aiop) /* override */              { return successor_->Read(aiop); }
virtual ssize_t ReadRaw(void *buff, off_t offset, size_t blen) /* override */     { return successor_->ReadRaw(buff, offset, blen); }
virtual ssize_t ReadV(XrdOucIOVec *readV, int n) /* override */ { return successor_->ReadV(readV, n); }
virtual ssize_t Write(const void *buff, off_t offset, size_t blen) /* override */ { return successor_->Write(buff, offset, blen); }
virtual int     Write(XrdSfsAio *aiop) /* override */             { return successor_->Write(aiop); }
virtual ssize_t WriteV(XrdOucIOVec *writeV, int n) /* override */ { return successor_->WriteV(writeV, n); }

                // Methods common to both
virtual int     Close(long long *retsz=0) /* override */ { return successor_->Close(retsz); }
virtual int     Fctl(int cmd, int alen, const char *args, char **resp=0) /* override */ { return successor_->Fctl(cmd, alen, args, resp); }
virtual const char     *getTID() /* override */ { return successor_->getTID(); }

                XrdOssDFHandler(XrdOssDF *successor) : XrdOssDF(successor->getTID(), successor->DFType(), successor->getFD()), successor_(successor) { }
virtual        ~XrdOssDFHandler() { delete successor_; }

protected:
  XrdOssDF *successor_;
};

class XrdOssHandler : public XrdOss {
public:
// derived class must provide its own
// virtual XrdOssDF *newDir(const char *tident)=0;
// virtual XrdOssDF *newFile(const char *tident)=0;

virtual int       Chmod(const char *path, mode_t mode, XrdOucEnv *envP=0) /* override */ { return successor_->Chmod(path, mode, envP); }

virtual void      Connect(XrdOucEnv &env) /* override */ { successor_->Connect(env); }

virtual int       Create(const char *tident, const char *path, mode_t access_mode,
                      XrdOucEnv &env, int Opts=0) /* override */ { return successor_->Create(tident, path, access_mode, env, Opts); }

virtual void      Disc(XrdOucEnv &env) /* override */ { successor_->Disc(env); }
virtual void      EnvInfo(XrdOucEnv *envP) /* override */ { successor_->EnvInfo(envP); }
virtual uint64_t  Features() /* override */ { return successor_->Features(); }
virtual int       FSctl(int cmd, int alen, const char *args, char **resp=0) /* override */ { return successor_->FSctl(cmd, alen, args, resp); }

// derived class must provide its own
// virtual int       Init(XrdSysLogger *, const char *)=0;
// virtual int       Init(XrdSysLogger *lp, const char *cfn, XrdOucEnv *envP) {return Init(lp, cfn);}
virtual int       Mkdir(const char *path, mode_t mode, int mkpath=0, XrdOucEnv *envP=0) /* override */ { return successor_->Mkdir(path, mode, mkpath, envP); }

virtual int       Reloc(const char *tident, const char *path,
                     const char *cgName, const char *anchor=0) /* override */ { return successor_->Reloc(tident, path, cgName, anchor); }

virtual int       Remdir(const char *path, int Opts=0, XrdOucEnv *eP=0) /* override */ { return successor_->Remdir(path, Opts, eP); }
virtual int       Rename(const char *oldname, const char *newname,
                      XrdOucEnv  *old_env=0, XrdOucEnv  *new_env=0) /* override */ { return successor_->Rename(oldname, newname, old_env, new_env); }
virtual int       Stat(const char *path, struct stat *buff, int opts=0,
                    XrdOucEnv  *EnvP=0) /* override */ { return successor_->Stat(path, buff, opts, EnvP); }

virtual int       Stats(char *bp, int bl) /* override */ { return successor_->Stats(bp, bl); }

                  // Specialized stat type function (none supported by default)
virtual int       StatFS(const char *path, char *buff, int &blen, XrdOucEnv *eP=0) /* override */ { return successor_->StatFS(path, buff, blen, eP); }
virtual int       StatLS(XrdOucEnv &env, const char *cgrp, char *buff, int &blen) /* override */ { return successor_->StatLS(env, cgrp, buff, blen); }
virtual int       StatPF(const char *path, struct stat *buff, int opts) /* override */ { return successor_->StatPF(path, buff, opts); }
virtual int       StatPF(const char *path, struct stat *buff) /* override */ { return successor_->StatPF(path, buff); }
virtual int       StatVS(XrdOssVSInfo *sP, const char *sname=0, int updt=0) /* override */ { return successor_->StatVS(sP, sname, updt); }
virtual int       StatXA(const char *path, char *buff, int &blen, XrdOucEnv *eP=0) /* override */ { return successor_->StatXA(path, buff, blen, eP); }
virtual int       StatXP(const char *path, unsigned long long &attr, XrdOucEnv *eP=0) /* override */ { return successor_->StatXP(path, attr, eP); }

virtual int       Truncate(const char *path, unsigned long long size,
                        XrdOucEnv *envP=0) /* override */ { return successor_->Truncate(path, size, envP); }
virtual int       Unlink(const char *path, int Opts=0, XrdOucEnv *eP=0) /* override */ { return successor_->Unlink(path, Opts, eP); }

                  // Default Name-to-Name Methods
virtual int       Lfn2Pfn(const char *Path, char *buff, int blen) /* override */ { return successor_->Lfn2Pfn(Path, buff, blen); }

virtual
const char       *Lfn2Pfn(const char *Path, char *buff, int blen, int &rc) /* override */ { return successor_->Lfn2Pfn(Path, buff, blen, rc); }

                XrdOssHandler(XrdOss *successor) : successor_(successor) { }
virtual        ~XrdOssHandler() { }

protected:
  XrdOss *successor_;
};
#endif
