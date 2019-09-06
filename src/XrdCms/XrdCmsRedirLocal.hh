//------------------------------------------------------------------------------
// Copyright (c) 2019 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
// Author: Paul-Niklas Kramp <p.n.kramp@gsi.de>
//         Jan Knedlik <j.knedlik@gsi.de>
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------


/* README:
   Options for xrootd config
   - Enable:
       - Enable with ofs.cmslib libXrdCmsRedirectLocal.so
   - allow only readonly request to be redirected to local, default is false
       - XrdCmsRedirLocal.readonlyredirect true
*/

#ifndef XRDCMSREDIRPLUGIN_HH_
#define XRDCMSREDIRPLUGIN_HH_
#include <XrdCms/XrdCmsFinder.hh>
#include <XrdCms/XrdCmsClient.hh>
#include <XrdNet/XrdNetAddr.hh>
#include <XrdOss/XrdOss.hh>
#include <XrdOuc/XrdOucEnv.hh>
#include <XrdSec/XrdSecEntity.hh>
#include <XrdSfs/XrdSfsInterface.hh>
#include <XrdOuc/XrdOucStream.hh>
#include <XrdOuc/XrdOucString.hh>
#include <XrdVersion.hh>
#include <string>
#include <fcntl.h>

class XrdCmsRedirLocal : public XrdCmsClient {
public:
  XrdCmsRedirLocal(XrdSysLogger *Logger, int opMode, int myPort, XrdOss *theSS);
  ~XrdCmsRedirLocal();
  int Configure(const char *cfn, char *Parms, XrdOucEnv *EnvInfo);
  void loadConfig(const char *filename);
  int Locate(XrdOucErrInfo &Resp, const char *path, int flags,
             XrdOucEnv *EnvInfo);

  int Space(XrdOucErrInfo &Resp, const char *path, XrdOucEnv *EnvInfo);
  void Added(const char *path, int Pend = 0) {
    nativeCmsFinder->Added(path, Pend);
  }
  int Forward(XrdOucErrInfo &Resp, const char *cmd, const char *arg1 = 0,
              const char *arg2 = 0, XrdOucEnv *Env1 = 0, XrdOucEnv *Env2 = 0) {
    return nativeCmsFinder->Forward(Resp, cmd, arg1, arg2, Env1, Env2);
  }
  int isRemote() { return nativeCmsFinder->isRemote(); }
  XrdOucTList *Managers() { return nativeCmsFinder->Managers(); }
  int Prepare(XrdOucErrInfo &Resp, XrdSfsPrep &pargs, XrdOucEnv *Info = 0) {
    return nativeCmsFinder->Prepare(Resp, pargs, Info);
  }
  void Removed(const char *path) { return nativeCmsFinder->Removed(path); }
  void Resume(int Perm = 1) { nativeCmsFinder->Resume(Perm); }
  void Suspend(int Perm = 1) { nativeCmsFinder->Suspend(Perm); }
  int Resource(int n) { return nativeCmsFinder->Resource(n); }
  int Reserve(int n = 1) { return nativeCmsFinder->Reserve(n); }
  int Release(int n = 1) { return nativeCmsFinder->Release(n); }

  //---------------------------------------------------------------------------
  //! used to forward requests to CmsFinder with regular implementation
  //---------------------------------------------------------------------------
  XrdCmsClient *nativeCmsFinder;
  XrdOss *theSS;
  bool readOnlyredirect;
};

#endif // XRDCMSREDIRPLUGIN_HH_
