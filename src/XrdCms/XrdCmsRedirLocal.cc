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
                         XrdOss *theSS) : XrdCmsClient(amLocal),
                         nativeCmsFinder(new XrdCmsFinderRMT(Logger, opMode, myPort)),
                         readOnlyredirect(true),
                         httpRedirect(false),
                         Say(0,"cms_") {
  Say.logger(Logger);
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
  if(localroot.empty())
  {
    Say.Emsg("RedirLocal", "oss.localroot (replaced by xrdcmsredirlocal for localredirect) " \
    "and xrdcmsredirlocal.localroot are undefined, define xrdcmsredirlocal.localroot");
    return 0;
  }
  if(localroot[0] != '/')
  {
    Say.Emsg("RedirLocal", "oss.localroot or xrdcmsredirlocal.localroot needs to be an absolute path");
    return 0;
  }
  if (nativeCmsFinder)
    return nativeCmsFinder->Configure(cfn, Parms, EnvInfo);
  return 0; // means false
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
    if (strcmp(word, "xrdcmsredirlocal.readonlyredirect") == 0){
      readOnlyredirect = std::string(Config.GetWord(true)).find("true") != std::string::npos;
    }
    // search for httpredirect,
    // which allows http(s) calls to be redirected to local
    else if (strcmp(word, "xrdcmsredirlocal.httpredirect") == 0){
      httpRedirect = std::string(Config.GetWord(true)).find("true") != std::string::npos;
    }
    // search for newer localroot, overwrite given oss.localroot if defined,
    // which manually sets localroot to prepend
    else if (strcmp(word, "xrdcmsredirlocal.localroot") == 0){
      localroot = std::string(Config.GetWord(false));
    }
    // search for oss.localroot,
    // which manually sets localroot to prepend
    else if (strcmp(word, "oss.localroot") == 0 && localroot.empty()){
      localroot = std::string(Config.GetWord(false));
    }
  }
  Config.Close();
}

//------------------------------------------------------------------------------
//! Preconditions:
//! Writing must be enabled  via xrdcmsredirlocal.readonlyredirect false
//! Client has urlRedirSupport
//! Client has localredirect capability
//! Flag is one of: SFS_O_RDONLY, SFS_O_RDWR, SFS_O_WRONLY, SFS_O_CREAT, SFS_O_TRUNC
//! [Only HTTP] Flag may also be SFS_O_STAT in case of read access
//! [Only HTTP] xrdcmsredirlocal.httpRedirect is true in the config
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
    // check if path contains localroot to know if potential redirection loop
    // is happening. If yes, remove localroot from path, then rerun default
    // locate with regular path and return to client
    // localroot must be larger than 1 to avoid always being triggered by "/"
    if (localroot.size() > 1 && strncmp(path, localroot.c_str(), localroot.size()) == 0)
    {
      // now check if localhost was tried before, to make sure we're handling
      // the redirection loop
      int param = 0; // need it to get Env
      //EnvInfo->Env(param) gets already tried hosts
      if(strstr(EnvInfo->Env(param), "tried=localhost") != nullptr)
      {
        std::string newPath(path);
        // remove localroot
        newPath = "//" + newPath.substr(localroot.size());
        // get regular target host
        rcode = nativeCmsFinder->Locate(Resp, newPath.c_str(), flags, EnvInfo);
        // set new error message to full url:port//newPath
        const std::string errText { std::string(Resp.getErrText()) + ':' + std::to_string(Resp.getErrInfo()) + newPath};
        Resp.setErrInfo(0, errText.c_str());
        // now have normal redirection to dataserver at url:port
        return rcode;
      }
    }
    std::string dialect = EnvInfo->secEnv()->addrInfo->Dialect();
    // get regular target host
    rcode = nativeCmsFinder->Locate(Resp, path, flags, EnvInfo);

    // check if http redirect to local filesystem is allowed
    if (strncmp(dialect.c_str(), "http", 4) == 0 && !httpRedirect)
      return rcode;

    // define target host from locate result
    XrdNetAddr target(-1); // port is necessary, but can be any
    target.Set(Resp.getErrText());
    // does the target host have a private IP?
    if (!target.isPrivate())
      return rcode;
    // does the client host have a private IP?
    if (!EnvInfo->secEnv()->addrInfo->isPrivate())
      return rcode;

    // as we can't rely on the flags from http clients, we do not perform the below
    if (strncmp(dialect.c_str(), "http", 4) != 0)
    {
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
    }

    // http gets SFS_O_STAT flag when opening to read, instead of SFS_O_RDONLY
    // in case of http dialect and stat, we do not perform the checks below
    if (!(strncmp(dialect.c_str(), "http", 4) == 0 && flags == 0x20000000))
    {
      // only allow simple (but most prominent) operations to avoid complications
      // RDONLY, WRONLY, RDWR, CREAT, TRUNC are allowed
      if (flags > 0x202)
        return rcode;
      // always use native function if readOnlyredirect is configured and a
      // non readonly flag is passed
      if (readOnlyredirect && !(flags == SFS_O_RDONLY))
        return rcode;
    }
    // passed all checks, now to actual business
    // prepend manually configured localroot
    std::string ppath = "file://" + localroot + path;
    if (strncmp(dialect.c_str(), "http", 4) == 0)
    {
      // set info which will be sent to client
      // eliminate the resource name so it is not doubled in XrdHttpReq::Redir.
      Resp.setErrInfo(-1, ppath.substr(0, ppath.find(path)).c_str());
    }
    else{
      // set info which will be sent to client
      Resp.setErrInfo(-1, ppath.c_str());
    }
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
