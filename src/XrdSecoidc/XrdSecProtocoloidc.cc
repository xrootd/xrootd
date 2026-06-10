/******************************************************************************/
/*                                                                            */
/*                 X r d S e c P r o t o c o l o i d c . c c                  */
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
#include <vector>

#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "XrdVersion.hh"

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucOIDC.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSec/XrdSecInterface.hh"

#ifndef EAUTH
#define EAUTH EBADE
#endif

XrdVERSIONINFO(XrdSecProtocoloidcObject,secoidc);

namespace
{

XrdSecCredentials *Fatal(XrdOucErrInfo *erp, const char *eMsg, int rc,
                           bool hdr=true)
{
   if (!erp) std::cerr <<(hdr ? "Secoidc: " : "") <<eMsg <<"\n" <<std::flush;
      else {const char *eVec[2] = {(hdr ? "Secoidc: " : ""), eMsg};
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
   std::cerr << "Secoidc: " << msg << "\n" << std::flush;
}

} // namespace

class XrdSecProtocoloidc : public XrdSecProtocol
{
public:
       int                Authenticate  (XrdSecCredentials *cred,
                                         XrdSecParameters **parms,
                                         XrdOucErrInfo     *einfo=0);

       void               Delete() {delete this;}

       XrdSecCredentials *getCredentials(XrdSecParameters *parms=0,
                                         XrdOucErrInfo    *einfo=0);

       bool               needTLS() {return true;}

       XrdSecProtocoloidc(const char *parms, XrdOucErrInfo *erp, bool &aOK);
       XrdSecProtocoloidc(const char *hname, XrdNetAddrInfo &endPoint)
                        : XrdSecProtocol("oidc"), maxTSize(8192)
                        {Entity.host = strdup(hname);
                         Entity.name = strdup("anon");
                         Entity.addrInfo = &endPoint;
                        }

      ~XrdSecProtocoloidc() {if (Entity.host)  free(Entity.host);
                            if (Entity.name)  free(Entity.name);
                            if (Entity.creds) free(Entity.creds);
                           }

static const int oidcVersion = 0;

private:
XrdSecCredentials *findToken(XrdOucErrInfo *erp, bool &isbad);
XrdSecCredentials *readToken(XrdOucErrInfo *erp, const char *path, bool &isbad);
XrdSecCredentials *retToken(const char *tok, int tsz);

int                 maxTSize;
};

XrdSecProtocoloidc::XrdSecProtocoloidc(const char *parms, XrdOucErrInfo *erp,
                                       bool &aOK)
                                      : XrdSecProtocol("oidc"), maxTSize(8192)
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

XrdSecCredentials *XrdSecProtocoloidc::retToken(const char *tok, int tsz)
{
   int bsz = 5 + tsz + 1;
   char *bp = (char *)malloc(bsz);
   if (!bp) return 0;
   strcpy(bp, "oidc");
   memcpy(bp+5, tok, tsz);
   bp[5+tsz] = '\0';
   return std::make_unique<XrdSecCredentials>(bp, bsz).release();
}

XrdSecCredentials *XrdSecProtocoloidc::readToken(XrdOucErrInfo *erp,
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
   clientDebugLog(std::string("Using OIDC token file '") + path + "'");

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
   int tlen = 0;
   const char *tok = XrdOucOIDC::StripToken(buff.data(), tlen);
   XrdSecCredentials *ret = (tok ? retToken(tok, tlen) : nullptr);
   if (!ret) return Fatal(erp, "Token value malformed.", EINVAL);
   isbad = true;
   return ret;
}

XrdSecCredentials *XrdSecProtocoloidc::findToken(XrdOucErrInfo *erp, bool &isbad)
{
   static const char *loc[] = {
     "BEARER_TOKEN", "BEARER_TOKEN_FILE",
     "XDG_RUNTIME_DIR", "/tmp/bt_u%d"
   };

   for (size_t i = 0; i < sizeof(loc)/sizeof(loc[0]); i++)
       {std::string key(loc[i]);
        if (!key.empty() && key[0] == '/')
           {char tokPath[MAXPATHLEN+8];
            snprintf(tokPath, sizeof(tokPath), key.c_str(), int(geteuid()));
            XrdSecCredentials *r = readToken(erp, tokPath, isbad);
            if (r || isbad) return r;
            continue;
           }

        const char *env = getenv(key.c_str());
        if (!env || !(*env)) continue;

        if (hasSuffix(key, "_DIR"))
           {char tokPath[MAXPATHLEN+8];
            snprintf(tokPath, sizeof(tokPath), "%s/bt_u%d", env, int(geteuid()));
            XrdSecCredentials *r = readToken(erp, tokPath, isbad);
            if (r || isbad) return r;
            continue;
           }

        if (hasSuffix(key, "_FILE"))
           {clientDebugLog(std::string("Trying OIDC token file from ") + key
                         + "='" + env + "'");
            XrdSecCredentials *r = readToken(erp, env, isbad);
            if (r || isbad) return r;
            continue;
           }

        int envLen = static_cast<int>(strlen(env));
        if (envLen > maxTSize) continue;
        int tlen = 0;
        const char *tok = XrdOucOIDC::StripToken(env, tlen, envLen);
        if (tok) return retToken(tok, tlen);
       }

   isbad = false;
   return 0;
}

XrdSecCredentials *XrdSecProtocoloidc::getCredentials(XrdSecParameters *parms,
                                                      XrdOucErrInfo *error)
{
   (void)parms;
   bool isbad = false;
   XrdSecCredentials *resp = findToken(error, isbad);
   if (resp || isbad) return resp;
   Fatal(error, "No OIDC token found in environment/token-file locations.",
         ENOPROTOOPT);
   return 0;
}

int XrdSecProtocoloidc::Authenticate(XrdSecCredentials *cred,
                                     XrdSecParameters **parms,
                                     XrdOucErrInfo *erp)
{
   (void)parms;
   if (!cred || cred->size <= 6 || !cred->buffer)
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

   if (strcmp(cred->buffer, "oidc"))
      {Fatal(erp, "Authentication protocol id mismatch.", EINVAL);
       return -1;
      }

   int tokRawLen = cred->size - 5;
   const char *tok = cred->buffer + 5;
   if (tokRawLen <= 0 || !*tok)
      {Fatal(erp, "Null token.", EINVAL);
       return -1;
      }
   int tlen = 0;
   tok = XrdOucOIDC::StripToken(tok, tlen, tokRawLen);
   if (!tok || tlen <= 0)
      {Fatal(erp, "Token value malformed.", EINVAL);
       return -1;
      }

   std::string identity, emsg;
   std::map<std::string, std::string> entityAttrs;
   if (!XrdOucOIDC::ValidateToken(tok, identity, emsg, nullptr, &entityAttrs))
      {Fatal(erp, emsg.c_str(), EAUTH, false);
       return -1;
      }

   if (Entity.name) free(Entity.name);
   Entity.name = strdup(identity.c_str());
   strncpy(Entity.prot, "oidc", sizeof(Entity.prot));
   for (const auto &attr : entityAttrs)
      Entity.eaAPI->Add(attr.first, attr.second, true);
   return 0;
}

extern "C"
{
char *XrdSecProtocoloidcInit(const char mode, const char *parms, XrdOucErrInfo *erp)
{
   static char nilstr = 0;
   if (mode == 'c') return &nilstr;
   return XrdOucOIDC::InitSecProtocol(parms, erp);
}
}

extern "C"
{
XrdSecProtocol *XrdSecProtocoloidcObject(const char mode,
                                         const char *hostname,
                                               XrdNetAddrInfo &endPoint,
                                         const char *parms,
                                               XrdOucErrInfo *erp)
{
   if (!endPoint.isUsingTLS())
      {Fatal(erp, "security protocol 'oidc' disallowed for non-TLS connections.",
             ENOTSUP, false);
       return 0;
      }

   if (mode == 'c')
      {bool aOK = false;
       auto prot = std::make_unique<XrdSecProtocoloidc>(parms, erp, aOK);
       if (aOK) return prot.release();
       return nullptr;
      }

   return std::make_unique<XrdSecProtocoloidc>(hostname, endPoint).release();
}
}
