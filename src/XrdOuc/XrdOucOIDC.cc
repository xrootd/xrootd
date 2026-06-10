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
#include <climits>
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
#include <sstream>
#include <string>
#include <string_view>
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

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucJson.hh"
#include "XrdOuc/XrdOucOIDC.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

#ifndef EAUTH
#define EAUTH EBADE
#endif

namespace XrdOucOIDC
{
namespace detail
{
// RAII wrappers for the C library handles used throughout this plugin. They
// make ownership explicit and guarantee cleanup on every return path, removing
// the need for manual *_free()/goto bookkeeping. (Some are also reused by the
// unit-test translation unit, which #includes this file.)
struct EvpPkeyDeleter    {void operator()(EVP_PKEY *p)     const noexcept {EVP_PKEY_free(p);}};
struct EvpMdCtxDeleter   {void operator()(EVP_MD_CTX *p)   const noexcept {EVP_MD_CTX_free(p);}};
struct BignumDeleter     {void operator()(BIGNUM *p)       const noexcept {BN_free(p);}};
struct EvpPkeyCtxDeleter {void operator()(EVP_PKEY_CTX *p) const noexcept {EVP_PKEY_CTX_free(p);}};
struct CurlDeleter       {void operator()(CURL *p)         const noexcept {curl_easy_cleanup(p);}};

using EvpPkeyPtr    = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;
using EvpMdCtxPtr   = std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter>;
using BignumPtr     = std::unique_ptr<BIGNUM, BignumDeleter>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter>;
using CurlPtr       = std::unique_ptr<CURL, CurlDeleter>;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
struct OsslParamBldDeleter {void operator()(OSSL_PARAM_BLD *p) const noexcept {OSSL_PARAM_BLD_free(p);}};
struct OsslParamDeleter    {void operator()(OSSL_PARAM *p)     const noexcept {OSSL_PARAM_free(p);}};
using OsslParamBldPtr = std::unique_ptr<OSSL_PARAM_BLD, OsslParamBldDeleter>;
using OsslParamPtr    = std::unique_ptr<OSSL_PARAM, OsslParamDeleter>;
#else
struct RsaDeleter {void operator()(RSA *p) const noexcept {RSA_free(p);}};
using RsaPtr = std::unique_ptr<RSA, RsaDeleter>;
#endif

void Fatal(XrdOucErrInfo *erp, const char *eMsg, int rc, bool hdr=true)
{
   if (!erp) std::cerr <<(hdr ? "Secoidc: " : "") <<eMsg <<"\n" <<std::flush;
      else {const char *eVec[2] = {(hdr ? "Secoidc: " : ""), eMsg};
            erp->setErrInfo(rc, eVec, 2);
           }
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

std::string sha256hex(const char *data, size_t len)
{
   unsigned char md[EVP_MAX_MD_SIZE];
   unsigned int mdLen = 0;
   if (!EVP_Digest(data, len, md, &mdLen, EVP_sha256(), nullptr))
      return std::string(data, len);
   static const char hex[] = "0123456789abcdef";
   std::string out;
   out.reserve(mdLen * 2);
   for (unsigned int i = 0; i < mdLen; ++i)
      {out.push_back(hex[md[i] >> 4]);
       out.push_back(hex[md[i] & 0xf]);
      }
   return out;
}

bool parseJsonObject(const std::string &json, nlohmann::json &obj)
{
   obj = nlohmann::json::parse(json, 0, false);
   return !obj.is_discarded() && obj.is_object();
}

bool getStringClaim(const nlohmann::json &obj, const char *claim, std::string &val)
{
   if (!obj.is_object()) return false;
   auto it = obj.find(claim);
   if (it == obj.end() || !it->is_string()) return false;
   val = it->get<std::string>();
   return !val.empty();
}

bool getStringClaim(const std::string &json, const char *claim, std::string &val)
{
   nlohmann::json obj;
   if (!parseJsonObject(json, obj)) return false;
   return getStringClaim(obj, claim, val);
}

bool getEntityClaimValue(const nlohmann::json &obj, const char *claim,
                         std::string &val)
{
   if (!obj.is_object()) return false;
   auto it = obj.find(claim);
   if (it == obj.end()) return false;
   if (it->is_string())
      {
       val = it->get<std::string>();
       return !val.empty();
      }
   if (it->is_array())
      {
       val.clear();
       for (const auto &elem : *it)
          {
           if (!elem.is_string()) continue;
           const std::string one = elem.get<std::string>();
           if (one.empty()) continue;
           if (!val.empty()) val.push_back(' ');
           val += one;
          }
       return !val.empty();
      }
   return false;
}

struct EntityClaimMapping {
   std::string jwtClaim;
   std::string attrKey;
};

std::vector<EntityClaimMapping> EntityClaimMappings;

bool isValidJwtClaimName(const std::string &claim)
{
   if (claim.empty()) return false;
   for (unsigned char c : claim)
      if (!isalnum(c) && c != '_' && c != '-') return false;
   return true;
}

bool isSafeEntityAttrKey(const std::string &key)
{
   if (key.empty()) return false;
   for (unsigned char c : key)
      if (!isalnum(c) && c != '.' && c != '_' && c != '-') return false;
   return true;
}

bool isSafeEntityAttrValue(const std::string &val)
{
   if (val.empty() || val.size() > 4096) return false;
   for (unsigned char c : val)
      if (c < 0x20 || c == 0x7f) return false;
   return true;
}

bool parseEntityClaimSpec(const std::string &specIn, EntityClaimMapping &out,
                          std::string &emsg)
{
   std::string spec = XrdOucUtils::trimCopy(specIn);
   if (spec.empty())
      {emsg = "empty entity-claim entry";
       return false;
      }
   size_t eq = spec.find('=');
   if (eq != std::string::npos)
      {out.jwtClaim = XrdOucUtils::trimCopy(spec.substr(0, eq));
       out.attrKey = XrdOucUtils::trimCopy(spec.substr(eq + 1));
      }
   else
      {out.jwtClaim = spec;
       out.attrKey = "token." + spec;
      }
   if (!isValidJwtClaimName(out.jwtClaim))
      {emsg = "invalid JWT claim name in entity-claim: " + out.jwtClaim;
       return false;
      }
   if (!isSafeEntityAttrKey(out.attrKey))
      {emsg = "invalid entity attribute key in entity-claim: " + out.attrKey;
       return false;
      }
   return true;
}

bool storeEntityClaimEntry(const std::string &spec, std::string &emsg)
{
   EntityClaimMapping mapping;
   if (!parseEntityClaimSpec(spec, mapping, emsg)) return false;
   EntityClaimMappings.push_back(mapping);
   return true;
}

bool splitCSV(const std::string &val, std::vector<std::string> &out);

bool canonicalAbsolutePath(const std::string &path, std::string &result)
{
   if (path.empty() || path[0] != '/') return false;

   size_t pos = 0;
   std::vector<std::string> components;
   do
      {
       while (pos < path.size() && path[pos] == '/') pos++;
       size_t nextPos = path.find('/', pos);
       std::string component = path.substr(pos, nextPos - pos);
       pos = nextPos;
       if (component.empty() || component == ".") continue;
       if (component == "..")
          {if (!components.empty()) components.pop_back();
          }
       else components.push_back(component);
      }
   while (pos != std::string::npos);

   if (components.empty())
      {result = "/";
       return true;
      }
   result.clear();
   for (const auto &component : components) result += "/" + component;
   return true;
}

bool setIssuerBasePath(const std::string &val, std::string &target,
                       std::string &emsg)
{
   if (!canonicalAbsolutePath(val, target))
      {emsg = "invalid absolute path: " + val;
       return false;
      }
   return true;
}

bool appendIssuerPathOption(const std::string &val,
                            std::vector<std::string> &target,
                            std::string &emsg)
{
   std::string normalized;
   if (!canonicalAbsolutePath(val, normalized))
      {emsg = "invalid absolute path: " + val;
       return false;
      }
   target.push_back(normalized);
   return true;
}

std::string pathsToJsonArray(const std::vector<std::string> &paths)
{
   nlohmann::json arr = nlohmann::json::array();
   for (const auto &path : paths) arr.push_back(path);
   return arr.dump();
}

void collectEntityClaims(const std::string &payloadJSON,
                         std::map<std::string, std::string> &out)
{
   if (EntityClaimMappings.empty()) return;
   nlohmann::json payloadObj;
   if (!parseJsonObject(payloadJSON, payloadObj)) return;
   for (const auto &mapping : EntityClaimMappings)
      {std::string val;
       if (getEntityClaimValue(payloadObj, mapping.jwtClaim.c_str(), val)
       &&  isSafeEntityAttrValue(val))
          out[mapping.attrKey] = val;
      }
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

std::atomic<int> expiry{1}; // 1=require, 0=ignore, -1=optional
std::atomic<int> MaxTokSize{8192};
std::atomic<int> ClockSkew{60};
std::atomic JwksRefresh{300};
bool customIdentityClaims = false;
std::atomic<bool> DebugToken{false};
std::atomic<bool> DebugTokenClaims{false};
std::atomic<int> TokenCacheMax{10000};
std::atomic TokenCacheNoExpTTL{60};
std::string JwksCacheFile;
int JwksCacheTTL = 0;
std::vector<std::string> IdentityClaims = {
   "preferred_username", "upn", "username", "name", "sub"
};

void freeKeys(std::map<std::string, EvpPkeyPtr> &keys);

struct IssuerPolicy {
   std::string issuer;
   std::vector<std::string> audiences;
   std::string oidcConfigURL;
   std::string jwksURL;
   std::string forcedIdentityClaim;
   std::string basePath;
   std::vector<std::string> restrictedPaths;

   std::mutex keysMtx;
   std::map<std::string, EvpPkeyPtr> jwksKeys;
   time_t lastJwksLoad = 0;

   ~IssuerPolicy()
      {
       std::scoped_lock lock(keysMtx);
       freeKeys(jwksKeys);
      }
};

std::vector<std::shared_ptr<IssuerPolicy>> IssuerPolicies;
std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> IssuerPolicyByIssuer;
std::unordered_map<std::string, std::string> EmailIdentityMap;
std::mutex ConfigMtx;
void addIssuerPolicyEntityAttrs(const std::shared_ptr<IssuerPolicy> &policy,
                                std::map<std::string, std::string> &out)
{
   if (!policy) return;
   if (!policy->basePath.empty() && isSafeEntityAttrValue(policy->basePath))
      out["base_path"] = policy->basePath;
   if (!policy->restrictedPaths.empty())
      {std::string json = pathsToJsonArray(policy->restrictedPaths);
       if (isSafeEntityAttrValue(json)) out["restricted_path"] = json;
      }
}

bool lookupIssuerPolicyFromPayload(const std::string &payloadJSON,
                                   std::shared_ptr<IssuerPolicy> &policy)
{
   nlohmann::json payloadObj;
   if (!parseJsonObject(payloadJSON, payloadObj)) return false;
   std::string iss;
   if (!getStringClaim(payloadObj, "iss", iss)) return false;
   std::scoped_lock lock(ConfigMtx);
   auto pIt = IssuerPolicyByIssuer.find(iss);
   if (pIt == IssuerPolicyByIssuer.end()) return false;
   policy = pIt->second;
   return true;
}

void populateEntityAttrs(const std::string &payloadJSON,
                         std::map<std::string, std::string> &out)
{
   out.clear();
   collectEntityClaims(payloadJSON, out);
   std::shared_ptr<IssuerPolicy> policy;
   if (lookupIssuerPolicyFromPayload(payloadJSON, policy))
      addIssuerPolicyEntityAttrs(policy, out);
}

std::string OIDCConfigPath = "/etc/xrootd/oidc.cfg";
bool OIDCConfigWatch = false;
bool OIDCConfigStatValid = false;
ino_t OIDCConfigIno = 0;
time_t OIDCConfigMTime = 0;

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

void freeKeys(std::map<std::string, EvpPkeyPtr> &keys)
{
   // The map now owns each EVP_PKEY through a unique_ptr, so clearing it
   // releases every key without manual EVP_PKEY_free() calls.
   keys.clear();
}

void clearIssuerPolicies()
{
   for (auto &p : IssuerPolicies)
      {
       if (!p) continue;
       std::scoped_lock lock(p->keysMtx);
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

std::string normalizeEmailKey(const std::string &email)
{
   return XrdOucUtils::toLowerCopy(XrdOucUtils::trimCopy(email));
}

bool clientDebugEnabled()
{
   const char *dbg = getenv("XrdSecDEBUG");
   if (!dbg || !*dbg) return false;
   std::string v = XrdOucUtils::toLowerCopy(XrdOucUtils::trimCopy(dbg));
   return !(v == "0" || v == "off" || v == "false" || v == "no");
}

void clientDebugLog(const std::string &msg)
{
   if (!clientDebugEnabled()) return;
   std::cerr << "Secoidc: " << msg << "\n" << std::flush;
}

void appendCliOpt(std::string &opts, const std::string &key, const std::string *val = nullptr)
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
   if (XrdOucUtils::toLowerCopy(name.substr(0, 6)) != "issuer") return false;
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

// Parse "email=username" and store in target (email key is normalized).
bool addEmailMapEntry(const std::string &spec,
                      std::unordered_map<std::string, std::string> &target,
                      std::string &emsg)
{
   size_t eq = spec.find('=');
   if (eq == std::string::npos)
      {emsg = "email-map entry must be email=username";
       return false;
      }
   std::string email = normalizeEmailKey(stripQuotes(XrdOucUtils::trimCopy(spec.substr(0, eq))));
   std::string uname = XrdOucUtils::trimCopy(stripQuotes(spec.substr(eq + 1)));
   if (email.empty() || uname.empty())
      {emsg = "invalid email-map entry";
       return false;
      }
   target[email] = uname;
   return true;
}

bool storeEmailMapEntry(const std::string &spec, std::string &emsg)
{
   std::scoped_lock lock(ConfigMtx);
   return addEmailMapEntry(spec, EmailIdentityMap, emsg);
}

bool addIniKV(const std::string &keyIn, const std::string &valIn, bool inIssuer,
              std::string &opts, std::string &emsg)
{
   std::string key = XrdOucUtils::toLowerCopy(XrdOucUtils::trimCopy(keyIn));
   std::string val = XrdOucUtils::trimCopy(stripQuotes(XrdOucUtils::trimCopy(valIn)));
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
           std::string one = XrdOucUtils::trimCopy(val.substr(pos, comma == std::string::npos ? std::string::npos
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
   if (key == "base-path" || key == "base_path")
      {
       if (!inIssuer) {emsg = "base_path requires an [issuer ...] section or issuer=..."; return false;}
       std::string normalized;
       if (!canonicalAbsolutePath(val, normalized))
          {emsg = "invalid absolute path: " + val;
           return false;
          }
       appendCliOpt(opts, "-base-path", &normalized);
       return true;
      }
   if (key == "restricted-path" || key == "restricted_path")
      {
       if (!inIssuer) {emsg = "restricted_path requires an [issuer ...] section or issuer=..."; return false;}
       std::string normalized;
       if (!canonicalAbsolutePath(val, normalized))
          {emsg = "invalid absolute path: " + val;
           return false;
          }
       appendCliOpt(opts, "-restricted-path", &normalized);
       return true;
      }
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
           std::string one = XrdOucUtils::trimCopy(val.substr(pos, comma == std::string::npos ? std::string::npos
                                                                                  : comma - pos));
           if (!one.empty()) appendCliOpt(opts, "-identity-claim", &one);
           if (comma == std::string::npos) break;
           pos = comma + 1;
          }
       return true;
      }

   if (key == "entity-claim")
      {
       size_t pos = 0;
       while (pos <= val.size())
          {
           size_t comma = val.find(',', pos);
           std::string one = XrdOucUtils::trimCopy(val.substr(pos, comma == std::string::npos ? std::string::npos
                                                                                  : comma - pos));
           if (!one.empty()) appendCliOpt(opts, "-entity-claim", &one);
           if (comma == std::string::npos) break;
           pos = comma + 1;
          }
       return true;
      }

   if (key == "debug-token" || key == "show-token-claims")
      {
       bool enabled = false;
       if (!XrdOucUtils::parseBool(val, enabled))
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

struct SafeFileResult {
   std::string contents;
   ino_t ino;
   time_t mtime;
   bool found;
};

bool safeReadConfigFile(const char *path, SafeFileResult &result, std::string &emsg)
{
   result.found = false;
   result.contents.clear();
   result.ino = 0;
   result.mtime = 0;

   int flags = O_RDONLY;
#ifdef O_NOFOLLOW
   flags |= O_NOFOLLOW;
#endif
   int fd = open(path, flags);
   if (fd < 0)
      {
       if (errno == ENOENT) return true;
       emsg = std::string("unable to open ") + path + ": " + strerror(errno);
       return false;
      }

   struct stat st;
   if (fstat(fd, &st) != 0)
      {int rc = errno; close(fd);
       emsg = std::string("unable to stat ") + path + ": " + strerror(rc);
       return false;
      }

   if (!S_ISREG(st.st_mode))
      {close(fd);
       emsg = std::string(path) + ": config path must be a regular file";
       return false;
      }

   uid_t euid = geteuid();
   if (st.st_uid != euid)
      {close(fd);
       emsg = std::string(path) + ": config file owner uid "
            + std::to_string(static_cast<unsigned long long>(st.st_uid))
            + " does not match process euid "
            + std::to_string(static_cast<unsigned long long>(euid));
       return false;
      }

   if (st.st_mode & (S_IWGRP | S_IWOTH))
      {close(fd);
       emsg = std::string(path) + ": must not be writable by group/other";
       return false;
      }

   if (st.st_size < 0 || st.st_size > 10 * 1024 * 1024)
      {close(fd);
       emsg = std::string(path) + ": config file too large";
       return false;
      }

   size_t fsize = static_cast<size_t>(st.st_size);
   result.contents.resize(fsize);
   size_t got = 0;
   while (got < fsize)
      {ssize_t rd = read(fd, &result.contents[got], fsize - got);
       if (rd < 0)
          {if (errno == EINTR) continue;
           int rc = errno; close(fd);
           emsg = std::string("read error on ") + path + ": " + strerror(rc);
           return false;
          }
       if (rd == 0) break;
       got += static_cast<size_t>(rd);
      }
   close(fd);
   result.contents.resize(got);
   result.ino = st.st_ino;
   result.mtime = st.st_mtime;
   result.found = true;
   return true;
}

bool loadOIDCIniAsArgs(const char *path, std::string &opts, bool &found,
                       std::string &emsg, ino_t *inoOut = nullptr,
                       time_t *mtimeOut = nullptr)
{
   opts.clear();
   emsg.clear();
   found = false;

   SafeFileResult sfr;
   if (!safeReadConfigFile(path, sfr, emsg)) return false;
   if (!sfr.found) return true;
   found = true;
   if (inoOut) *inoOut = sfr.ino;
   if (mtimeOut) *mtimeOut = sfr.mtime;

   std::istringstream in(sfr.contents);

   bool inIssuer = false;
   bool inEmailMap = false;
   std::string line;
   int lineNo = 0;
   while (std::getline(in, line))
      {
       ++lineNo;
       std::string t = XrdOucUtils::trimCopy(line);
       if (t.empty() || t[0] == '#' || t[0] == ';') continue;

       if (t.front() == '[' && t.back() == ']')
          {
           std::string sec = XrdOucUtils::trimCopy(t.substr(1, t.size() - 2));
           if (XrdOucUtils::toLowerCopy(sec) == "global")
              {inIssuer = false;
               inEmailMap = false;
               continue;
              }
           if (XrdOucUtils::toLowerCopy(sec) == "email-map")
              {inIssuer = false;
               inEmailMap = true;
               continue;
              }
           if (startsWithIssuerSection(sec))
              {
               std::string val = XrdOucUtils::trimCopy(sec.substr(6));
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
           std::string localErr;
           if (!addEmailMapEntry(t, EmailIdentityMap, localErr))
              {emsg = std::string(path) + ":" + std::to_string(lineNo)
                    + ": " + localErr;
               return false;
              }
           continue;
          }

       std::string key = (eq == std::string::npos ? t : t.substr(0, eq));
       std::string val = (eq == std::string::npos ? "true" : t.substr(eq + 1));
       std::string localErr;
       if (!addIniKV(key, val, inIssuer, opts, localErr))
          {emsg = std::string(path) + ":" + std::to_string(lineNo) + ": " + localErr;
           return false;
          }
       if (XrdOucUtils::toLowerCopy(XrdOucUtils::trimCopy(key)) == "issuer") inIssuer = true;
      }
   return true;
}

bool refreshJWKSForPolicy(std::shared_ptr<IssuerPolicy> policy, bool force,
                          std::string &emsg);

bool splitCSV(const std::string &val, std::vector<std::string> &out)
{
   out.clear();
   size_t pos = 0;
   while (pos <= val.size())
      {
       size_t comma = val.find(',', pos);
       if (std::string one = XrdOucUtils::trimCopy(val.substr(pos, comma == std::string::npos ? std::string::npos
                                                                                 : comma - pos));
           !one.empty())
          out.push_back(one);
       if (comma == std::string::npos) break;
       pos = comma + 1;
      }
   return true;
}

bool parseReloadableIniSections(const char *path,
                                const std::string &fileContents,
                                std::vector<std::shared_ptr<IssuerPolicy>> &outPolicies,
                                std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> &outByIssuer,
                                std::unordered_map<std::string, std::string> &outEmailMap,
                                std::string &emsg)
{
   outPolicies.clear();
   outByIssuer.clear();
   outEmailMap.clear();

   std::istringstream in(fileContents);

   bool inIssuer = false;
   bool inEmailMap = false;
   std::shared_ptr<IssuerPolicy> curPolicy;
   std::string line;
   int lineNo = 0;
   while (std::getline(in, line))
      {
       ++lineNo;
       std::string t = XrdOucUtils::trimCopy(line);
       if (t.empty() || t[0] == '#' || t[0] == ';') continue;

       if (t.front() == '[' && t.back() == ']')
          {
           inIssuer = false;
           inEmailMap = false;
           curPolicy.reset();
           std::string sec = XrdOucUtils::trimCopy(t.substr(1, t.size() - 2));
           if (XrdOucUtils::toLowerCopy(sec) == "global") continue;
           if (XrdOucUtils::toLowerCopy(sec) == "email-map")
              {inEmailMap = true; continue;}
           if (startsWithIssuerSection(sec))
              {
               std::string iss = XrdOucUtils::trimCopy(sec.substr(6));
               if (!iss.empty())
                  iss = XrdOucUtils::trimCopy(stripQuotes(iss));
               if (iss.empty()) {inIssuer = true; continue;}
               auto it = outByIssuer.find(iss);
               if (it != outByIssuer.end()) curPolicy = it->second;
               else
                  {
                   curPolicy = std::make_shared<IssuerPolicy>();
                   curPolicy->issuer = iss;
                   outPolicies.push_back(curPolicy);
                   outByIssuer[iss] = curPolicy;
                  }
               inIssuer = true;
               continue;
              }
           emsg = std::string(path) + ":" + std::to_string(lineNo)
                + ": unsupported section '" + sec + "'";
           return false;
          }

       size_t eq = t.find('=');
       std::string key = XrdOucUtils::toLowerCopy(XrdOucUtils::trimCopy(eq == std::string::npos ? t : t.substr(0, eq)));
       std::string val = XrdOucUtils::trimCopy(stripQuotes(eq == std::string::npos ? "true" : t.substr(eq + 1)));

       if (inEmailMap)
          {
           std::string localErr;
           if (!addEmailMapEntry(t, outEmailMap, localErr))
              {emsg = std::string(path) + ":" + std::to_string(lineNo)
                    + ": " + localErr;
               return false;
              }
           continue;
          }

       if (!inIssuer) continue; // reloadable sections only

       if (key == "issuer")
          {
           if (val.empty())
              {emsg = std::string(path) + ":" + std::to_string(lineNo) + ": issuer value is empty";
               return false;
              }
           auto it = outByIssuer.find(val);
           if (it != outByIssuer.end()) curPolicy = it->second;
           else
              {
               curPolicy = std::make_shared<IssuerPolicy>();
               curPolicy->issuer = val;
               outPolicies.push_back(curPolicy);
               outByIssuer[val] = curPolicy;
              }
           continue;
          }
       if (!curPolicy)
          {
           emsg = std::string(path) + ":" + std::to_string(lineNo)
                + ": issuer-scoped key requires issuer to be set";
           return false;
          }
       if (key == "audience")
          {
           std::vector<std::string> items;
           splitCSV(val, items);
           for (const auto &one : items) curPolicy->audiences.push_back(one);
           continue;
          }
       if (key == "oidc-config-url")
          {curPolicy->oidcConfigURL = val; continue;}
       if (key == "jwks-url")
          {curPolicy->jwksURL = val; continue;}
       if (key == "forced-identity-claim")
          {curPolicy->forcedIdentityClaim = val; continue;}
       if (key == "base-path" || key == "base_path")
          {
           std::string localErr;
           if (!setIssuerBasePath(val, curPolicy->basePath, localErr))
              {emsg = std::string(path) + ":" + std::to_string(lineNo)
                    + ": " + localErr;
               return false;
              }
           continue;
          }
       if (key == "restricted-path" || key == "restricted_path")
          {
           std::string localErr;
           if (!appendIssuerPathOption(val, curPolicy->restrictedPaths, localErr))
              {emsg = std::string(path) + ":" + std::to_string(lineNo)
                    + ": " + localErr;
               return false;
              }
           continue;
          }
       // Ignore unsupported/global keys in reload mode by design.
      }

   return true;
}

bool validateAndWarmReloadableConfig(std::vector<std::shared_ptr<IssuerPolicy>> &policies,
                                     std::string &emsg)
{
   if (policies.empty())
      {emsg = "At least one issuer must be configured in reloadable config";
       return false;
      }
   for (auto &policy : policies)
      {
       if (policy->oidcConfigURL.empty() && !policy->issuer.empty())
          policy->oidcConfigURL = joinURL(policy->issuer, "/.well-known/openid-configuration");
       if (policy->oidcConfigURL.empty() && policy->jwksURL.empty())
          {emsg = "issuer '" + policy->issuer + "' requires oidc-config-url or jwks-url";
           return false;
          }
       if ((!policy->oidcConfigURL.empty() && !hasPrefix(policy->oidcConfigURL.c_str(), "https://"))
       ||  (!policy->jwksURL.empty() && !hasPrefix(policy->jwksURL.c_str(), "https://")))
          {emsg = "issuer '" + policy->issuer + "' has non-https OIDC/JWKS URL";
           return false;
          }
       if (!refreshJWKSForPolicy(policy, true, emsg))
          {emsg = "issuer '" + policy->issuer + "': " + emsg;
           return false;
          }
      }
   return true;
}

void clearTokenCache()
{
   std::scoped_lock lock(TokenCacheMtx);
   TokenCache.clear();
}

// Lightweight metadata probe used on the authentication hot path: it does not
// read the file contents, so an unchanged config costs only open()+fstat().
bool statConfigFileMeta(const char *path, ino_t &ino, time_t &mtime,
                        bool &found, std::string &emsg)
{
   ino = 0;
   mtime = 0;
   found = false;

   int flags = O_RDONLY;
#ifdef O_NOFOLLOW
   flags |= O_NOFOLLOW;
#endif
   int fd = open(path, flags);
   if (fd < 0)
      {if (errno == ENOENT) return true;
       emsg = std::string("unable to open ") + path + ": " + strerror(errno);
       return false;
      }
   struct stat st;
   if (fstat(fd, &st) != 0)
      {int rc = errno; close(fd);
       emsg = std::string("unable to stat ") + path + ": " + strerror(rc);
       return false;
      }
   close(fd);
   if (!S_ISREG(st.st_mode))
      {emsg = std::string(path) + ": config path must be a regular file";
       return false;
      }
   ino = st.st_ino;
   mtime = st.st_mtime;
   found = true;
   return true;
}

void maybeReloadOIDCFileConfig()
{
   std::string cfgPath;
   {
      std::scoped_lock lock(ConfigMtx);
      if (!OIDCConfigWatch || OIDCConfigPath.empty()) return;
      cfgPath = OIDCConfigPath;
   }
   std::string emsg;

   // Fast path: stat only. Avoid reading the (potentially large) file on every
   // authentication; the full read+parse happens only when ino/mtime changes.
   ino_t curIno = 0;
   time_t curMtime = 0;
   bool curFound = false;
   if (!statConfigFileMeta(cfgPath.c_str(), curIno, curMtime, curFound, emsg))
      {
       OIDCLog.Emsg("Auth", "oidc", ("config stat failed: " + emsg).c_str());
       return;
      }
   if (!curFound)
      {
       OIDCLog.Emsg("Auth", "oidc", ("config file disappeared: " + cfgPath).c_str());
       return;
      }
   {
      std::scoped_lock lock(ConfigMtx);
      if (OIDCConfigStatValid && curIno == OIDCConfigIno && curMtime == OIDCConfigMTime) return;
   }

   // Config changed (or first observation): read and parse the full contents.
   SafeFileResult sfr;
   if (!safeReadConfigFile(cfgPath.c_str(), sfr, emsg))
      {
       OIDCLog.Emsg("Auth", "oidc", ("config read failed: " + emsg).c_str());
       return;
      }
   if (!sfr.found)
      {
       OIDCLog.Emsg("Auth", "oidc", ("config file disappeared: " + cfgPath).c_str());
       return;
      }

   std::vector<std::shared_ptr<IssuerPolicy>> newPolicies;
   std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> newByIssuer;
   std::unordered_map<std::string, std::string> newEmailMap;
   if (!parseReloadableIniSections(cfgPath.c_str(), sfr.contents, newPolicies, newByIssuer, newEmailMap, emsg))
      {
       OIDCLog.Emsg("Auth", "oidc", ("config reload parse failed: " + emsg).c_str());
       return;
      }
   if (!validateAndWarmReloadableConfig(newPolicies, emsg))
      {
       OIDCLog.Emsg("Auth", "oidc", ("config reload validation failed: " + emsg).c_str());
       return;
      }

   std::scoped_lock lock(ConfigMtx);
   IssuerPolicies.swap(newPolicies);
   IssuerPolicyByIssuer.swap(newByIssuer);
   EmailIdentityMap.swap(newEmailMap);
   OIDCConfigIno = sfr.ino;
   OIDCConfigMTime = sfr.mtime;
   OIDCConfigStatValid = true;
   clearTokenCache();
   OIDCLog.Emsg("Auth", "oidc", ("config reloaded from " + cfgPath).c_str());
}

// Maximum bytes we will buffer from any single HTTP response (4 MiB).
const size_t kFetchBodyLimit = 4 * 1024 * 1024;

struct FetchSink {
   std::string *dst;
   bool truncated;
};

size_t curlWriteCB(char *ptr, size_t sz, size_t nmemb, void *ud)
{
   FetchSink *sink = static_cast<FetchSink *>(ud);
   if (nmemb != 0 && sz > SIZE_MAX / nmemb)
      {sink->truncated = true; return 0;}
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
   CurlPtr c(curl_easy_init());
   if (!c) {emsg = "curl init failed"; return false;}
   FetchSink sink;
   sink.dst = &body;
   sink.truncated = false;
   curl_easy_setopt(c.get(), CURLOPT_URL, url.c_str());
   // Follow redirects but only to HTTPS targets; cap at 3 hops.
   curl_easy_setopt(c.get(), CURLOPT_FOLLOWLOCATION, 1L);
   curl_easy_setopt(c.get(), CURLOPT_MAXREDIRS, 3L);
#if CURL_AT_LEAST_VERSION(7, 85, 0)
   curl_easy_setopt(c.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
   curl_easy_setopt(c.get(), CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
   curl_easy_setopt(c.get(), CURLOPT_TIMEOUT, 15L);
   curl_easy_setopt(c.get(), CURLOPT_CONNECTTIMEOUT, 5L);
   curl_easy_setopt(c.get(), CURLOPT_WRITEFUNCTION, curlWriteCB);
   curl_easy_setopt(c.get(), CURLOPT_WRITEDATA, &sink);
   curl_easy_setopt(c.get(), CURLOPT_SSL_VERIFYPEER, 1L);
   curl_easy_setopt(c.get(), CURLOPT_SSL_VERIFYHOST, 2L);
   CURLcode rc = curl_easy_perform(c.get());
   long httpCode = 0;
   curl_easy_getinfo(c.get(), CURLINFO_RESPONSE_CODE, &httpCode);
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

bool getUintClaim(const nlohmann::json &obj, const char *claim, uint64_t &out)
{
   if (!obj.is_object()) return false;
   auto it = obj.find(claim);
   if (it == obj.end()) return false;
   if (it->is_number_unsigned())
      {out = it->get<uint64_t>();
       return true;
      }
   if (it->is_number_integer())
      {int64_t v = it->get<int64_t>();
       if (v < 0) return false;
       out = static_cast<uint64_t>(v);
       return true;
      }
   return false;
}

bool hasStringInArrayClaim(const nlohmann::json &obj, const char *claim,
                           const std::string &want)
{
   if (!obj.is_object() || !obj.contains(claim)) return false;
   const nlohmann::json &arr = obj.at(claim);
   if (!arr.is_array()) return false;
   for (size_t i = 0; i < arr.size(); ++i)
      {
       if (arr[i].is_string() && arr[i].get<std::string>() == want) return true;
      }
   return false;
}

bool verifyRS256(EVP_PKEY *pkey, std::string_view signedData,
                 std::string_view sig)
{
   EvpMdCtxPtr ctx(EVP_MD_CTX_new());
   if (!ctx) return false;
   return EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, pkey) == 1
       && EVP_DigestVerifyUpdate(ctx.get(), signedData.data(), signedData.size()) == 1
       && EVP_DigestVerifyFinal(ctx.get(),
            reinterpret_cast<const unsigned char *>(sig.data()), sig.size()) == 1;
}

EvpPkeyPtr makeRSAPublicKey(std::string_view modulus,
                            std::string_view exponent)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
   EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr));
   if (!ctx) return nullptr;
   BignumPtr bnN(BN_bin2bn(reinterpret_cast<const unsigned char *>(modulus.data()),
                           modulus.size(), nullptr));
   BignumPtr bnE(BN_bin2bn(reinterpret_cast<const unsigned char *>(exponent.data()),
                           exponent.size(), nullptr));
   OsslParamBldPtr bld(OSSL_PARAM_BLD_new());
   if (!bnN || !bnE || !bld) return nullptr;
   if (OSSL_PARAM_BLD_push_BN(bld.get(), "n", bnN.get()) <= 0
   ||  OSSL_PARAM_BLD_push_BN(bld.get(), "e", bnE.get()) <= 0)
      return nullptr;
   OsslParamPtr params(OSSL_PARAM_BLD_to_param(bld.get()));
   if (!params) return nullptr;
   EVP_PKEY *raw = nullptr;
   if (EVP_PKEY_fromdata_init(ctx.get()) <= 0
   ||  EVP_PKEY_fromdata(ctx.get(), &raw, EVP_PKEY_PUBLIC_KEY, params.get()) <= 0)
      return nullptr;
   return EvpPkeyPtr(raw);
#else
   BignumPtr bnN(BN_bin2bn(reinterpret_cast<const unsigned char *>(modulus.data()),
                           modulus.size(), nullptr));
   BignumPtr bnE(BN_bin2bn(reinterpret_cast<const unsigned char *>(exponent.data()),
                           exponent.size(), nullptr));
   if (!bnN || !bnE) return nullptr;

   RsaPtr rsa(RSA_new());
   if (!rsa) return nullptr;
   if (RSA_set0_key(rsa.get(), bnN.get(), bnE.get(), nullptr) != 1) return nullptr;
   // Ownership of the bignums has transferred to rsa.
   bnN.release();
   bnE.release();

   EvpPkeyPtr pkey(EVP_PKEY_new());
   if (!pkey) return nullptr;
   if (EVP_PKEY_assign_RSA(pkey.get(), rsa.get()) != 1) return nullptr;
   // Ownership of rsa has transferred to pkey.
   rsa.release();
   return pkey;
#endif
}

bool loadJWKS(const std::string &json, std::map<std::string, EvpPkeyPtr> &keys,
              std::string &emsg)
{
   keys.clear();
   nlohmann::json root;
   if (!parseJsonObject(json, root))
      {
       emsg = "invalid JWKS JSON";
       return false;
      }
   auto kIt = root.find("keys");
   if (kIt == root.end() || !kIt->is_array())
      {
       emsg = "invalid JWKS JSON: missing keys array";
       return false;
      }
   for (const auto &kobj : *kIt)
      {
       std::string kty, kid, n, e;
       if (!getStringClaim(kobj, "kty", kty)
       ||  !getStringClaim(kobj, "kid", kid)
       ||  !getStringClaim(kobj, "n", n)
       ||  !getStringClaim(kobj, "e", e))
          continue;
       if (kty != "RSA") continue;
       std::string use;
       if (getStringClaim(kobj, "use", use) && use != "sig") continue;
       std::string nb, eb;
       if (!decodeBase64URL(n, nb) || !decodeBase64URL(e, eb))
          continue;
       EvpPkeyPtr pkey = makeRSAPublicKey(nb, eb);
       if (!pkey) continue;
       // Assigning into the owning map releases any previous key for this kid.
       keys[kid] = std::move(pkey);
      }
   if (keys.empty())
      {emsg = "no usable RSA keys in JWKS";
       return false;
      }
   return true;
}

int jwksCacheEffectiveTTL()
{
   return (JwksCacheTTL > 0 ? JwksCacheTTL : JwksRefresh.load());
}

// Return true iff path is a regular file owned by the effective UID and not
// writable by group or other (same policy as the main config file).
bool checkCacheFilePerms(const char *path, std::string &emsg)
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

   std::string line;
   std::string curIssuer;
   int lineNo = 0;
   while (std::getline(in, line))
      {
       ++lineNo;
       std::string t = XrdOucUtils::trimCopy(line);
       if (t.empty() || t[0] == '#' || t[0] == ';') continue;
       if (t.front() == '[' && t.back() == ']')
          {
           std::string sec = XrdOucUtils::trimCopy(t.substr(1, t.size() - 2));
           if (!startsWithIssuerSection(sec))
              {emsg = "invalid JWKS cache section at line " + std::to_string(lineNo);
               return false;
              }
           curIssuer = stripQuotes(XrdOucUtils::trimCopy(sec.substr(6)));
           if (!curIssuer.empty()) out[curIssuer];
           continue;
          }
       if (curIssuer.empty()) continue;
       size_t eq = t.find('=');
       if (eq == std::string::npos) continue;
       std::string key = XrdOucUtils::toLowerCopy(XrdOucUtils::trimCopy(t.substr(0, eq)));
       std::string val = XrdOucUtils::trimCopy(stripQuotes(t.substr(eq + 1)));
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

   if (!checkCacheFilePerms(JwksCacheFile.c_str(), emsg)) return false;

   std::string tmp = JwksCacheFile + ".tmp";
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

   std::string content;
   for (const auto &it : cache)
      {
       if (it.first.empty() || it.second.jwksB64.empty()) continue;
       content += "[issuer \"" + it.first + "\"]\n";
       content += "fetched_at=" + std::to_string(static_cast<long long>(it.second.fetchedAt)) + "\n";
       if (!it.second.jwksUrl.empty())
          content += "jwks_url=" + it.second.jwksUrl + "\n";
       content += "jwks_b64=" + it.second.jwksB64 + "\n\n";
      }

   const char *ptr = content.data();
   size_t remaining = content.size();
   while (remaining > 0)
      {ssize_t wr = write(tfd, ptr, remaining);
       if (wr < 0)
          {if (errno == EINTR) continue;
           int rc = errno;
           close(tfd); unlink(tmp.c_str());
           emsg = std::string("write error on JWKS cache temp: ") + strerror(rc);
           return false;
          }
       ptr += wr;
       remaining -= static_cast<size_t>(wr);
      }
   close(tfd);
   if (rename(tmp.c_str(), JwksCacheFile.c_str()) != 0)
      {
       emsg = std::string("unable to replace JWKS cache file: ") + strerror(errno);
       unlink(tmp.c_str());
       return false;
      }
   return true;
}

bool loadJWKSFromDiskCacheForPolicy(std::shared_ptr<IssuerPolicy> policy, time_t now,
                                    std::map<std::string, EvpPkeyPtr> &keysOut,
                                    std::string &emsg)
{
   keysOut.clear();
   if (!policy || JwksCacheFile.empty()) return false;
   std::scoped_lock lock(JwksDiskCacheMtx);
   std::unordered_map<std::string, JwksDiskEntry> cache;
   if (!loadJWKSCacheMap(cache, emsg)) return false;
   auto it = cache.find(policy->issuer);
   if (it == cache.end() || it->second.jwksB64.empty()) return false;
   if (int ttl = jwksCacheEffectiveTTL();
       ttl > 0 && (now - it->second.fetchedAt) > ttl) return false;

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
   std::scoped_lock lock(JwksDiskCacheMtx);
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
   std::scoped_lock lock(policy->keysMtx);
   time_t now = time(nullptr);
   if (!force && !policy->jwksKeys.empty()
   && (now - policy->lastJwksLoad) < JwksRefresh) return true;

   if (!force && JwksCacheFile.size())
      {
       std::map<std::string, EvpPkeyPtr> cachedKeys;
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
          {emsg = "jwks_uri missing or empty in OIDC discovery";
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
       std::map<std::string, EvpPkeyPtr> cachedKeys;
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

   std::map<std::string, EvpPkeyPtr> newKeys;
   if (!loadJWKS(jwks, newKeys, emsg)) return false;
   freeKeys(policy->jwksKeys);
   policy->jwksKeys.swap(newKeys);
   policy->lastJwksLoad = now;
   storeJWKSInDiskCacheForPolicy(policy, useJwks, jwks, now);
   return true;
}

bool audienceMatches(std::shared_ptr<IssuerPolicy> policy,
                     const nlohmann::json &payloadObj)
{
   if (!policy || policy->audiences.empty()) return true;
   std::string tokAud;
   bool haveSingleAud = getStringClaim(payloadObj, "aud", tokAud);
   for (const auto &aud : policy->audiences)
      {
       if ((haveSingleAud && tokAud == aud)
       ||   hasStringInArrayClaim(payloadObj, "aud", aud))
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
   maybeReloadOIDCFileConfig();
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
   nlohmann::json hdrObj, payloadObj;
   if (!parseJsonObject(headerJSON, hdrObj) || !parseJsonObject(payloadJSON, payloadObj))
      {emsg = "JWT JSON decode failed"; return false;}

   std::string alg;
   std::string kid;
   if (!getStringClaim(hdrObj, "alg", alg)) {emsg = "JWT alg missing"; return false;}
   if (alg != "RS256") {emsg = "unsupported JWT alg"; return false;}
   getStringClaim(hdrObj, "kid", kid);

   std::string tokIss;
   if (!getStringClaim(payloadObj, "iss", tokIss))
      {emsg = "token issuer missing"; return false;}
   std::shared_ptr<IssuerPolicy> policy;
   {
      std::scoped_lock lock(ConfigMtx);
      auto pIt = IssuerPolicyByIssuer.find(tokIss);
      if (pIt == IssuerPolicyByIssuer.end())
         {emsg = "token issuer not configured";
          return false;
         }
      policy = pIt->second;
   }

   if (!audienceMatches(policy, payloadObj))
      {emsg = "token audience mismatch"; return false;}

   uint64_t exp = 0;
   bool haveExp = getUintClaim(payloadObj, "exp", exp);
   if (expiry > 0 && !haveExp) {emsg = "token expiry missing"; return false;}
   time_t now = time(nullptr);
   // expiry == 0 means "ignore": a present-but-expired exp is not enforced.
   if (expiry != 0 && haveExp && exp + ClockSkew < static_cast<uint64_t>(now))
      {emsg = "token expired"; return false;}
   uint64_t nbf = 0;
   if (getUintClaim(payloadObj, "nbf", nbf)
   &&  nbf > static_cast<uint64_t>(now) + ClockSkew)
      {emsg = "token not yet valid"; return false;}
   if (haveExp) expOut = exp;

   if (!refreshJWKSForPolicy(policy, false, emsg))
      return false;

   std::string signedData = jwt.substr(0, dot2);
   bool verified = false;
   {
      std::scoped_lock lock(policy->keysMtx);
      if (!kid.empty())
         {auto it = policy->jwksKeys.find(kid);
          if (it != policy->jwksKeys.end())
             verified = verifyRS256(it->second.get(), signedData, sig);
         } else {
          for (auto &it : policy->jwksKeys)
              {if (verifyRS256(it.second.get(), signedData, sig))
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
       std::scoped_lock lock(policy->keysMtx);
       if (!kid.empty())
          {auto it = policy->jwksKeys.find(kid);
           if (it != policy->jwksKeys.end())
              verified = verifyRS256(it->second.get(), signedData, sig);
          } else {
           for (auto &it : policy->jwksKeys)
               {if (verifyRS256(it.second.get(), signedData, sig))
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
       if (!getStringClaim(payloadObj, forced.c_str(), identity)
       ||  identity.empty())
          {emsg = "token identity claim missing: " + forced;
           return false;
          }
       if (forced == "email")
          {
           std::string rawEmail = identity;
           std::string emailKey = normalizeEmailKey(rawEmail);
           std::string mappedUser;
           {
              std::scoped_lock lock(ConfigMtx);
              auto mIt = EmailIdentityMap.find(emailKey);
              if (mIt != EmailIdentityMap.end()) mappedUser = mIt->second;
           }
           if (mappedUser.empty())
              {emsg = "token email is not mapped to a username";
               return false;
              }
           identity = mappedUser;
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
          {if (getStringClaim(payloadObj, claim.c_str(), identity) && !identity.empty())
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
   std::scoped_lock lock(TokenCacheMtx);
   auto it = TokenCache.find(tok);
   if (it == TokenCache.end()) return false;
   // Lazily drop a single expired entry instead of scanning the whole cache.
   if (it->second.expiresAt <= now)
      {TokenCache.erase(it);
       return false;
      }
   out = it->second;
   return true;
}

size_t tokenCacheSize()
{
   std::scoped_lock lock(TokenCacheMtx);
   return TokenCache.size();
}

void tokenCacheStore(const std::string &tok, const CachedTokenEntry &entry, uint64_t now)
{
   if (TokenCacheMax == 0) return;
   std::scoped_lock lock(TokenCacheMtx);
   // Only pay for a full scan when we are about to exceed the bound. The common
   // case (cache below capacity) is a single insert with no O(n) work.
   if (TokenCacheMax > 0 && TokenCache.size() >= static_cast<size_t>(TokenCacheMax)
   &&  TokenCache.find(tok) == TokenCache.end())
      {
       pruneTokenCacheLocked(now);
       if (TokenCache.size() >= static_cast<size_t>(TokenCacheMax))
          {
           // Still full of unexpired entries: evict the soonest-to-expire one
           // so the entry we drop is the closest to becoming useless anyway.
           auto victim = TokenCache.begin();
           for (auto it = TokenCache.begin(); it != TokenCache.end(); ++it)
              if (it->second.expiresAt < victim->second.expiresAt) victim = it;
           if (victim != TokenCache.end()) TokenCache.erase(victim);
          }
      }
   TokenCache[tok] = entry;
}

void debugPrintToken(const char *tident, const char *mappedName,
                     const std::string &headerJSON,
                     const std::string &payloadJSON, bool cacheHit,
                     const std::string &identityMethod = std::string())
{
   if (!DebugToken && !DebugTokenClaims) return;
   const char *tid = (tident && *tident ? tident : "oidc");
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
   nlohmann::json hObj, pObj;
   parseJsonObject(headerJSON, hObj);
   parseJsonObject(payloadJSON, pObj);
   std::string kid, alg, typ;
   std::string iss, aud, sub, prefUser, azp;
   uint64_t iat = 0, nbf = 0, exp = 0;
   bool haveIat = getUintClaim(pObj, "iat", iat);
   bool haveNbf = getUintClaim(pObj, "nbf", nbf);
   bool haveExp = getUintClaim(pObj, "exp", exp);

   getStringClaim(hObj, "kid", kid);
   getStringClaim(hObj, "alg", alg);
   getStringClaim(hObj, "typ", typ);
   getStringClaim(pObj, "iss", iss);
   getStringClaim(pObj, "aud", aud);
   getStringClaim(pObj, "sub", sub);
   getStringClaim(pObj, "preferred_username", prefUser);
   getStringClaim(pObj, "azp", azp);

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
   msg += " mapped_name='" + std::string(mappedName ? mappedName : "") + "'";
   if (!identityMethod.empty())
      msg += " identity_method='" + identityMethod + "'";
   msg += " cache_hit=" + std::string(cacheHit ? "1" : "0");
   msg += " cache_hits=" + std::to_string(hits);
   msg += " cache_misses=" + std::to_string(misses);
   msg += " cache_size=" + std::to_string(csize);
   OIDCLog.Emsg("Auth", tid, msg.c_str());
}

// Outcome of trying to apply one Init option token to the global config.
enum class OptResult { NotMine, Ok, Error };

// Consume the next token as an integer in [lo,hi], optionally honouring a
// trailing 'k'/'K' (x1024) multiplier. On failure sets erp and returns false.
bool nextIntArg(XrdOucTokenizer &cfg, XrdOucErrInfo *erp, const char *optName,
                long lo, long hi, bool kSuffix, long &out)
{
   char *arg = cfg.GetToken();
   if (!arg)
      {Fatal(erp, (std::string(optName) + " argument missing").c_str(), EINVAL);
       return false;
      }
   char *endP = nullptr;
   long v = strtol(arg, &endP, 10);
   if (kSuffix && (*endP == 'k' || *endP == 'K')) {v *= 1024; endP++;}
   if (v < lo || v > hi || *endP)
      {Fatal(erp, (std::string(optName) + " argument invalid").c_str(), EINVAL);
       return false;
      }
   out = v;
   return true;
}

// Fetch an argument that must follow optName and requires a current issuer
// policy. Returns nullptr (erp set) on a missing argument or missing issuer.
const char *nextPolicyArg(XrdOucTokenizer &cfg, XrdOucErrInfo *erp,
                          const char *optName,
                          const std::shared_ptr<IssuerPolicy> &curPolicy)
{
   char *arg = cfg.GetToken();
   if (!arg)
      {Fatal(erp, (std::string(optName) + " argument missing").c_str(), EINVAL);
       return nullptr;
      }
   if (!curPolicy)
      {Fatal(erp, (std::string(optName) + " requires a prior -issuer").c_str(), EINVAL);
       return nullptr;
      }
   return arg;
}

OptResult applyNumericInitOpt(const char *val, XrdOucTokenizer &cfg,
                              XrdOucErrInfo *erp)
{
   long v = 0;
   if (!strcmp(val, "-maxsz"))
      {if (!nextIntArg(cfg, erp, "-maxsz", 1, 524288, true, v)) return OptResult::Error;
       MaxTokSize = static_cast<int>(v); return OptResult::Ok;
      }
   if (!strcmp(val, "-jwks-refresh"))
      {if (!nextIntArg(cfg, erp, "-jwks-refresh", 1, LONG_MAX, false, v)) return OptResult::Error;
       JwksRefresh = static_cast<int>(v); return OptResult::Ok;
      }
   if (!strcmp(val, "-jwks-cache-ttl"))
      {if (!nextIntArg(cfg, erp, "-jwks-cache-ttl", 0, LONG_MAX, false, v)) return OptResult::Error;
       JwksCacheTTL = static_cast<int>(v); return OptResult::Ok;
      }
   if (!strcmp(val, "-clock-skew"))
      {if (!nextIntArg(cfg, erp, "-clock-skew", 0, 3600, false, v)) return OptResult::Error;
       ClockSkew = static_cast<int>(v); return OptResult::Ok;
      }
   if (!strcmp(val, "-token-cache-max"))
      {if (!nextIntArg(cfg, erp, "-token-cache-max", 0, LONG_MAX, false, v)) return OptResult::Error;
       TokenCacheMax = static_cast<int>(v); return OptResult::Ok;
      }
   if (!strcmp(val, "-token-cache-noexp-ttl"))
      {if (!nextIntArg(cfg, erp, "-token-cache-noexp-ttl", 0, LONG_MAX, false, v)) return OptResult::Error;
       TokenCacheNoExpTTL = static_cast<int>(v); return OptResult::Ok;
      }
   return OptResult::NotMine;
}

OptResult applyIssuerInitOpt(const char *val, XrdOucTokenizer &cfg,
                             std::shared_ptr<IssuerPolicy> &curPolicy,
                             XrdOucErrInfo *erp)
{
   if (!strcmp(val, "-issuer"))
      {char *arg = cfg.GetToken();
       if (!arg) {Fatal(erp, "-issuer argument missing", EINVAL); return OptResult::Error;}
       auto pIt = IssuerPolicyByIssuer.find(arg);
       if (pIt != IssuerPolicyByIssuer.end()) curPolicy = pIt->second;
       else
          {curPolicy = std::make_shared<IssuerPolicy>();
           curPolicy->issuer = arg;
           IssuerPolicies.push_back(curPolicy);
           IssuerPolicyByIssuer[curPolicy->issuer] = curPolicy;
          }
       return OptResult::Ok;
      }
   if (!strcmp(val, "-audience"))
      {const char *arg = nextPolicyArg(cfg, erp, "-audience", curPolicy);
       if (!arg) return OptResult::Error;
       curPolicy->audiences.push_back(arg); return OptResult::Ok;
      }
   if (!strcmp(val, "-oidc-config-url"))
      {const char *arg = nextPolicyArg(cfg, erp, "-oidc-config-url", curPolicy);
       if (!arg) return OptResult::Error;
       curPolicy->oidcConfigURL = arg; return OptResult::Ok;
      }
   if (!strcmp(val, "-jwks-url"))
      {const char *arg = nextPolicyArg(cfg, erp, "-jwks-url", curPolicy);
       if (!arg) return OptResult::Error;
       curPolicy->jwksURL = arg; return OptResult::Ok;
      }
   if (!strcmp(val, "-forced-identity-claim"))
      {const char *arg = nextPolicyArg(cfg, erp, "-forced-identity-claim", curPolicy);
       if (!arg) return OptResult::Error;
       curPolicy->forcedIdentityClaim = arg; return OptResult::Ok;
      }
   if (!strcmp(val, "-base-path"))
      {const char *arg = nextPolicyArg(cfg, erp, "-base-path", curPolicy);
       if (!arg) return OptResult::Error;
       std::string localErr;
       if (!setIssuerBasePath(arg, curPolicy->basePath, localErr))
          {Fatal(erp, localErr.c_str(), EINVAL); return OptResult::Error;}
       return OptResult::Ok;
      }
   if (!strcmp(val, "-restricted-path"))
      {const char *arg = nextPolicyArg(cfg, erp, "-restricted-path", curPolicy);
       if (!arg) return OptResult::Error;
       std::string localErr;
       if (!appendIssuerPathOption(arg, curPolicy->restrictedPaths, localErr))
          {Fatal(erp, localErr.c_str(), EINVAL); return OptResult::Error;}
       return OptResult::Ok;
      }
   return OptResult::NotMine;
}

OptResult applyOtherInitOpt(const char *val, XrdOucTokenizer &cfg,
                            XrdOucErrInfo *erp)
{
   if (!strcmp(val, "-expiry"))
      {char *arg = cfg.GetToken();
       if (!arg) {Fatal(erp, "-expiry argument missing", EINVAL); return OptResult::Error;}
            if (!strcmp(arg, "ignore"))   expiry =  0;
       else if (!strcmp(arg, "optional")) expiry = -1;
       else if (!strcmp(arg, "required")) expiry =  1;
       else {Fatal(erp, "-expiry argument invalid", EINVAL); return OptResult::Error;}
       return OptResult::Ok;
      }
   if (!strcmp(val, "-config-file"))
      {// Already consumed in the pre-scan; just swallow the argument here.
       if (!cfg.GetToken()) {Fatal(erp, "-config-file argument missing", EINVAL); return OptResult::Error;}
       return OptResult::Ok;
      }
   if (!strcmp(val, "-jwks-cache-file"))
      {char *arg = cfg.GetToken();
       if (!arg) {Fatal(erp, "-jwks-cache-file argument missing", EINVAL); return OptResult::Error;}
       JwksCacheFile = arg; return OptResult::Ok;
      }
   if (!strcmp(val, "-identity-claim"))
      {char *arg = cfg.GetToken();
       if (!arg) {Fatal(erp, "-identity-claim argument missing", EINVAL); return OptResult::Error;}
       if (!customIdentityClaims) {IdentityClaims.clear(); customIdentityClaims = true;}
       IdentityClaims.push_back(arg); return OptResult::Ok;
      }
   if (!strcmp(val, "-entity-claim"))
      {char *arg = cfg.GetToken();
       if (!arg) {Fatal(erp, "-entity-claim argument missing", EINVAL); return OptResult::Error;}
       std::string localErr;
       if (!storeEntityClaimEntry(arg, localErr))
          {Fatal(erp, localErr.c_str(), EINVAL); return OptResult::Error;}
       return OptResult::Ok;
      }
   if (!strcmp(val, "-email-map"))
      {char *arg = cfg.GetToken();
       if (!arg) {Fatal(erp, "-email-map argument missing", EINVAL); return OptResult::Error;}
       std::string localErr;
       if (!storeEmailMapEntry(arg, localErr))
          {Fatal(erp, localErr.c_str(), EINVAL); return OptResult::Error;}
       return OptResult::Ok;
      }
   if (!strcmp(val, "-debug-token"))       {DebugToken = true;       return OptResult::Ok;}
   if (!strcmp(val, "-show-token-claims")) {DebugTokenClaims = true; return OptResult::Ok;}
   return OptResult::NotMine;
}

// Pre-scan: pull out -config-file (consumed here) and accumulate the remaining
// tokens into inlineParms. Returns false (erp set) on a malformed -config-file.
bool prescanInitParms(const char *parms, std::string &selectedCfgPath,
                      bool &requestedCfgOverride, std::string &inlineParms,
                      XrdOucErrInfo *erp)
{
   if (!parms || !*parms) return true;
   std::vector<char> pbuf(parms, parms + strlen(parms) + 1);
   XrdOucTokenizer t(pbuf.data());
   char *tok = nullptr;
   // Multiple sec.protparm lines are joined with newlines by XrdSecServer; each
   // record must be scanned (same pattern as other sec plugins, e.g. gsi).
   while (t.GetLine())
        {
         while ((tok = t.GetToken()))
              {
               if (!strcmp(tok, "-config-file"))
                  {char *path = t.GetToken();
                   if (!path || !*path)
                      {Fatal(erp, "-config-file argument missing", EINVAL);
                       return false;
                      }
                   selectedCfgPath = path;
                   requestedCfgOverride = true;
                   continue;
                  }
               if (!inlineParms.empty()) inlineParms.push_back(' ');
               inlineParms += tok;
              }
        }
   return true;
}

// Parse the effective option string into the global config and issuer policies.
bool parseInitParms(const char *parms, std::shared_ptr<IssuerPolicy> &curPolicy,
                    XrdOucErrInfo *erp)
{
   if (!parms || !*parms) return true;
   std::vector<char> cfgParms(parms, parms + strlen(parms) + 1);
   XrdOucTokenizer cfg(cfgParms.data());
   char *val = nullptr;
   while (cfg.GetLine())
        {
         while ((val = cfg.GetToken()))
              {
               OptResult r = applyNumericInitOpt(val, cfg, erp);
               if (r == OptResult::Error) return false;
               if (r == OptResult::Ok) continue;
               r = applyIssuerInitOpt(val, cfg, curPolicy, erp);
               if (r == OptResult::Error) return false;
               if (r == OptResult::Ok) continue;
               r = applyOtherInitOpt(val, cfg, erp);
               if (r == OptResult::Error) return false;
               if (r == OptResult::Ok) continue;
               XrdOucString eTxt("Invalid parameter - "); eTxt += val;
               Fatal(erp, eTxt.c_str(), EINVAL);
               return false;
              }
        }
   return true;
}

// Resolve discovery URLs and warm the JWKS cache for every configured issuer.
bool validateAndWarmInitIssuers(XrdOucErrInfo *erp)
{
   for (auto &policy : IssuerPolicies)
      {
       if (policy->oidcConfigURL.empty() && !policy->issuer.empty())
          policy->oidcConfigURL = joinURL(policy->issuer, "/.well-known/openid-configuration");
       if (policy->oidcConfigURL.empty() && policy->jwksURL.empty())
          {std::string e = "issuer '" + policy->issuer + "' requires -oidc-config-url or -jwks-url";
           Fatal(erp, e.c_str(), EINVAL); return false;
          }
       if ((!policy->oidcConfigURL.empty() && !hasPrefix(policy->oidcConfigURL.c_str(), "https://"))
       ||  (!policy->jwksURL.empty() && !hasPrefix(policy->jwksURL.c_str(), "https://")))
          {std::string e = "issuer '" + policy->issuer + "' has non-https OIDC/JWKS URL";
           Fatal(erp, e.c_str(), EINVAL); return false;
          }
       std::string emsg;
       if (!refreshJWKSForPolicy(policy, true, emsg))
          {std::string e = "issuer '" + policy->issuer + "': " + emsg;
           Fatal(erp, e.c_str(), EINVAL); return false;
          }
      }
   return true;
}

static const uint64_t oidcVersion = 0;

bool isConfigured()
{
   std::scoped_lock lock(ConfigMtx);
   return !IssuerPolicies.empty();
}

const char *stripToken(const char *bTok, int &sz, int maxLen)
{
   return Strip(bTok, sz, maxLen);
}

bool validateToken(const char *rawTok, std::string &identity, std::string &emsg,
                   uint64_t *expTime,
                   std::map<std::string, std::string> *entityAttrs)
{
   if (!rawTok || !*rawTok)
      {emsg = "missing token";
       return false;
      }

   int tlen = 0;
   const char *tok = Strip(rawTok, tlen);
   if (!tok || tlen <= 0)
      {emsg = "token value malformed";
       return false;
      }

   if (tlen > MaxTokSize)
      {emsg = "token too large";
       return false;
      }

   std::string payloadJSON, headerJSON, msgRC, identityMethod;
   std::string tokKey = sha256hex(tok, static_cast<size_t>(tlen));
   uint64_t now = static_cast<uint64_t>(time(nullptr));
   CachedTokenEntry cached;
   if (tokenCacheLookup(tokKey, now, cached))
      {
       TokenCacheHits++;
       identity = cached.identity;
       if (expTime) *expTime = cached.expiresAt;
       if (entityAttrs) populateEntityAttrs(cached.payloadJSON, *entityAttrs);
       debugPrintToken("oidc", identity.c_str(), cached.headerJSON,
                       cached.payloadJSON, true, cached.identityMethod);
       return true;
      }
   TokenCacheMisses++;

   uint64_t expOut = 0;
   if (!parseAndValidateJWT(tok, payloadJSON, headerJSON, identity, expOut, msgRC,
                             &identityMethod))
      {emsg = msgRC;
       return false;
      }

   debugPrintToken("oidc", identity.c_str(), headerJSON, payloadJSON, false,
                   identityMethod);

   CachedTokenEntry ins;
   ins.identity = identity;
   ins.identityMethod = identityMethod;
   ins.headerJSON = headerJSON;
   ins.payloadJSON = payloadJSON;
   ins.expiresAt = (expOut ? expOut : now + static_cast<uint64_t>(TokenCacheNoExpTTL));
   tokenCacheStore(tokKey, ins, now);
   if (expTime) *expTime = ins.expiresAt;
   if (entityAttrs) populateEntityAttrs(payloadJSON, *entityAttrs);
   return true;
}

char *initSecProtocol(const char *parms, XrdOucErrInfo *erp)
{
   OIDCLog.logger(&OIDCLogger);

   {
      std::scoped_lock lock(ConfigMtx);
      clearIssuerPolicies();
      EmailIdentityMap.clear();
   }
   EntityClaimMappings.clear();
   JwksCacheFile.clear();
   JwksCacheTTL = 0;
   std::shared_ptr<IssuerPolicy> curPolicy;
   std::string fileBackedParms;
   std::string inlineParms;
   std::string selectedCfgPath = "/etc/xrootd/oidc.cfg";
   bool requestedCfgOverride = false;
   bool loadedCfgFile = false;
   OIDCConfigWatch = false;
   OIDCConfigStatValid = false;

   if (!prescanInitParms(parms, selectedCfgPath, requestedCfgOverride, inlineParms, erp))
      return nullptr;

   ino_t cfgIno = 0;
   time_t cfgMtime = 0;
   if (!parms || !*parms || requestedCfgOverride)
      {
       bool cfgFound = false;
       std::string cfgErr;
       if (!loadOIDCIniAsArgs(selectedCfgPath.c_str(), fileBackedParms, cfgFound, cfgErr,
                              &cfgIno, &cfgMtime))
          {Fatal(erp, cfgErr.c_str(), EINVAL);
           return nullptr;
          }
       if (!cfgFound)
          {std::string e = std::string("Missing required OIDC config file: ")
                         + selectedCfgPath;
           Fatal(erp, e.c_str(), ENOENT);
           return nullptr;
          }
       loadedCfgFile = true;
      }

   std::string effectiveParms;
   if (!fileBackedParms.empty()) effectiveParms += fileBackedParms;
   if (!inlineParms.empty())
      {
       if (!effectiveParms.empty()) effectiveParms.push_back(' ');
       effectiveParms += inlineParms;
      }
   if (!parseInitParms(effectiveParms.empty() ? parms : effectiveParms.c_str(),
                       curPolicy, erp))
      return nullptr;

   if (IssuerPolicies.empty())
      {Fatal(erp, "At least one -issuer must be configured.", EINVAL);
       return nullptr;
      }

   if (IdentityClaims.empty())
      {Fatal(erp, "At least one identity claim must be configured.", EINVAL);
       return nullptr;
      }

   curl_global_init(CURL_GLOBAL_DEFAULT);

   if (!validateAndWarmInitIssuers(erp)) return nullptr;

   if (loadedCfgFile)
      {
       OIDCConfigPath = selectedCfgPath;
       OIDCConfigIno = cfgIno;
       OIDCConfigMTime = cfgMtime;
       OIDCConfigStatValid = true;
       OIDCConfigWatch = true;
      }

   char buff[256];
   snprintf(buff, sizeof(buff), "TLS:%" PRIu64 ":%d:", oidcVersion, MaxTokSize.load());
   return strdup(buff);
}

} // namespace detail

bool IsConfigured()
{
   return detail::isConfigured();
}

const char *StripToken(const char *bTok, int &sz, int maxLen)
{
   return detail::stripToken(bTok, sz, maxLen);
}

bool ValidateToken(const char *rawTok, std::string &identity, std::string &emsg,
                   uint64_t *expTime,
                   std::map<std::string, std::string> *entityAttrs)
{
   return detail::validateToken(rawTok, identity, emsg, expTime, entityAttrs);
}

char *InitSecProtocol(const char *parms, XrdOucErrInfo *erp)
{
   return detail::initSecProtocol(parms, erp);
}

bool Init(XrdSysLogger *logger, const char *parms, std::string &emsg)
{
   detail::OIDCLog.logger(logger ? logger : &detail::OIDCLogger);
   XrdOucErrInfo erp;
   char *rc = detail::initSecProtocol(parms, &erp);
   if (!rc)
      {int ec = 0;
       const char *et = erp.getErrText(ec);
       emsg = (et && *et ? et : "OIDC initialization failed");
       return false;
      }
   free(rc);
   return true;
}

} // namespace XrdOucOIDC
