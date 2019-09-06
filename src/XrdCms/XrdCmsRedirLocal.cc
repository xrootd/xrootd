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

#include <XrdCms/XrdCmsRedirLocal.hh>

//------------------------------------------------------------------------------
//! Necessary implementation for XRootD to get the Plug-in
//------------------------------------------------------------------------------
extern "C" XrdCmsClient *XrdCmsGetClient(XrdSysLogger *Logger, int opMode,
                                         int myPort, XrdOss *theSS) {
  XrdCmsRedirLocal *plugin = new XrdCmsRedirLocal(Logger, opMode, myPort, theSS);
  return plugin;
}

//------------------------------------------------------------------------------
//! Constructor
//------------------------------------------------------------------------------
XrdCmsRedirLocal::XrdCmsRedirLocal(XrdSysLogger *Logger, int opMode, int myPort,
                         XrdOss *theSS) : XrdCmsClient(amLocal) {
  nativeCmsFinder = new XrdCmsFinderRMT(Logger, opMode, myPort);
  this->theSS = theSS;
  readOnlyredirect = false;
}
//------------------------------------------------------------------------------
//! Destructor
//------------------------------------------------------------------------------
XrdCmsRedirLocal::~XrdCmsRedirLocal() { delete nativeCmsFinder; }

//------------------------------------------------------------------------------
//! Configure the nativeCmsFinder
//------------------------------------------------------------------------------
int XrdCmsRedirLocal::Configure(const char *cfn, char *Parms, XrdOucEnv *EnvInfo) {
  loadConfig(cfn);
  if (nativeCmsFinder)
    return nativeCmsFinder->Configure(cfn, Parms, EnvInfo);
  return 0;
}

void XrdCmsRedirLocal::loadConfig(const char *filename) {
  XrdOucStream Config;
  int cfgFD;
  char *word;

  if ((cfgFD = open(filename, O_RDONLY, 0)) < 0) {
    return;
  }
  Config.Attach(cfgFD);
  while ((word = Config.GetFirstWord(true))) { //get word in lower case
    // search for readonlyredirect,
    // which only allows read calls to be redirected to local
    if (strcmp(word, "XrdCmsRedirLocal.readonlyredirect") == 0){
      // get next word in lower case
      std::string readWord = std::string(Config.GetWord(true));//to lower case
      if(readWord.find("true") != string::npos){
        readOnlyredirect = true;
      }
      else {
        readOnlyredirect = false;
      }
    }
  }
  Config.Close();
}

//------------------------------------------------------------------------------
//! Preconditions:
//! Client Protocol Version is >= 784
//! Flag is one of: SFS_O_RDONLY, SFS_O_RDWR, SFS_O_WRONLY, SFS_O_CREAT, SFS_O_TRUNC
//! Locate the file, get Client IP and target IP.
//! 1) If both are private, redirect to local does apply.
//!    set ErrInfo of param Resp and return SFS_REDIRECT.
//! 2) Not both are private, redirect to local does NOT apply.
//!    return nativeCmsFinder->Locate, for normal redirection procedure
//!
//! @Param Resp: Either set manually here or passed to nativeCmsFinder->Locate
//! @Param path: The path of the file, passed to nativeCmsFinder->Locate
//! @Param flags: The open flags, passed to nativeCmsFinder->Locate
//! @Param EnvInfo: Contains the secEnv, which contains the addressInfo of the
//!                 Client. Checked to see if redirect to local conditions apply
//------------------------------------------------------------------------------
int XrdCmsRedirLocal::Locate(XrdOucErrInfo &Resp, const char *path, int flags,
                        XrdOucEnv *EnvInfo) {
  int rcode = 0;
  if (nativeCmsFinder) {
    // get regular target host
    rcode = nativeCmsFinder->Locate(Resp, path, flags, EnvInfo);
    // define target host from locate result
    XrdNetAddr target(-1); // port is necessary, but can be any
    target.Set(Resp.getErrText());
    // does the target host have a private IP?
    if (!target.isPrivate())
      return rcode;

    // does the client host have a private IP?
    if (!EnvInfo->secEnv()->addrInfo->isPrivate())
      return rcode;

    // get client url redirect capability
    int urlRedirSupport = Resp.getUCap();
    urlRedirSupport &= XrdOucEI::uUrlOK;
    if (!urlRedirSupport)
      return rcode;

    // get client localredirect capability
    int clientLRedirSupport = Resp.getUCap();
    clientLRedirSupport &= XrdOucEI::uLclF;
    if (!clientLRedirSupport)
      return rcode;

    // only allow simple (but most prominent) operations to avoid complications
    // RDONLY, WRONLY, RDWR, CREAT, TRUNC are allowed
    if (flags > 0x202)
      return rcode;
    // always use native function if readOnlyredirect is configured and a
    // non readonly flag is passed
    if (readOnlyredirect && !(flags == SFS_O_RDONLY))
      return rcode;

    // passed all checks, now to actual business
    // build a buffer with a total acceptable buffer length,
    // which must have a larger capacity than localroot and filename concatenated
    int rc = 0;
    int maxPathLength = 4096;
    char *buff = new char[maxPathLength];
    // prepend oss.localroot
    const char *ppath = theSS->Lfn2Pfn(path, buff, maxPathLength, rc);
    // set info which will be sent to client
    Resp.setErrInfo(-1, ppath);
    delete[] buff;
    return SFS_REDIRECT;
  }
  return rcode;
}

//------------------------------------------------------------------------------
//! Space
//! Calls nativeCmsFinder->Space
//------------------------------------------------------------------------------
int XrdCmsRedirLocal::Space(XrdOucErrInfo &Resp, const char *path,
                       XrdOucEnv *EnvInfo) {
  if (nativeCmsFinder)
    return nativeCmsFinder->Space(Resp, path, EnvInfo);
  return 0;
}

XrdVERSIONINFO(XrdCmsGetClient, XrdCmsRedirLocal);
