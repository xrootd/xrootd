/******************************************************************************/
/*                                                                            */
/*                          X r d S e c J W T . c c                           */
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
#include <cmath>
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
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/rsa.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <openssl/params.h>
#endif

#include <fcntl.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucJson.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucUtils.hh"
#include "XrdSecjwt/XrdSecJWT.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

#ifndef EAUTH
#define EAUTH EBADE
#endif

namespace XrdSecJWT
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
struct EcdsaSigDeleter   {void operator()(ECDSA_SIG *p)    const noexcept {ECDSA_SIG_free(p);}};

using EvpPkeyPtr    = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;
using EvpMdCtxPtr   = std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter>;
using BignumPtr     = std::unique_ptr<BIGNUM, BignumDeleter>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter>;
using CurlPtr       = std::unique_ptr<CURL, CurlDeleter>;
using EcdsaSigPtr   = std::unique_ptr<ECDSA_SIG, EcdsaSigDeleter>;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
struct OsslParamBldDeleter {void operator()(OSSL_PARAM_BLD *p) const noexcept {OSSL_PARAM_BLD_free(p);}};
struct OsslParamDeleter    {void operator()(OSSL_PARAM *p)     const noexcept {OSSL_PARAM_free(p);}};
using OsslParamBldPtr = std::unique_ptr<OSSL_PARAM_BLD, OsslParamBldDeleter>;
using OsslParamPtr    = std::unique_ptr<OSSL_PARAM, OsslParamDeleter>;
#else
struct RsaDeleter   {void operator()(RSA *p)    const noexcept {RSA_free(p);}};
struct EcKeyDeleter {void operator()(EC_KEY *p) const noexcept {EC_KEY_free(p);}};
using RsaPtr   = std::unique_ptr<RSA, RsaDeleter>;
using EcKeyPtr = std::unique_ptr<EC_KEY, EcKeyDeleter>;
#endif

void Fatal(XrdOucErrInfo *erp, const char *eMsg, int rc, bool hdr=true)
{
   if (!erp) std::cerr <<(hdr ? "Secjwt: " : "") <<eMsg <<"\n" <<std::flush;
      else {const char *eVec[2] = {(hdr ? "Secjwt: " : ""), eMsg};
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

// Returns the lowercase hex SHA-256 of the input, or an empty string if the
// digest fails. Callers must treat an empty result as an error; the raw token
// is never used as a cache key.
std::string sha256hex(std::string_view data)
{
   unsigned char md[EVP_MAX_MD_SIZE];
   unsigned int mdLen = 0;
   if (data.empty()
   ||  !EVP_Digest(data.data(), data.size(), md, &mdLen, EVP_sha256(), nullptr)
   ||  !mdLen)
      return std::string();
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
   // Dots are part of WLCG claim names such as "wlcg.ver".
   for (unsigned char c : claim)
      if (!isalnum(c) && c != '_' && c != '-' && c != '.') return false;
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

// A name copied onto XrdSecEntity.name. Rejects an empty value, a leading
// '-', and anything outside [A-Za-z0-9._@-], so a claim cannot carry a path
// separator or shell metacharacter into the account name.
bool isSafeAccountName(const std::string &name)
{
   if (name.empty() || name[0] == '-') return false;
   for (unsigned char c : name)
      if (!isalnum(c) && c != '_' && c != '.' && c != '@' && c != '-')
         return false;
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

bool startsWithCI(std::string_view s, std::string_view pfx)
{
   if (s.size() < pfx.size()) return false;
   for (size_t i = 0; i < pfx.size(); ++i)
      if (std::tolower(static_cast<unsigned char>(s[i])) !=
          std::tolower(static_cast<unsigned char>(pfx[i])))
         return false;
   return true;
}

// Strip whitespace and an optional Bearer prefix. The view is the exact input.
std::string_view Strip(std::string_view in)
{
   auto isSp = [](char c) {
      return std::isspace(static_cast<unsigned char>(c)) != 0;
   };
   while (!in.empty() && isSp(in.front())) in.remove_prefix(1);
   if (in.empty()) return {};

   // RFC 6750: the "Bearer" scheme name is case-insensitive.
   if (startsWithCI(in, "Bearer%20")) in.remove_prefix(9);
   else if (startsWithCI(in, "Bearer ")) in.remove_prefix(7);

   while (!in.empty() && isSp(in.front())) in.remove_prefix(1);
   while (!in.empty() && isSp(in.back())) in.remove_suffix(1);
   return in;
}

// Defaults for every tunable; initSecProtocol() resets to these so that a
// second initialization in the same process (e.g. a fallback security service
// instance) never accumulates state from the first.
constexpr int  kDefaultExpiry          = 1;     // 1=require, 0=ignore, -1=optional
constexpr int  kDefaultMaxTokSize      = 8192;
constexpr int  kDefaultClockSkew       = 60;
constexpr int  kDefaultJwksRefresh     = 300;
constexpr int  kDefaultJwksRefreshMin  = 30;    // forced-refresh cooldown (s)
constexpr int  kDefaultTokenCacheMax   = 10000;
constexpr int  kDefaultTokenCacheNoExpTTL = 60;
const std::vector<std::string> kDefaultIdentityClaims = {
   "preferred_username", "upn", "username", "name", "sub"
};

std::atomic<int> expiry{kDefaultExpiry};
std::atomic<int> MaxTokSize{kDefaultMaxTokSize};
std::atomic<int> ClockSkew{kDefaultClockSkew};
std::atomic<int> JwksRefresh{kDefaultJwksRefresh};
// Minimum interval between *forced* JWKS refreshes for one issuer, and between
// retries of a periodic refresh that just failed. A forced refresh is triggered
// by a token whose signature does not verify with the cached keys; without a
// cooldown, unauthenticated clients could make the server hammer the issuer's
// JWKS endpoint with garbage tokens. The same interval stops an unreachable
// issuer from turning every authentication into its own fetch.
std::atomic<int> JwksRefreshMin{kDefaultJwksRefreshMin};
bool customIdentityClaims = false;
std::atomic<bool> DebugToken{false};
std::atomic<bool> DebugTokenClaims{false};
std::atomic<int> TokenCacheMax{kDefaultTokenCacheMax};
std::atomic<int> TokenCacheNoExpTTL{kDefaultTokenCacheNoExpTTL};
std::string JwksCacheFile;
int JwksCacheTTL = 0;
std::vector<std::string> IdentityClaims = kDefaultIdentityClaims;

void freeKeys(std::map<std::string, EvpPkeyPtr> &keys);

struct IssuerPolicy {
   std::string issuer;
   std::vector<std::string> audiences;
   std::string jwtConfigURL;
   std::string jwksURL;
   std::string forcedIdentityClaim;
   // SciTokens default_user. Used only when this issuer has no username_claim
   // and map_subject is not set. Empty means the usual identity-claim order.
   std::string defaultUser;
   // Claim copied onto XrdSecEntity.grps. SciTokens' groups_claim overrides
   // the default. The group list is not an access decision.
   std::string groupsClaim{"wlcg.groups"};
   // SciTokens authorization_strategy, canonical form capability+group+mapping.
   // Empty means the access plugin default, which is all three.
   std::string authzStrategy;
   // Per-issuer onmissing: passthrough, allow, or deny. Empty means the
   // access plugin's accjwt.onmissing.
   std::string onmissing;
   // Absolute name_mapfile path. The access plugin reads it and may set
   // request.name. Empty means there is no map.
   std::string nameMapFile;
   std::string basePath;
   std::vector<std::string> restrictedPaths;

   // keysMtx guards the key set and the refresh bookkeeping below. It is only
   // ever held for in-memory work. fetchMtx serializes the (slow) network
   // fetch so that a burst of concurrent refresh requests results in a single
   // fetch, including when that fetch fails.
   std::mutex keysMtx;
   std::map<std::string, EvpPkeyPtr> jwksKeys;
   time_t lastJwksLoad = 0;
   time_t lastForcedAttempt = 0;
   time_t lastFetchFailure = 0;
   std::string lastFetchError;
   std::mutex fetchMtx;
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
   nlohmann::json payloadObj;
   const bool havePayload = parseJsonObject(payloadJSON, payloadObj);
   if (havePayload)
      {std::string sub;
       if (!out.count("token.subject")
       &&  getStringClaim(payloadObj, "sub", sub)
       &&  isSafeEntityAttrValue(sub))
          out["token.subject"] = sub;
      }
   if (lookupIssuerPolicyFromPayload(payloadJSON, policy))
      {addIssuerPolicyEntityAttrs(policy, out);
       if (havePayload && !policy->groupsClaim.empty())
          {std::string groups;
           if (getEntityClaimValue(payloadObj, policy->groupsClaim.c_str(), groups)
           &&  isSafeEntityAttrValue(groups))
              out["jwt.grps"] = groups;
          }
       // The matched issuer is the virtual organization. The protocol copies
       // this onto Entity.vorg and does not publish it as an attribute.
       if (!policy->issuer.empty() && isSafeEntityAttrValue(policy->issuer))
          out["jwt.vorg"] = policy->issuer;
       if (!policy->authzStrategy.empty()
       &&  isSafeEntityAttrValue(policy->authzStrategy))
          out["jwt.strategy"] = policy->authzStrategy;
       if (!policy->onmissing.empty()
       &&  isSafeEntityAttrValue(policy->onmissing))
          out["jwt.onmissing"] = policy->onmissing;
       if (!policy->nameMapFile.empty()
       &&  isSafeEntityAttrValue(policy->nameMapFile))
          out["jwt.mapfile"] = policy->nameMapFile;
      }
}

const char *const kDefaultJWTConfigPath = "/etc/xrootd/jwt.cfg";
std::string JWTConfigPath = kDefaultJWTConfigPath;
bool JWTConfigWatch = false;
// Issuer options and -email-map from startup, reapplied after each file reload.
// The file replaces issuer policies; without this, an inline -issuer or
// -audience disappears on the first edit of a watched file.
std::string InlineInitParms;
bool SciTokensConfig = false;
bool JWTConfigStatValid = false;
ino_t JWTConfigIno = 0;
time_t JWTConfigMTime = 0;
time_t JWTConfigCTime = 0;
off_t JWTConfigSize = 0;
// A file version that failed to parse or whose issuer keys could not be
// fetched. Later authentications skip it until the signature changes, so a
// bad file is not re-read and re-fetched on every cache miss.
bool JWTConfigRejectedValid = false;
ino_t JWTConfigRejectedIno = 0;
time_t JWTConfigRejectedMTime = 0;
time_t JWTConfigRejectedCTime = 0;
off_t JWTConfigRejectedSize = 0;
// Set while one thread performs a reload so concurrent authentications that
// observe the same change do not each re-read the file and re-fetch JWKS.
std::atomic<bool> JWTReloadInProgress{false};

XrdSysLogger JWTLogger;
XrdSysError  JWTLog(0, "secjwt_");

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
   std::cerr << "Secjwt: " << msg << "\n" << std::flush;
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

// SciTokens authorization_strategy. Canonical form uses '+' so the value is
// one token on a sec.protparm line. Empty input is rejected.
bool normalizeAuthzStrategy(const std::string &raw, std::string &canonical,
                            std::string &emsg)
{
   canonical.clear();
   emsg.clear();
   unsigned bits = 0;
   bool any = false;
   size_t pos = 0;
   while (pos < raw.size())
      {
       while (pos < raw.size()
           && (raw[pos] == ' ' || raw[pos] == ',' || raw[pos] == '+'
            || raw[pos] == '|' || raw[pos] == '\t'))
          ++pos;
       if (pos >= raw.size()) break;
       size_t end = pos;
       while (end < raw.size()
           && raw[end] != ' ' && raw[end] != ',' && raw[end] != '+'
           && raw[end] != '|' && raw[end] != '\t')
          ++end;
       std::string word = raw.substr(pos, end - pos);
       pos = end;
       for (char &c : word)
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
       any = true;
       if (word == "capability") bits |= 1u;
       else if (word == "group") bits |= 2u;
       else if (word == "mapping") bits |= 4u;
       else
          {emsg = "unknown authorization_strategy value: " + word;
           return false;
          }
      }
   if (!any)
      {emsg = "authorization_strategy has no value";
       return false;
      }
   if (bits & 1u) canonical += "capability";
   if (bits & 2u)
      {if (!canonical.empty()) canonical += "+";
       canonical += "group";
      }
   if (bits & 4u)
      {if (!canonical.empty()) canonical += "+";
       canonical += "mapping";
      }
   return true;
}

// Per-issuer onmissing. Canonical form is lowercase. Empty input is rejected.
bool normalizeOnMissing(const std::string &raw, std::string &canonical,
                        std::string &emsg)
{
   canonical.clear();
   emsg.clear();
   std::string word = XrdOucUtils::trimCopy(raw);
   for (char &c : word)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
   if (word == "passthrough" || word == "allow" || word == "deny")
      {canonical = word;
       return true;
      }
   if (word.empty())
      {emsg = "onmissing has no value";
       return false;
      }
   emsg = "unknown onmissing value: " + word;
   return false;
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

   if (key == "jwt-config-url")      {appendCliOpt(opts, "-jwt-config-url", &val); return true;}
   if (key == "jwks-url")             {appendCliOpt(opts, "-jwks-url", &val); return true;}
   if (key == "forced-identity-claim"){appendCliOpt(opts, "-forced-identity-claim", &val); return true;}
   if (key == "authorization-strategy" || key == "authorization_strategy")
      {
       if (!inIssuer)
          {emsg = "authorization_strategy requires an [issuer ...] section or issuer=...";
           return false;
          }
       std::string canonical, localErr;
       if (!normalizeAuthzStrategy(val, canonical, localErr))
          {emsg = localErr;
           return false;
          }
       appendCliOpt(opts, "-authorization-strategy", &canonical);
       return true;
      }
   if (key == "onmissing")
      {
       if (!inIssuer)
          {emsg = "onmissing requires an [issuer ...] section or issuer=...";
           return false;
          }
       std::string canonical, localErr;
       if (!normalizeOnMissing(val, canonical, localErr))
          {emsg = localErr;
           return false;
          }
       appendCliOpt(opts, "-onmissing", &canonical);
       return true;
      }
   if (key == "name-mapfile" || key == "name_mapfile")
      {
       if (!inIssuer)
          {emsg = "name_mapfile requires an [issuer ...] section or issuer=...";
           return false;
          }
       std::string canonical;
       if (!canonicalAbsolutePath(val, canonical))
          {emsg = "name_mapfile is not an absolute path: " + val;
           return false;
          }
       appendCliOpt(opts, "-name-mapfile", &canonical);
       return true;
      }
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
   if (key == "jwks-refresh-min")     {appendCliOpt(opts, "-jwks-refresh-min", &val); return true;}
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
   time_t ctime;
   off_t size;
   bool found;
};

bool safeReadConfigFile(const char *path, SafeFileResult &result, std::string &emsg)
{
   result.found = false;
   result.contents.clear();
   result.ino = 0;
   result.mtime = 0;
   result.ctime = 0;
   result.size = 0;

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
   result.ctime = st.st_ctime;
   result.size = st.st_size;
   result.found = true;
   return true;
}

bool loadJWTIniAsArgs(const char *path, std::string &opts, bool &found,
                       std::string &emsg, SafeFileResult *metaOut = nullptr)
{
   opts.clear();
   emsg.clear();
   found = false;

   SafeFileResult sfr;
   if (!safeReadConfigFile(path, sfr, emsg)) return false;
   if (!sfr.found) return true;
   found = true;
   if (metaOut)
      {metaOut->ino = sfr.ino;
       metaOut->mtime = sfr.mtime;
       metaOut->ctime = sfr.ctime;
       metaOut->size = sfr.size;
       metaOut->found = true;
      }

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

       // issuer= starts an issuer at the top level and in [global], as startup does.
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
           inIssuer = true;
           continue;
          }

       if (!inIssuer) continue; // reloadable sections only
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
       if (key == "jwt-config-url")
          {curPolicy->jwtConfigURL = val; continue;}
       if (key == "jwks-url")
          {curPolicy->jwksURL = val; continue;}
       if (key == "forced-identity-claim")
          {curPolicy->forcedIdentityClaim = val; continue;}
       if (key == "authorization-strategy" || key == "authorization_strategy")
          {
           std::string canonical, localErr;
           if (!normalizeAuthzStrategy(val, canonical, localErr))
              {emsg = std::string(path) + ":" + std::to_string(lineNo)
                    + ": " + localErr;
               return false;
              }
           curPolicy->authzStrategy = canonical;
           continue;
          }
       if (key == "onmissing")
          {
           std::string canonical, localErr;
           if (!normalizeOnMissing(val, canonical, localErr))
              {emsg = std::string(path) + ":" + std::to_string(lineNo)
                    + ": " + localErr;
               return false;
              }
           curPolicy->onmissing = canonical;
           continue;
          }
       if (key == "name-mapfile" || key == "name_mapfile")
          {
           std::string canonical;
           if (!canonicalAbsolutePath(val, canonical))
              {emsg = std::string(path) + ":" + std::to_string(lineNo)
                    + ": name_mapfile is not an absolute path: " + val;
               return false;
              }
           curPolicy->nameMapFile = canonical;
           continue;
          }
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

// Split a SciTokens list on commas and spaces (audience, base_path).
void splitSciTokensList(const std::string &in, std::vector<std::string> &out)
{
   out.clear();
   size_t pos = 0;
   while (pos < in.size())
      {
       while (pos < in.size() && (in[pos] == ',' || in[pos] == ' ')) ++pos;
       if (pos >= in.size()) break;
       size_t next = in.find_first_of(", ", pos);
       std::string one = in.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
       if (!one.empty()) out.push_back(one);
       if (next == std::string::npos) break;
       pos = next;
      }
}

// One absolute path stays a string. Several become a JSON array, which is what
// XrdAccjwt receives on the base_path attribute.
bool assignBasePathAttr(const std::string &raw, std::string &basePath, std::string &emsg)
{
   std::vector<std::string> items, paths;
   splitSciTokensList(raw, items);
   for (const auto &item : items)
      {
       std::string canon;
       if (!canonicalAbsolutePath(item, canon))
          {emsg = "base_path entry is not an absolute path: " + item;
           return false;
          }
       paths.push_back(canon);
      }
   if (paths.empty())
      {emsg = "base_path is empty";
       return false;
      }
   basePath = (paths.size() == 1) ? paths[0] : pathsToJsonArray(paths);
   return true;
}

bool appendRestrictedPaths(const std::string &raw,
                           std::vector<std::string> &dst,
                           std::string &emsg)
{
   std::vector<std::string> items;
   splitSciTokensList(raw, items);
   if (items.empty())
      {emsg = "restricted_path is empty";
       return false;
      }
   for (const auto &item : items)
      if (!appendIssuerPathOption(item, dst, emsg)) return false;
   return true;
}

bool parseAudienceJson(const std::string &raw, std::vector<std::string> &out, std::string &emsg)
{
   nlohmann::json parsed = nlohmann::json::parse(raw, nullptr, false);
   if (parsed.is_discarded())
      {emsg = "audience_json is not JSON";
       return false;
      }
   if (parsed.is_string())
      {std::string one = parsed.get<std::string>();
       if (!one.empty()) out.push_back(one);
       return true;
      }
   if (!parsed.is_array())
      {emsg = "audience_json must be a string or an array of strings";
       return false;
      }
   for (const auto &el : parsed)
      {
       if (!el.is_string())
          {emsg = "audience_json must be a string or an array of strings";
           return false;
          }
       std::string one = el.get<std::string>();
       if (!one.empty()) out.push_back(one);
      }
   return true;
}

// Read a SciTokens INI (not the jwt.cfg dialect). A [Global] onmissing is
// ignored: the plugin-wide default stays accjwt.onmissing. An issuer
// onmissing is copied onto that issuer. authorization_strategy is copied
// onto the issuer so the access plugin can turn capability grants off.
// name_mapfile is the absolute path of the JSON rules the access plugin
// applies to request.name. default_user and groups_claim are identity and
// are applied here.
bool parseSciTokensConfig(const char *path,
                          const std::string &fileContents,
                          std::vector<std::shared_ptr<IssuerPolicy>> &outPolicies,
                          std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> &outByIssuer,
                          std::string &emsg)
{
   outPolicies.clear();
   outByIssuer.clear();
   emsg.clear();

   struct Pending {
      bool active = false;
      int lineNo = 0;
      std::string label;
      std::string issuer;
      std::string basePathRaw;
      std::string restrictedRaw;
      bool mapSubject = false;
      std::string usernameClaim;
      std::string defaultUser;
      std::string groupsClaim;
      std::string authzStrategy;
      std::string onmissing;
      std::string nameMapFile;
   } pending;

   std::vector<std::string> globalAudiences;
   auto fail = [&](int lineNo, const std::string &msg) -> bool {
      emsg = std::string(path) + ":" + std::to_string(lineNo) + ": " + msg;
      return false;
   };
   auto flush = [&]() -> bool {
      if (!pending.active) return true;
      if (pending.issuer.empty())
         return fail(pending.lineNo, "SciTokens section '" + pending.label + "' requires issuer");
      if (pending.basePathRaw.empty())
         return fail(pending.lineNo, "SciTokens section '" + pending.label + "' requires base_path");
      std::string baseAttr, localErr;
      if (!assignBasePathAttr(pending.basePathRaw, baseAttr, localErr))
         return fail(pending.lineNo, localErr);
      std::vector<std::string> restricted;
      if (!pending.restrictedRaw.empty()
      &&  !appendRestrictedPaths(pending.restrictedRaw, restricted, localErr))
         return fail(pending.lineNo, localErr);

      std::shared_ptr<IssuerPolicy> policy;
      auto it = outByIssuer.find(pending.issuer);
      if (it != outByIssuer.end()) policy = it->second;
      else
         {
          policy = std::make_shared<IssuerPolicy>();
          policy->issuer = pending.issuer;
          outPolicies.push_back(policy);
          outByIssuer[pending.issuer] = policy;
         }
      policy->basePath = baseAttr;
      policy->restrictedPaths = std::move(restricted);
      if (!pending.defaultUser.empty() && !isSafeAccountName(pending.defaultUser))
         return fail(pending.lineNo, "default_user is not a safe account name");
      if (!pending.groupsClaim.empty() && !isValidJwtClaimName(pending.groupsClaim))
         return fail(pending.lineNo, "groups_claim is not a valid claim name");
      if (!pending.usernameClaim.empty()) policy->forcedIdentityClaim = pending.usernameClaim;
      else if (pending.mapSubject) policy->forcedIdentityClaim = "sub";
      else policy->forcedIdentityClaim.clear();
      policy->defaultUser = pending.defaultUser;
      policy->groupsClaim = pending.groupsClaim.empty() ? std::string("wlcg.groups")
                                                        : pending.groupsClaim;
      if (!pending.authzStrategy.empty())
         {
          std::string canonical, localErr;
          if (!normalizeAuthzStrategy(pending.authzStrategy, canonical, localErr))
             return fail(pending.lineNo, localErr);
          policy->authzStrategy = canonical;
         }
      else policy->authzStrategy.clear();
      if (!pending.onmissing.empty())
         {
          std::string canonical, localErr;
          if (!normalizeOnMissing(pending.onmissing, canonical, localErr))
             return fail(pending.lineNo, localErr);
          policy->onmissing = canonical;
         }
      else policy->onmissing.clear();
      if (!pending.nameMapFile.empty())
         {
          std::string canonical;
          if (!canonicalAbsolutePath(pending.nameMapFile, canonical))
             return fail(pending.lineNo, "name_mapfile is not an absolute path: "
                         + pending.nameMapFile);
          policy->nameMapFile = canonical;
         }
      else policy->nameMapFile.clear();
      pending = Pending{};
      return true;
   };

   enum class Sec { None, Global, Issuer };
   Sec sec = Sec::None;
   std::istringstream in(fileContents);
   std::string line;
   int lineNo = 0;
   while (std::getline(in, line))
      {
       ++lineNo;
       std::string t = XrdOucUtils::trimCopy(line);
       if (t.empty() || t[0] == '#' || t[0] == ';') continue;
       if (t.front() == '[' && t.back() == ']')
          {
           if (!flush()) return false;
           std::string name = XrdOucUtils::trimCopy(t.substr(1, t.size() - 2));
           std::string lower = XrdOucUtils::toLowerCopy(name);
           if (lower == "global")
              {sec = Sec::Global; continue;}
           if (startsWithIssuerSection(name))
              {
               std::string label = XrdOucUtils::trimCopy(name.substr(6));
               if (label.empty())
                  return fail(lineNo, "SciTokens issuer section needs a name");
               pending.active = true;
               pending.lineNo = lineNo;
               pending.label = label;
               sec = Sec::Issuer;
               continue;
              }
           return fail(lineNo, "unsupported section '" + name + "'");
          }

       size_t eq = t.find('=');
       std::string key = XrdOucUtils::toLowerCopy(XrdOucUtils::trimCopy(eq == std::string::npos ? t : t.substr(0, eq)));
       std::string rawVal = XrdOucUtils::trimCopy(eq == std::string::npos ? "" : t.substr(eq + 1));
       std::string val = stripQuotes(rawVal);

       if (sec == Sec::Global)
          {
           if (key == "audience")
              {
               std::vector<std::string> items;
               splitSciTokensList(val, items);
               globalAudiences.insert(globalAudiences.end(), items.begin(), items.end());
              }
           else if (key == "audience_json")
              {
               std::string localErr;
               if (!parseAudienceJson(rawVal, globalAudiences, localErr))
                  return fail(lineNo, localErr);
              }
           continue;
          }
       if (sec != Sec::Issuer) continue;
       if (key == "issuer")
          {
           if (val.empty()) return fail(lineNo, "issuer value is empty");
           pending.issuer = val;
          }
       else if (key == "base_path" || key == "base-path") pending.basePathRaw = val;
       else if (key == "restricted_path" || key == "restricted-path") pending.restrictedRaw = val;
       else if (key == "map_subject")
          {
           if (!XrdOucUtils::parseBool(val, pending.mapSubject))
              return fail(lineNo, "invalid boolean for map_subject: " + val);
          }
       else if (key == "username_claim") pending.usernameClaim = val;
       else if (key == "default_user") pending.defaultUser = val;
       else if (key == "groups_claim") pending.groupsClaim = val;
       else if (key == "authorization_strategy") pending.authzStrategy = val;
       else if (key == "onmissing") pending.onmissing = val;
       else if (key == "name_mapfile") pending.nameMapFile = val;
      }
   if (!flush()) return false;
   if (outPolicies.empty())
      {emsg = std::string(path) + ": no SciTokens issuer configured";
       return false;
      }
   for (auto &policy : outPolicies) policy->audiences = globalAudiences;
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
       if (policy->jwtConfigURL.empty() && !policy->issuer.empty())
          policy->jwtConfigURL = joinURL(policy->issuer, "/.well-known/openid-configuration");
       if (policy->jwtConfigURL.empty() && policy->jwksURL.empty())
          {emsg = "issuer '" + policy->issuer + "' requires jwt-config-url or jwks-url";
           return false;
          }
       if ((!policy->jwtConfigURL.empty() && !hasPrefix(policy->jwtConfigURL.c_str(), "https://"))
       ||  (!policy->jwksURL.empty() && !hasPrefix(policy->jwksURL.c_str(), "https://")))
          {emsg = "issuer '" + policy->issuer + "' has non-https JWT/JWKS URL";
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
// The change signature is (inode, mtime, ctime, size) so that two edits within
// the same second, or an in-place rewrite of the same size, are still noticed.
bool statConfigFileMeta(const char *path, SafeFileResult &meta,
                        bool &found, std::string &emsg)
{
   meta.ino = 0;
   meta.mtime = 0;
   meta.ctime = 0;
   meta.size = 0;
   meta.found = false;
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
   meta.ino = st.st_ino;
   meta.mtime = st.st_mtime;
   meta.ctime = st.st_ctime;
   meta.size = st.st_size;
   meta.found = true;
   found = true;
   return true;
}

// Must be called with ConfigMtx held.
bool configMetaUnchangedLocked(const SafeFileResult &meta)
{
   return JWTConfigStatValid
       && meta.ino   == JWTConfigIno
       && meta.mtime == JWTConfigMTime
       && meta.ctime == JWTConfigCTime
       && meta.size  == JWTConfigSize;
}

// Must be called with ConfigMtx held.
void recordConfigMetaLocked(const SafeFileResult &meta)
{
   JWTConfigIno = meta.ino;
   JWTConfigMTime = meta.mtime;
   JWTConfigCTime = meta.ctime;
   JWTConfigSize = meta.size;
   JWTConfigStatValid = true;
}

// Must be called with ConfigMtx held.
bool rejectedConfigMetaLocked(const SafeFileResult &meta)
{
   return JWTConfigRejectedValid
       && meta.ino   == JWTConfigRejectedIno
       && meta.mtime == JWTConfigRejectedMTime
       && meta.ctime == JWTConfigRejectedCTime
       && meta.size  == JWTConfigRejectedSize;
}

// Must be called with ConfigMtx held.
void recordRejectedConfigMetaLocked(const SafeFileResult &meta)
{
   JWTConfigRejectedIno = meta.ino;
   JWTConfigRejectedMTime = meta.mtime;
   JWTConfigRejectedCTime = meta.ctime;
   JWTConfigRejectedSize = meta.size;
   JWTConfigRejectedValid = true;
}

// Must be called with ConfigMtx held.
void clearRejectedConfigMetaLocked()
{
   JWTConfigRejectedValid = false;
}

// Reapply startup inline issuer options and -email-map onto a freshly parsed
// file. Other inline options (expiry, entity-claim, cache sizes) stay as they
// were at startup and are not applied again.
bool overlayInlineIssuerParms(const std::string &parms,
                             std::vector<std::shared_ptr<IssuerPolicy>> &policies,
                             std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> &byIssuer,
                             std::unordered_map<std::string, std::string> &emailMap,
                             std::string &emsg);

void maybeReloadJWTFileConfig()
{
   std::string cfgPath;
   bool sciTokens = false;
   {
      std::scoped_lock lock(ConfigMtx);
      if (!JWTConfigWatch || JWTConfigPath.empty()) return;
      cfgPath = JWTConfigPath;
      sciTokens = SciTokensConfig;
   }
   std::string emsg;

   // Fast path: stat only. Avoid reading the (potentially large) file on every
   // authentication; the full read+parse happens only when the signature changes.
   SafeFileResult cur;
   bool curFound = false;
   if (!statConfigFileMeta(cfgPath.c_str(), cur, curFound, emsg))
      {
       JWTLog.Emsg("Auth", "jwt", ("config stat failed: " + emsg).c_str());
       return;
      }
   if (!curFound)
      {
       JWTLog.Emsg("Auth", "jwt", ("config file disappeared: " + cfgPath).c_str());
       return;
      }
   {
      std::scoped_lock lock(ConfigMtx);
      if (configMetaUnchangedLocked(cur) || rejectedConfigMetaLocked(cur)) return;
   }

   // Only one thread performs the reload; others that observed the same change
   // simply continue with the currently installed configuration.
   if (JWTReloadInProgress.exchange(true)) return;
   struct ReloadGuard {~ReloadGuard() {JWTReloadInProgress = false;}} reloadGuard;

   // Re-check under the flag: the previous reloader may have just finished.
   {
      std::scoped_lock lock(ConfigMtx);
      if (configMetaUnchangedLocked(cur) || rejectedConfigMetaLocked(cur)) return;
   }

   // Config changed (or first observation): read and parse the full contents.
   SafeFileResult sfr;
   if (!safeReadConfigFile(cfgPath.c_str(), sfr, emsg))
      {
       JWTLog.Emsg("Auth", "jwt", ("config read failed: " + emsg).c_str());
       return;
      }
   if (!sfr.found)
      {
       JWTLog.Emsg("Auth", "jwt", ("config file disappeared: " + cfgPath).c_str());
       return;
      }

   std::vector<std::shared_ptr<IssuerPolicy>> newPolicies;
   std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> newByIssuer;
   std::unordered_map<std::string, std::string> newEmailMap;
   std::string inlineParms;
   {
      std::scoped_lock lock(ConfigMtx);
      inlineParms = InlineInitParms;
   }
   bool parsed = false;
   if (sciTokens)
      parsed = parseSciTokensConfig(cfgPath.c_str(), sfr.contents, newPolicies, newByIssuer, emsg);
   else
      parsed = parseReloadableIniSections(cfgPath.c_str(), sfr.contents, newPolicies, newByIssuer, newEmailMap, emsg);
   if (parsed && !overlayInlineIssuerParms(inlineParms, newPolicies, newByIssuer, newEmailMap, emsg))
      parsed = false;
   if (!parsed || !validateAndWarmReloadableConfig(newPolicies, emsg))
      {
       JWTLog.Emsg("Auth", "jwt", ("config reload failed: " + emsg).c_str());
       std::scoped_lock lock(ConfigMtx);
       recordRejectedConfigMetaLocked(sfr);
       return;
      }

   std::scoped_lock lock(ConfigMtx);
   IssuerPolicies.swap(newPolicies);
   IssuerPolicyByIssuer.swap(newByIssuer);
   // A SciTokens file has no [email-map]. Keep maps supplied with -email-map.
   if (!sciTokens) EmailIdentityMap.swap(newEmailMap);
   recordConfigMetaLocked(sfr);
   clearRejectedConfigMetaLocked();
   clearTokenCache();
   JWTLog.Emsg("Auth", "jwt", ("config reloaded from " + cfgPath).c_str());
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

// curl_global_init() is not thread-safe and must run exactly once per process,
// regardless of how many times the plugin is (re)initialized.
void ensureCurlGlobalInit()
{
   static std::once_flag once;
   std::call_once(once, [] {curl_global_init(CURL_GLOBAL_DEFAULT);});
}

bool fetchURL(const std::string &url, std::string &body, std::string &emsg)
{
   body.clear();
   ensureCurlGlobalInit();
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
   // Never let libcurl install signal handlers (SIGALRM for resolver timeouts);
   // that is unsafe in a multithreaded server.
   curl_easy_setopt(c.get(), CURLOPT_NOSIGNAL, 1L);
   curl_easy_setopt(c.get(), CURLOPT_WRITEFUNCTION, curlWriteCB);
   curl_easy_setopt(c.get(), CURLOPT_WRITEDATA, &sink);
   curl_easy_setopt(c.get(), CURLOPT_SSL_VERIFYPEER, 1L);
   curl_easy_setopt(c.get(), CURLOPT_SSL_VERIFYHOST, 2L);
   // libcurl does not always consult SSL_CERT_FILE on its own. A private
   // issuer, including the live test, exports that variable (or CURL_CA_BUNDLE)
   // so verification uses the PEM bundle instead of the system CAs.
   const char *caFile = getenv("SSL_CERT_FILE");
   if (!caFile || !*caFile) caFile = getenv("CURL_CA_BUNDLE");
   if (caFile && *caFile)
      curl_easy_setopt(c.get(), CURLOPT_CAINFO, caFile);
   CURLcode rc = curl_easy_perform(c.get());
   long httpCode = 0;
   curl_easy_getinfo(c.get(), CURLINFO_RESPONSE_CODE, &httpCode);
   if (sink.truncated)
      {emsg = "JWKS/JWT response body too large (> 4 MiB), aborting";
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
   // Some issuers emit NumericDate claims as floating point (e.g. 1.7e9).
   if (it->is_number_float())
      {double v = it->get<double>();
       if (!std::isfinite(v) || v < 0 || v >= 18446744073709551615.0) return false;
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

/******************************************************************************/
/*                     J W S   a l g o r i t h m   s u p p o r t              */
/******************************************************************************/

// Supported JWS algorithms. This is a strict allowlist: anything else
// (including "none" and the HMAC family) is rejected before any key lookup.
enum class JwsFamily { RSA_PKCS1, RSA_PSS, ECDSA };

struct JwsAlg {
   const char *name;
   JwsFamily   family;
   const EVP_MD *(*md)();
   int         ecCurveNid;   // ECDSA only: required curve
   int         ecCoordBytes; // ECDSA only: size of r and s in the raw signature
};

const JwsAlg *lookupJwsAlg(const std::string &alg)
{
   static const JwsAlg kAlgs[] = {
      {"RS256", JwsFamily::RSA_PKCS1, EVP_sha256, 0, 0},
      {"RS384", JwsFamily::RSA_PKCS1, EVP_sha384, 0, 0},
      {"RS512", JwsFamily::RSA_PKCS1, EVP_sha512, 0, 0},
      {"PS256", JwsFamily::RSA_PSS,   EVP_sha256, 0, 0},
      {"PS384", JwsFamily::RSA_PSS,   EVP_sha384, 0, 0},
      {"PS512", JwsFamily::RSA_PSS,   EVP_sha512, 0, 0},
      {"ES256", JwsFamily::ECDSA,     EVP_sha256, NID_X9_62_prime256v1, 32},
      {"ES384", JwsFamily::ECDSA,     EVP_sha384, NID_secp384r1,        48},
      {"ES512", JwsFamily::ECDSA,     EVP_sha512, NID_secp521r1,        66},
   };
   for (const auto &a : kAlgs)
      if (alg == a.name) return &a;
   return nullptr;
}

int ecCurveNidOf(EVP_PKEY *pkey)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
   char name[80] = {0};
   size_t len = 0;
   if (EVP_PKEY_get_utf8_string_param(pkey, OSSL_PKEY_PARAM_GROUP_NAME,
                                      name, sizeof(name), &len) != 1)
      return NID_undef;
   int nid = OBJ_sn2nid(name);
   if (nid == NID_undef) nid = OBJ_ln2nid(name);
   return nid;
#else
   const EC_KEY *ec = EVP_PKEY_get0_EC_KEY(pkey);
   if (!ec) return NID_undef;
   const EC_GROUP *grp = EC_KEY_get0_group(ec);
   return grp ? EC_GROUP_get_curve_name(grp) : NID_undef;
#endif
}

// Convert a JWS raw ECDSA signature (r || s, fixed width) into the DER
// encoding that EVP_DigestVerifyFinal expects.
bool rawEcdsaToDer(std::string_view raw, int coordBytes, std::vector<unsigned char> &der)
{
   if (coordBytes <= 0 || raw.size() != static_cast<size_t>(2 * coordBytes)) return false;
   const unsigned char *p = reinterpret_cast<const unsigned char *>(raw.data());
   BignumPtr r(BN_bin2bn(p, coordBytes, nullptr));
   BignumPtr s(BN_bin2bn(p + coordBytes, coordBytes, nullptr));
   if (!r || !s) return false;
   EcdsaSigPtr sig(ECDSA_SIG_new());
   if (!sig) return false;
   if (ECDSA_SIG_set0(sig.get(), r.get(), s.get()) != 1) return false;
   r.release(); s.release(); // owned by sig now
   int len = i2d_ECDSA_SIG(sig.get(), nullptr);
   if (len <= 0) return false;
   der.resize(static_cast<size_t>(len));
   unsigned char *out = der.data();
   return i2d_ECDSA_SIG(sig.get(), &out) == len;
}

bool verifyJWS(const JwsAlg &alg, EVP_PKEY *pkey, std::string_view signedData,
               std::string_view sig)
{
   if (!pkey) return false;
   const int keyType = EVP_PKEY_base_id(pkey);

   std::vector<unsigned char> derSig;
   const unsigned char *sigPtr = reinterpret_cast<const unsigned char *>(sig.data());
   size_t sigLen = sig.size();

   switch (alg.family)
      {case JwsFamily::RSA_PKCS1:
       case JwsFamily::RSA_PSS:
            if (keyType != EVP_PKEY_RSA && keyType != EVP_PKEY_RSA_PSS) return false;
            break;
       case JwsFamily::ECDSA:
            if (keyType != EVP_PKEY_EC) return false;
            if (ecCurveNidOf(pkey) != alg.ecCurveNid) return false;
            if (!rawEcdsaToDer(sig, alg.ecCoordBytes, derSig)) return false;
            sigPtr = derSig.data();
            sigLen = derSig.size();
            break;
      }

   EvpMdCtxPtr ctx(EVP_MD_CTX_new());
   if (!ctx) return false;
   EVP_PKEY_CTX *pctx = nullptr; // owned by ctx
   if (EVP_DigestVerifyInit(ctx.get(), &pctx, alg.md(), nullptr, pkey) != 1) return false;
   if (alg.family == JwsFamily::RSA_PSS)
      {if (!pctx
       ||  EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) <= 0
       ||  EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, RSA_PSS_SALTLEN_DIGEST) <= 0)
          return false;
      }
   return EVP_DigestVerifyUpdate(ctx.get(), signedData.data(), signedData.size()) == 1
       && EVP_DigestVerifyFinal(ctx.get(), sigPtr, sigLen) == 1;
}

// JWKS entries without a "kid" are stored under a synthetic name that starts
// with a NUL byte so it can never collide with a real kid.
inline bool isAnonymousKid(const std::string &name)
{
   return !name.empty() && name[0] == '\0';
}

// Try the token signature against a set of keys.
//  - token has a kid we hold      : only that key is tried.
//  - token has a kid we don't hold: only JWKS entries *without* a kid are
//                                   tried (some issuers publish kid-less keys
//                                   yet stamp a kid into the header); a token
//                                   naming a kid that is neither known nor
//                                   coverable by an anonymous key fails.
//  - token has no kid             : every key is tried.
// Each attempt is a single public-key operation.
bool verifyAgainstKeys(const JwsAlg &alg, const std::string &kid,
                       const std::map<std::string, EvpPkeyPtr> &keys,
                       std::string_view signedData, std::string_view sig)
{
   if (!kid.empty())
      {auto it = keys.find(kid);
       if (it != keys.end()) return verifyJWS(alg, it->second.get(), signedData, sig);
       for (const auto &k : keys)
           if (isAnonymousKid(k.first)
           &&  verifyJWS(alg, k.second.get(), signedData, sig)) return true;
       return false;
      }
   for (const auto &it : keys)
       if (verifyJWS(alg, it.second.get(), signedData, sig)) return true;
   return false;
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

int jwkCurveNid(const std::string &crv, size_t &coordBytes)
{
   if (crv == "P-256") {coordBytes = 32; return NID_X9_62_prime256v1;}
   if (crv == "P-384") {coordBytes = 48; return NID_secp384r1;}
   if (crv == "P-521") {coordBytes = 66; return NID_secp521r1;}
   coordBytes = 0;
   return NID_undef;
}

// RFC 7518 requires fixed-width coordinates, but be tolerant of issuers that
// strip leading zeros: left-pad to the curve size. Oversized input is invalid.
bool padCoordinate(std::string_view in, size_t width, std::string &out)
{
   if (in.empty() || in.size() > width) return false;
   out.assign(width - in.size(), '\0');
   out.append(in.data(), in.size());
   return true;
}

EvpPkeyPtr makeECPublicKey(const std::string &crv, std::string_view xIn,
                           std::string_view yIn)
{
   size_t coordBytes = 0;
   const int nid = jwkCurveNid(crv, coordBytes);
   if (nid == NID_undef) return nullptr;
   std::string x, y;
   if (!padCoordinate(xIn, coordBytes, x) || !padCoordinate(yIn, coordBytes, y))
      return nullptr;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
   // Uncompressed point encoding: 0x04 || X || Y
   std::string pub;
   pub.reserve(1 + x.size() + y.size());
   pub.push_back('\x04');
   pub.append(x);
   pub.append(y);

   OsslParamBldPtr bld(OSSL_PARAM_BLD_new());
   if (!bld) return nullptr;
   const char *groupName = OBJ_nid2sn(nid);
   if (!groupName) return nullptr;
   if (OSSL_PARAM_BLD_push_utf8_string(bld.get(), OSSL_PKEY_PARAM_GROUP_NAME,
                                       groupName, 0) <= 0
   ||  OSSL_PARAM_BLD_push_octet_string(bld.get(), OSSL_PKEY_PARAM_PUB_KEY,
                                        pub.data(), pub.size()) <= 0)
      return nullptr;
   OsslParamPtr params(OSSL_PARAM_BLD_to_param(bld.get()));
   if (!params) return nullptr;
   EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr));
   if (!ctx) return nullptr;
   EVP_PKEY *raw = nullptr;
   if (EVP_PKEY_fromdata_init(ctx.get()) <= 0
   ||  EVP_PKEY_fromdata(ctx.get(), &raw, EVP_PKEY_PUBLIC_KEY, params.get()) <= 0)
      return nullptr;
   return EvpPkeyPtr(raw);
#else
   EcKeyPtr ec(EC_KEY_new_by_curve_name(nid));
   if (!ec) return nullptr;
   BignumPtr bx(BN_bin2bn(reinterpret_cast<const unsigned char *>(x.data()), x.size(), nullptr));
   BignumPtr by(BN_bin2bn(reinterpret_cast<const unsigned char *>(y.data()), y.size(), nullptr));
   if (!bx || !by) return nullptr;
   if (EC_KEY_set_public_key_affine_coordinates(ec.get(), bx.get(), by.get()) != 1)
      return nullptr;
   EvpPkeyPtr pkey(EVP_PKEY_new());
   if (!pkey) return nullptr;
   if (EVP_PKEY_assign_EC_KEY(pkey.get(), ec.get()) != 1) return nullptr;
   ec.release(); // owned by pkey
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
   size_t anonIdx = 0;
   for (const auto &kobj : *kIt)
      {
       std::string kty;
       if (!getStringClaim(kobj, "kty", kty)) continue;
       std::string use;
       if (getStringClaim(kobj, "use", use) && use != "sig") continue;

       EvpPkeyPtr pkey;
       if (kty == "RSA")
          {std::string n, e, nb, eb;
           if (!getStringClaim(kobj, "n", n) || !getStringClaim(kobj, "e", e)) continue;
           if (!decodeBase64URL(n, nb) || !decodeBase64URL(e, eb)) continue;
           pkey = makeRSAPublicKey(nb, eb);
          }
       else if (kty == "EC")
          {std::string crv, x, y, xb, yb;
           if (!getStringClaim(kobj, "crv", crv)
           ||  !getStringClaim(kobj, "x", x)
           ||  !getStringClaim(kobj, "y", y)) continue;
           if (!decodeBase64URL(x, xb) || !decodeBase64URL(y, yb)) continue;
           pkey = makeECPublicKey(crv, xb, yb);
          }
       else continue;
       if (!pkey) continue;

       // A JWK without "kid" is still usable (see verifyAgainstKeys). Store it
       // under a synthetic name that cannot collide with a real kid (leading NUL).
       std::string kid;
       if (!getStringClaim(kobj, "kid", kid))
          kid = std::string("\0nokid#", 7) + std::to_string(anonIdx++);
       // Assigning into the owning map releases any previous key for this kid.
       keys[kid] = std::move(pkey);
      }
   if (keys.empty())
      {emsg = "no usable RSA/EC signing keys in JWKS";
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
       JWTLog.Emsg("Auth", "jwt", ("jwks cache load failed: " + emsg).c_str());
       return;
      }
   JwksDiskEntry &e = cache[policy->issuer];
   e.jwksUrl = jwksUrl;
   e.fetchedAt = fetchedAt;
   e.jwksB64 = encodeBase64URL(jwksJson);
   if (!writeJWKSCacheMap(cache, emsg))
      JWTLog.Emsg("Auth", "jwt", ("jwks cache write failed: " + emsg).c_str());
}

// Install a freshly loaded key set. Only in-memory work happens under keysMtx.
void installKeysLocked(IssuerPolicy &policy, std::map<std::string, EvpPkeyPtr> &keys,
                       time_t now)
{
   freeKeys(policy.jwksKeys);
   policy.jwksKeys.swap(keys);
   policy.lastJwksLoad = now;
   policy.lastFetchFailure = 0;
   policy.lastFetchError.clear();
}

bool acceptDiscoveryDocument(const std::string &json,
                             const std::string &configuredIssuer,
                             std::string &jwksUrl,
                             std::string &emsg);

// Fetch the JWKS for a policy from the network (via discovery if needed).
// Runs without any policy lock held. On success jwksUrl/jwksJson are set.
bool fetchJWKSDocument(const IssuerPolicy &policy, std::string &jwksUrl,
                       std::string &jwksJson, std::string &emsg)
{
   jwksUrl = policy.jwksURL;
   if (jwksUrl.empty())
      {if (policy.jwtConfigURL.empty())
          {emsg = "JWT config URL not set";
           return false;
          }
       std::string cfg;
       if (!fetchURL(policy.jwtConfigURL, cfg, emsg))
          {emsg = "failed OpenID discovery fetch: " + emsg;
           return false;
          }
       if (!acceptDiscoveryDocument(cfg, policy.issuer, jwksUrl, emsg))
          return false;
      }
   if (!fetchURL(jwksUrl, jwksJson, emsg))
      {emsg = "failed JWKS fetch: " + emsg;
       return false;
      }
   return true;
}

// Refresh the key set of an issuer.
//
//  force == false : periodic refresh; a no-op while the cached keys are younger
//                   than -jwks-refresh. After a failed fetch, further periodic
//                   attempts are skipped for -jwks-refresh-min and the keys
//                   already in memory are used.
//  force == true  : the caller saw a signature that did not verify and suspects
//                   key rotation. To keep unauthenticated clients from turning
//                   this into a fetch storm against the issuer, forced refreshes
//                   are rate-limited to one per -jwks-refresh-min seconds per
//                   issuer (the limit is waived while no keys are held at all).
//
// The network fetch never runs while keysMtx is held, so token verification
// for this issuer is never blocked behind a slow or unreachable JWKS endpoint.
// A failed fetch is recorded under that lock: waiters that were already queued
// share the error instead of starting their own fetch.
bool refreshJWKSForPolicy(std::shared_ptr<IssuerPolicy> policy, bool force,
                          std::string &emsg)
{
   if (!policy)
      {emsg = "missing issuer policy";
       return false;
      }
   const time_t entered = time(nullptr);

   {
      std::scoped_lock lock(policy->keysMtx);
      const bool haveKeys = !policy->jwksKeys.empty();
      if (!force && haveKeys && (entered - policy->lastJwksLoad) < JwksRefresh)
         return true;
      // A recent failed fetch: keep the keys we have and do not contact the
      // issuer again until -jwks-refresh-min has elapsed.
      if (!force && haveKeys && policy->lastFetchFailure
      &&  (entered - policy->lastFetchFailure) < JwksRefreshMin)
         return true;
      if (force && haveKeys
      &&  policy->lastForcedAttempt
      &&  (entered - policy->lastForcedAttempt) < JwksRefreshMin)
         {emsg = "JWKS refresh rate-limited";
          return false;
         }
      // Claim the forced-refresh slot before fetching so that concurrent callers
      // within the cooldown fail fast instead of queueing behind this fetch.
      if (force) policy->lastForcedAttempt = entered;
   }

   // Serialize the slow path per issuer: one fetch serves a whole burst.
   std::scoped_lock fetchLock(policy->fetchMtx);
   {
      std::scoped_lock lock(policy->keysMtx);
      // Another thread completed a refresh while we waited for fetchMtx.
      if (!policy->jwksKeys.empty() && policy->lastJwksLoad >= entered) return true;
      // Another thread's fetch failed while we waited. Share that result.
      if (policy->lastFetchFailure >= entered)
         {emsg = policy->lastFetchError;
          return false;
         }
      if (!force && !policy->jwksKeys.empty() && policy->lastFetchFailure
      &&  (entered - policy->lastFetchFailure) < JwksRefreshMin)
         return true;
   }

   const time_t now = time(nullptr);

   // Periodic refresh may be served from the on-disk cache when still fresh.
   if (!force && JwksCacheFile.size())
      {
       std::map<std::string, EvpPkeyPtr> cachedKeys;
       std::string cmsg;
       if (loadJWKSFromDiskCacheForPolicy(policy, now, cachedKeys, cmsg))
          {
           std::scoped_lock lock(policy->keysMtx);
           installKeysLocked(*policy, cachedKeys, now);
           return true;
          }
      }

   std::string useJwks, jwks;
   if (!fetchJWKSDocument(*policy, useJwks, jwks, emsg))
      {
       // Network failure: fall back to the disk cache if we have one.
       std::string fetchErr = emsg;
       std::map<std::string, EvpPkeyPtr> cachedKeys;
       std::string cmsg;
       if (JwksCacheFile.size()
       &&  loadJWKSFromDiskCacheForPolicy(policy, now, cachedKeys, cmsg))
          {
           std::scoped_lock lock(policy->keysMtx);
           installKeysLocked(*policy, cachedKeys, now);
           emsg.clear();
           return true;
          }
       emsg = fetchErr;
       {std::scoped_lock lock(policy->keysMtx);
        policy->lastFetchFailure = time(nullptr);
        policy->lastFetchError = fetchErr;
       }
       return false;
      }

   std::map<std::string, EvpPkeyPtr> newKeys;
   if (!loadJWKS(jwks, newKeys, emsg))
      {std::scoped_lock lock(policy->keysMtx);
       policy->lastFetchFailure = time(nullptr);
       policy->lastFetchError = emsg;
       return false;
      }
   {
      std::scoped_lock lock(policy->keysMtx);
      installKeysLocked(*policy, newKeys, now);
   }
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

// OIDC Discovery requires the issuer in the metadata to be the issuer URL
// that was fetched. One trailing slash is ignored so a configured issuer and
// a metadata issuer that differ only by that still match.
std::string issuerIdForCompare(std::string_view iss)
{
   while (!iss.empty()
      &&  std::isspace(static_cast<unsigned char>(iss.back())))
      iss.remove_suffix(1);
   if (iss.size() > 1 && iss.back() == '/') iss.remove_suffix(1);
   return std::string(iss);
}

bool issuerIdentifiersEqual(const std::string &configured,
                            const std::string &discovered)
{
   return issuerIdForCompare(configured) == issuerIdForCompare(discovered);
}

bool audienceClaimPresent(const nlohmann::json &payload)
{
   if (!payload.is_object() || !payload.contains("aud")) return false;
   const nlohmann::json &aud = payload.at("aud");
   if (aud.is_string()) return !aud.get<std::string>().empty();
   if (!aud.is_array()) return false;
   for (const auto &one : aud)
      if (one.is_string() && !one.get<std::string>().empty()) return true;
   return false;
}

bool scopeValueIsStorage(std::string_view scope)
{
   return (scope.size() >= 8 && scope.compare(0, 8, "storage.") == 0)
       || (scope.size() >= 5 && scope.compare(0, 5, "read:") == 0)
       || (scope.size() >= 6 && scope.compare(0, 6, "write:") == 0);
}

// WLCG (wlcg.ver) and SciTokens (read:/write:) access tokens. azp is an ID
// token check and is not part of those profiles.
bool isStorageAccessToken(const nlohmann::json &payload)
{
   std::string wlcgVer;
   if (getStringClaim(payload, "wlcg.ver", wlcgVer)) return true;
   if (!payload.is_object() || !payload.contains("scope")) return false;
   const nlohmann::json &scope = payload.at("scope");
   if (scope.is_string())
      {
       const std::string text = scope.get<std::string>();
       std::string_view rest = text;
       while (!rest.empty())
          {
           while (!rest.empty()
              &&  std::isspace(static_cast<unsigned char>(rest.front())))
              rest.remove_prefix(1);
           if (rest.empty()) break;
           size_t n = 0;
           while (n < rest.size()
              &&  !std::isspace(static_cast<unsigned char>(rest[n])))
              ++n;
           if (scopeValueIsStorage(rest.substr(0, n))) return true;
           rest.remove_prefix(n);
          }
       return false;
      }
   if (!scope.is_array()) return false;
   for (const auto &one : scope)
      if (one.is_string() && scopeValueIsStorage(one.get<std::string>()))
         return true;
   return false;
}

// RFC 7515: a crit header must be understood. This plugin implements no
// header extensions, so any crit member is a rejection.
bool headerCritAccepted(const nlohmann::json &hdr, std::string &emsg)
{
   if (!hdr.is_object() || !hdr.contains("crit")) return true;
   emsg = "JWT crit header is not supported";
   return false;
}

bool scopeTextHasWord(std::string_view text, std::string_view word)
{
   while (!text.empty())
      {
       while (!text.empty()
          &&  std::isspace(static_cast<unsigned char>(text.front())))
          text.remove_prefix(1);
       if (text.empty()) break;
       size_t n = 0;
       while (n < text.size()
          &&  !std::isspace(static_cast<unsigned char>(text[n])))
          ++n;
       if (text.substr(0, n) == word) return true;
       text.remove_prefix(n);
      }
   return false;
}

// An ID token carries the openid scope, or the OIDC hash/nonce claims.
// A multi-audience access token without those is left alone.
bool tokenRequestsOpenID(const nlohmann::json &payload)
{
   if (!payload.is_object()) return false;
   if (payload.contains("nonce") || payload.contains("at_hash")) return true;
   if (!payload.contains("scope")) return false;
   const nlohmann::json &scope = payload.at("scope");
   if (scope.is_string())
      {const std::string text = scope.get<std::string>();
       return scopeTextHasWord(text, "openid");
      }
   if (!scope.is_array()) return false;
   for (const auto &one : scope)
      if (one.is_string() && one.get<std::string>() == "openid") return true;
   return false;
}

// OIDC Core: an ID token with more than one aud value must include azp, and
// azp must itself be one of those audiences. WLCG and SciTokens access tokens
// skip this, as do access tokens that are not OpenID ID tokens.
bool authorizedPartyOk(const nlohmann::json &payload, std::string &emsg)
{
   if (isStorageAccessToken(payload) || !tokenRequestsOpenID(payload)) return true;
   if (!payload.is_object() || !payload.contains("aud")) return true;
   const nlohmann::json &aud = payload.at("aud");
   if (!aud.is_array()) return true;
   size_t n = 0;
   for (const auto &one : aud)
      if (one.is_string() && !one.get<std::string>().empty()) ++n;
   if (n <= 1) return true;
   std::string azp;
   if (!getStringClaim(payload, "azp", azp))
      {emsg = "token azp missing";
       return false;
      }
   if (!hasStringInArrayClaim(payload, "aud", azp))
      {emsg = "token azp is not an audience";
       return false;
      }
   return true;
}

bool acceptDiscoveryDocument(const std::string &json,
                             const std::string &configuredIssuer,
                             std::string &jwksUrl,
                             std::string &emsg)
{
   jwksUrl.clear();
   nlohmann::json cfg;
   if (!parseJsonObject(json, cfg))
      {emsg = "invalid OpenID discovery JSON";
       return false;
      }
   std::string discoveredIss;
   if (!getStringClaim(cfg, "issuer", discoveredIss))
      {emsg = "issuer missing or empty in OpenID discovery";
       return false;
      }
   if (!issuerIdentifiersEqual(configuredIssuer, discoveredIss))
      {emsg = "OpenID discovery issuer does not match configured issuer";
       return false;
      }
   if (!getStringClaim(cfg, "jwks_uri", jwksUrl))
      {emsg = "jwks_uri missing or empty in OpenID discovery";
       return false;
      }
   if (!hasPrefix(jwksUrl.c_str(), "https://"))
      {emsg = "jwks_uri in discovery must use https://";
       return false;
      }
   return true;
}

bool parseAndValidateJWT(std::string_view rawTok, std::string &payloadJSON,
                         std::string &headerJSON,
                         std::string &identity, uint64_t &expOut,
                         std::string &emsg,
                         std::string *identityMethod = nullptr)
{
   payloadJSON.clear();
   headerJSON.clear();
   identity.clear();
   expOut = 0;

   std::string_view tok = Strip(rawTok);
   if (tok.empty()) {emsg = "invalid token format"; return false;}
   std::string jwt(tok);

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
   const JwsAlg *jwsAlg = lookupJwsAlg(alg);
   if (!jwsAlg) {emsg = "unsupported JWT alg"; return false;}
   if (!headerCritAccepted(hdrObj, emsg)) return false;
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
   // WLCG profile requires aud. The value is compared only when this issuer
   // has an audience configured; without that, any aud is still accepted.
   std::string wlcgVer;
   if (getStringClaim(payloadObj, "wlcg.ver", wlcgVer)
   &&  !audienceClaimPresent(payloadObj))
      {emsg = "token audience missing"; return false;}
   if (!authorizedPartyOk(payloadObj, emsg)) return false;

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
      verified = verifyAgainstKeys(*jwsAlg, kid, policy->jwksKeys, signedData, sig);
   }
   if (!verified)
      {
       // If no refresh source is configured, fail with a signature error
       // instead of masking it with an JWT/JWKS URL configuration error.
       if (policy->jwksURL.empty() && policy->jwtConfigURL.empty())
          {emsg = "JWT signature validation failed";
           return false;
          }
       // The signature may have been produced with a freshly rotated key; try
       // once more after a (rate-limited) forced refresh. A refused or failed
       // refresh is reported as a signature failure, which is what the caller
       // actually observed.
       std::string rmsg;
       if (!refreshJWKSForPolicy(policy, true, rmsg))
          {emsg = "JWT signature validation failed (" + rmsg + ")";
           return false;
          }
       std::scoped_lock lock(policy->keysMtx);
       verified = verifyAgainstKeys(*jwsAlg, kid, policy->jwksKeys, signedData, sig);
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
      } else if (!policy->defaultUser.empty()) {
       // SciTokens: no username_claim and map_subject is off, so the issuer's
       // default_user is the account. A subject claim is not consulted.
       identity = policy->defaultUser;
       if (identityMethod) *identityMethod = "default-user";
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
   if (!isSafeAccountName(identity))
      {emsg = "token identity is not a safe account name";
       return false;
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
   const char *tid = (tident && *tident ? tident : "jwt");
   uint64_t hits = TokenCacheHits.load();
   uint64_t misses = TokenCacheMisses.load();
   size_t csize = tokenCacheSize();

   if (DebugToken)
      {std::string hMsg = std::string("jwt.header ") + headerJSON;
       std::string pMsg = std::string("jwt.payload ") + payloadJSON;
       std::string sMsg = "jwt.cache.stats"
                          " cache_hit=" + std::string(cacheHit ? "1" : "0") +
                          " hits=" + std::to_string(hits) +
                          " misses=" + std::to_string(misses) +
                          " size=" + std::to_string(csize);
       JWTLog.Emsg("Auth", tid, hMsg.c_str());
       JWTLog.Emsg("Auth", tid, pMsg.c_str());
       JWTLog.Emsg("Auth", tid, sMsg.c_str());
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

   std::string msg = "jwt.claims";
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
   JWTLog.Emsg("Auth", tid, msg.c_str());
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
      {if (!nextIntArg(cfg, erp, "-jwks-refresh", 1, INT_MAX, false, v)) return OptResult::Error;
       JwksRefresh = static_cast<int>(v); return OptResult::Ok;
      }
   if (!strcmp(val, "-jwks-refresh-min"))
      {if (!nextIntArg(cfg, erp, "-jwks-refresh-min", 0, INT_MAX, false, v)) return OptResult::Error;
       JwksRefreshMin = static_cast<int>(v); return OptResult::Ok;
      }
   if (!strcmp(val, "-jwks-cache-ttl"))
      {if (!nextIntArg(cfg, erp, "-jwks-cache-ttl", 0, INT_MAX, false, v)) return OptResult::Error;
       JwksCacheTTL = static_cast<int>(v); return OptResult::Ok;
      }
   if (!strcmp(val, "-clock-skew"))
      {if (!nextIntArg(cfg, erp, "-clock-skew", 0, 3600, false, v)) return OptResult::Error;
       ClockSkew = static_cast<int>(v); return OptResult::Ok;
      }
   if (!strcmp(val, "-token-cache-max"))
      {if (!nextIntArg(cfg, erp, "-token-cache-max", 0, INT_MAX, false, v)) return OptResult::Error;
       TokenCacheMax = static_cast<int>(v); return OptResult::Ok;
      }
   if (!strcmp(val, "-token-cache-noexp-ttl"))
      {if (!nextIntArg(cfg, erp, "-token-cache-noexp-ttl", 0, INT_MAX, false, v)) return OptResult::Error;
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
   if (!strcmp(val, "-jwt-config-url"))
      {const char *arg = nextPolicyArg(cfg, erp, "-jwt-config-url", curPolicy);
       if (!arg) return OptResult::Error;
       curPolicy->jwtConfigURL = arg; return OptResult::Ok;
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
   if (!strcmp(val, "-authorization-strategy"))
      {const char *arg = nextPolicyArg(cfg, erp, "-authorization-strategy", curPolicy);
       if (!arg) return OptResult::Error;
       std::string canonical, localErr;
       if (!normalizeAuthzStrategy(arg, canonical, localErr))
          {Fatal(erp, localErr.c_str(), EINVAL); return OptResult::Error;}
       curPolicy->authzStrategy = canonical; return OptResult::Ok;
      }
   if (!strcmp(val, "-onmissing"))
      {const char *arg = nextPolicyArg(cfg, erp, "-onmissing", curPolicy);
       if (!arg) return OptResult::Error;
       std::string canonical, localErr;
       if (!normalizeOnMissing(arg, canonical, localErr))
          {Fatal(erp, localErr.c_str(), EINVAL); return OptResult::Error;}
       curPolicy->onmissing = canonical; return OptResult::Ok;
      }
   if (!strcmp(val, "-name-mapfile"))
      {const char *arg = nextPolicyArg(cfg, erp, "-name-mapfile", curPolicy);
       if (!arg) return OptResult::Error;
       std::string canonical;
       if (!canonicalAbsolutePath(arg, canonical))
          {Fatal(erp, "name_mapfile is not an absolute path.", EINVAL);
           return OptResult::Error;}
       curPolicy->nameMapFile = canonical; return OptResult::Ok;
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

// Options that take one argument and must not be applied again on reload.
bool inlineOptionTakesArg(const char *val)
{
   return !strcmp(val, "-maxsz")
       || !strcmp(val, "-jwks-refresh")
       || !strcmp(val, "-jwks-refresh-min")
       || !strcmp(val, "-jwks-cache-ttl")
       || !strcmp(val, "-clock-skew")
       || !strcmp(val, "-token-cache-max")
       || !strcmp(val, "-token-cache-noexp-ttl")
       || !strcmp(val, "-expiry")
       || !strcmp(val, "-config-file")
       || !strcmp(val, "-scitokens-config")
       || !strcmp(val, "-jwks-cache-file")
       || !strcmp(val, "-identity-claim")
       || !strcmp(val, "-entity-claim");
}

bool overlayInlineIssuerParms(const std::string &parms,
                             std::vector<std::shared_ptr<IssuerPolicy>> &policies,
                             std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> &byIssuer,
                             std::unordered_map<std::string, std::string> &emailMap,
                             std::string &emsg)
{
   if (parms.empty()) return true;

   // applyIssuerInitOpt writes the process-wide maps. Install the file's
   // policies for the duration of the parse, then put the live maps back.
   std::scoped_lock lock(ConfigMtx);
   auto savedPolicies = std::move(IssuerPolicies);
   auto savedByIssuer = std::move(IssuerPolicyByIssuer);
   auto savedEmail = std::move(EmailIdentityMap);
   IssuerPolicies = std::move(policies);
   IssuerPolicyByIssuer = std::move(byIssuer);
   EmailIdentityMap = std::move(emailMap);

   bool ok = true;
   std::vector<char> buf(parms.begin(), parms.end());
   buf.push_back('\0');
   XrdOucTokenizer cfg(buf.data());
   XrdOucErrInfo erp;
   std::shared_ptr<IssuerPolicy> curPolicy;
   char *val = nullptr;
   while (ok && cfg.GetLine())
      {
       while (ok && (val = cfg.GetToken()))
            {
             OptResult r = applyIssuerInitOpt(val, cfg, curPolicy, &erp);
             if (r == OptResult::Error) {ok = false; break;}
             if (r == OptResult::Ok) continue;
             if (!strcmp(val, "-email-map"))
                {char *arg = cfg.GetToken();
                 if (!arg)
                    {Fatal(&erp, "-email-map argument missing", EINVAL);
                     ok = false;
                     break;
                    }
                 if (!addEmailMapEntry(arg, EmailIdentityMap, emsg))
                    {Fatal(&erp, emsg.c_str(), EINVAL);
                     ok = false;
                    }
                 continue;
                }
             if (!strcmp(val, "-debug-token") || !strcmp(val, "-show-token-claims"))
                continue;
             if (inlineOptionTakesArg(val))
                {if (!cfg.GetToken())
                    {Fatal(&erp, (std::string(val) + " argument missing").c_str(), EINVAL);
                     ok = false;
                    }
                 continue;
                }
             Fatal(&erp, (std::string("Invalid parameter - ") + val).c_str(), EINVAL);
             ok = false;
            }
      }
   if (!ok)
      {int ec = 0;
       const char *et = erp.getErrText(ec);
       if (et && *et) emsg = et;
       else if (emsg.empty()) emsg = "inline issuer option rejected";
      }
   else
      {policies = std::move(IssuerPolicies);
       byIssuer = std::move(IssuerPolicyByIssuer);
       emailMap = std::move(EmailIdentityMap);
      }
   IssuerPolicies = std::move(savedPolicies);
   IssuerPolicyByIssuer = std::move(savedByIssuer);
   EmailIdentityMap = std::move(savedEmail);
   return ok;
}

// Pre-scan: pull out -config-file (consumed here) and accumulate the remaining
// tokens into inlineParms. Returns false (erp set) on a malformed -config-file.
bool prescanInitParms(const char *parms, std::string &selectedCfgPath,
                      bool &requestedCfgOverride, std::string &inlineParms,
                      XrdOucErrInfo *erp,
                      std::string *sciTokensCfgPath, bool *requestedSciTokens)
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
               if (!strcmp(tok, "-config-file") || !strcmp(tok, "-scitokens-config"))
                  {const bool sci = !strcmp(tok, "-scitokens-config");
                   char *path = t.GetToken();
                   if (!path || !*path)
                      {Fatal(erp, (std::string(tok) + " argument missing").c_str(), EINVAL);
                       return false;
                      }
                   if ((sci && requestedCfgOverride) || (!sci && requestedSciTokens && *requestedSciTokens))
                      {Fatal(erp, "-config-file and -scitokens-config cannot be combined", EINVAL);
                       return false;
                      }
                   if (sci)
                      {if (!sciTokensCfgPath || !requestedSciTokens)
                          {Fatal(erp, "-scitokens-config is not available", EINVAL);
                           return false;
                          }
                       *sciTokensCfgPath = path;
                       *requestedSciTokens = true;
                      }
                   else
                      {selectedCfgPath = path;
                       requestedCfgOverride = true;
                      }
                   continue;
                  }
               if (!inlineParms.empty()) inlineParms.push_back(' ');
               inlineParms += tok;
              }
        }
   return true;
}

// Parse the effective option string into the global config and issuer policies.
// Option tokens are char* from XrdOucTokenizer, in accordance with the other
// security plugins.
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
       if (policy->jwtConfigURL.empty() && !policy->issuer.empty())
          policy->jwtConfigURL = joinURL(policy->issuer, "/.well-known/openid-configuration");
       if (policy->jwtConfigURL.empty() && policy->jwksURL.empty())
          {std::string e = "issuer '" + policy->issuer + "' requires -jwt-config-url or -jwks-url";
           Fatal(erp, e.c_str(), EINVAL); return false;
          }
       if ((!policy->jwtConfigURL.empty() && !hasPrefix(policy->jwtConfigURL.c_str(), "https://"))
       ||  (!policy->jwksURL.empty() && !hasPrefix(policy->jwksURL.c_str(), "https://")))
          {std::string e = "issuer '" + policy->issuer + "' has non-https JWT/JWKS URL";
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

static const uint64_t jwtVersion = 0;

bool isConfigured()
{
   std::scoped_lock lock(ConfigMtx);
   return !IssuerPolicies.empty();
}

std::string_view stripToken(std::string_view token)
{
   return Strip(token);
}

bool validateToken(std::string_view rawTok, std::string &identity, std::string &emsg,
                   uint64_t *expTime,
                   std::map<std::string, std::string> *entityAttrs)
{
   if (rawTok.empty())
      {emsg = "missing token";
       return false;
      }

   std::string_view tok = Strip(rawTok);
   if (tok.empty())
      {emsg = "token value malformed";
       return false;
      }

   if (static_cast<int>(tok.size()) > MaxTokSize)
      {emsg = "token too large";
       return false;
      }

   std::string payloadJSON, headerJSON, msgRC, identityMethod;
   std::string tokKey = sha256hex(tok);
   if (tokKey.empty())
      {emsg = "token fingerprint failed";
       return false;
      }
   // Before the cache lookup, so a removed issuer or audience takes effect
   // even when this token is already cached. An unchanged file is one stat.
   maybeReloadJWTFileConfig();
   uint64_t now = static_cast<uint64_t>(time(nullptr));
   CachedTokenEntry cached;
   if (tokenCacheLookup(tokKey, now, cached))
      {
       TokenCacheHits++;
       identity = cached.identity;
       if (expTime) *expTime = cached.expiresAt;
       if (entityAttrs)
          {populateEntityAttrs(cached.payloadJSON, *entityAttrs);
           std::string typ;
           if (getStringClaim(cached.headerJSON, "typ", typ)
           &&  isSafeEntityAttrValue(typ))
              (*entityAttrs)["jwt.typ"] = typ;
           if (isSafeEntityAttrValue(identity))
              (*entityAttrs)["request.name"] = identity;
          }
       debugPrintToken("jwt", identity.c_str(), cached.headerJSON,
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

   debugPrintToken("jwt", identity.c_str(), headerJSON, payloadJSON, false,
                   identityMethod);

   CachedTokenEntry ins;
   ins.identity = identity;
   ins.identityMethod = identityMethod;
   ins.headerJSON = headerJSON;
   ins.payloadJSON = payloadJSON;
   // Cache lifetime: when exp is enforced, the entry lives exactly as long as
   // validation would keep accepting the token (exp + clock skew). When exp is
   // ignored, or absent, fall back to the no-exp TTL; otherwise an already
   // expired-but-accepted token would be inserted dead and never hit.
   if (expOut && expiry != 0)
      ins.expiresAt = expOut + static_cast<uint64_t>(ClockSkew.load());
   else
      ins.expiresAt = now + static_cast<uint64_t>(TokenCacheNoExpTTL.load());
   tokenCacheStore(tokKey, ins, now);
   if (expTime) *expTime = ins.expiresAt;
   if (entityAttrs)
      {populateEntityAttrs(payloadJSON, *entityAttrs);
       std::string typ;
       if (getStringClaim(headerJSON, "typ", typ) && isSafeEntityAttrValue(typ))
          (*entityAttrs)["jwt.typ"] = typ;
       if (isSafeEntityAttrValue(identity))
          (*entityAttrs)["request.name"] = identity;
      }
   return true;
}

// Return every tunable to its built-in default. initSecProtocol() may run more
// than once in a process (e.g. when a second XrdSecService instance is created);
// each run must start from a clean slate rather than layering onto the last.
void resetGlobalsToDefaults()
{
   {
      std::scoped_lock lock(ConfigMtx);
      clearIssuerPolicies();
      EmailIdentityMap.clear();
      JWTConfigPath = kDefaultJWTConfigPath;
      JWTConfigWatch = false;
      InlineInitParms.clear();
      JWTConfigStatValid = false;
      JWTConfigIno = 0;
      JWTConfigMTime = 0;
      JWTConfigCTime = 0;
      JWTConfigSize = 0;
      clearRejectedConfigMetaLocked();
   }
   EntityClaimMappings.clear();
   SciTokensConfig = false;
   JwksCacheFile.clear();
   JwksCacheTTL = 0;
   expiry = kDefaultExpiry;
   MaxTokSize = kDefaultMaxTokSize;
   ClockSkew = kDefaultClockSkew;
   JwksRefresh = kDefaultJwksRefresh;
   JwksRefreshMin = kDefaultJwksRefreshMin;
   TokenCacheMax = kDefaultTokenCacheMax;
   TokenCacheNoExpTTL = kDefaultTokenCacheNoExpTTL;
   DebugToken = false;
   DebugTokenClaims = false;
   customIdentityClaims = false;
   IdentityClaims = kDefaultIdentityClaims;
   clearTokenCache();
   TokenCacheHits = 0;
   TokenCacheMisses = 0;
}

char *initSecProtocol(const char *parms, XrdOucErrInfo *erp)
{
   JWTLog.logger(&JWTLogger);

   resetGlobalsToDefaults();
   std::shared_ptr<IssuerPolicy> curPolicy;
   std::string fileBackedParms;
   std::string inlineParms;
   std::string selectedCfgPath = kDefaultJWTConfigPath;
   std::string sciTokensCfgPath;
   bool requestedCfgOverride = false;
   bool requestedSciTokens = false;
   bool loadedCfgFile = false;

   if (!prescanInitParms(parms, selectedCfgPath, requestedCfgOverride, inlineParms, erp,
                         &sciTokensCfgPath, &requestedSciTokens))
      return nullptr;

   SafeFileResult cfgMeta{};
   if (requestedSciTokens)
      {
       std::string cfgErr;
       SafeFileResult sfr;
       if (!safeReadConfigFile(sciTokensCfgPath.c_str(), sfr, cfgErr))
          {Fatal(erp, cfgErr.c_str(), EINVAL);
           return nullptr;
          }
       if (!sfr.found)
          {std::string e = std::string("Missing SciTokens config file: ") + sciTokensCfgPath;
           Fatal(erp, e.c_str(), ENOENT);
           return nullptr;
          }
       if (!parseSciTokensConfig(sciTokensCfgPath.c_str(), sfr.contents,
                                 IssuerPolicies, IssuerPolicyByIssuer, cfgErr))
          {Fatal(erp, cfgErr.c_str(), EINVAL);
           return nullptr;
          }
       SciTokensConfig = true;
       selectedCfgPath = sciTokensCfgPath;
       cfgMeta.ino = sfr.ino;
       cfgMeta.mtime = sfr.mtime;
       cfgMeta.ctime = sfr.ctime;
       cfgMeta.size = sfr.size;
       cfgMeta.found = true;
       loadedCfgFile = true;
      }
   else if (!parms || !*parms || requestedCfgOverride)
      {
       bool cfgFound = false;
       std::string cfgErr;
       if (!loadJWTIniAsArgs(selectedCfgPath.c_str(), fileBackedParms, cfgFound, cfgErr,
                              &cfgMeta))
          {Fatal(erp, cfgErr.c_str(), EINVAL);
           return nullptr;
          }
       if (!cfgFound)
          {std::string e = std::string("Missing required JWT config file: ")
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
   const char *toParse = parms;
   if (requestedSciTokens) toParse = inlineParms.c_str();
   else if (!effectiveParms.empty()) toParse = effectiveParms.c_str();
   if (!parseInitParms(toParse, curPolicy, erp))
      return nullptr;

   if (requestedSciTokens)
      {
       std::string claimErr;
       auto haveClaim = [](const std::string &name) {
          for (const auto &mapping : EntityClaimMappings)
             if (mapping.jwtClaim == name) return true;
          return false;
       };
       if ((!haveClaim("scope") && !storeEntityClaimEntry("scope", claimErr))
       ||  (!haveClaim("wlcg.ver") && !storeEntityClaimEntry("wlcg.ver", claimErr)))
          {Fatal(erp, claimErr.c_str(), EINVAL);
           return nullptr;
          }
      }

   if (IssuerPolicies.empty())
      {Fatal(erp, "At least one -issuer must be configured.", EINVAL);
       return nullptr;
      }

   if (IdentityClaims.empty())
      {Fatal(erp, "At least one identity claim must be configured.", EINVAL);
       return nullptr;
      }

   ensureCurlGlobalInit();

   if (!validateAndWarmInitIssuers(erp)) return nullptr;

   if (loadedCfgFile)
      {
       std::scoped_lock lock(ConfigMtx);
       JWTConfigPath = selectedCfgPath;
       recordConfigMetaLocked(cfgMeta);
       JWTConfigWatch = true;
       InlineInitParms = inlineParms;
      }

   char buff[256];
   snprintf(buff, sizeof(buff), "TLS:%" PRIu64 ":%d:", jwtVersion, MaxTokSize.load());
   return strdup(buff);
}

} // namespace detail

bool IsConfigured()
{
   return detail::isConfigured();
}

int MaxTokenSize()
{
   return detail::MaxTokSize.load();
}

std::string_view StripToken(std::string_view token)
{
   return detail::stripToken(token);
}

bool ValidateToken(std::string_view rawTok, std::string &identity, std::string &emsg,
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
   detail::JWTLog.logger(logger ? logger : &detail::JWTLogger);
   XrdOucErrInfo erp;
   char *rc = detail::initSecProtocol(parms, &erp);
   if (!rc)
      {int ec = 0;
       const char *et = erp.getErrText(ec);
       emsg = (et && *et ? et : "JWT initialization failed");
       return false;
      }
   free(rc);
   return true;
}

} // namespace XrdSecJWT
