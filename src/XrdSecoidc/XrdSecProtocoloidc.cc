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

#define __STDC_FORMAT_MACROS 1

#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <atomic>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <curl/curl.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/param_build.h>
#include <openssl/params.h>
#endif

#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "XrdVersion.hh"

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
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

bool hasPrefix(const char *s, const char *pfx)
{
   return s && pfx && strncmp(s, pfx, strlen(pfx)) == 0;
}

int b64Value(unsigned char c)
{
   if (c >= 'A' && c <= 'Z') return c - 'A';
   if (c >= 'a' && c <= 'z') return c - 'a' + 26;
   if (c >= '0' && c <= '9') return c - '0' + 52;
   if (c == '+') return 62;
   if (c == '/') return 63;
   return -1;
}

bool decodeBase64URL(const std::string &in, std::string &out)
{
   out.clear();
   if (in.empty()) return false;

   std::string b64 = in;
   for (char &c : b64)
      {if (c == '-') c = '+';
       else if (c == '_') c = '/';
      }
   while ((b64.size() % 4) != 0) b64.push_back('=');

   out.reserve((b64.size() / 4) * 3);
   int val = 0;
   int bits = -8;
   for (unsigned char c : b64)
      {if (isspace(c)) continue;
       if (c == '=') break;
       int d = b64Value(c);
       if (d < 0) return false;
       val = (val << 6) | d;
       bits += 6;
       if (bits >= 0)
          {out.push_back(char((val >> bits) & 0xFF));
           bits -= 8;
          }
      }
   return !out.empty();
}

std::string encodeBase64URL(const std::string &in)
{
   static const char tbl[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
   std::string out;
   out.reserve(((in.size() + 2) / 3) * 4);
   size_t i = 0;
   while (i + 2 < in.size())
      {uint32_t v = (uint32_t((unsigned char)in[i]) << 16)
                  | (uint32_t((unsigned char)in[i + 1]) << 8)
                  |  uint32_t((unsigned char)in[i + 2]);
       out.push_back(tbl[(v >> 18) & 0x3F]);
       out.push_back(tbl[(v >> 12) & 0x3F]);
       out.push_back(tbl[(v >> 6) & 0x3F]);
       out.push_back(tbl[v & 0x3F]);
       i += 3;
      }
   if (i < in.size())
      {uint32_t v = uint32_t((unsigned char)in[i]) << 16;
       out.push_back(tbl[(v >> 18) & 0x3F]);
       if (i + 1 < in.size())
          {v |= uint32_t((unsigned char)in[i + 1]) << 8;
           out.push_back(tbl[(v >> 12) & 0x3F]);
           out.push_back(tbl[(v >> 6) & 0x3F]);
          } else out.push_back(tbl[(v >> 12) & 0x3F]);
      }
   return out;
}

size_t skipWs(const std::string &s, size_t at)
{
   while (at < s.size() && isspace(static_cast<unsigned char>(s[at]))) at++;
   return at;
}

bool decodeJsonString(const std::string &json, size_t &at, std::string &out)
{
   if (at >= json.size() || json[at] != '"') return false;
   at++;
   out.clear();
   while (at < json.size())
      {char c = json[at++];
       if (c == '"') return true;
       if (c != '\\') {out.push_back(c); continue;}
       if (at >= json.size()) return false;
       char esc = json[at++];
            if (esc == '"' ) out.push_back('"');
       else if (esc == '\\') out.push_back('\\');
       else if (esc == '/' ) out.push_back('/');
       else if (esc == 'b' ) out.push_back('\b');
       else if (esc == 'f' ) out.push_back('\f');
       else if (esc == 'n' ) out.push_back('\n');
       else if (esc == 'r' ) out.push_back('\r');
       else if (esc == 't' ) out.push_back('\t');
       else if (esc == 'u')
          {// Keep parser permissive; map unicode escapes to '?' for identity use.
           if (at + 4 > json.size()) return false;
           at += 4;
           out.push_back('?');
          }
       else return false;
      }
   return false;
}

bool getStringClaim(const std::string &json, const char *claim, std::string &val)
{
   std::string key("\"");
   key += claim;
   key += "\"";
   size_t pos = 0;
   while ((pos = json.find(key, pos)) != std::string::npos)
      {size_t at = skipWs(json, pos + key.size());
       if (at >= json.size() || json[at] != ':') {pos += key.size(); continue;}
       at = skipWs(json, at + 1);
       if (at >= json.size() || json[at] != '"') {pos += key.size(); continue;}
       if (!decodeJsonString(json, at, val)) {pos += key.size(); continue;}
       if (!val.empty()) return true;
       pos += key.size();
      }
   return false;
}

// Strip whitespace and optional Bearer prefix from a length-bounded buffer.
// maxLen must be the number of usable bytes at bTok; strlen is never called.
const char *Strip(const char *bTok, int &sz, int maxLen = -1)
{
   const char *sTok = bTok;
   sz = 0;
   if (!sTok || maxLen == 0) return 0;

   // Determine safe end pointer without calling strlen on untrusted data.
   const char *endPtr;
   if (maxLen < 0)
      {// Legacy call-site that already guarantees NUL termination.
       endPtr = sTok + strlen(sTok);
      }
   else
      {// Server-side path: bound by explicit length; find NUL within range.
       endPtr = sTok + maxLen;
       const char *nul = static_cast<const char *>(memchr(sTok, '\0', maxLen));
       if (nul) endPtr = nul;
      }

   while (sTok < endPtr && isspace(static_cast<unsigned char>(*sTok))) sTok++;
   if (sTok >= endPtr) return 0;

   if ((endPtr - sTok) >= 9 && hasPrefix(sTok, "Bearer%20")) sTok += 9;
   else if ((endPtr - sTok) >= 7 && hasPrefix(sTok, "Bearer ")) sTok += 7;

   while (sTok < endPtr && isspace(static_cast<unsigned char>(*sTok))) sTok++;
   if (sTok >= endPtr) return 0;

   while (endPtr > sTok && isspace(static_cast<unsigned char>(*(endPtr-1)))) endPtr--;
   sz = static_cast<int>(endPtr - sTok);
   return (sz > 0 ? sTok : 0);
}

int expiry = 1; // 1=require, 0=ignore, -1=optional
int MaxTokSize = 8192;
int ClockSkew = 60;
int JwksRefresh = 300;
bool customIdentityClaims = false;
bool DebugToken = false;
bool DebugTokenClaims = false;
int TokenCacheMax = 10000;
int TokenCacheNoExpTTL = 60;
std::string JwksCacheFile;
int JwksCacheTTL = 0;
std::vector<std::string> IdentityClaims = {
   "preferred_username", "upn", "username", "name", "sub"
};

struct IssuerPolicy {
   std::string issuer;
   std::vector<std::string> audiences;
   std::string oidcConfigURL;
   std::string jwksURL;
   std::string forcedIdentityClaim;

   std::mutex keysMtx;
   std::map<std::string, EVP_PKEY *> jwksKeys;
   time_t lastJwksLoad = 0;
};

std::vector<std::shared_ptr<IssuerPolicy>> IssuerPolicies;
std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> IssuerPolicyByIssuer;
std::unordered_map<std::string, std::string> EmailIdentityMap;

XrdSysLogger OIDCLogger;
XrdSysError  OIDCLog(0, "secoidc_");

struct CachedTokenEntry {
   std::string identity;
   std::string identityMethod; // e.g. "default", "claim:preferred_username", "email-map:foo@bar.com"
   std::string headerJSON;
   std::string payloadJSON;
   uint64_t    expiresAt = 0; // epoch seconds
};

std::mutex TokenCacheMtx;
std::unordered_map<std::string, CachedTokenEntry> TokenCache;
std::atomic<uint64_t> TokenCacheHits(0);
std::atomic<uint64_t> TokenCacheMisses(0);
std::mutex JwksDiskCacheMtx;

struct JwksDiskEntry {
   std::string jwksUrl;
   time_t fetchedAt = 0;
   std::string jwksB64;
};

void freeKeys(std::map<std::string, EVP_PKEY *> &keys)
{
   for (auto &it : keys) EVP_PKEY_free(it.second);
   keys.clear();
}

void clearIssuerPolicies()
{
   for (auto &p : IssuerPolicies)
      {
       if (!p) continue;
       std::lock_guard<std::mutex> lock(p->keysMtx);
       freeKeys(p->jwksKeys);
      }
   IssuerPolicies.clear();
   IssuerPolicyByIssuer.clear();
}

std::string joinURL(const std::string &base, const char *sfx)
{
   if (base.empty()) return std::string();
   if (hasSuffix(base, "/")) return base + (sfx[0] == '/' ? sfx + 1 : sfx);
   return base + (sfx[0] == '/' ? sfx : std::string("/") + sfx);
}

std::string trimCopy(const std::string &s)
{
   size_t b = 0, e = s.size();
   while (b < e && isspace(static_cast<unsigned char>(s[b]))) b++;
   while (e > b && isspace(static_cast<unsigned char>(s[e - 1]))) e--;
   return s.substr(b, e - b);
}

std::string toLowerCopy(const std::string &s)
{
   std::string out(s);
   for (size_t i = 0; i < out.size(); ++i)
      out[i] = static_cast<char>(tolower(static_cast<unsigned char>(out[i])));
   return out;
}

std::string normalizeEmailKey(const std::string &email)
{
   return toLowerCopy(trimCopy(email));
}

bool parseBool(const std::string &s, bool &out)
{
   std::string v = toLowerCopy(trimCopy(s));
   if (v.empty() || v == "1" || v == "true" || v == "yes" || v == "on")
      {out = true; return true;}
   if (v == "0" || v == "false" || v == "no" || v == "off")
      {out = false; return true;}
   return false;
}

bool clientDebugEnabled()
{
   const char *dbg = getenv("XrdSecDEBUG");
   if (!dbg || !*dbg) return false;
   std::string v = toLowerCopy(trimCopy(dbg));
   return !(v == "0" || v == "off" || v == "false" || v == "no");
}

void clientDebugLog(const std::string &msg)
{
   if (!clientDebugEnabled()) return;
   std::cerr << "Secoidc: " << msg << "\n" << std::flush;
}

void appendCliOpt(std::string &opts, const std::string &key, const std::string *val = 0)
{
   if (!opts.empty()) opts.push_back(' ');
   opts += key;
   if (val)
      {opts.push_back(' ');
       opts += *val;
      }
}

bool startsWithIssuerSection(const std::string &name)
{
   if (name.size() < 6) return false;
   if (toLowerCopy(name.substr(0, 6)) != "issuer") return false;
   return name.size() == 6 || isspace(static_cast<unsigned char>(name[6]));
}

std::string stripQuotes(const std::string &s)
{
   if (s.size() >= 2
   && ((s.front() == '"' && s.back() == '"')
    || (s.front() == '\'' && s.back() == '\'')))
      return s.substr(1, s.size() - 2);
   return s;
}

bool addIniKV(const std::string &keyIn, const std::string &valIn, bool inIssuer,
              std::string &opts, std::string &emsg)
{
   std::string key = toLowerCopy(trimCopy(keyIn));
   std::string val = trimCopy(stripQuotes(trimCopy(valIn)));
   if (key.empty())
      {emsg = "empty key in config file";
       return false;
      }

   if (key == "issuer")
      {if (val.empty()) {emsg = "issuer value is empty"; return false;}
       appendCliOpt(opts, "-issuer", &val);
       return true;
      }

   if (key == "audience")
      {
       if (!inIssuer) {emsg = "audience requires an [issuer ...] section or issuer=..."; return false;}
       size_t pos = 0;
       while (pos <= val.size())
          {
           size_t comma = val.find(',', pos);
           std::string one = trimCopy(val.substr(pos, comma == std::string::npos ? std::string::npos
                                                                                  : comma - pos));
           if (!one.empty()) appendCliOpt(opts, "-audience", &one);
           if (comma == std::string::npos) break;
           pos = comma + 1;
          }
       return true;
      }

   if (key == "oidc-config-url")      {appendCliOpt(opts, "-oidc-config-url", &val); return true;}
   if (key == "jwks-url")             {appendCliOpt(opts, "-jwks-url", &val); return true;}
   if (key == "forced-identity-claim"){appendCliOpt(opts, "-forced-identity-claim", &val); return true;}
   if (key == "maxsz")                {appendCliOpt(opts, "-maxsz", &val); return true;}
   if (key == "expiry")               {appendCliOpt(opts, "-expiry", &val); return true;}
   if (key == "jwks-refresh")         {appendCliOpt(opts, "-jwks-refresh", &val); return true;}
   if (key == "jwks-cache-file")      {appendCliOpt(opts, "-jwks-cache-file", &val); return true;}
   if (key == "jwks-cache-ttl")       {appendCliOpt(opts, "-jwks-cache-ttl", &val); return true;}
   if (key == "clock-skew")           {appendCliOpt(opts, "-clock-skew", &val); return true;}
   if (key == "token-cache-max")      {appendCliOpt(opts, "-token-cache-max", &val); return true;}
   if (key == "token-cache-noexp-ttl"){appendCliOpt(opts, "-token-cache-noexp-ttl", &val); return true;}

   if (key == "identity-claim")
      {
       size_t pos = 0;
       while (pos <= val.size())
          {
           size_t comma = val.find(',', pos);
           std::string one = trimCopy(val.substr(pos, comma == std::string::npos ? std::string::npos
                                                                                  : comma - pos));
           if (!one.empty()) appendCliOpt(opts, "-identity-claim", &one);
           if (comma == std::string::npos) break;
           pos = comma + 1;
          }
       return true;
      }

   if (key == "debug-token" || key == "show-token-claims")
      {
       bool enabled = false;
       if (!parseBool(val, enabled))
          {emsg = "invalid boolean for " + key + ": " + val;
           return false;
          }
       if (enabled)
          appendCliOpt(opts, key == "debug-token" ? "-debug-token" : "-show-token-claims");
       return true;
      }

   emsg = "unsupported key in config file: " + key;
   return false;
}

bool loadOIDCIniAsArgs(const char *path, std::string &opts, bool &found, std::string &emsg)
{
   opts.clear();
   emsg.clear();
   found = false;

   struct stat st;
   if (stat(path, &st) != 0)
      {
       if (errno == ENOENT) return true;
       emsg = std::string("unable to access ") + path + ": " + strerror(errno);
       return false;
      }
   found = true;

   if (!S_ISREG(st.st_mode))
      {emsg = std::string(path) + ": config path must be a regular file";
       return false;
      }

   uid_t euid = geteuid();
   if (st.st_uid != euid)
      {emsg = std::string(path) + ": config file owner uid "
            + std::to_string(static_cast<unsigned long long>(st.st_uid))
            + " does not match process euid "
            + std::to_string(static_cast<unsigned long long>(euid));
       return false;
      }

   if (st.st_mode & (S_IWGRP | S_IWOTH))
      {emsg = std::string(path)
            + ": config file must not be writable by group/other";
       return false;
      }

   std::ifstream in(path);
   if (!in.is_open())
      {emsg = std::string("unable to open ") + path + ": " + strerror(errno);
       return false;
      }

   bool inIssuer = false;
   bool inEmailMap = false;
   std::string line;
   int lineNo = 0;
   while (std::getline(in, line))
      {
       ++lineNo;
       std::string t = trimCopy(line);
       if (t.empty() || t[0] == '#' || t[0] == ';') continue;

       if (t.front() == '[' && t.back() == ']')
          {
           std::string sec = trimCopy(t.substr(1, t.size() - 2));
           if (toLowerCopy(sec) == "global")
              {inIssuer = false;
               inEmailMap = false;
               continue;
              }
           if (toLowerCopy(sec) == "email-map")
              {inIssuer = false;
               inEmailMap = true;
               continue;
              }
           if (startsWithIssuerSection(sec))
              {
               std::string val = trimCopy(sec.substr(6));
               if (val.empty())
                  {inIssuer = false;
                   inEmailMap = false;
                   continue;
                  }
               val = stripQuotes(val);
               std::string localErr;
               if (!addIniKV("issuer", val, false, opts, localErr))
                  {emsg = std::string(path) + ":" + std::to_string(lineNo) + ": " + localErr;
                   return false;
                  }
               inIssuer = true;
               inEmailMap = false;
               continue;
              }
           emsg = std::string(path) + ":" + std::to_string(lineNo)
                + ": unsupported section '" + sec + "'";
           return false;
          }

       size_t eq = t.find('=');
       if (inEmailMap)
          {
           if (eq == std::string::npos)
              {emsg = std::string(path) + ":" + std::to_string(lineNo)
                    + ": email-map entry must be key=value";
               return false;
              }
           std::string email = normalizeEmailKey(stripQuotes(t.substr(0, eq)));
           std::string uname = trimCopy(stripQuotes(t.substr(eq + 1)));
           if (email.empty() || uname.empty())
              {emsg = std::string(path) + ":" + std::to_string(lineNo)
                    + ": invalid email-map entry";
               return false;
              }
           EmailIdentityMap[email] = uname;
           continue;
          }

       std::string key = (eq == std::string::npos ? t : t.substr(0, eq));
       std::string val = (eq == std::string::npos ? "true" : t.substr(eq + 1));
       std::string localErr;
       if (!addIniKV(key, val, inIssuer, opts, localErr))
          {emsg = std::string(path) + ":" + std::to_string(lineNo) + ": " + localErr;
           return false;
          }
       if (toLowerCopy(trimCopy(key)) == "issuer") inIssuer = true;
      }
   return true;
}

// Maximum bytes we will buffer from any single HTTP response (4 MiB).
static const size_t kFetchBodyLimit = 4 * 1024 * 1024;

struct FetchSink {
   std::string *dst;
   bool truncated;
};

size_t curlWriteCB(char *ptr, size_t sz, size_t nmemb, void *ud)
{
   FetchSink *sink = static_cast<FetchSink *>(ud);
   size_t incoming = sz * nmemb;
   if (sink->dst->size() + incoming > kFetchBodyLimit)
      {// Returning 0 aborts the transfer with CURLE_WRITE_ERROR.
       sink->truncated = true;
       return 0;
      }
   sink->dst->append(ptr, incoming);
   return incoming;
}

bool fetchURL(const std::string &url, std::string &body, std::string &emsg)
{
   body.clear();
   CURL *c = curl_easy_init();
   if (!c) {emsg = "curl init failed"; return false;}
   FetchSink sink;
   sink.dst = &body;
   sink.truncated = false;
   curl_easy_setopt(c, CURLOPT_URL, url.c_str());
   // Follow redirects but only to HTTPS targets; cap at 3 hops.
   curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
   curl_easy_setopt(c, CURLOPT_MAXREDIRS, 3L);
#ifdef CURLOPT_REDIR_PROTOCOLS_STR
   curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
   curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
   curl_easy_setopt(c, CURLOPT_TIMEOUT, 15L);
   curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 5L);
   curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curlWriteCB);
   curl_easy_setopt(c, CURLOPT_WRITEDATA, &sink);
   curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
   curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST, 2L);
   CURLcode rc = curl_easy_perform(c);
   long httpCode = 0;
   curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpCode);
   curl_easy_cleanup(c);
   if (sink.truncated)
      {emsg = "JWKS/OIDC response body too large (> 4 MiB), aborting";
       return false;
      }
   if (rc != CURLE_OK)
      {emsg = curl_easy_strerror(rc);
       return false;
      }
   if (httpCode < 200 || httpCode >= 300)
      {char b[64];
       snprintf(b, sizeof(b), "HTTP status %ld", httpCode);
       emsg = b;
       return false;
      }
   return true;
}

bool getUintClaim(const std::string &json, const char *claim, uint64_t &out)
{
   std::string key("\"");
   key += claim;
   key += "\"";
   size_t pos = json.find(key);
   if (pos == std::string::npos) return false;
   size_t at = skipWs(json, pos + key.size());
   if (at >= json.size() || json[at] != ':') return false;
   at = skipWs(json, at + 1);
   if (at >= json.size() || !isdigit(static_cast<unsigned char>(json[at]))) return false;
   uint64_t v = 0;
   while (at < json.size() && isdigit(static_cast<unsigned char>(json[at])))
      {v = v * 10 + (json[at] - '0');
       at++;
      }
   out = v;
   return true;
}

bool hasStringInArrayClaim(const std::string &json, const char *claim,
                           const std::string &want)
{
   std::string key("\"");
   key += claim;
   key += "\"";
   size_t pos = json.find(key);
   if (pos == std::string::npos) return false;
   size_t at = skipWs(json, pos + key.size());
   if (at >= json.size() || json[at] != ':') return false;
   at = skipWs(json, at + 1);
   if (at >= json.size() || json[at] != '[') return false;
   at++;
   while (at < json.size())
      {at = skipWs(json, at);
       if (at >= json.size()) return false;
       if (json[at] == ']') return false;
       if (json[at] != '"') return false;
       std::string val;
       if (!decodeJsonString(json, at, val)) return false;
       if (val == want) return true;
       at = skipWs(json, at);
       if (at >= json.size()) return false;
       if (json[at] == ',') {at++; continue;}
       if (json[at] == ']') return false;
       return false;
      }
   return false;
}

bool verifyRS256(EVP_PKEY *pkey, const std::string &signedData,
                 const std::string &sig)
{
   EVP_MD_CTX *ctx = EVP_MD_CTX_new();
   if (!ctx) return false;
   bool ok = false;
   if (EVP_DigestVerifyInit(ctx, 0, EVP_sha256(), 0, pkey) == 1
   &&  EVP_DigestVerifyUpdate(ctx, signedData.data(), signedData.size()) == 1
   &&  EVP_DigestVerifyFinal(ctx,
         reinterpret_cast<const unsigned char *>(sig.data()), sig.size()) == 1)
      ok = true;
   EVP_MD_CTX_free(ctx);
   return ok;
}

EVP_PKEY *makeRSAPublicKey(const std::string &modulus,
                           const std::string &exponent)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
   BIGNUM *bnN = 0;
   BIGNUM *bnE = 0;
   OSSL_PARAM_BLD *bld = 0;
   OSSL_PARAM *params = 0;
   EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(0, "RSA", 0);
   if (!ctx) return 0;
   EVP_PKEY *pkey = 0;
   bnN = BN_bin2bn(reinterpret_cast<const unsigned char *>(modulus.data()),
                   modulus.size(), 0);
   bnE = BN_bin2bn(reinterpret_cast<const unsigned char *>(exponent.data()),
                   exponent.size(), 0);
   bld = OSSL_PARAM_BLD_new();
   if (!bnN || !bnE || !bld) goto done;
   if (OSSL_PARAM_BLD_push_BN(bld, "n", bnN) <= 0
   ||  OSSL_PARAM_BLD_push_BN(bld, "e", bnE) <= 0)
      goto done;
   params = OSSL_PARAM_BLD_to_param(bld);
   if (!params) goto done;
   if (EVP_PKEY_fromdata_init(ctx) <= 0
   ||  EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0)
      pkey = 0;

done:
   BN_free(bnN);
   BN_free(bnE);
   OSSL_PARAM_free(params);
   OSSL_PARAM_BLD_free(bld);
   EVP_PKEY_CTX_free(ctx);
   return pkey;
#else
   BIGNUM *bnN = 0;
   BIGNUM *bnE = 0;
   RSA *rsa = 0;
   EVP_PKEY *pkey = 0;

   bnN = BN_bin2bn(reinterpret_cast<const unsigned char *>(modulus.data()),
                   modulus.size(), 0);
   bnE = BN_bin2bn(reinterpret_cast<const unsigned char *>(exponent.data()),
                   exponent.size(), 0);
   if (!bnN || !bnE) goto done;

   rsa = RSA_new();
   if (!rsa) goto done;
   if (RSA_set0_key(rsa, bnN, bnE, 0) != 1) goto done;
   // Ownership transferred to rsa by RSA_set0_key().
   bnN = 0;
   bnE = 0;

   pkey = EVP_PKEY_new();
   if (!pkey) goto done;
   if (EVP_PKEY_assign_RSA(pkey, rsa) != 1)
      {EVP_PKEY_free(pkey);
       pkey = 0;
       goto done;
      }
   // Ownership transferred to pkey by EVP_PKEY_assign_RSA().
   rsa = 0;

done:
   BN_free(bnN);
   BN_free(bnE);
   RSA_free(rsa);
   return pkey;
#endif
}

bool loadJWKS(const std::string &json, std::map<std::string, EVP_PKEY *> &keys,
              std::string &emsg)
{
   keys.clear();
   size_t pos = 0;
   while ((pos = json.find("\"kty\"", pos)) != std::string::npos)
      {size_t objStart = json.rfind('{', pos);
       size_t objEnd = json.find('}', pos);
       if (objStart == std::string::npos || objEnd == std::string::npos) break;
       std::string obj = json.substr(objStart, objEnd - objStart + 1);
       std::string kty, kid, n, e;
       if (!getStringClaim(obj, "kty", kty)
       ||  !getStringClaim(obj, "kid", kid)
       ||  !getStringClaim(obj, "n", n)
       ||  !getStringClaim(obj, "e", e))
          {pos = objEnd + 1;
           continue;
          }
       if (kty != "RSA") {pos = objEnd + 1; continue;}
       std::string nb, eb;
       if (!decodeBase64URL(n, nb) || !decodeBase64URL(e, eb))
          {pos = objEnd + 1;
           continue;
          }
       EVP_PKEY *pkey = makeRSAPublicKey(nb, eb);
       if (!pkey)
          {
           pos = objEnd + 1;
           continue;
          }
       keys[kid] = pkey;
       pos = objEnd + 1;
      }
   if (keys.empty())
      {emsg = "no usable RSA keys in JWKS";
       return false;
      }
   return true;
}

int jwksCacheEffectiveTTL()
{
   return (JwksCacheTTL > 0 ? JwksCacheTTL : JwksRefresh);
}

// Return true iff path is a regular file owned by the effective UID and not
// writable by group or other (same policy as the main config file).
static bool checkCacheFilePerms(const char *path, std::string &emsg)
{
   struct stat st;
   if (stat(path, &st) != 0)
      {if (errno == ENOENT) return true; // not yet created – OK
       emsg = std::string("stat JWKS cache file failed: ") + strerror(errno);
       return false;
      }
   if (!S_ISREG(st.st_mode))
      {emsg = std::string("JWKS cache file is not a regular file: ") + path;
       return false;
      }
   if (st.st_uid != geteuid())
      {emsg = std::string("JWKS cache file not owned by the running UID: ") + path;
       return false;
      }
   if (st.st_mode & (S_IWGRP | S_IWOTH))
      {emsg = std::string("JWKS cache file must not be group/other writable: ") + path;
       return false;
      }
   return true;
}

bool loadJWKSCacheMap(std::unordered_map<std::string, JwksDiskEntry> &out,
                      std::string &emsg)
{
   out.clear();
   emsg.clear();
   if (JwksCacheFile.empty()) return true;

   // Security: verify ownership and permissions before trusting contents.
   if (!checkCacheFilePerms(JwksCacheFile.c_str(), emsg)) return false;

   std::ifstream in(JwksCacheFile.c_str());
   if (!in.is_open())
      {
       if (errno == ENOENT) return true;
       emsg = std::string("unable to open JWKS cache file: ") + strerror(errno);
       return false;
      }

   std::string line, curIssuer;
   int lineNo = 0;
   while (std::getline(in, line))
      {
       ++lineNo;
       std::string t = trimCopy(line);
       if (t.empty() || t[0] == '#' || t[0] == ';') continue;
       if (t.front() == '[' && t.back() == ']')
          {
           std::string sec = trimCopy(t.substr(1, t.size() - 2));
           if (!startsWithIssuerSection(sec))
              {emsg = "invalid JWKS cache section at line " + std::to_string(lineNo);
               return false;
              }
           curIssuer = stripQuotes(trimCopy(sec.substr(6)));
           if (!curIssuer.empty()) out[curIssuer];
           continue;
          }
       if (curIssuer.empty()) continue;
       size_t eq = t.find('=');
       if (eq == std::string::npos) continue;
       std::string key = toLowerCopy(trimCopy(t.substr(0, eq)));
       std::string val = trimCopy(stripQuotes(t.substr(eq + 1)));
       auto &e = out[curIssuer];
       if (key == "jwks_url") e.jwksUrl = val;
       else if (key == "fetched_at")
          {
           char *endP = 0;
           long long vv = strtoll(val.c_str(), &endP, 10);
           if (endP && *endP == '\0' && vv > 0) e.fetchedAt = static_cast<time_t>(vv);
          }
       else if (key == "jwks_b64") e.jwksB64 = val;
      }
   return true;
}

bool writeJWKSCacheMap(const std::unordered_map<std::string, JwksDiskEntry> &cache,
                       std::string &emsg)
{
   emsg.clear();
   if (JwksCacheFile.empty()) return true;

   // Refuse to overwrite a cache file with unsafe permissions.
   if (!checkCacheFilePerms(JwksCacheFile.c_str(), emsg)) return false;

   std::string tmp = JwksCacheFile + ".tmp";
   // Write with owner-only permissions (0600); mode is masked by umask but we
   // call chmod explicitly after the write to ensure the final file is safe.
   int tfd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
   if (tfd < 0)
      {emsg = std::string("unable to create JWKS cache temp file: ") + strerror(errno);
       return false;
      }
   if (fchmod(tfd, 0600) != 0)
      {emsg = std::string("unable to set JWKS cache temp file permissions: ") + strerror(errno);
       close(tfd); unlink(tmp.c_str());
       return false;
      }
   close(tfd);
   std::ofstream out(tmp.c_str(), std::ios::out | std::ios::trunc);
   if (!out.is_open())
      {emsg = std::string("unable to write JWKS cache temp file: ") + strerror(errno);
       return false;
      }

   for (const auto &it : cache)
      {
       if (it.first.empty() || it.second.jwksB64.empty()) continue;
       out << "[issuer \"" << it.first << "\"]\n";
       out << "fetched_at=" << static_cast<long long>(it.second.fetchedAt) << "\n";
       if (!it.second.jwksUrl.empty())
          out << "jwks_url=" << it.second.jwksUrl << "\n";
       out << "jwks_b64=" << it.second.jwksB64 << "\n\n";
      }
   out.close();
   if (rename(tmp.c_str(), JwksCacheFile.c_str()) != 0)
      {
       emsg = std::string("unable to replace JWKS cache file: ") + strerror(errno);
       unlink(tmp.c_str());
       return false;
      }
   return true;
}

bool loadJWKSFromDiskCacheForPolicy(std::shared_ptr<IssuerPolicy> policy, time_t now,
                                    std::map<std::string, EVP_PKEY *> &keysOut,
                                    std::string &emsg)
{
   keysOut.clear();
   if (!policy || JwksCacheFile.empty()) return false;
   std::lock_guard<std::mutex> lock(JwksDiskCacheMtx);
   std::unordered_map<std::string, JwksDiskEntry> cache;
   if (!loadJWKSCacheMap(cache, emsg)) return false;
   auto it = cache.find(policy->issuer);
   if (it == cache.end() || it->second.jwksB64.empty()) return false;
   int ttl = jwksCacheEffectiveTTL();
   if (ttl > 0 && (now - it->second.fetchedAt) > ttl) return false;

   std::string jwksJson;
   if (!decodeBase64URL(it->second.jwksB64, jwksJson))
      {emsg = "cached JWKS decode failed";
       return false;
      }
   if (!loadJWKS(jwksJson, keysOut, emsg)) return false;
   return true;
}

void storeJWKSInDiskCacheForPolicy(std::shared_ptr<IssuerPolicy> policy,
                                   const std::string &jwksUrl,
                                   const std::string &jwksJson,
                                   time_t fetchedAt)
{
   if (!policy || JwksCacheFile.empty() || jwksJson.empty()) return;
   std::lock_guard<std::mutex> lock(JwksDiskCacheMtx);
   std::unordered_map<std::string, JwksDiskEntry> cache;
   std::string emsg;
   if (!loadJWKSCacheMap(cache, emsg))
      {
       OIDCLog.Emsg("Auth", "oidc", ("jwks cache load failed: " + emsg).c_str());
       return;
      }
   JwksDiskEntry &e = cache[policy->issuer];
   e.jwksUrl = jwksUrl;
   e.fetchedAt = fetchedAt;
   e.jwksB64 = encodeBase64URL(jwksJson);
   if (!writeJWKSCacheMap(cache, emsg))
      OIDCLog.Emsg("Auth", "oidc", ("jwks cache write failed: " + emsg).c_str());
}

bool refreshJWKSForPolicy(std::shared_ptr<IssuerPolicy> policy, bool force,
                          std::string &emsg)
{
   if (!policy)
      {emsg = "missing issuer policy";
       return false;
      }
   std::lock_guard<std::mutex> lock(policy->keysMtx);
   time_t now = time(0);
   if (!force && !policy->jwksKeys.empty()
   && (now - policy->lastJwksLoad) < JwksRefresh) return true;

   if (!force && JwksCacheFile.size())
      {
       std::map<std::string, EVP_PKEY *> cachedKeys;
       std::string cmsg;
       if (loadJWKSFromDiskCacheForPolicy(policy, now, cachedKeys, cmsg))
          {
           freeKeys(policy->jwksKeys);
           policy->jwksKeys.swap(cachedKeys);
           policy->lastJwksLoad = now;
           return true;
          }
      }

   std::string useJwks = policy->jwksURL;
   if (useJwks.empty())
      {if (policy->oidcConfigURL.empty())
          {emsg = "OIDC config URL not set";
           return false;
          }
       std::string cfg;
       if (!fetchURL(policy->oidcConfigURL, cfg, emsg))
          {emsg = "failed OIDC discovery fetch: " + emsg;
           return false;
          }
       if (!getStringClaim(cfg, "jwks_uri", useJwks))
          {emsg = "jwks_uri not found in OIDC discovery";
           return false;
          }
       if (!hasPrefix(useJwks.c_str(), "https://"))
          {emsg = "jwks_uri in discovery must use https://";
           return false;
          }
      }

   std::string jwks;
   if (!fetchURL(useJwks, jwks, emsg))
      {
       std::string fetchErr = "failed JWKS fetch: " + emsg;
       std::map<std::string, EVP_PKEY *> cachedKeys;
       std::string cmsg;
       if (JwksCacheFile.size()
       &&  loadJWKSFromDiskCacheForPolicy(policy, now, cachedKeys, cmsg))
          {
           freeKeys(policy->jwksKeys);
           policy->jwksKeys.swap(cachedKeys);
           policy->lastJwksLoad = now;
           emsg.clear();
           return true;
          }
       emsg = fetchErr;
       return false;
      }

   std::map<std::string, EVP_PKEY *> newKeys;
   if (!loadJWKS(jwks, newKeys, emsg)) return false;
   freeKeys(policy->jwksKeys);
   policy->jwksKeys.swap(newKeys);
   policy->lastJwksLoad = now;
   storeJWKSInDiskCacheForPolicy(policy, useJwks, jwks, now);
   return true;
}

bool audienceMatches(std::shared_ptr<IssuerPolicy> policy,
                     const std::string &payloadJSON)
{
   if (!policy || policy->audiences.empty()) return true;
   for (const auto &aud : policy->audiences)
      {
       std::string tokAud;
       if ((getStringClaim(payloadJSON, "aud", tokAud) && tokAud == aud)
       ||   hasStringInArrayClaim(payloadJSON, "aud", aud))
          return true;
      }
   return false;
}

bool parseAndValidateJWT(const char *rawTok, std::string &payloadJSON,
                         std::string &headerJSON,
                         std::string &identity, uint64_t &expOut,
                         std::string &emsg,
                         std::string *identityMethod = nullptr)
{
   payloadJSON.clear();
   headerJSON.clear();
   identity.clear();
   expOut = 0;

   int tlen = 0;
   const char *tok = Strip(rawTok, tlen);
   if (!tok || tlen <= 0) {emsg = "invalid token format"; return false;}
   std::string jwt(tok, tlen);

   size_t dot1 = jwt.find('.');
   size_t dot2 = (dot1 == std::string::npos ? std::string::npos : jwt.find('.', dot1+1));
   if (dot1 == std::string::npos || dot2 == std::string::npos)
      {emsg = "token is not JWT"; return false;}
   std::string h64 = jwt.substr(0, dot1);
   std::string p64 = jwt.substr(dot1 + 1, dot2 - dot1 - 1);
   std::string s64 = jwt.substr(dot2 + 1);
   std::string hdr, sig;
   if (!decodeBase64URL(h64, hdr) || !decodeBase64URL(p64, payloadJSON)
   ||  !decodeBase64URL(s64, sig))
      {emsg = "JWT decode failed"; return false;}
   headerJSON = hdr;

   std::string alg, kid;
   if (!getStringClaim(hdr, "alg", alg)) {emsg = "JWT alg missing"; return false;}
   if (alg != "RS256") {emsg = "unsupported JWT alg"; return false;}
   getStringClaim(hdr, "kid", kid);

   std::string tokIss;
   if (!getStringClaim(payloadJSON, "iss", tokIss))
      {emsg = "token issuer missing"; return false;}
   auto pIt = IssuerPolicyByIssuer.find(tokIss);
   if (pIt == IssuerPolicyByIssuer.end())
      {emsg = "token issuer not configured";
       return false;
      }
   std::shared_ptr<IssuerPolicy> policy = pIt->second;

   if (!audienceMatches(policy, payloadJSON))
      {emsg = "token audience mismatch"; return false;}

   uint64_t exp = 0;
   bool haveExp = getUintClaim(payloadJSON, "exp", exp);
   if (expiry > 0 && !haveExp) {emsg = "token expiry missing"; return false;}
   time_t now = time(0);
   if (haveExp && exp + ClockSkew < static_cast<uint64_t>(now))
      {emsg = "token expired"; return false;}
   uint64_t nbf = 0;
   if (getUintClaim(payloadJSON, "nbf", nbf)
   &&  nbf > static_cast<uint64_t>(now) + ClockSkew)
      {emsg = "token not yet valid"; return false;}
   if (haveExp) expOut = exp;

   if (!refreshJWKSForPolicy(policy, false, emsg))
      return false;

   std::string signedData = jwt.substr(0, dot2);
   bool verified = false;
   {
      std::lock_guard<std::mutex> lock(policy->keysMtx);
      if (!kid.empty())
         {auto it = policy->jwksKeys.find(kid);
          if (it != policy->jwksKeys.end())
             verified = verifyRS256(it->second, signedData, sig);
         } else {
          for (auto &it : policy->jwksKeys)
              {if (verifyRS256(it.second, signedData, sig))
                  {verified = true;
                   break;
                  }
              }
         }
   }
   if (!verified)
      {
       // If no refresh source is configured, fail with a signature error
       // instead of masking it with an OIDC/JWKS URL configuration error.
       if (policy->jwksURL.empty() && policy->oidcConfigURL.empty())
          {emsg = "JWT signature validation failed";
           return false;
          }
       if (!refreshJWKSForPolicy(policy, true, emsg))
          return false;
       std::lock_guard<std::mutex> lock(policy->keysMtx);
       if (!kid.empty())
          {auto it = policy->jwksKeys.find(kid);
           if (it != policy->jwksKeys.end())
              verified = verifyRS256(it->second, signedData, sig);
          } else {
           for (auto &it : policy->jwksKeys)
               {if (verifyRS256(it.second, signedData, sig))
                   {verified = true;
                    break;
                   }
               }
          }
       if (!verified) {emsg = "JWT signature validation failed"; return false;}
      }

   if (!policy->forcedIdentityClaim.empty())
      {
       std::string forced = policy->forcedIdentityClaim;
       if (!getStringClaim(payloadJSON, forced.c_str(), identity)
       ||  identity.empty())
          {emsg = "token identity claim missing: " + forced;
           return false;
          }
       if (forced == "email")
          {
           std::string rawEmail = identity;
           std::string emailKey = normalizeEmailKey(rawEmail);
           auto mIt = EmailIdentityMap.find(emailKey);
           if (mIt == EmailIdentityMap.end() || mIt->second.empty())
              {emsg = "token email is not mapped to a username";
               return false;
              }
           identity = mIt->second;
           if (identityMethod)
              *identityMethod = "email-map:" + rawEmail;
          }
       else
          {
           if (identityMethod)
              *identityMethod = "claim:" + forced;
          }
      } else {
       std::string usedClaim;
       for (const auto &claim : IdentityClaims)
          {if (getStringClaim(payloadJSON, claim.c_str(), identity) && !identity.empty())
              {usedClaim = claim;
               break;
              }
          }
       if (identity.empty())
          {emsg = "token identity claim missing";
           return false;
          }
       if (identityMethod)
          *identityMethod = "default:" + usedClaim;
      }
   return true;
}

void pruneTokenCacheLocked(uint64_t now)
{
   for (auto it = TokenCache.begin(); it != TokenCache.end(); )
      {
       if (it->second.expiresAt <= now) it = TokenCache.erase(it);
       else ++it;
      }
}

bool tokenCacheLookup(const std::string &tok, uint64_t now, CachedTokenEntry &out)
{
   if (TokenCacheMax == 0) return false;
   std::lock_guard<std::mutex> lock(TokenCacheMtx);
   pruneTokenCacheLocked(now);
   auto it = TokenCache.find(tok);
   if (it == TokenCache.end()) return false;
   out = it->second;
   return true;
}

size_t tokenCacheSize()
{
   std::lock_guard<std::mutex> lock(TokenCacheMtx);
   return TokenCache.size();
}

void tokenCacheStore(const std::string &tok, const CachedTokenEntry &entry, uint64_t now)
{
   if (TokenCacheMax == 0) return;
   std::lock_guard<std::mutex> lock(TokenCacheMtx);
   pruneTokenCacheLocked(now);
   if (TokenCacheMax > 0 && TokenCache.size() >= static_cast<size_t>(TokenCacheMax))
      {
       // Simple bounded cache policy: evict one arbitrary oldest bucket element.
       TokenCache.erase(TokenCache.begin());
      }
   TokenCache[tok] = entry;
}

void debugPrintToken(const XrdSecEntity &ent, const std::string &headerJSON,
                     const std::string &payloadJSON, bool cacheHit,
                     const std::string &identityMethod = std::string())
{
   if (!DebugToken && !DebugTokenClaims) return;
   const char *tid = (ent.tident ? ent.tident : "oidc");
   uint64_t hits = TokenCacheHits.load();
   uint64_t misses = TokenCacheMisses.load();
   size_t csize = tokenCacheSize();

   if (DebugToken)
      {std::string hMsg = std::string("oidc.jwt.header ") + headerJSON;
       std::string pMsg = std::string("oidc.jwt.payload ") + payloadJSON;
       std::string sMsg = "oidc.cache.stats"
                          " cache_hit=" + std::string(cacheHit ? "1" : "0") +
                          " hits=" + std::to_string(hits) +
                          " misses=" + std::to_string(misses) +
                          " size=" + std::to_string(csize);
       OIDCLog.Emsg("Auth", tid, hMsg.c_str());
       OIDCLog.Emsg("Auth", tid, pMsg.c_str());
       OIDCLog.Emsg("Auth", tid, sMsg.c_str());
       return;
      }

   // Claims-only mode: log selected fields, avoid full token payload output.
   std::string kid, alg, typ;
   std::string iss, aud, sub, prefUser, azp;
   uint64_t iat = 0, nbf = 0, exp = 0;
   bool haveIat = getUintClaim(payloadJSON, "iat", iat);
   bool haveNbf = getUintClaim(payloadJSON, "nbf", nbf);
   bool haveExp = getUintClaim(payloadJSON, "exp", exp);

   getStringClaim(headerJSON, "kid", kid);
   getStringClaim(headerJSON, "alg", alg);
   getStringClaim(headerJSON, "typ", typ);
   getStringClaim(payloadJSON, "iss", iss);
   getStringClaim(payloadJSON, "aud", aud);
   getStringClaim(payloadJSON, "sub", sub);
   getStringClaim(payloadJSON, "preferred_username", prefUser);
   getStringClaim(payloadJSON, "azp", azp);

   std::string msg = "oidc.jwt.claims";
   msg += " alg='" + alg + "'";
   msg += " kid='" + kid + "'";
   msg += " typ='" + typ + "'";
   msg += " iss='" + iss + "'";
   msg += " aud='" + aud + "'";
   msg += " sub='" + sub + "'";
   msg += " preferred_username='" + prefUser + "'";
   msg += " azp='" + azp + "'";
   if (haveIat) msg += " iat=" + std::to_string(iat);
   if (haveNbf) msg += " nbf=" + std::to_string(nbf);
   if (haveExp) msg += " exp=" + std::to_string(exp);
   msg += " mapped_name='" + std::string(ent.name ? ent.name : "") + "'";
   if (!identityMethod.empty())
      msg += " identity_method='" + identityMethod + "'";
   msg += " cache_hit=" + std::string(cacheHit ? "1" : "0");
   msg += " cache_hits=" + std::to_string(hits);
   msg += " cache_misses=" + std::to_string(misses);
   msg += " cache_size=" + std::to_string(csize);
   OIDCLog.Emsg("Auth", tid, msg.c_str());
}
}

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
                        : XrdSecProtocol("oidc"), maxTSize(MaxTokSize)
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
                                      : XrdSecProtocol("oidc"), maxTSize(MaxTokSize)
{
   aOK = false;
   if (!parms || !(*parms))
      {Fatal(erp, "Client parameters not specified.", EINVAL);
       return;
      }

   std::string pstr(parms);
   // Protocol parameters may be prefixed by protocol name and TLS marker.
   // Extract max token size as the last numeric field before the trailing ':'.
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
   if (v <= 0 || !endP || *endP != '\0')
      {Fatal(erp, "Invalid max token size in parameters.", EINVAL);
       return;
      }
   maxTSize = v;
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
   return new XrdSecCredentials(bp, bsz);
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

   char *buff = (char *)malloc(st.st_size + 1);
   if (!buff) {close(fd); return Fatal(erp, "Insufficient memory.", ENOMEM);}
   ssize_t got = 0;
   while (got < st.st_size)
      {ssize_t rd = read(fd, buff + got, st.st_size - got);
       if (rd < 0)
          {if (errno == EINTR) continue;
           close(fd);
           free(buff);
           return Fatal(erp, "Unable to read token file.", EIO);
          }
       if (rd == 0)
          {close(fd);
           free(buff);
           return Fatal(erp, "Unable to read token file.", EIO);
          }
       got += rd;
      }
   close(fd);

   buff[st.st_size] = 0;
   int tlen = 0;
   const char *tok = Strip(buff, tlen);
   XrdSecCredentials *ret = (tok ? retToken(tok, tlen) : 0);
   free(buff);
   if (!ret) return Fatal(erp, "Token value malformed.", EINVAL);
   isbad = true;
   return ret;
}

XrdSecCredentials *XrdSecProtocoloidc::findToken(XrdOucErrInfo *erp, bool &isbad)
{
   static const char *loc[] = {
     "XRD_SSO_TOKEN", "XRD_SSO_TOKEN_FILE",
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

        // Inline env-var token: validate size before any string ops.
        int envLen = static_cast<int>(strlen(env));
        if (envLen > MaxTokSize)
           {OIDCLog.Emsg("findToken", "Ignoring oversized inline env token from", key.c_str());
            continue;
           }
        int tlen = 0;
        const char *tok = Strip(env, tlen, envLen);
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

   // Enforce an upper bound before doing any string operations on untrusted data.
   if (cred->size > MaxTokSize + 16)
      {Fatal(erp, "Credential too large.", EINVAL);
       return -1;
      }

   // Ensure the credential buffer is NUL-terminated within the declared size
   // so strcmp and other string ops are safe even with a hostile sender.
   if (memchr(cred->buffer, '\0', cred->size) == 0)
      {Fatal(erp, "Credential not NUL-terminated.", EINVAL);
       return -1;
      }

   if (strcmp(cred->buffer, "oidc"))
      {Fatal(erp, "Authentication protocol id mismatch.", EINVAL);
       return -1;
      }

   // Token starts after the "oidc\0" prefix; bound it to the remaining buffer.
   int tokRawLen = cred->size - 5;
   const char *tok = cred->buffer + 5;
   if (tokRawLen <= 0 || !*tok)
      {Fatal(erp, "Null token.", EINVAL);
       return -1;
      }
   int tlen = 0;
   tok = Strip(tok, tlen, tokRawLen);
   if (!tok || tlen <= 0)
      {Fatal(erp, "Token value malformed.", EINVAL);
       return -1;
      }

   std::string payloadJSON, headerJSON, identity, msgRC;
   std::string tokKey(tok);
   uint64_t now = static_cast<uint64_t>(time(0));
   CachedTokenEntry cached;
   if (tokenCacheLookup(tokKey, now, cached))
      {
       TokenCacheHits++;
       if (Entity.name) free(Entity.name);
       Entity.name = strdup(cached.identity.c_str());
       strncpy(Entity.prot, "oidc", sizeof(Entity.prot));
       debugPrintToken(Entity, cached.headerJSON, cached.payloadJSON, true,
                       cached.identityMethod);
       return 0;
      }
   TokenCacheMisses++;

   uint64_t expTime = 0;
   std::string identityMethod;
   if (!parseAndValidateJWT(tok, payloadJSON, headerJSON, identity, expTime, msgRC,
                             &identityMethod))
      {Fatal(erp, msgRC.c_str(), EAUTH, false);
       return -1;
      }

   if (Entity.name) free(Entity.name);
   Entity.name = strdup(identity.c_str());
   strncpy(Entity.prot, "oidc", sizeof(Entity.prot));
   debugPrintToken(Entity, headerJSON, payloadJSON, false, identityMethod);

   CachedTokenEntry ins;
   ins.identity = identity;
   ins.identityMethod = identityMethod;
   ins.headerJSON = headerJSON;
   ins.payloadJSON = payloadJSON;
   ins.expiresAt = (expTime ? expTime : now + static_cast<uint64_t>(TokenCacheNoExpTTL));
   tokenCacheStore(tokKey, ins, now);
   return 0;
}

extern "C"
{
char *XrdSecProtocoloidcInit(const char mode, const char *parms, XrdOucErrInfo *erp)
{
   static char nilstr = 0;
   uint64_t opts = XrdSecProtocoloidc::oidcVersion;

   if (mode == 'c') return &nilstr;
   OIDCLog.logger(&OIDCLogger);

   clearIssuerPolicies();
   EmailIdentityMap.clear();
   JwksCacheFile.clear();
   JwksCacheTTL = 0;
   std::shared_ptr<IssuerPolicy> curPolicy;
   std::string fileBackedParms;

   if (!parms || !*parms)
      {
       bool cfgFound = false;
       std::string cfgErr;
       static const char *kOIDCCfgPath = "/etc/xrootd/oidc.cfg";
       if (!loadOIDCIniAsArgs(kOIDCCfgPath, fileBackedParms, cfgFound, cfgErr))
          {Fatal(erp, cfgErr.c_str(), EINVAL);
           return 0;
          }
       if (!cfgFound)
          {std::string e = std::string("Missing required OIDC config file: ")
                         + kOIDCCfgPath;
           Fatal(erp, e.c_str(), ENOENT);
           return 0;
          }
       if (cfgFound && !fileBackedParms.empty())
          parms = fileBackedParms.c_str();
      }

   if (parms && *parms)
      {XrdOucString cfgParms(parms);
       XrdOucTokenizer cfg(const_cast<char *>(cfgParms.c_str()));
       char *endP = 0, *val = 0;
       cfg.GetLine();
       while((val = cfg.GetToken()))
            {     if (!strcmp(val, "-maxsz"))
                     {if (!(val = cfg.GetToken()))
                         {Fatal(erp, "-maxsz argument missing", EINVAL);
                          return 0;
                         }
                      MaxTokSize = strtol(val, &endP, 10);
                      if (*endP == 'k' || *endP == 'K') {MaxTokSize *= 1024; endP++;}
                      if (MaxTokSize <= 0 || MaxTokSize > 524288 || *endP)
                         {Fatal(erp, "-maxsz argument invalid", EINVAL);
                          return 0;
                         }
                     }
               else if (!strcmp(val, "-expiry"))
                     {if (!(val = cfg.GetToken()))
                         {Fatal(erp, "-expiry argument missing", EINVAL);
                          return 0;
                         }
                           if (!strcmp(val, "ignore"))   expiry =  0;
                      else if (!strcmp(val, "optional")) expiry = -1;
                      else if (!strcmp(val, "required")) expiry =  1;
                      else {Fatal(erp, "-expiry argument invalid", EINVAL);
                            return 0;
                           }
                     }
               else if (!strcmp(val, "-issuer"))
                     {if (!(val = cfg.GetToken()))
                        {Fatal(erp, "-issuer argument missing", EINVAL);
                         return 0;
                        }
                      auto pIt = IssuerPolicyByIssuer.find(val);
                      if (pIt != IssuerPolicyByIssuer.end())
                         curPolicy = pIt->second;
                      else
                         {curPolicy = std::make_shared<IssuerPolicy>();
                          curPolicy->issuer = val;
                          IssuerPolicies.push_back(curPolicy);
                          IssuerPolicyByIssuer[curPolicy->issuer] = curPolicy;
                         }
                     }
               else if (!strcmp(val, "-audience"))
                     {if (!(val = cfg.GetToken()))
                        {Fatal(erp, "-audience argument missing", EINVAL);
                         return 0;
                        }
                      if (!curPolicy)
                         {Fatal(erp, "-audience requires a prior -issuer", EINVAL);
                          return 0;
                         }
                      curPolicy->audiences.push_back(val);
                     }
               else if (!strcmp(val, "-oidc-config-url"))
                     {if (!(val = cfg.GetToken()))
                        {Fatal(erp, "-oidc-config-url argument missing", EINVAL);
                         return 0;
                        }
                      if (!curPolicy)
                         {Fatal(erp, "-oidc-config-url requires a prior -issuer", EINVAL);
                          return 0;
                         }
                      curPolicy->oidcConfigURL = val;
                     }
               else if (!strcmp(val, "-jwks-url"))
                     {if (!(val = cfg.GetToken()))
                        {Fatal(erp, "-jwks-url argument missing", EINVAL);
                          return 0;
                         }
                      if (!curPolicy)
                         {Fatal(erp, "-jwks-url requires a prior -issuer", EINVAL);
                          return 0;
                         }
                      curPolicy->jwksURL = val;
                     }
               else if (!strcmp(val, "-jwks-refresh"))
                     {if (!(val = cfg.GetToken()))
                        {Fatal(erp, "-jwks-refresh argument missing", EINVAL);
                         return 0;
                        }
                      JwksRefresh = strtol(val, &endP, 10);
                      if (JwksRefresh <= 0 || *endP)
                         {Fatal(erp, "-jwks-refresh argument invalid", EINVAL);
                          return 0;
                         }
                     }
              else if (!strcmp(val, "-jwks-cache-file"))
                    {if (!(val = cfg.GetToken()))
                       {Fatal(erp, "-jwks-cache-file argument missing", EINVAL);
                        return 0;
                       }
                     JwksCacheFile = val;
                    }
              else if (!strcmp(val, "-jwks-cache-ttl"))
                    {if (!(val = cfg.GetToken()))
                       {Fatal(erp, "-jwks-cache-ttl argument missing", EINVAL);
                        return 0;
                       }
                     JwksCacheTTL = strtol(val, &endP, 10);
                     if (JwksCacheTTL < 0 || *endP)
                        {Fatal(erp, "-jwks-cache-ttl argument invalid", EINVAL);
                         return 0;
                        }
                    }
               else if (!strcmp(val, "-clock-skew"))
                     {if (!(val = cfg.GetToken()))
                        {Fatal(erp, "-clock-skew argument missing", EINVAL);
                         return 0;
                        }
                      ClockSkew = strtol(val, &endP, 10);
                      if (ClockSkew < 0 || ClockSkew > 3600 || *endP)
                         {Fatal(erp, "-clock-skew argument invalid", EINVAL);
                          return 0;
                         }
                     }
               else if (!strcmp(val, "-identity-claim"))
                     {if (!(val = cfg.GetToken()))
                        {Fatal(erp, "-identity-claim argument missing", EINVAL);
                         return 0;
                        }
                      if (!customIdentityClaims)
                         {IdentityClaims.clear();
                          customIdentityClaims = true;
                         }
                      IdentityClaims.push_back(val);
                     }
              else if (!strcmp(val, "-forced-identity-claim"))
                    {if (!(val = cfg.GetToken()))
                       {Fatal(erp, "-forced-identity-claim argument missing", EINVAL);
                        return 0;
                       }
                     if (!curPolicy)
                        {Fatal(erp, "-forced-identity-claim requires a prior -issuer", EINVAL);
                         return 0;
                        }
                     curPolicy->forcedIdentityClaim = val;
                    }
               else if (!strcmp(val, "-debug-token"))
                     {
                      DebugToken = true;
                     }
               else if (!strcmp(val, "-show-token-claims"))
                     {
                      DebugTokenClaims = true;
                     }
               else if (!strcmp(val, "-token-cache-max"))
                     {if (!(val = cfg.GetToken()))
                        {Fatal(erp, "-token-cache-max argument missing", EINVAL);
                         return 0;
                        }
                      TokenCacheMax = strtol(val, &endP, 10);
                      if (TokenCacheMax < 0 || *endP)
                         {Fatal(erp, "-token-cache-max argument invalid", EINVAL);
                          return 0;
                         }
                     }
               else if (!strcmp(val, "-token-cache-noexp-ttl"))
                     {if (!(val = cfg.GetToken()))
                        {Fatal(erp, "-token-cache-noexp-ttl argument missing", EINVAL);
                         return 0;
                        }
                      TokenCacheNoExpTTL = strtol(val, &endP, 10);
                      if (TokenCacheNoExpTTL < 0 || *endP)
                         {Fatal(erp, "-token-cache-noexp-ttl argument invalid", EINVAL);
                          return 0;
                         }
                     }
               else {XrdOucString eTxt("Invalid parameter - "); eTxt += val;
                     Fatal(erp, eTxt.c_str(), EINVAL);
                     return 0;
                    }
            }
      }

   if (IssuerPolicies.empty())
      {Fatal(erp, "At least one -issuer must be configured.", EINVAL);
       return 0;
      }

   if (IdentityClaims.empty())
      {Fatal(erp, "At least one identity claim must be configured.", EINVAL);
       return 0;
      }

   curl_global_init(CURL_GLOBAL_DEFAULT);

   for (auto &policy : IssuerPolicies)
      {
       if (policy->oidcConfigURL.empty() && !policy->issuer.empty())
          policy->oidcConfigURL = joinURL(policy->issuer, "/.well-known/openid-configuration");

       if (policy->oidcConfigURL.empty() && policy->jwksURL.empty())
          {std::string e = "issuer '" + policy->issuer
             + "' requires -oidc-config-url or -jwks-url";
           Fatal(erp, e.c_str(), EINVAL);
           return 0;
          }
       if ((!policy->oidcConfigURL.empty() && !hasPrefix(policy->oidcConfigURL.c_str(), "https://"))
       ||  (!policy->jwksURL.empty() && !hasPrefix(policy->jwksURL.c_str(), "https://")))
          {std::string e = "issuer '" + policy->issuer
             + "' has non-https OIDC/JWKS URL";
           Fatal(erp, e.c_str(), EINVAL);
           return 0;
          }

       std::string emsg;
       if (!refreshJWKSForPolicy(policy, true, emsg))
          {std::string e = "issuer '" + policy->issuer + "': " + emsg;
           Fatal(erp, e.c_str(), EINVAL);
           return 0;
          }
      }

   char buff[256];
   snprintf(buff, sizeof(buff), "TLS:%" PRIu64 ":%d:", opts, MaxTokSize);
   return strdup(buff);
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
       XrdSecProtocoloidc *prot = new XrdSecProtocoloidc(parms, erp, aOK);
       if (aOK) return prot;
       delete prot;
       return 0;
      }

   XrdSecProtocoloidc *prot = new XrdSecProtocoloidc(hostname, endPoint);
   if (!prot)
      {Fatal(erp, "insufficient memory for protocol.", ENOMEM, false);
       return 0;
      }
   return prot;
}
}
