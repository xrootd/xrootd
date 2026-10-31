/******************************************************************************/
/*                                                                            */
/*                  X r d S e c P r o t o c o l j w t . c c                   */
/*                                                                            */
/* (c) 2026 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/******************************************************************************/

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "XrdVersion.hh"

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdSecjwt/XrdSecJWT.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSec/XrdSecInterface.hh"

#ifndef EAUTH
#define EAUTH EBADE
#endif

XrdVERSIONINFO(XrdSecProtocoljwtObject,secjwt);

namespace
{

XrdSecCredentials *Fatal(XrdOucErrInfo *erp, const char *eMsg, int rc,
                           bool hdr=true)
{
   if (!erp) std::cerr <<(hdr ? "Secjwt: " : "") <<eMsg <<"\n" <<std::flush;
      else {const char *eVec[2] = {(hdr ? "Secjwt: " : ""), eMsg};
            erp->setErrInfo(rc, eVec, 2);
           }
   return 0;
}

bool hasSuffix(const std::string &s, const char *sfx)
{
   size_t n = strlen(sfx);
   return s.size() >= n && s.compare(s.size()-n, n, sfx) == 0;
}

void clientDebugLog(const std::string &msg)
{
   const char *dbg = getenv("XrdSecDEBUG");
   if (!dbg || !*dbg) return;
   std::string v(dbg);
   for (char &c : v) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
   if (v == "0" || v == "off" || v == "false" || v == "no") return;
   std::cerr << "Secjwt: " << msg << "\n" << std::flush;
}

// "jwt" NUL, token bytes, trailing NUL.
std::string frameJwtCredential(std::string_view tok)
{
   std::string framed;
   framed.reserve(4 + tok.size() + 1);
   framed.append("jwt", 3);
   framed.push_back('\0');
   framed.append(tok);
   framed.push_back('\0');
   return framed;
}

} // namespace

class XrdSecProtocoljwt : public XrdSecProtocol
{
public:
       int                Authenticate  (XrdSecCredentials *cred,
                                         XrdSecParameters **parms,
                                         XrdOucErrInfo     *einfo=0);

       void               Delete() {delete this;}

       XrdSecCredentials *getCredentials(XrdSecParameters *parms=0,
                                         XrdOucErrInfo    *einfo=0);

       bool               needTLS() {return true;}

       XrdSecProtocoljwt(const char *parms, XrdOucErrInfo *erp, bool &aOK);
       // Server side: the size limit is the configured -maxsz, not a constant,
       // so that the framing check below agrees with the validator.
       XrdSecProtocoljwt(const char *hname, XrdNetAddrInfo &endPoint)
                        : XrdSecProtocol("jwt"),
                          maxTSize(XrdSecJWT::MaxTokenSize())
                        {// XrdSecEntity fields are C strings, in accordance with
                         // the other security plugins.
                         Entity.host = strdup(hname);
                         Entity.name = strdup("anon");
                         Entity.addrInfo = &endPoint;
                        }

      ~XrdSecProtocoljwt() {if (Entity.host)  free(Entity.host);
                            if (Entity.name)  free(Entity.name);
                            if (Entity.vorg)  free(Entity.vorg);
                            if (Entity.grps)  free(Entity.grps);
                            if (Entity.creds) free(Entity.creds);
                           }

static const int jwtVersion = 0;

private:
XrdSecCredentials *findToken(XrdOucErrInfo *erp, bool &isbad);
XrdSecCredentials *readToken(XrdOucErrInfo *erp, const char *path, bool &isbad);
XrdSecCredentials *retToken(std::string_view tok);

int                 maxTSize;
};

XrdSecProtocoljwt::XrdSecProtocoljwt(const char *parms, XrdOucErrInfo *erp,
                                       bool &aOK)
                                      : XrdSecProtocol("jwt"), maxTSize(8192)
{
   aOK = false;
   if (!parms || !(*parms))
      {Fatal(erp, "Client parameters not specified.", EINVAL);
       return;
      }

   std::string pstr(parms);
   size_t end = pstr.rfind(':');
   if (end == std::string::npos || end == 0)
      {Fatal(erp, "Malformed client parameters.", EINVAL);
       return;
      }
   size_t begin = pstr.rfind(':', end - 1);
   if (begin == std::string::npos || begin + 1 >= end)
      {Fatal(erp, "Malformed client parameters.", EINVAL);
       return;
      }
   std::string maxField = pstr.substr(begin + 1, end - begin - 1);
   char *endP = 0;
   long v = strtol(maxField.c_str(), &endP, 10);
   if (v <= 0 || v > 524288 || !endP || *endP != '\0')
      {Fatal(erp, "Invalid max token size in parameters.", EINVAL);
       return;
      }
   maxTSize = static_cast<int>(v);
   aOK = true;
}

XrdSecCredentials *XrdSecProtocoljwt::retToken(std::string_view tok)
{
   // XrdSecCredentials takes ownership of a malloc'd buffer, in accordance
   // with the other security plugins.
   std::string framed = frameJwtCredential(tok);
   char *bp = static_cast<char *>(malloc(framed.size()));
   if (!bp) return nullptr;
   memcpy(bp, framed.data(), framed.size());
   return std::make_unique<XrdSecCredentials>(bp, static_cast<int>(framed.size())).release();
}

XrdSecCredentials *XrdSecProtocoljwt::readToken(XrdOucErrInfo *erp,
                                                 const char *path, bool &isbad)
{
   isbad = true;
   int flags = O_RDONLY;
#ifdef O_NOFOLLOW
   flags |= O_NOFOLLOW;
#endif
   int fd = open(path, flags);
   if (fd < 0)
      {if (errno == ENOENT) {isbad = false; return 0;}
       return Fatal(erp, XrdSysE2T(errno), errno);
      }
   clientDebugLog(std::string("Using JWT token file '") + path + "'");

   struct stat st;
   if (fstat(fd, &st) != 0)
      {close(fd);
       return Fatal(erp, XrdSysE2T(errno), errno);
      }

   if (!S_ISREG(st.st_mode))
      {close(fd);
       return Fatal(erp, "Token path must be a regular file.", EINVAL);
      }

   if (st.st_uid != geteuid())
      {close(fd);
       return Fatal(erp, "Token file owner must match effective uid.", EACCES);
      }

   if (st.st_mode & (S_IRWXG | S_IRWXO))
      {close(fd);
       return Fatal(erp, "Token file permissions too open; require owner-only access.", EACCES);
      }

   if (st.st_size <= 0 || st.st_size > maxTSize)
      {close(fd);
       return Fatal(erp, "Token file size invalid or exceeds limit.", EINVAL);
      }

   std::vector<char> buff(static_cast<size_t>(st.st_size) + 1);
   ssize_t got = 0;
   while (got < st.st_size)
      {ssize_t rd = read(fd, buff.data() + got, st.st_size - got);
       if (rd < 0)
          {if (errno == EINTR) continue;
           close(fd);
           return Fatal(erp, "Unable to read token file.", EIO);
          }
       if (rd == 0)
          {close(fd);
           return Fatal(erp, "Unable to read token file.", EIO);
          }
       got += rd;
      }
   close(fd);

   buff[st.st_size] = 0;
   std::string_view tok = XrdSecJWT::StripToken(buff.data());
   XrdSecCredentials *ret = (!tok.empty() ? retToken(tok) : nullptr);
   if (!ret) return Fatal(erp, "Token value malformed.", EINVAL);
   isbad = true;
   return ret;
}

XrdSecCredentials *XrdSecProtocoljwt::findToken(XrdOucErrInfo *erp, bool &isbad)
{
   static const char *loc[] = {
     "BEARER_TOKEN", "BEARER_TOKEN_FILE",
     "XDG_RUNTIME_DIR", "/tmp/bt_u%d"
   };

   for (size_t i = 0; i < sizeof(loc)/sizeof(loc[0]); i++)
       {std::string key(loc[i]);
        if (!key.empty() && key[0] == '/')
           {std::string tokPath = "/tmp/bt_u" + std::to_string(static_cast<int>(geteuid()));
            XrdSecCredentials *r = readToken(erp, tokPath.c_str(), isbad);
            if (r || isbad) return r;
            continue;
           }

        const char *env = getenv(key.c_str());
        if (!env || !(*env)) continue;

        if (hasSuffix(key, "_DIR"))
           {std::string tokPath = std::string(env) + "/bt_u"
                                + std::to_string(static_cast<int>(geteuid()));
            XrdSecCredentials *r = readToken(erp, tokPath.c_str(), isbad);
            if (r || isbad) return r;
            continue;
           }

        if (hasSuffix(key, "_FILE"))
           {clientDebugLog(std::string("Trying JWT token file from ") + key
                         + "='" + env + "'");
            XrdSecCredentials *r = readToken(erp, env, isbad);
            if (r || isbad) return r;
            continue;
           }

        std::string_view envTok(env);
        if (static_cast<int>(envTok.size()) > maxTSize) continue;
        std::string_view tok = XrdSecJWT::StripToken(envTok);
        if (!tok.empty()) return retToken(tok);
       }

   isbad = false;
   return 0;
}

XrdSecCredentials *XrdSecProtocoljwt::getCredentials(XrdSecParameters *parms,
                                                      XrdOucErrInfo *error)
{
   (void)parms;
   bool isbad = false;
   XrdSecCredentials *resp = findToken(error, isbad);
   if (resp || isbad) return resp;
   Fatal(error, "No JWT token found in environment/token-file locations.",
         ENOPROTOOPT);
   return 0;
}

int XrdSecProtocoljwt::Authenticate(XrdSecCredentials *cred,
                                     XrdSecParameters **parms,
                                     XrdOucErrInfo *erp)
{
   (void)parms;
   if (!cred || cred->size <= 5 || !cred->buffer)
      {Fatal(erp, "Missing credential data.", EINVAL);
       return -1;
      }

   if (cred->size > maxTSize + 16)
      {Fatal(erp, "Credential too large.", EINVAL);
       return -1;
      }

   if (memchr(cred->buffer, '\0', cred->size) == nullptr)
      {Fatal(erp, "Credential not NUL-terminated.", EINVAL);
       return -1;
      }

   if (strcmp(cred->buffer, "jwt"))
      {Fatal(erp, "Authentication protocol id mismatch.", EINVAL);
       return -1;
      }

   const char *raw = cred->buffer + 4;
   size_t rawLen = static_cast<size_t>(cred->size - 4);
   if (const void *nul = memchr(raw, '\0', rawLen))
      rawLen = static_cast<const char *>(nul) - raw;
   if (rawLen == 0)
      {Fatal(erp, "Null token.", EINVAL);
       return -1;
      }
   std::string_view tok = XrdSecJWT::StripToken(std::string_view(raw, rawLen));
   if (tok.empty())
      {Fatal(erp, "Token value malformed.", EINVAL);
       return -1;
      }

   std::string identity, emsg;
   std::map<std::string, std::string> entityAttrs;
   if (!XrdSecJWT::ValidateToken(tok, identity, emsg, nullptr, &entityAttrs))
      {Fatal(erp, emsg.c_str(), EAUTH, false);
       return -1;
      }

   // XrdSecEntity name and prot are C strings, in accordance with the other
   // security plugins.
   if (Entity.name) free(Entity.name);
   Entity.name = strdup(identity.c_str());
   strncpy(Entity.prot, "jwt", sizeof(Entity.prot));
   // Groups travel in the attribute map only so the protocol can install
   // them on Entity.grps. They are not published as an entity attribute.
   std::string groups;
   auto git = entityAttrs.find("jwt.grps");
   if (git != entityAttrs.end())
      {groups = std::move(git->second);
       entityAttrs.erase(git);
      }
   if (Entity.grps) free(Entity.grps);
   Entity.grps = groups.empty() ? nullptr : strdup(groups.c_str());
   // jwt.vorg is the matched issuer. It is the virtual organization, not an
   // entity attribute.
   std::string vorg;
   auto vit = entityAttrs.find("jwt.vorg");
   if (vit != entityAttrs.end())
      {vorg = std::move(vit->second);
       entityAttrs.erase(vit);
      }
   if (Entity.vorg) free(Entity.vorg);
   Entity.vorg = vorg.empty() ? nullptr : strdup(vorg.c_str());
   for (const auto &attr : entityAttrs)
      Entity.eaAPI->Add(attr.first, attr.second, true);
   return 0;
}

extern "C"
{
char *XrdSecProtocoljwtInit(const char mode, const char *parms, XrdOucErrInfo *erp)
{
   static char nilstr = 0;
   if (mode == 'c') return &nilstr;
   return XrdSecJWT::InitSecProtocol(parms, erp);
}
}

extern "C"
{
XrdSecProtocol *XrdSecProtocoljwtObject(const char mode,
                                         const char *hostname,
                                               XrdNetAddrInfo &endPoint,
                                         const char *parms,
                                               XrdOucErrInfo *erp)
{
   if (!endPoint.isUsingTLS())
      {Fatal(erp, "security protocol 'jwt' disallowed for non-TLS connections.",
             ENOTSUP, false);
       return 0;
      }

   if (mode == 'c')
      {bool aOK = false;
       auto prot = std::make_unique<XrdSecProtocoljwt>(parms, erp, aOK);
       if (aOK) return prot.release();
       return nullptr;
      }

   return std::make_unique<XrdSecProtocoljwt>(hostname, endPoint).release();
}
}
