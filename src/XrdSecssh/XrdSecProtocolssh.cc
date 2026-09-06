/******************************************************************************/
/*                                                                            */
/*                  X r d S e c P r o t o c o l s s h . c c                   */
/*                                                                            */
/* (c) 2026 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/******************************************************************************/

#define __STDC_FORMAT_MACROS 1

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <strings.h>
#include <pwd.h>

#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/param_build.h>
#include <openssl/params.h>
#endif

#include "XrdVersion.hh"
#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdNet/XrdNetUtils.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucErrInfo.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"

#ifndef EAUTH
#define EAUTH EBADE
#endif

XrdVERSIONINFO(XrdSecProtocolsshObject,secssh);

// RAII wrappers for OpenSSL handles. Defined at file scope (not in an anonymous
// namespace) because XrdSecProtocolssh, which has external linkage, uses
// EvpPkeyPtr as a member type.
struct EvpPkeyDeleter    {void operator()(EVP_PKEY *p)     const noexcept {EVP_PKEY_free(p);}};
struct EvpMdCtxDeleter   {void operator()(EVP_MD_CTX *p)   const noexcept {EVP_MD_CTX_free(p);}};
struct BignumDeleter     {void operator()(BIGNUM *p)       const noexcept {BN_free(p);}};
struct BioDeleter        {void operator()(BIO *p)          const noexcept {BIO_free(p);}};
struct EvpPkeyCtxDeleter {void operator()(EVP_PKEY_CTX *p) const noexcept {EVP_PKEY_CTX_free(p);}};

using EvpPkeyPtr    = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;
using EvpMdCtxPtr   = std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter>;
using BignumPtr     = std::unique_ptr<BIGNUM, BignumDeleter>;
using BioPtr        = std::unique_ptr<BIO, BioDeleter>;
using EvpPkeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter>;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
struct OsslParamBldDeleter {void operator()(OSSL_PARAM_BLD *p) const noexcept {OSSL_PARAM_BLD_free(p);}};
struct OsslParamDeleter    {void operator()(OSSL_PARAM *p)     const noexcept {OSSL_PARAM_free(p);}};
using OsslParamBldPtr = std::unique_ptr<OSSL_PARAM_BLD, OsslParamBldDeleter>;
using OsslParamPtr    = std::unique_ptr<OSSL_PARAM, OsslParamDeleter>;
#else
struct RsaDeleter {void operator()(RSA *p) const noexcept {RSA_free(p);}};
using RsaPtr = std::unique_ptr<RSA, RsaDeleter>;
#endif

namespace
{
XrdSecCredentials *FatalC(XrdOucErrInfo *erp, const char *eMsg, int rc, bool hdr=true)
{
   if (!erp) std::cerr << (hdr ? "Secssh: " : "") << eMsg << "\n" << std::flush;
      else {
         const char *eVec[2] = {(hdr ? "Secssh: " : ""), eMsg};
         erp->setErrInfo(rc, eVec, 2);
      }
   return 0;
}

int FatalS(XrdOucErrInfo *erp, const char *eMsg, int rc, bool hdr=true)
{
   FatalC(erp, eMsg, rc, hdr);
   return -1;
}

// Protocol version 1: the challenge response carries the hostname the client
// connected to and the signed payload binds nonce, key fingerprint and that
// hostname (see challengePayload()).
static const uint8_t kProtoVersion = 1;
static const int kDefaultMaxCredSize = 8192;
static const int kDefaultNonceTTL = 30;
static const int kMinRsaBits = 2048;
static const int kAgentIoTimeoutSec = 10;
static const size_t kMaxHostnameLen = 255;
static const size_t kMaxSshWireFieldLen = 65536;
static const size_t kMaxKeysFileLineLen = 8192;
static const size_t kMaxKeysFileB64Len = 16384;
static const size_t kMaxPrincipalMapLineLen = 1024;
static const size_t kMaxLocalUsernameLen = 64;
static const size_t kDebugFpPrefixLen = 12;

bool sshWireFieldLenOk(uint32_t n)
{
   return n <= kMaxSshWireFieldLen;
}

std::string redactFp(const std::string &fp)
{
   if (fp.size() <= kDebugFpPrefixLen) return fp;
   return fp.substr(0, kDebugFpPrefixLen) + "...";
}

bool isValidMappedUsername(const std::string &user)
{
   if (user.empty() || user.size() > kMaxLocalUsernameLen) return false;
   unsigned char c0 = static_cast<unsigned char>(user[0]);
   if (!isalnum(c0) && c0 != '_') return false;
   for (size_t i = 1; i < user.size(); ++i)
      {
         unsigned char c = static_cast<unsigned char>(user[i]);
         if (!isalnum(c) && c != '_' && c != '-' && c != '.') return false;
      }
   return true;
}

// Canonical form of a hostname for comparison: lower-case, no trailing dot,
// no IPv6 brackets. Returns an empty string for values that cannot be a
// hostname or IP literal.
std::string normalizeHostname(const std::string &in)
{
   std::string h = in;
   if (h.size() >= 2 && h.front() == '[' && h.back() == ']')
      h = h.substr(1, h.size() - 2);
   while (!h.empty() && h.back() == '.') h.pop_back();
   if (h.empty() || h.size() > kMaxHostnameLen) return std::string();
   for (char &c : h)
      {
         unsigned char uc = static_cast<unsigned char>(c);
         if (!isalnum(uc) && uc != '-' && uc != '.' && uc != ':' && uc != '_')
            return std::string();
         c = static_cast<char>(tolower(uc));
      }
   return h;
}

bool hasPrefix(const char *s, const char *pfx)
{
   return s && pfx && strncmp(s, pfx, strlen(pfx)) == 0;
}

std::string trim(const std::string &in)
{
   size_t b = 0;
   while (b < in.size() && isspace(static_cast<unsigned char>(in[b]))) b++;
   size_t e = in.size();
   while (e > b && isspace(static_cast<unsigned char>(in[e-1]))) e--;
   return in.substr(b, e - b);
}

bool b64Decode(const std::string &in, std::string &out)
{
   out.clear();
   if (in.empty() || in.size() > kMaxKeysFileB64Len) return false;
   bool hasData = false;
   for (unsigned char c : in)
      {if (c != '=') {hasData = true; break;}}
   if (!hasData) return false;
   std::string s = in;
   while ((s.size() % 4) != 0) s.push_back('=');
   std::vector<unsigned char> buf(s.size() + 4);
   int n = EVP_DecodeBlock(buf.data(),
                           reinterpret_cast<const unsigned char *>(s.data()),
                           static_cast<int>(s.size()));
   if (n < 0) return false;
   while (!s.empty() && s.back() == '=') {n--; s.pop_back();}
   if (n < 0) return false;
   out.assign(reinterpret_cast<const char *>(buf.data()), static_cast<size_t>(n));
   return true;
}

std::string b64Encode(const unsigned char *data, size_t len)
{
   if (!data || !len) return "";
   std::vector<unsigned char> out(((len + 2) / 3) * 4 + 4);
   int n = EVP_EncodeBlock(out.data(), data, static_cast<int>(len));
   if (n <= 0) return "";
   return std::string(reinterpret_cast<char *>(out.data()), static_cast<size_t>(n));
}

bool sha256Base64(const std::string &data, std::string &out)
{
   EvpMdCtxPtr ctx(EVP_MD_CTX_new());
   if (!ctx) return false;
   unsigned char md[EVP_MAX_MD_SIZE];
   unsigned int mdLen = 0;
   bool ok = EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) == 1
          && EVP_DigestUpdate(ctx.get(), data.data(), data.size()) == 1
          && EVP_DigestFinal_ex(ctx.get(), md, &mdLen) == 1;
   if (!ok) return false;
   out = b64Encode(md, mdLen);
   while (!out.empty() && out.back() == '=') out.pop_back();
   out.insert(0, "SHA256:");
   return true;
}

bool extractEd25519RawFromSshBlob(const std::string &blob, std::string &raw32)
{
   raw32.clear();
   if (blob.size() < 4) return false;
   const size_t sz = blob.size();
   const unsigned char *p = reinterpret_cast<const unsigned char *>(blob.data());
   uint32_t n1net = 0;
   memcpy(&n1net, p, 4);
   uint32_t n1 = ntohl(n1net);
   p += 4;
   if (!sshWireFieldLenOk(n1)) return false;
   // Use 64-bit arithmetic so attacker-supplied 32-bit lengths cannot wrap.
   if (static_cast<uint64_t>(n1) + 8 > sz) return false;
   std::string alg(reinterpret_cast<const char *>(p), n1);
   p += n1;
   if (alg != "ssh-ed25519") return false;
   uint32_t n2net = 0;
   memcpy(&n2net, p, 4);
   uint32_t n2 = ntohl(n2net);
   p += 4;
   if (!sshWireFieldLenOk(n2)) return false;
   if (static_cast<uint64_t>(n1) + n2 + 8 != sz) return false;
   if (n2 != 32) return false;
   raw32.assign(reinterpret_cast<const char *>(p), 32);
   return true;
}

bool parseSshString(const std::string &blob, size_t &at, std::string &val)
{
   val.clear();
   if (at + 4 > blob.size()) return false;
   uint32_t nnet = 0;
   memcpy(&nnet, blob.data() + at, 4);
   at += 4;
   uint32_t n = ntohl(nnet);
   if (!sshWireFieldLenOk(n) || at + n > blob.size()) return false;
   val.assign(blob.data() + at, n);
   at += n;
   return true;
}

void appendSshString(std::string &out, const std::string &s)
{
   uint32_t n = htonl(static_cast<uint32_t>(s.size()));
   out.append(reinterpret_cast<const char *>(&n), 4);
   out.append(s);
}

std::string encodeMpint(const unsigned char *buf, size_t len)
{
   std::string mp;
   if (!buf || len == 0)
      {
         uint32_t z = 0;
         mp.append(reinterpret_cast<const char *>(&z), 4);
         return mp;
      }
   bool needPad = (buf[0] & 0x80) != 0;
   uint32_t n = static_cast<uint32_t>(len + (needPad ? 1 : 0));
   uint32_t nn = htonl(n);
   mp.append(reinterpret_cast<const char *>(&nn), 4);
   if (needPad) mp.push_back('\0');
   mp.append(reinterpret_cast<const char *>(buf), len);
   return mp;
}

std::string bnToMpint(const BIGNUM *bn)
{
   if (!bn) return std::string();
   int n = BN_num_bytes(bn);
   if (n < 0) return std::string();
   std::vector<unsigned char> buf(static_cast<size_t>(n));
   if (n > 0) BN_bn2bin(bn, buf.data());
   return encodeMpint((n > 0 ? buf.data() : nullptr), static_cast<size_t>(n));
}

EvpPkeyPtr makeRSAPublicKeyFromNE(const std::string &nBin, const std::string &eBin)
{
   BignumPtr bnN(BN_bin2bn(reinterpret_cast<const unsigned char *>(nBin.data()),
                           static_cast<int>(nBin.size()), nullptr));
   BignumPtr bnE(BN_bin2bn(reinterpret_cast<const unsigned char *>(eBin.data()),
                           static_cast<int>(eBin.size()), nullptr));
   if (!bnN || !bnE) return nullptr;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
   EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr));
   if (!ctx) return nullptr;
   OsslParamBldPtr bld(OSSL_PARAM_BLD_new());
   if (!bld) return nullptr;
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
   RsaPtr rsa(RSA_new());
   if (!rsa) return nullptr;
   if (RSA_set0_key(rsa.get(), bnN.get(), bnE.get(), nullptr) != 1) return nullptr;
   bnN.release();
   bnE.release();
   EvpPkeyPtr pkey(EVP_PKEY_new());
   if (!pkey) return nullptr;
   if (EVP_PKEY_assign_RSA(pkey.get(), rsa.get()) != 1) return nullptr;
   rsa.release();
   return pkey;
#endif
}

bool extractRsaNEFromSshBlob(const std::string &blob, std::string &nBin, std::string &eBin)
{
   nBin.clear();
   eBin.clear();
   size_t at = 0;
   std::string alg, eMp, nMp;
   if (!parseSshString(blob, at, alg)) return false;
   if (alg != "ssh-rsa") return false;
   if (!parseSshString(blob, at, eMp)) return false;
   if (!parseSshString(blob, at, nMp)) return false;
   if (at != blob.size()) return false;
   // mpint payloads returned by parseSshString include only value bytes.
   eBin = eMp;
   nBin = nMp;
   if (eBin.empty() || nBin.empty()) return false;
   return true;
}

bool makeSshRsaBlobFromPkey(EVP_PKEY *pkey, std::string &blob)
{
   blob.clear();
   if (!pkey) return false;
   BignumPtr bnN, bnE;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
   BIGNUM *nRaw = nullptr, *eRaw = nullptr;
   if (EVP_PKEY_get_bn_param(pkey, "n", &nRaw) != 1
   ||  EVP_PKEY_get_bn_param(pkey, "e", &eRaw) != 1)
      return false;
   bnN.reset(nRaw);
   bnE.reset(eRaw);
#else
   RsaPtr rsa(EVP_PKEY_get1_RSA(pkey));
   if (!rsa) return false;
   const BIGNUM *nRef = nullptr, *eRef = nullptr;
   RSA_get0_key(rsa.get(), &nRef, &eRef, nullptr);
   if (!nRef || !eRef) return false;
   bnN.reset(BN_dup(nRef));
   bnE.reset(BN_dup(eRef));
   if (!bnN || !bnE) return false;
#endif
   std::string mpE = bnToMpint(bnE.get());
   std::string mpN = bnToMpint(bnN.get());
   if (mpE.empty() || mpN.empty()) return false;
   appendSshString(blob, "ssh-rsa");
   blob.append(mpE);
   blob.append(mpN);
   return true;
}

std::string makeEd25519SshBlob(const std::string &raw32)
{
   std::string out;
   if (raw32.size() != 32) return out;
   const char *alg = "ssh-ed25519";
   uint32_t aLen = htonl(11);
   uint32_t kLen = htonl(32);
   out.append(reinterpret_cast<const char *>(&aLen), 4);
   out.append(alg, 11);
   out.append(reinterpret_cast<const char *>(&kLen), 4);
   out.append(raw32);
   return out;
}

struct WireHdr
{
   char id[4];
   unsigned char ver;
   unsigned char op;
   unsigned char rsvd[2];
};

static const unsigned char OpInit      = 'I';
static const unsigned char OpChallenge = 'C';
static const unsigned char OpResponse  = 'R';

static const unsigned char kAgentFailure = 5;
static const unsigned char kAgentRequestIdentities = 11;
static const unsigned char kAgentIdentitiesAnswer = 12;
static const unsigned char kAgentSignRequest = 13;
static const unsigned char kAgentSignResponse = 14;
static const uint32_t kAgentRsaSha256Flag = 2;

XrdSecCredentials *makeCredentialsFromString(const std::string &s)
{
   char *b = static_cast<char *>(malloc(s.size()));
   if (!b) return nullptr;
   memcpy(b, s.data(), s.size());
   return std::make_unique<XrdSecCredentials>(b, static_cast<int>(s.size())).release();
}

XrdSecParameters *makeParametersFromString(const std::string &s)
{
   char *b = static_cast<char *>(malloc(s.size()));
   if (!b) return nullptr;
   memcpy(b, s.data(), s.size());
   return std::make_unique<XrdSecParameters>(b, static_cast<int>(s.size())).release();
}

bool readU16(const char *&p, const char *e, uint16_t &v)
{
   if (e - p < 2) return false;
   memcpy(&v, p, 2);
   p += 2;
   v = ntohs(v);
   return true;
}

[[maybe_unused]] bool readU32(const char *&p, const char *e, uint32_t &v)
{
   if (e - p < 4) return false;
   memcpy(&v, p, 4);
   p += 4;
   v = ntohl(v);
   return true;
}

void putU16(std::string &out, uint16_t v)
{
   uint16_t n = htons(v);
   out.append(reinterpret_cast<const char *>(&n), 2);
}

void putU32(std::string &out, uint32_t v)
{
   uint32_t n = htonl(v);
   out.append(reinterpret_cast<const char *>(&n), 4);
}

bool readBlob(const std::string &buf, size_t &at, std::string &out)
{
   out.clear();
   if (at + 4 > buf.size()) return false;
   uint32_t n = 0;
   memcpy(&n, buf.data() + at, 4);
   at += 4;
   n = ntohl(n);
   if (!sshWireFieldLenOk(n) || at + n > buf.size()) return false;
   out.assign(buf.data() + at, n);
   at += n;
   return true;
}

void putBlob(std::string &out, const std::string &val)
{
   putU32(out, static_cast<uint32_t>(val.size()));
   out.append(val);
}

bool isTrueEnv(const char *v)
{
   if (!v || !*v) return false;
   if (!strcmp(v, "0")) return false;
   if (!strcasecmp(v, "false")) return false;
   if (!strcasecmp(v, "no")) return false;
   if (!strcasecmp(v, "off")) return false;
   return true;
}

struct TrustedKey
{
   std::string user;
   std::string alg;
   std::string fp;
   std::string sshBlob;
   EvpPkeyPtr pkey;
};

// Per-connection challenge state. Lives inside the XrdSecProtocolssh object
// that issued the challenge, so it disappears with the connection and cannot
// be consumed by, or block, any other connection.
struct PendingChallenge
{
   std::string nonce;
   std::string fp;
   std::string user;
   std::string verifyAlg;
   std::string verifyBlob;
   time_t      expiresAt = 0;
};

// Revocation entries (loaded from -revoked-keys-file, hot-reloaded).
struct RevocationList
{
   std::unordered_map<std::string, bool> keyFps;   // SHA256 fp of key or cert blob
   std::unordered_map<uint64_t, bool>    serials;  // certificate serial numbers
   std::unordered_map<std::string, bool> keyIds;   // certificate key ids
   bool empty() const {return keyFps.empty() && serials.empty() && keyIds.empty();}
};

// Hot-reload bookkeeping for a file that is re-read when its inode/mtime
// changes. The owning mutex also protects the parsed data derived from it.
struct HotFileState
{
   bool   statValid = false;
   ino_t  ino = 0;
   time_t mtime = 0;
};

// Trust anchors: populated once in XrdSecProtocolsshInit() (under Gm) and read
// without locking afterwards.
std::mutex Gm;
std::unordered_map<std::string, TrustedKey> TrustedByFP;
std::unordered_map<std::string, TrustedKey> TrustedCAByFP;
std::string KeysFile = "/etc/xrootd/ssh_authorized_keys";
std::string CAKeysFile;
bool PrincipalAsUser = false;
bool AllowEmptyPrincipals = false;
std::unordered_map<std::string, bool> DenyUsers = {{"root", true}};
std::unordered_map<std::string, bool> AcceptedHosts;

// Hot-reloaded data.
std::mutex PrincipalMapMu;
std::unordered_map<std::string, std::string> PrincipalMap;
std::string PrincipalMapFile;
HotFileState PrincipalMapState;

std::mutex RevokedMu;
RevocationList Revoked;
std::string RevokedKeysFile;
HotFileState RevokedState;

std::atomic<int> MaxCredSize{kDefaultMaxCredSize};
std::atomic<int> NonceTTL{kDefaultNonceTTL};
std::atomic<bool> DebugSSH{false};
XrdSysLogger SSHLogger;
XrdSysError SSHLog(&SSHLogger, "secssh_");

void debugLog(const char *where, const std::string &msg)
{
   if (!DebugSSH.load(std::memory_order_relaxed)) return;
   SSHLog.Emsg(where, "ssh", msg.c_str());
}

// Unconditional server-side log line (configuration warnings, rejected
// authentications). Never returned to the client.
void warnLog(const char *where, const std::string &msg)
{
   SSHLog.Emsg(where, "ssh", msg.c_str());
}

// Message returned to a client for rejections whose detail must not be
// disclosed (key -> account mapping, trusted-CA set, ...).
static const char *kGenericAuthFailure = "SSH authentication failed.";

bool isDeniedUser(const std::string &user)
{
   return DenyUsers.find(user) != DenyUsers.end();
}

bool isAcceptedHost(const std::string &host)
{
   std::string h = normalizeHostname(host);
   if (h.empty()) return false;
   return AcceptedHosts.find(h) != AcceptedHosts.end();
}

void addAcceptedHost(const std::string &host)
{
   std::string h = normalizeHostname(host);
   if (!h.empty()) AcceptedHosts[h] = true;
}

// Default accepted hostnames: the canonical name, the kernel hostname and the
// loopback names. Explicit -hostnames entries are added on top.
void addDefaultAcceptedHosts()
{
   char hn[256];
   memset(hn, 0, sizeof(hn));
   if (gethostname(hn, sizeof(hn) - 1) == 0 && *hn)
      {
         addAcceptedHost(hn);
         const char *dot = strchr(hn, '.');
         if (dot && dot != hn) addAcceptedHost(std::string(hn, dot - hn));
      }
   char *fqdn = XrdNetUtils::MyHostName(0);
   if (fqdn) {addAcceptedHost(fqdn); free(fqdn);}
   addAcceptedHost("localhost");
   addAcceptedHost("localhost.localdomain");
   addAcceptedHost("127.0.0.1");
   addAcceptedHost("::1");
}

bool validateAgentSocket(const char *sockPath, std::string &emsg)
{
   emsg.clear();
   if (!sockPath || !*sockPath)
      {emsg = "ssh-agent socket path is empty";
       return false;
      }
   struct stat st;
   if (stat(sockPath, &st) != 0)
      {emsg = std::string("unable to stat ssh-agent socket: ")
            + sockPath + " (" + XrdSysE2T(errno) + ")";
       return false;
      }
   if (!S_ISSOCK(st.st_mode))
      {emsg = std::string("ssh-agent path is not a socket: ") + sockPath;
       return false;
      }
   if (st.st_uid != geteuid())
      {emsg = std::string("ssh-agent socket owner must match effective uid: ") + sockPath;
       return false;
      }
   if (st.st_mode & (S_IRWXG | S_IRWXO))
      {emsg = std::string("ssh-agent socket must not be group/other accessible: ") + sockPath;
       return false;
      }
   return true;
}

// Opens a client private key file with O_NOFOLLOW and validates it via fstat().
// On success returns the open descriptor (caller owns it) so that the key is
// read from the very inode that was checked; returns -1 on failure.
int openCheckedPrivateKeyFile(const char *path, std::string &emsg)
{
   emsg.clear();
   if (!path || !*path)
      {emsg = "private key path is empty";
       return -1;
      }
   int flags = O_RDONLY;
#ifdef O_NOFOLLOW
   flags |= O_NOFOLLOW;
#endif
   int fd = open(path, flags);
   if (fd < 0)
      {emsg = std::string("unable to open private key file: ") + path
            + " (" + XrdSysE2T(errno) + ")";
       return -1;
      }
   struct stat st;
   if (fstat(fd, &st) != 0)
      {int rc = errno; close(fd);
       emsg = std::string("unable to stat private key file: ") + path
            + " (" + XrdSysE2T(rc) + ")";
       return -1;
      }
   if (!S_ISREG(st.st_mode))
      {close(fd);
       emsg = std::string("private key path is not a regular file: ") + path;
       return -1;
      }
   if (st.st_uid != geteuid())
      {close(fd);
       emsg = std::string("private key file owner must match effective uid: ") + path;
       return -1;
      }
   if (st.st_mode & (S_IRWXG | S_IRWXO))
      {close(fd);
       emsg = std::string("private key file must not be group/other accessible: ") + path;
       return -1;
      }
   if (st.st_size > 1024 * 1024)
      {close(fd);
       emsg = std::string("private key file too large: ") + path;
       return -1;
      }
   return fd;
}

[[maybe_unused]] bool safeStatPrivateKeyFile(const char *path, std::string &emsg)
{
   int fd = openCheckedPrivateKeyFile(path, emsg);
   if (fd < 0) return false;
   close(fd);
   return true;
}

int connectAgentSocket(const char *sockPath)
{
   if (!sockPath || !*sockPath) return -1;
   std::string emsg;
   if (!validateAgentSocket(sockPath, emsg)) return -1;
   int fd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (fd < 0) return -1;
   // Bound every read/write so a wedged agent cannot hang the client forever.
   struct timeval tv;
   tv.tv_sec = kAgentIoTimeoutSec;
   tv.tv_usec = 0;
   (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
   (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
   struct sockaddr_un addr;
   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;
   if (strlen(sockPath) >= sizeof(addr.sun_path))
      {close(fd); return -1;}
   strcpy(addr.sun_path, sockPath);
   if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0)
      {close(fd); return -1;}
   return fd;
}

bool writeAll(int fd, const char *buf, size_t len)
{
   size_t off = 0;
   while (off < len)
      {
         ssize_t n = write(fd, buf + off, len - off);
         if (n < 0)
            {if (errno == EINTR) continue;
             return false;
            }
         if (n == 0) return false;
         off += static_cast<size_t>(n);
      }
   return true;
}

bool readAll(int fd, char *buf, size_t len)
{
   size_t off = 0;
   while (off < len)
      {
         ssize_t n = read(fd, buf + off, len - off);
         if (n < 0)
            {if (errno == EINTR) continue;
             return false;
            }
         if (n == 0) return false;
         off += static_cast<size_t>(n);
      }
   return true;
}

bool agentRpc(int fd, const std::string &payload, std::string &reply)
{
   reply.clear();
   std::string framed;
   putU32(framed, static_cast<uint32_t>(payload.size()));
   framed.append(payload);
   if (!writeAll(fd, framed.data(), framed.size())) return false;

   uint32_t nnet = 0;
   if (!readAll(fd, reinterpret_cast<char *>(&nnet), sizeof(nnet))) return false;
   uint32_t n = ntohl(nnet);
   if (n == 0 || n > 1024 * 1024) return false;
   reply.resize(n);
   if (!readAll(fd, &reply[0], n)) return false;
   return true;
}

bool getSshBlobAlg(const std::string &blob, std::string &alg)
{
   alg.clear();
   size_t at = 0;
   if (!readBlob(blob, at, alg)) return false;
   return !alg.empty();
}

bool readU64BE(const std::string &buf, size_t &at, uint64_t &v)
{
   if (at + 8 > buf.size()) return false;
   const unsigned char *p = reinterpret_cast<const unsigned char *>(buf.data() + at);
   v = (static_cast<uint64_t>(p[0]) << 56)
     | (static_cast<uint64_t>(p[1]) << 48)
     | (static_cast<uint64_t>(p[2]) << 40)
     | (static_cast<uint64_t>(p[3]) << 32)
     | (static_cast<uint64_t>(p[4]) << 24)
     | (static_cast<uint64_t>(p[5]) << 16)
     | (static_cast<uint64_t>(p[6]) << 8)
     |  static_cast<uint64_t>(p[7]);
   at += 8;
   return true;
}

bool readU32BE(const std::string &buf, size_t &at, uint32_t &v)
{
   if (at + 4 > buf.size()) return false;
   uint32_t n = 0;
   memcpy(&n, buf.data() + at, 4);
   at += 4;
   v = ntohl(n);
   return true;
}

bool isSshUserCertAlg(const std::string &alg, std::string &baseAlg)
{
   if (alg == "ssh-ed25519-cert-v01@openssh.com")
      {baseAlg = "ssh-ed25519"; return true;}
   if (alg == "ssh-rsa-cert-v01@openssh.com")
      {baseAlg = "ssh-rsa"; return true;}
   return false;
}

bool makePkeyFromSshBlob(const std::string &alg, const std::string &blob, EvpPkeyPtr &pk)
{
   pk.reset();
   if (alg == "ssh-ed25519")
      {
         std::string rawPub;
         if (!extractEd25519RawFromSshBlob(blob, rawPub)) return false;
         pk.reset(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
                  reinterpret_cast<const unsigned char *>(rawPub.data()), rawPub.size()));
         return static_cast<bool>(pk);
      }
   if (alg == "ssh-rsa")
      {
         std::string nBin, eBin;
         if (!extractRsaNEFromSshBlob(blob, nBin, eBin)) return false;
         pk = makeRSAPublicKeyFromNE(nBin, eBin);
         if (!pk) return false;
         if (EVP_PKEY_bits(pk.get()) < kMinRsaBits) {pk.reset(); return false;}
         return true;
      }
   return false;
}

bool rsaKeySizeOk(EVP_PKEY *pk)
{
   return pk && EVP_PKEY_base_id(pk) == EVP_PKEY_RSA && EVP_PKEY_bits(pk) >= kMinRsaBits;
}

void clearTrustedMap(std::unordered_map<std::string, TrustedKey> &m)
{
   m.clear();
}

void clearTrusted()
{
   clearTrustedMap(TrustedByFP);
   clearTrustedMap(TrustedCAByFP);
}

bool splitPrincipalList(const std::string &raw, std::vector<std::string> &out)
{
   out.clear();
   size_t at = 0;
   while (at < raw.size())
      {
         std::string p;
         if (!readBlob(raw, at, p)) return false;
         out.push_back(p);
      }
   return true;
}

bool listContains(const std::vector<std::string> &vals, const std::string &v)
{
   for (const auto &it : vals) if (it == v) return true;
   return false;
}

bool verifyData(EVP_PKEY *pub, const std::string &msg, const std::string &sig);


bool parseUid(const std::string &s, uid_t &uid)
{
   if (s.empty()) return false;
   for (char c : s) if (!isdigit(static_cast<unsigned char>(c))) return false;
   char *endp = 0;
   unsigned long long v = strtoull(s.c_str(), &endp, 10);
   if (!endp || *endp) return false;
   uid = static_cast<uid_t>(v);
   return static_cast<unsigned long long>(uid) == v;
}

bool resolveLocalUser(const std::string &token, std::string &userOut)
{
   userOut.clear();
   if (token.empty()) return false;
   uid_t uid = 0;
   if (parseUid(token, uid))
      {
         long bufSz = sysconf(_SC_GETPW_R_SIZE_MAX);
         if (bufSz < 1024) bufSz = 4096;
         std::vector<char> buf(static_cast<size_t>(bufSz));
         struct passwd pw;
         struct passwd *res = 0;
         if (getpwuid_r(uid, &pw, buf.data(), buf.size(), &res) != 0
         ||  !res || !res->pw_name || !*res->pw_name) return false;
         userOut = res->pw_name;
         return true;
      }
   long bufSz = sysconf(_SC_GETPW_R_SIZE_MAX);
   if (bufSz < 1024) bufSz = 4096;
   std::vector<char> buf(static_cast<size_t>(bufSz));
   struct passwd pw;
   struct passwd *res = 0;
   if (getpwnam_r(token.c_str(), &pw, buf.data(), buf.size(), &res) != 0
   ||  !res || !res->pw_name || !*res->pw_name) return false;
   userOut = res->pw_name;
   return true;
}

void addDefaultClientKeyCandidates(std::vector<std::string> &out)
{
   out.clear();
   const char *home = getenv("HOME");
   std::string homeBuf;
   if ((!home || !*home))
      {
         long bufSz = sysconf(_SC_GETPW_R_SIZE_MAX);
         if (bufSz < 1024) bufSz = 4096;
         std::vector<char> buf(static_cast<size_t>(bufSz));
         struct passwd pw;
         struct passwd *res = 0;
         if (getpwuid_r(geteuid(), &pw, buf.data(), buf.size(), &res) == 0
         &&  res && res->pw_dir && *res->pw_dir)
            {homeBuf = res->pw_dir; home = homeBuf.c_str();}
      }
   if (!home || !*home) return;
   std::string h(home);
   out.push_back(h + "/.ssh/id_ed25519");
   out.push_back(h + "/.ssh/id_rsa");
}

struct SafeFileResult
{
   std::string contents;
   ino_t ino;
   time_t mtime;
   bool found;
};

bool safeReadFile(const char *path, SafeFileResult &result, std::string &emsg);

// Lightweight metadata probe for hot-reload paths: open()+fstat() only, with the
// same ownership/permission checks as safeReadFile(), but no content read.
bool statTrustedFileMeta(const char *path, ino_t &ino, time_t &mtime,
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
      {
       if (errno == ENOENT) return true;
       emsg = std::string("unable to open file: ") + path + " (" + XrdSysE2T(errno) + ")";
       return false;
      }
   struct stat st;
   if (fstat(fd, &st) != 0)
      {int rc = errno; close(fd);
       emsg = std::string("unable to stat file: ") + path + " (" + XrdSysE2T(rc) + ")";
       return false;
      }
   close(fd);
   if (!S_ISREG(st.st_mode))
      {emsg = std::string("file is not regular: ") + path;
       return false;
      }
   if (st.st_uid != geteuid())
      {emsg = std::string("file owner must match effective uid: ") + path;
       return false;
      }
   if (st.st_mode & (S_IWGRP | S_IWOTH))
      {emsg = std::string("file must not be group/other writable: ") + path;
       return false;
      }
   ino = st.st_ino;
   mtime = st.st_mtime;
   found = true;
   return true;
}

bool safeReadFile(const char *path, SafeFileResult &result, std::string &emsg)
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
       emsg = std::string("unable to open file: ") + path + " (" + XrdSysE2T(errno) + ")";
       return false;
      }
   struct stat st;
   if (fstat(fd, &st) != 0)
      {int rc = errno; close(fd);
       emsg = std::string("unable to stat file: ") + path + " (" + XrdSysE2T(rc) + ")";
       return false;
      }
   if (!S_ISREG(st.st_mode))
      {close(fd);
       emsg = std::string("file is not regular: ") + path;
       return false;
      }
   if (st.st_uid != geteuid())
      {close(fd);
       emsg = std::string("file owner must match effective uid: ") + path;
       return false;
      }
   if (st.st_mode & (S_IWGRP | S_IWOTH))
      {close(fd);
       emsg = std::string("file must not be group/other writable: ") + path;
       return false;
      }
   if (st.st_size < 0 || st.st_size > 10 * 1024 * 1024)
      {close(fd);
       emsg = std::string("file too large: ") + path;
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
           emsg = std::string("read error on ") + path + " (" + XrdSysE2T(rc) + ")";
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

// Parses principal-map contents into newMap. Performs user resolution (NSS)
// and therefore must be called without holding PrincipalMapMu.
bool parsePrincipalMap(const std::string &contents,
                       std::unordered_map<std::string, std::string> &newMap,
                       std::string &emsg)
{
   newMap.clear();
   std::istringstream in(contents);
   std::string line;
   int lineNo = 0;
   while (std::getline(in, line))
      {
         lineNo++;
         std::string t = trim(line);
         if (t.empty() || t[0] == '#') continue;
         if (t.size() > kMaxPrincipalMapLineLen)
            {emsg = "principal-map-file line too long at line "
                    + std::to_string(lineNo);
             return false;
            }
         std::vector<char> lineBuf(t.begin(), t.end());
         lineBuf.push_back('\0');
         XrdOucTokenizer tok(lineBuf.data());
         tok.GetLine();
         char *principal = tok.GetToken();
         char *target = tok.GetToken();
         if (!principal || !target)
            {emsg = "invalid principal-map-file line " + std::to_string(lineNo)
                    + " (expected '<principal> <username|uid>' format)";
             return false;
            }
         std::string resolved;
         if (!resolveLocalUser(target, resolved))
            {emsg = "invalid principal-map-file target at line " + std::to_string(lineNo)
                    + " (not a valid local user/uid)";
             return false;
            }
         if (!isValidMappedUsername(resolved))
            {emsg = "principal-map-file target username is invalid at line "
                    + std::to_string(lineNo);
             return false;
            }
         if (newMap.count(principal))
            warnLog("Init", "principal-map-file: duplicate principal '" + std::string(principal)
                            + "' at line " + std::to_string(lineNo) + " overrides earlier entry");
         newMap[principal] = resolved;
      }
   return true;
}

// Installs freshly parsed principal-map data. Caller must hold PrincipalMapMu.
void installPrincipalMapLocked(std::unordered_map<std::string, std::string> &newMap,
                               const SafeFileResult &sfr)
{
   PrincipalMap.swap(newMap);
   PrincipalMapState.ino = sfr.ino;
   PrincipalMapState.mtime = sfr.mtime;
   PrincipalMapState.statValid = true;
   debugLog("Init", std::string("loaded principal map entries=")
                    + std::to_string(PrincipalMap.size()));
}

bool loadPrincipalMap(const SafeFileResult &sfr, std::string &emsg)
{
   if (PrincipalMapFile.empty()) return true;
   std::unordered_map<std::string, std::string> newMap;
   if (!parsePrincipalMap(sfr.contents, newMap, emsg)) return false;
   std::lock_guard<std::mutex> lock(PrincipalMapMu);
   installPrincipalMapLocked(newMap, sfr);
   return true;
}

// Parses a revocation file. Accepted line forms (comments with '#'):
//   <alg> <base64-blob> [comment]      revoke this public key (raw or as cert subject)
//   sha256:<fp> | SHA256:<fp>          revoke by key/cert fingerprint
//   serial:<n>                          revoke certificate serial number
//   id:<key-id>                         revoke certificate key id
bool parseRevocationList(const std::string &contents, RevocationList &out, std::string &emsg)
{
   out = RevocationList();
   std::istringstream in(contents);
   std::string line;
   int lineNo = 0;
   while (std::getline(in, line))
      {
         lineNo++;
         std::string t = trim(line);
         if (t.empty() || t[0] == '#') continue;
         if (t.size() > kMaxKeysFileLineLen)
            {emsg = "revoked-keys-file line too long at line " + std::to_string(lineNo);
             return false;
            }
         std::vector<char> lineBuf(t.begin(), t.end());
         lineBuf.push_back('\0');
         XrdOucTokenizer tok(lineBuf.data());
         tok.GetLine();
         char *a = tok.GetToken();
         char *b = tok.GetToken();
         if (!a) continue;
         std::string tag(a);
         std::string lower;
         for (char c : tag) lower.push_back(static_cast<char>(tolower(static_cast<unsigned char>(c))));
         auto valueAfterColon = [&](const std::string &pfx, std::string &val) -> bool
            {
               if (lower.compare(0, pfx.size(), pfx) != 0) return false;
               val = tag.substr(pfx.size());
               if (val.empty() && b) val = b;   // "serial: 123" form
               return true;
            };
         std::string val;
         if (valueAfterColon("serial:", val))
            {
               if (val.empty() || !std::all_of(val.begin(), val.end(),
                                               [](char c){return isdigit(static_cast<unsigned char>(c));}))
                  {emsg = "invalid serial in revoked-keys-file at line " + std::to_string(lineNo);
                   return false;
                  }
               out.serials[strtoull(val.c_str(), nullptr, 10)] = true;
               continue;
            }
         if (valueAfterColon("id:", val))
            {
               if (val.empty())
                  {emsg = "empty key id in revoked-keys-file at line " + std::to_string(lineNo);
                   return false;
                  }
               out.keyIds[val] = true;
               continue;
            }
         if (valueAfterColon("sha256:", val))
            {
               if (val.empty())
                  {emsg = "empty fingerprint in revoked-keys-file at line " + std::to_string(lineNo);
                   return false;
                  }
               while (!val.empty() && val.back() == '=') val.pop_back();
               out.keyFps["SHA256:" + val] = true;
               continue;
            }
         if (hasPrefix(a, "ssh-") || hasPrefix(a, "ecdsa-") || hasPrefix(a, "sk-"))
            {
               if (!b)
                  {emsg = "missing key material in revoked-keys-file at line " + std::to_string(lineNo);
                   return false;
                  }
               std::string blob, fp;
               if (!b64Decode(b, blob) || !sha256Base64(blob, fp))
                  {emsg = "invalid key material in revoked-keys-file at line " + std::to_string(lineNo);
                   return false;
                  }
               out.keyFps[fp] = true;
               continue;
            }
         emsg = "unrecognised revoked-keys-file entry at line " + std::to_string(lineNo);
         return false;
      }
   return true;
}

bool loadRevocationList(const SafeFileResult &sfr, std::string &emsg)
{
   if (RevokedKeysFile.empty()) return true;
   RevocationList rl;
   if (!parseRevocationList(sfr.contents, rl, emsg)) return false;
   std::lock_guard<std::mutex> lock(RevokedMu);
   Revoked = rl;
   RevokedState.ino = sfr.ino;
   RevokedState.mtime = sfr.mtime;
   RevokedState.statValid = true;
   debugLog("Init", std::string("loaded revocation entries keys=")
                    + std::to_string(rl.keyFps.size()) + " serials="
                    + std::to_string(rl.serials.size()) + " ids="
                    + std::to_string(rl.keyIds.size()));
   return true;
}

// Generic hot-reload driver: stat-only probe first, full read+parse only when
// the inode or mtime changed. `loader` must do its own locking.
bool ensureHotFileFresh(const std::string &path, const char *what,
                        std::mutex &mu, HotFileState &state,
                        bool (*loader)(const SafeFileResult &, std::string &),
                        std::string &emsg)
{
   if (path.empty()) return true;

   ino_t curIno = 0;
   time_t curMtime = 0;
   bool curFound = false;
   if (!statTrustedFileMeta(path.c_str(), curIno, curMtime, curFound, emsg))
      return false;
   if (!curFound)
      {emsg = std::string(what) + " disappeared: " + path;
       return false;
      }

   {
      std::lock_guard<std::mutex> lock(mu);
      if (state.statValid && curIno == state.ino && curMtime == state.mtime)
         return true;
   }

   SafeFileResult sfr;
   if (!safeReadFile(path.c_str(), sfr, emsg)) return false;
   if (!sfr.found)
      {emsg = std::string(what) + " disappeared: " + path;
       return false;
      }
   {
      std::lock_guard<std::mutex> lock(mu);
      if (state.statValid && sfr.ino == state.ino && sfr.mtime == state.mtime)
         return true;
   }
   if (!loader(sfr, emsg)) return false;
   debugLog("Auth", std::string("reloaded ") + what + " file='" + path + "'");
   return true;
}

bool ensurePrincipalMapFresh(std::string &emsg)
{
   return ensureHotFileFresh(PrincipalMapFile, "principal-map-file",
                             PrincipalMapMu, PrincipalMapState, loadPrincipalMap, emsg);
}

bool ensureRevocationFresh(std::string &emsg)
{
   return ensureHotFileFresh(RevokedKeysFile, "revoked-keys-file",
                             RevokedMu, RevokedState, loadRevocationList, emsg);
}

bool principalMappingEnabled()
{
   if (PrincipalAsUser) return true;
   std::lock_guard<std::mutex> lock(PrincipalMapMu);
   return !PrincipalMap.empty();
}

// Revocation check for a raw key or a certificate. Any of: subject/raw key
// fingerprint, certificate fingerprint, serial or key id.
bool isRevoked(const std::string &keyFp, const std::string &certFp,
               bool isCert, uint64_t serial, const std::string &keyId,
               std::string &why)
{
   std::lock_guard<std::mutex> lock(RevokedMu);
   if (Revoked.empty()) return false;
   if (!keyFp.empty() && Revoked.keyFps.count(keyFp))
      {why = "key is revoked"; return true;}
   if (isCert)
      {
         if (!certFp.empty() && Revoked.keyFps.count(certFp))
            {why = "certificate is revoked"; return true;}
         if (Revoked.serials.count(serial))
            {why = "certificate serial is revoked"; return true;}
         if (!keyId.empty() && Revoked.keyIds.count(keyId))
            {why = "certificate key id is revoked"; return true;}
      }
   return false;
}

// Maps certificate principals to a local user. When the client requested a
// specific user, a principal that maps to that user is preferred over the
// first mappable principal (OpenSSH semantics: any listed principal is valid).
// NSS lookups happen outside PrincipalMapMu.
bool mapPrincipalsToUser(const std::vector<std::string> &principals,
                         const std::string &reqUser,
                         std::string &mappedUser,
                         std::string &mapMethod,
                         std::string &emsg)
{
   mappedUser.clear();
   mapMethod.clear();
   if (principals.empty())
      {emsg = "SSH certificate principals are required for principal mapping";
       return false;
      }

   // Snapshot the file-map targets for all principals under the lock.
   std::vector<std::string> fileTargets(principals.size());
   {
      std::lock_guard<std::mutex> lock(PrincipalMapMu);
      for (size_t i = 0; i < principals.size(); ++i)
         {
            auto it = PrincipalMap.find(principals[i]);
            if (it != PrincipalMap.end() && isValidMappedUsername(it->second))
               fileTargets[i] = it->second;
         }
   }

   std::string firstUser, firstMethod;
   for (size_t i = 0; i < principals.size(); ++i)
      {
         std::string cand, method;
         if (PrincipalAsUser)
            {
               std::string resolved;
               if (resolveLocalUser(principals[i], resolved) && isValidMappedUsername(resolved))
                  {cand = resolved; method = "principal-as-user";}
            }
         if (cand.empty() && !fileTargets[i].empty())
            {cand = fileTargets[i]; method = "principal-map-file";}
         if (cand.empty()) continue;
         if (!reqUser.empty() && cand == reqUser)
            {mappedUser = cand; mapMethod = method; return true;}
         if (firstUser.empty()) {firstUser = cand; firstMethod = method;}
      }
   if (!firstUser.empty())
      {mappedUser = firstUser; mapMethod = firstMethod; return true;}
   emsg = "No principal could be mapped to a valid local user";
   return false;
}


bool loadTrustedKeyFile(const std::string &path,
                        const std::string &fileContents,
                        std::unordered_map<std::string, TrustedKey> &outMap,
                        bool requireUser,
                        std::string &emsg)
{
   clearTrustedMap(outMap);
   std::istringstream in(fileContents);
   std::string line;
   int lineNo = 0;
   while (std::getline(in, line))
      {
       lineNo++;
       std::string t = trim(line);
       if (t.empty() || t[0] == '#') continue;
       if (t.size() > kMaxKeysFileLineLen)
          {emsg = std::string("keys-file line too long at line ")
               + std::to_string(lineNo) + " in " + path;
           return false;
          }

       std::vector<char> lineBuf(t.begin(), t.end());
       lineBuf.push_back('\0');
       XrdOucTokenizer tok(lineBuf.data());
       tok.GetLine();
       char *a = tok.GetToken(); // user|alg
       char *b = tok.GetToken(); // alg|key
       char *c = tok.GetToken(); // key|comment
       if (!a || !b) continue;

       std::string user;
       std::string alg;
       std::string keyb64;
       if (requireUser)
          {
            if (hasPrefix(a, "ssh-"))
               {
                  alg = a;
                  keyb64 = b;
                  if (c) {
                     user = c;
                     size_t at = user.find('@');
                     if (at != std::string::npos) user = user.substr(0, at);
                  }
               }
            else
               {
                  if (!c)
                     {emsg = "invalid keys-file line " + std::to_string(lineNo)
                             + " (expected '<user> ssh-ed25519|ssh-rsa <key>' format)";
                      return false;
                     }
                  user = a;
                  alg = b;
                  keyb64 = c;
               }
            if (user.empty())
               {emsg = "empty user mapping at line " + std::to_string(lineNo);
                return false;
               }
            if (!isValidMappedUsername(user))
               {emsg = "invalid username at keys-file line " + std::to_string(lineNo)
                    + " in " + path;
                return false;
               }
          }
       else
          {
            if (hasPrefix(a, "ssh-"))
               {
                  alg = a;
                  keyb64 = b;
               }
            else
               {
                  if (!c)
                     {emsg = "invalid CA keys-file line " + std::to_string(lineNo)
                             + " (expected 'ssh-ed25519|ssh-rsa <key>' format)";
                      return false;
                     }
                  // Allow lines like: cert-authority ssh-ed25519 AAAA...
                  alg = b;
                  keyb64 = c;
               }
          }

       const std::string where = path + ":" + std::to_string(lineNo);
       if (alg != "ssh-ed25519" && alg != "ssh-rsa")
          {warnLog("Init", "skipping " + where + ": unsupported key type '" + alg + "'");
           continue;
          }
       if (keyb64.size() > kMaxKeysFileB64Len)
          {warnLog("Init", "skipping " + where + ": key material too long");
           continue;
          }

       std::string blob;
       if (!b64Decode(keyb64, blob))
          {warnLog("Init", "skipping " + where + ": invalid base64 key material");
           continue;
          }
       EvpPkeyPtr pk;
       if (!makePkeyFromSshBlob(alg, blob, pk) || !pk)
          {warnLog("Init", "skipping " + where + ": malformed " + alg
                           + " key blob (or RSA modulus below "
                           + std::to_string(kMinRsaBits) + " bits)");
           continue;
          }

       std::string fp;
       if (!sha256Base64(blob, fp))
          {warnLog("Init", "skipping " + where + ": unable to fingerprint key");
           continue;
          }
       auto dup = outMap.find(fp);
       if (dup != outMap.end())
          warnLog("Init", where + ": duplicate key fp=" + redactFp(fp)
                          + (requireUser ? (" (user '" + dup->second.user
                                            + "' -> '" + user + "')") : std::string())
                          + " overrides earlier entry");

       TrustedKey k;
       k.user = user;
       k.alg = alg;
       k.fp = fp;
       k.sshBlob = blob;
       k.pkey = std::move(pk);
       outMap[fp] = std::move(k);
       debugLog("Init", std::string("accepted ")
                        + (requireUser ? "user key" : "ca key")
                        + " alg='" + k.alg + "'"
                        + (requireUser ? (" user='" + k.user + "'") : "")
                        + " fp='" + redactFp(k.fp) + "'");
      }
   return true;
}

bool loadTrustedKeys(std::string &emsg, bool allowEmpty = false)
{
   debugLog("Init", std::string("loading keys-file='") + KeysFile + "'");
   SafeFileResult sfr;
   if (!safeReadFile(KeysFile.c_str(), sfr, emsg)) return false;
   if (!sfr.found)
      {
       if (allowEmpty)
          {
           clearTrustedMap(TrustedByFP);
           debugLog("Init", "keys-file not found; relying on ca-keys-file for trust");
           return true;
          }
       emsg = std::string("keys-file not found: ") + KeysFile;
       return false;
      }
   if (!loadTrustedKeyFile(KeysFile, sfr.contents, TrustedByFP, true, emsg)) return false;
   if (TrustedByFP.empty())
      {
       if (allowEmpty)
          {
           debugLog("Init", "keys-file has no usable raw keys; relying on ca-keys-file for trust");
           return true;
          }
       emsg = std::string("no usable ssh keys (ssh-ed25519/ssh-rsa) loaded from keys-file: ")
            + KeysFile;
       return false;
      }
   debugLog("Init", std::string("loaded keys count=")
                    + std::to_string(TrustedByFP.size()));
   return true;
}

bool loadTrustedCAKeys(std::string &emsg)
{
   clearTrustedMap(TrustedCAByFP);
   if (CAKeysFile.empty()) return true;
   debugLog("Init", std::string("loading ca-keys-file='") + CAKeysFile + "'");
   SafeFileResult sfr;
   if (!safeReadFile(CAKeysFile.c_str(), sfr, emsg)) return false;
   if (!sfr.found)
      {emsg = std::string("ca-keys-file not found: ") + CAKeysFile;
       return false;
      }
   if (!loadTrustedKeyFile(CAKeysFile, sfr.contents, TrustedCAByFP, false, emsg)) return false;
   if (TrustedCAByFP.empty())
      {emsg = std::string("no usable CA keys (ssh-ed25519/ssh-rsa) loaded from ca-keys-file: ")
           + CAKeysFile;
       return false;
      }
   debugLog("Init", std::string("loaded ca keys count=")
                    + std::to_string(TrustedCAByFP.size()));
   return true;
}

struct CertInfo
{
   uint64_t    serial = 0;
   std::string keyId;
};

bool validateUserCert(const std::string &certBlob,
                      const std::string &reqUser,
                      std::string &mappedUser,
                      std::string &verifyAlg,
                      std::string &verifyBlob,
                      std::string &fp,
                      std::string &emsg,
                      CertInfo *info = nullptr)
{
   mappedUser.clear();
   verifyAlg.clear();
   verifyBlob.clear();
   fp.clear();

   if (TrustedCAByFP.empty())
      {emsg = "SSH user certificate presented but no ca-keys-file is configured";
       return false;
      }

   size_t at = 0;
   std::string certAlg;
   if (!readBlob(certBlob, at, certAlg))
      {emsg = "Malformed SSH certificate: missing algorithm";
       return false;
      }
   if (!isSshUserCertAlg(certAlg, verifyAlg))
      {emsg = "Unsupported SSH certificate algorithm: " + certAlg;
       return false;
      }
   std::string nonce;
   if (!readBlob(certBlob, at, nonce))
      {emsg = "Malformed SSH certificate: missing nonce";
       return false;
      }
   (void)nonce;

   if (verifyAlg == "ssh-ed25519")
      {
         std::string rawPub;
         if (!readBlob(certBlob, at, rawPub) || rawPub.size() != 32)
            {emsg = "Malformed ssh-ed25519 certificate public key";
             return false;
            }
         verifyBlob = makeEd25519SshBlob(rawPub);
      }
   else if (verifyAlg == "ssh-rsa")
      {
         std::string eMp, nMp;
         if (!readBlob(certBlob, at, eMp) || !readBlob(certBlob, at, nMp))
            {emsg = "Malformed ssh-rsa certificate public key";
             return false;
            }
         appendSshString(verifyBlob, "ssh-rsa");
         appendSshString(verifyBlob, eMp);
         appendSshString(verifyBlob, nMp);
      }
   if (verifyBlob.empty())
      {emsg = "Unable to build SSH certificate subject key";
       return false;
      }

   uint64_t serial = 0;
   uint32_t certType = 0;
   std::string keyId;
   std::string principalsBlob;
   uint64_t validAfter = 0;
   uint64_t validBefore = 0;
   std::string criticalOpts, exts, reserved, signerBlob;
   if (!readU64BE(certBlob, at, serial)
   ||  !readU32BE(certBlob, at, certType)
   ||  !readBlob(certBlob, at, keyId)
   ||  !readBlob(certBlob, at, principalsBlob)
   ||  !readU64BE(certBlob, at, validAfter)
   ||  !readU64BE(certBlob, at, validBefore)
   ||  !readBlob(certBlob, at, criticalOpts)
   ||  !readBlob(certBlob, at, exts)
   ||  !readBlob(certBlob, at, reserved)
   ||  !readBlob(certBlob, at, signerBlob))
      {emsg = "Malformed SSH certificate body";
       return false;
      }
   if (!criticalOpts.empty())
      {emsg = "SSH certificate contains unsupported critical options";
       return false;
      }
   size_t sigFieldStart = at;
   std::string sigOuter;
   if (!readBlob(certBlob, at, sigOuter) || at != certBlob.size())
      {emsg = "Malformed SSH certificate signature";
       return false;
      }
   if (certType != 1)
      {emsg = "SSH certificate is not a user certificate (type=1 required)";
       return false;
      }

   time_t now = time(0);
   if (now < 0)
      {emsg = "System clock error (time before epoch)";
       return false;
      }
   uint64_t nowU = static_cast<uint64_t>(now);
   if (validAfter && nowU < validAfter)
      {emsg = "SSH certificate is not yet valid";
       return false;
      }
   if (validBefore && validBefore != static_cast<uint64_t>(-1) && nowU > validBefore)
      {emsg = "SSH certificate expired";
       return false;
      }

   std::vector<std::string> principals;
   if (!splitPrincipalList(principalsBlob, principals))
      {emsg = "Malformed SSH certificate principals list";
       return false;
      }

   if (info) {info->serial = serial; info->keyId = keyId;}

   const bool mappingEnabled = principalMappingEnabled();
   if (principals.empty() && !mappingEnabled && !AllowEmptyPrincipals)
      {emsg = "SSH certificate has no principals (rejected; see -allow-empty-principals)";
       return false;
      }

   if (reqUser.empty())
      {
         if (mappingEnabled)
            {
               std::string ignoredMethod;
               if (!mapPrincipalsToUser(principals, "", mappedUser, ignoredMethod, emsg)) return false;
            }
         else
            {
               if (principals.empty())
                  {emsg = "SSH certificate requires explicit username mapping";
                   return false;
                  }
               mappedUser = principals[0];
            }
      }
   else
      {
         if (mappingEnabled)
            {
               std::string ignoredMethod;
               if (!mapPrincipalsToUser(principals, reqUser, mappedUser, ignoredMethod, emsg)) return false;
               if (mappedUser != reqUser)
                  {emsg = "Requested user does not match mapped principal user";
                   return false;
                  }
            }
         else
            {
               if (!principals.empty() && !listContains(principals, reqUser))
                  {emsg = "Requested user is not listed in SSH certificate principals";
                   return false;
                  }
               mappedUser = reqUser;
            }
      }

   std::string signerFp;
   if (!sha256Base64(signerBlob, signerFp))
      {emsg = "Unable to fingerprint SSH certificate signer key";
       return false;
      }
   auto caIt = TrustedCAByFP.find(signerFp);
   if (caIt == TrustedCAByFP.end())
      {emsg = "SSH certificate signer is not trusted (fp=" + signerFp + ")";
       return false;
      }

   size_t sat = 0;
   std::string sigAlg, sigRaw;
   if (!readBlob(sigOuter, sat, sigAlg) || !readBlob(sigOuter, sat, sigRaw) || sat != sigOuter.size())
      {emsg = "Malformed SSH certificate signature payload";
       return false;
      }
   if (caIt->second.alg == "ssh-rsa")
      {
         // Only SHA-256 RSA signatures are verified; the legacy SHA-1 "ssh-rsa"
         // label is rejected outright rather than silently failing verification.
         if (sigAlg != "rsa-sha2-256")
            {emsg = "Unsupported RSA CA signature algorithm in certificate: " + sigAlg
                  + " (rsa-sha2-256 required)";
             return false;
            }
      }
   else if (sigAlg != caIt->second.alg)
      {emsg = "SSH certificate signature algorithm mismatch";
       return false;
      }

   std::string signedData = certBlob.substr(0, sigFieldStart);
   if (!verifyData(caIt->second.pkey.get(), signedData, sigRaw))
      {emsg = "SSH certificate signature validation failed";
       return false;
      }

   if (!sha256Base64(certBlob, fp))
      {emsg = "Unable to fingerprint SSH certificate";
       return false;
      }

   if (!isValidMappedUsername(mappedUser))
      {emsg = "SSH certificate maps to invalid local username";
       return false;
      }

   debugLog("Auth", std::string("accepted user cert")
                    + " serial='" + std::to_string(serial) + "'"
                    + " subject_alg='" + verifyAlg + "'"
                    + " signer_fp='" + redactFp(signerFp) + "'"
                    + " cert_fp='" + redactFp(fp) + "'");
   return true;
}

bool signData(EVP_PKEY *priv, const std::string &msg, std::string &sig)
{
   sig.clear();
   if (!priv) return false;
   int ktype = EVP_PKEY_base_id(priv);
   EvpMdCtxPtr ctx(EVP_MD_CTX_new());
   if (!ctx) return false;
   bool ok = false;
   size_t sigLen = 0;
   if (ktype == EVP_PKEY_ED25519)
      {
         ok = EVP_DigestSignInit(ctx.get(), nullptr, nullptr, nullptr, priv) == 1;
         if (ok) ok = EVP_DigestSign(ctx.get(), nullptr, &sigLen,
                                     reinterpret_cast<const unsigned char *>(msg.data()),
                                     msg.size()) == 1;
         if (ok && sigLen > 0)
            {
               sig.resize(sigLen);
               ok = EVP_DigestSign(ctx.get(), reinterpret_cast<unsigned char *>(&sig[0]), &sigLen,
                                   reinterpret_cast<const unsigned char *>(msg.data()),
                                   msg.size()) == 1;
               if (ok) sig.resize(sigLen);
            }
      }
   else if (ktype == EVP_PKEY_RSA)
      {
         ok = EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, priv) == 1
           && EVP_DigestSignUpdate(ctx.get(), msg.data(), msg.size()) == 1
           && EVP_DigestSignFinal(ctx.get(), nullptr, &sigLen) == 1;
         if (ok && sigLen > 0)
            {
               sig.resize(sigLen);
               ok = EVP_DigestSignFinal(ctx.get(),
                       reinterpret_cast<unsigned char *>(&sig[0]), &sigLen) == 1;
               if (ok) sig.resize(sigLen);
            }
      }
   return ok;
}

bool verifyData(EVP_PKEY *pub, const std::string &msg, const std::string &sig)
{
   if (!pub || sig.empty()) return false;
   int ktype = EVP_PKEY_base_id(pub);
   EvpMdCtxPtr ctx(EVP_MD_CTX_new());
   if (!ctx) return false;
   bool ok = false;
   if (ktype == EVP_PKEY_ED25519)
      {
         ok = EVP_DigestVerifyInit(ctx.get(), nullptr, nullptr, nullptr, pub) == 1
           && EVP_DigestVerify(ctx.get(),
                 reinterpret_cast<const unsigned char *>(sig.data()), sig.size(),
                 reinterpret_cast<const unsigned char *>(msg.data()), msg.size()) == 1;
      }
   else if (ktype == EVP_PKEY_RSA)
      {
         ok = EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, pub) == 1
           && EVP_DigestVerifyUpdate(ctx.get(), msg.data(), msg.size()) == 1
           && EVP_DigestVerifyFinal(ctx.get(),
                 reinterpret_cast<const unsigned char *>(sig.data()), sig.size()) == 1;
      }
   return ok;
}

// Signed payload. Length-prefixed fields avoid delimiter ambiguity. `host` is
// the (normalised) hostname the client connected to; the server only accepts
// names it is known under, so a signature obtained by a rogue server cannot be
// relayed to another server.
std::string challengePayload(const std::string &nonce, const std::string &fp,
                             const std::string &host)
{
   std::string p("xrdsec-ssh-v2");
   appendSshString(p, host);
   appendSshString(p, nonce);
   appendSshString(p, fp);
   return p;
}

/******************************************************************************/
/*      O p e n S S H   p r i v a t e   k e y   f o r m a t   ( c l i e n t ) */
/******************************************************************************/

// Decodes an unencrypted "-----BEGIN OPENSSH PRIVATE KEY-----" file as written
// by ssh-keygen. Encrypted keys are refused with a pointer to ssh-agent.
bool parseOpenSshPrivateKey(const std::string &text, EvpPkeyPtr &out, std::string &emsg)
{
   out.reset();
   static const char kBegin[] = "-----BEGIN OPENSSH PRIVATE KEY-----";
   static const char kEnd[]   = "-----END OPENSSH PRIVATE KEY-----";
   size_t b = text.find(kBegin);
   if (b == std::string::npos) {emsg = "not an OpenSSH private key"; return false;}
   b += sizeof(kBegin) - 1;
   size_t e = text.find(kEnd, b);
   if (e == std::string::npos) {emsg = "truncated OpenSSH private key"; return false;}
   std::string b64;
   for (size_t i = b; i < e; ++i)
      {
         unsigned char c = static_cast<unsigned char>(text[i]);
         if (!isspace(c)) b64.push_back(static_cast<char>(c));
      }
   std::string bin;
   if (!b64Decode(b64, bin)) {emsg = "invalid base64 in OpenSSH private key"; return false;}

   static const char kMagic[] = "openssh-key-v1";
   if (bin.size() < sizeof(kMagic) || memcmp(bin.data(), kMagic, sizeof(kMagic)) != 0)
      {emsg = "bad OpenSSH private key magic"; return false;}
   size_t at = sizeof(kMagic);
   std::string cipher, kdf, kdfOpts, pubBlob, privBlob;
   uint32_t nKeys = 0;
   if (!readBlob(bin, at, cipher) || !readBlob(bin, at, kdf) || !readBlob(bin, at, kdfOpts)
   ||  !readU32BE(bin, at, nKeys))
      {emsg = "malformed OpenSSH private key header"; return false;}
   if (cipher != "none" || kdf != "none")
      {emsg = "encrypted OpenSSH private keys are not supported; decrypt it or use ssh-agent";
       return false;
      }
   if (nKeys != 1) {emsg = "OpenSSH private key must contain exactly one key"; return false;}
   if (!readBlob(bin, at, pubBlob) || !readBlob(bin, at, privBlob))
      {emsg = "malformed OpenSSH private key body"; return false;}

   size_t pat = 0;
   uint32_t check1 = 0, check2 = 0;
   std::string alg;
   if (!readU32BE(privBlob, pat, check1) || !readU32BE(privBlob, pat, check2) || check1 != check2
   ||  !readBlob(privBlob, pat, alg))
      {emsg = "malformed OpenSSH private key section"; return false;}

   if (alg == "ssh-ed25519")
      {
         std::string pub, priv;
         if (!readBlob(privBlob, pat, pub) || !readBlob(privBlob, pat, priv)
         ||  pub.size() != 32 || priv.size() != 64 || priv.compare(32, 32, pub) != 0)
            {emsg = "malformed OpenSSH ed25519 private key"; return false;}
         out.reset(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr,
                   reinterpret_cast<const unsigned char *>(priv.data()), 32));
         if (!out) {emsg = "unable to import ed25519 private key"; return false;}
         return true;
      }
   if (alg == "ssh-rsa")
      {
         std::string n, e, d, iqmp, p, q;
         if (!readBlob(privBlob, pat, n) || !readBlob(privBlob, pat, e) || !readBlob(privBlob, pat, d)
         ||  !readBlob(privBlob, pat, iqmp) || !readBlob(privBlob, pat, p) || !readBlob(privBlob, pat, q))
            {emsg = "malformed OpenSSH rsa private key"; return false;}
         auto toBn = [](const std::string &s) -> BignumPtr
            {return BignumPtr(BN_bin2bn(reinterpret_cast<const unsigned char *>(s.data()),
                                        static_cast<int>(s.size()), nullptr));};
         BignumPtr bnN = toBn(n), bnE = toBn(e), bnD = toBn(d), bnIqmp = toBn(iqmp),
                   bnP = toBn(p), bnQ = toBn(q);
         if (!bnN || !bnE || !bnD || !bnIqmp || !bnP || !bnQ)
            {emsg = "unable to import rsa private key"; return false;}
         // CRT exponents are not stored by OpenSSH; derive them.
         BignumPtr dmp1(BN_new()), dmq1(BN_new()), tmp(BN_new());
         std::unique_ptr<BN_CTX, void(*)(BN_CTX*)> ctx(BN_CTX_new(), BN_CTX_free);
         if (!dmp1 || !dmq1 || !tmp || !ctx
         ||  !BN_sub(tmp.get(), bnP.get(), BN_value_one()) || !BN_mod(dmp1.get(), bnD.get(), tmp.get(), ctx.get())
         ||  !BN_sub(tmp.get(), bnQ.get(), BN_value_one()) || !BN_mod(dmq1.get(), bnD.get(), tmp.get(), ctx.get()))
            {emsg = "unable to derive rsa CRT parameters"; return false;}
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
         EvpPkeyCtxPtr pctx(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr));
         OsslParamBldPtr bld(OSSL_PARAM_BLD_new());
         if (!pctx || !bld
         ||  OSSL_PARAM_BLD_push_BN(bld.get(), "n", bnN.get()) <= 0
         ||  OSSL_PARAM_BLD_push_BN(bld.get(), "e", bnE.get()) <= 0
         ||  OSSL_PARAM_BLD_push_BN(bld.get(), "d", bnD.get()) <= 0
         ||  OSSL_PARAM_BLD_push_BN(bld.get(), "rsa-factor1", bnP.get()) <= 0
         ||  OSSL_PARAM_BLD_push_BN(bld.get(), "rsa-factor2", bnQ.get()) <= 0
         ||  OSSL_PARAM_BLD_push_BN(bld.get(), "rsa-exponent1", dmp1.get()) <= 0
         ||  OSSL_PARAM_BLD_push_BN(bld.get(), "rsa-exponent2", dmq1.get()) <= 0
         ||  OSSL_PARAM_BLD_push_BN(bld.get(), "rsa-coefficient1", bnIqmp.get()) <= 0)
            {emsg = "unable to build rsa key parameters"; return false;}
         OsslParamPtr params(OSSL_PARAM_BLD_to_param(bld.get()));
         EVP_PKEY *raw = nullptr;
         if (!params || EVP_PKEY_fromdata_init(pctx.get()) <= 0
         ||  EVP_PKEY_fromdata(pctx.get(), &raw, EVP_PKEY_KEYPAIR, params.get()) <= 0)
            {emsg = "unable to import rsa private key"; return false;}
         out.reset(raw);
#else
         RsaPtr rsa(RSA_new());
         if (!rsa
         ||  RSA_set0_key(rsa.get(), bnN.get(), bnE.get(), bnD.get()) != 1)
            {emsg = "unable to import rsa private key"; return false;}
         bnN.release(); bnE.release(); bnD.release();
         if (RSA_set0_factors(rsa.get(), bnP.get(), bnQ.get()) != 1)
            {emsg = "unable to import rsa private key"; return false;}
         bnP.release(); bnQ.release();
         if (RSA_set0_crt_params(rsa.get(), dmp1.get(), dmq1.get(), bnIqmp.get()) != 1)
            {emsg = "unable to import rsa private key"; return false;}
         dmp1.release(); dmq1.release(); bnIqmp.release();
         EvpPkeyPtr pkey(EVP_PKEY_new());
         if (!pkey || EVP_PKEY_assign_RSA(pkey.get(), rsa.get()) != 1)
            {emsg = "unable to import rsa private key"; return false;}
         rsa.release();
         out = std::move(pkey);
#endif
         return true;
      }
   emsg = "unsupported OpenSSH private key type '" + alg + "' (supported: ssh-ed25519, ssh-rsa)";
   return false;
}

// Loads a client private key from an already-validated descriptor. Accepts
// PEM/PKCS#8 (via OpenSSL) and the OpenSSH native format.
bool readClientPrivateKey(int fd, EvpPkeyPtr &out, std::string &emsg)
{
   out.reset();
   std::string contents;
   char buf[4096];
   for (;;)
      {
         ssize_t n = read(fd, buf, sizeof(buf));
         if (n < 0) {if (errno == EINTR) continue; emsg = "read error on private key"; return false;}
         if (n == 0) break;
         contents.append(buf, static_cast<size_t>(n));
         if (contents.size() > 1024 * 1024) {emsg = "private key file too large"; return false;}
      }
   if (contents.find("-----BEGIN OPENSSH PRIVATE KEY-----") != std::string::npos)
      return parseOpenSshPrivateKey(contents, out, emsg);
   BioPtr bio(BIO_new_mem_buf(contents.data(), static_cast<int>(contents.size())));
   if (!bio) {emsg = "out of memory"; return false;}
   ERR_clear_error();
   // Never prompt on the terminal for a passphrase; encrypted keys are refused.
   auto noPassword = [](char *, int, int, void *) -> int {return 0;};
   out.reset(PEM_read_bio_PrivateKey(bio.get(), nullptr, noPassword, nullptr));
   if (!out)
      {emsg = "unable to parse private key; use an unencrypted PEM/PKCS8 or OpenSSH ed25519/rsa key";
       return false;
      }
   return true;
}
}

class XrdSecProtocolssh : public XrdSecProtocol
{
public:
   int Authenticate(XrdSecCredentials *cred, XrdSecParameters **parms, XrdOucErrInfo *einfo=0);
   void Delete() {delete this;}
   XrdSecCredentials *getCredentials(XrdSecParameters *parms, XrdOucErrInfo *einfo=0);
   bool needTLS() {return true;}

   // Client-side constructor: `hname` is the host the client connected to and
   // is bound into the signed challenge response.
   XrdSecProtocolssh(const char *hname, const char *parms, XrdOucErrInfo *erp, bool &aOK);
   // Server-side constructor.
   XrdSecProtocolssh(const char *hname, XrdNetAddrInfo &endPoint)
      : XrdSecProtocol("ssh"), maxCredSize(MaxCredSize)
   {
      Entity.host = strdup(hname);
      Entity.name = strdup("anon");
      Entity.addrInfo = &endPoint;
   }
   ~XrdSecProtocolssh()
   {
      if (Entity.host) free(Entity.host);
      if (Entity.name) free(Entity.name);
      if (Entity.creds) free(Entity.creds);
   }

   static const int sshVersion = kProtoVersion;

private:
   XrdSecCredentials *makeInitCred(XrdOucErrInfo *erp);
   XrdSecCredentials *makeResponseCred(XrdSecParameters *parms, XrdOucErrInfo *erp);
   bool ensureClientKeyLoaded(XrdOucErrInfo *erp);
   bool loadClientKeyFromFile(const char *kPath, XrdOucErrInfo *erp);
   bool loadClientKeyFromAgent(const char *sockPath, XrdOucErrInfo *erp);
   bool signWithAgent(const std::string &msg, std::string &sigOut, XrdOucErrInfo *erp);
   int  rejectAuth(XrdOucErrInfo *erp, const std::string &detail, const char *clientMsg,
                   int rc = EAUTH);

   int      maxCredSize = 0;
   // server side
   std::unique_ptr<PendingChallenge> pending;
   // client side
   EvpPkeyPtr privKey;
   bool useAgent = false;
   std::string targetHost;
   std::string clientUser;
   std::string clientSshBlob;
   std::string clientFingerprint;
   std::string clientKeyAlg;
   std::string agentSock;
};

// Logs the detailed reason server-side and returns the (possibly generic)
// message to the client.
int XrdSecProtocolssh::rejectAuth(XrdOucErrInfo *erp, const std::string &detail,
                                  const char *clientMsg, int rc)
{
   warnLog("Auth", std::string(Entity.tident ? Entity.tident : "?") + " rejected: " + detail);
   return FatalS(erp, clientMsg ? clientMsg : detail.c_str(), rc, false);
}

XrdSecProtocolssh::XrdSecProtocolssh(const char *hname, const char *parms,
                                     XrdOucErrInfo *erp, bool &aOK)
   : XrdSecProtocol("ssh"), maxCredSize(0)
{
   aOK = false;
   if (!hname || !*hname)
      {FatalC(erp, "Target host name not available for SSH challenge binding.", EINVAL);
       return;
      }
   targetHost = normalizeHostname(hname);
   if (targetHost.empty())
      {FatalC(erp, "Target host name is not a valid hostname.", EINVAL);
       return;
      }
   if (!parms || !*parms)
      {FatalC(erp, "Client parameters not specified.", EINVAL);
       return;
      }
   char *endP = 0;
   (void)strtoll(parms, &endP, 10); // opts/version currently unused
   if (!endP || *endP != ':')
      {FatalC(erp, "Malformed client parameters.", EINVAL);
       return;
      }
   parms = endP + 1;
   maxCredSize = strtol(parms, &endP, 10);
   if (maxCredSize <= 0 || !endP || *endP != ':')
      {FatalC(erp, "Invalid max credential size in parameters.", EINVAL);
       return;
      }
   aOK = true;
}

bool XrdSecProtocolssh::ensureClientKeyLoaded(XrdOucErrInfo *erp)
{
   if (privKey || useAgent) return true;
   const char *usrEnv = getenv("XRD_SSH_USER");
   bool hasUserOverride = (usrEnv && *usrEnv);
   const char *usr = usrEnv;
   if (!usr || !*usr) usr = getenv("USER");
   if (!usr || !*usr)
      {FatalC(erp, "Missing username (set XRD_SSH_USER or USER).", EINVAL);
       return false;
      }
   clientUser = usr;

   const char *kPath = getenv("XRD_SSH_KEY_FILE");
   if (!kPath || !*kPath) kPath = getenv("XRD_SSH_PRIVATE_KEY_FILE");
   const char *aMode = getenv("XRD_SSH_AGENT");
   const bool preferAgent = isTrueEnv(aMode);
   const char *sock = getenv("SSH_AUTH_SOCK");
   if (preferAgent)
      debugLog("ensureClientKeyLoaded", "client requested ssh-agent mode (XRD_SSH_AGENT=1)");
   if (sock && *sock)
      debugLog("ensureClientKeyLoaded", "SSH_AUTH_SOCK is set");
   else
      debugLog("ensureClientKeyLoaded", "SSH_AUTH_SOCK is not set");

   if (kPath && *kPath && !preferAgent)
      return loadClientKeyFromFile(kPath, erp);

   if (sock && *sock)
      {
         if (loadClientKeyFromAgent(sock, erp)) return true;
         if ((!kPath || !*kPath) || preferAgent) return false;
         debugLog("ensureClientKeyLoaded", "ssh-agent unavailable, trying key file fallback");
      }
   else if (preferAgent)
      {
         FatalC(erp, "XRD_SSH_AGENT requested but SSH_AUTH_SOCK is not set.", ENOENT);
         return false;
      }

   if (kPath && *kPath) return loadClientKeyFromFile(kPath, erp);

   if (!preferAgent && !hasUserOverride)
      {
         std::vector<std::string> candidates;
         addDefaultClientKeyCandidates(candidates);
         struct stat st;
         for (const auto &path : candidates)
            {
               if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
               debugLog("ensureClientKeyLoaded", std::string("trying default key file ") + path);
               if (loadClientKeyFromFile(path.c_str(), erp)) return true;
            }
      }

   FatalC(erp,
      "Missing credentials: set XRD_SSH_KEY_FILE, use ssh-agent, or provide default ~/.ssh/id_ed25519|id_rsa.",
      ENOENT);
   return false;
}

bool XrdSecProtocolssh::loadClientKeyFromFile(const char *kPath, XrdOucErrInfo *erp)
{
   std::string statMsg;
   int fd = openCheckedPrivateKeyFile(kPath, statMsg);
   if (fd < 0)
      {FatalC(erp, statMsg.c_str(), EACCES);
       return false;
      }
   EvpPkeyPtr k;
   std::string keyMsg;
   bool okRead = readClientPrivateKey(fd, k, keyMsg);
   close(fd);
   if (!okRead || !k)
      {FatalC(erp, keyMsg.c_str(), EINVAL);
       return false;
      }

   std::string blob;
   std::string alg;
   int ktype = EVP_PKEY_base_id(k.get());
   if (ktype == EVP_PKEY_RSA && !rsaKeySizeOk(k.get()))
      {FatalC(erp, "RSA private key is too small (minimum 2048 bits).", EINVAL);
       return false;
      }
   if (ktype == EVP_PKEY_ED25519)
      {
         size_t pubLen = 32;
         std::string rawPub(32, '\0');
         if (EVP_PKEY_get_raw_public_key(k.get(),
               reinterpret_cast<unsigned char *>(&rawPub[0]), &pubLen) != 1 || pubLen != 32)
            {FatalC(erp, "Private key is not usable as ed25519 key.", EINVAL);
             return false;
            }
         rawPub.resize(pubLen);
         blob = makeEd25519SshBlob(rawPub);
         alg = "ssh-ed25519";
      }
   else if (ktype == EVP_PKEY_RSA)
      {
         if (!makeSshRsaBlobFromPkey(k.get(), blob))
            {FatalC(erp, "Private key is not usable as ssh-rsa key.", EINVAL);
             return false;
            }
         alg = "ssh-rsa";
      }
   else
      {
         FatalC(erp, "Unsupported SSH private key type (supported: ed25519, rsa).", EINVAL);
         return false;
      }

   std::string fp;
   if (blob.empty() || !sha256Base64(blob, fp))
      {FatalC(erp, "Unable to compute key fingerprint.", EINVAL);
       return false;
      }

   privKey = std::move(k);
   useAgent = false;
   clientSshBlob = blob;
   clientFingerprint = fp;
   clientKeyAlg = alg;
   debugLog("loadClientKeyFromFile", std::string("loaded key alg=") + clientKeyAlg
      + " fp=" + redactFp(clientFingerprint));
   return true;
}

bool XrdSecProtocolssh::loadClientKeyFromAgent(const char *sockPath, XrdOucErrInfo *erp)
{
   std::string sockMsg;
   if (!validateAgentSocket(sockPath, sockMsg))
      {FatalC(erp, sockMsg.c_str(), EACCES);
       return false;
      }
   debugLog("loadClientKeyFromAgent", "connecting to ssh-agent");
   int fd = connectAgentSocket(sockPath);
   if (fd < 0)
      {FatalC(erp, "Unable to connect to ssh-agent via SSH_AUTH_SOCK.", errno ? errno : EINVAL);
       return false;
      }

   std::string req(1, static_cast<char>(kAgentRequestIdentities));
   std::string rep;
   if (!agentRpc(fd, req, rep))
      {close(fd);
       FatalC(erp, "ssh-agent identities request failed.", EIO);
       return false;
      }
   close(fd);
   if (rep.empty())
      {FatalC(erp, "ssh-agent returned empty identities response.", EIO);
       return false;
      }
   if (static_cast<unsigned char>(rep[0]) == kAgentFailure)
      {FatalC(erp, "ssh-agent rejected identities request.", EACCES);
       return false;
      }
   if (static_cast<unsigned char>(rep[0]) != kAgentIdentitiesAnswer)
      {FatalC(erp, "ssh-agent returned unexpected identities response type.", EPROTO);
       return false;
      }

   size_t at = 1;
   if (at + 4 > rep.size())
      {FatalC(erp, "Malformed ssh-agent identities response.", EPROTO);
       return false;
      }
   uint32_t nids = 0;
   memcpy(&nids, rep.data() + at, 4);
   at += 4;
   nids = ntohl(nids);

   const char *fpWant = getenv("XRD_SSH_AGENT_FINGERPRINT");
   for (uint32_t i = 0; i < nids; ++i)
      {
         std::string blob, comment, alg, fp;
         if (!readBlob(rep, at, blob) || !readBlob(rep, at, comment)) break;
         if (!getSshBlobAlg(blob, alg)) continue;
         std::string baseAlg;
         bool isCert = isSshUserCertAlg(alg, baseAlg);
         (void)baseAlg;
         if (!isCert && alg != "ssh-ed25519" && alg != "ssh-rsa") continue;
         if (!sha256Base64(blob, fp)) continue;
         if (fpWant && *fpWant && fp != fpWant) continue;
         useAgent = true;
         privKey.reset();
         agentSock = sockPath;
         clientSshBlob = blob;
         clientFingerprint = fp;
         clientKeyAlg = alg;
         debugLog("loadClientKeyFromAgent", std::string("selected agent key alg=") + alg
            + " fp=" + redactFp(fp));
         debugLog("loadClientKeyFromAgent", std::string("client auth mode=agent key=")
            + clientKeyAlg + " " + redactFp(clientFingerprint));
         return true;
      }

   FatalC(erp,
      "No supported identity found in ssh-agent (supported: ssh-ed25519, ssh-rsa, ssh-ed25519-cert-v01@openssh.com, ssh-rsa-cert-v01@openssh.com).",
      ENOENT);
   return false;
}

bool XrdSecProtocolssh::signWithAgent(const std::string &msg, std::string &sigOut,
                                      XrdOucErrInfo *erp)
{
   sigOut.clear();
   if (!useAgent || agentSock.empty() || clientSshBlob.empty())
      {FatalC(erp, "ssh-agent signing not initialized.", EINVAL);
       return false;
      }
   int fd = connectAgentSocket(agentSock.c_str());
   if (fd < 0)
      {FatalC(erp, "Unable to connect to ssh-agent for signing.", errno ? errno : EINVAL);
       return false;
      }
   std::string req(1, static_cast<char>(kAgentSignRequest));
   putBlob(req, clientSshBlob);
   putBlob(req, msg);
   std::string sigExpectAlg = clientKeyAlg;
   std::string certBaseAlg;
   if (isSshUserCertAlg(clientKeyAlg, certBaseAlg)) sigExpectAlg = certBaseAlg;
   putU32(req, (sigExpectAlg == "ssh-rsa") ? kAgentRsaSha256Flag : 0);
   debugLog("signWithAgent", std::string("signing challenge via agent key=")
      + clientKeyAlg + " " + redactFp(clientFingerprint));

   std::string rep;
   bool ok = agentRpc(fd, req, rep);
   close(fd);
   if (!ok)
      {FatalC(erp, "ssh-agent sign request failed.", EIO);
       return false;
      }
   if (rep.empty())
      {FatalC(erp, "ssh-agent returned empty sign response.", EIO);
       return false;
      }
   if (static_cast<unsigned char>(rep[0]) == kAgentFailure)
      {FatalC(erp, "ssh-agent rejected sign request.", EACCES);
       return false;
      }
   if (static_cast<unsigned char>(rep[0]) != kAgentSignResponse)
      {FatalC(erp, "ssh-agent returned unexpected sign response type.", EPROTO);
       return false;
      }

   size_t at = 1;
   std::string sigBlob, sigAlg, sigRaw;
   if (!readBlob(rep, at, sigBlob))
      {FatalC(erp, "Malformed ssh-agent sign response.", EPROTO);
       return false;
      }
   at = 0;
   if (!readBlob(sigBlob, at, sigAlg) || !readBlob(sigBlob, at, sigRaw))
      {FatalC(erp, "Malformed ssh-agent signature blob.", EPROTO);
       return false;
      }
   if (sigExpectAlg == "ssh-rsa")
      {
         if (sigAlg != "rsa-sha2-256")
            {
               std::string emsg("ssh-agent returned unsupported RSA signature algorithm: ");
               emsg += sigAlg;
               emsg += " (expected rsa-sha2-256)";
               FatalC(erp, emsg.c_str(), EPROTO);
               return false;
            }
      }
   else if (sigAlg != sigExpectAlg)
      {FatalC(erp, "ssh-agent signature algorithm mismatch.", EPROTO);
       return false;
      }
   sigOut.swap(sigRaw);
   return true;
}

XrdSecCredentials *XrdSecProtocolssh::makeInitCred(XrdOucErrInfo *erp)
{
   if (!ensureClientKeyLoaded(erp)) return 0;
   std::string buf;
   WireHdr h;
   memcpy(h.id, "ssh", 4);
   h.ver = kProtoVersion;
   h.op = OpInit;
   h.rsvd[0] = h.rsvd[1] = 0;
   buf.append(reinterpret_cast<const char *>(&h), sizeof(h));
   putU16(buf, static_cast<uint16_t>(clientUser.size()));
   putU16(buf, static_cast<uint16_t>(clientSshBlob.size()));
   buf.append(clientUser);
   buf.append(clientSshBlob);
   if (static_cast<int>(buf.size()) > maxCredSize)
      return FatalC(erp, "SSH init credential too large.", EMSGSIZE);
   XrdSecCredentials *ret = makeCredentialsFromString(buf);
   if (!ret) return FatalC(erp, "Insufficient memory.", ENOMEM);
   return ret;
}

XrdSecCredentials *XrdSecProtocolssh::makeResponseCred(XrdSecParameters *parms, XrdOucErrInfo *erp)
{
   if (!ensureClientKeyLoaded(erp)) return 0;
   if (!parms || parms->size < static_cast<int>(sizeof(WireHdr)))
      return FatalC(erp, "Missing SSH challenge parameters.", EINVAL);
   const char *p = parms->buffer;
   const char *e = parms->buffer + parms->size;
   const WireHdr *h = reinterpret_cast<const WireHdr *>(p);
   if (memcmp(h->id, "ssh", 4) != 0 || h->op != OpChallenge || h->ver != kProtoVersion)
      return FatalC(erp, "Invalid SSH challenge format.", EINVAL);
   p += sizeof(WireHdr);
   uint16_t nLen = 0, fLen = 0;
   if (!readU16(p, e, nLen) || !readU16(p, e, fLen))
      return FatalC(erp, "Malformed SSH challenge.", EINVAL);
   if (nLen == 0 || fLen == 0 || (e - p) != (nLen + fLen))
      return FatalC(erp, "Malformed SSH challenge lengths.", EINVAL);
   std::string nonce(p, nLen);
   p += nLen;
   std::string fp(p, fLen);
   if (fp != clientFingerprint)
      return FatalC(erp, "Server challenge key fingerprint mismatch.", EAUTH);
   if (targetHost.empty())
      return FatalC(erp, "Target host name not available for SSH challenge binding.", EINVAL);

   std::string payload = challengePayload(nonce, fp, targetHost);
   std::string sig;
   bool okSign = useAgent ? signWithAgent(payload, sig, erp)
                          : signData(privKey.get(), payload, sig);
   if (!okSign)
      return useAgent ? 0 : FatalC(erp, "Unable to sign SSH challenge.", EINVAL);

   std::string out;
   WireHdr rh;
   memcpy(rh.id, "ssh", 4);
   rh.ver = kProtoVersion;
   rh.op = OpResponse;
   rh.rsvd[0] = rh.rsvd[1] = 0;
   out.append(reinterpret_cast<const char *>(&rh), sizeof(rh));
   putU16(out, static_cast<uint16_t>(sig.size()));
   putU16(out, static_cast<uint16_t>(targetHost.size()));
   out.append(sig);
   out.append(targetHost);
   if (static_cast<int>(out.size()) > maxCredSize)
      return FatalC(erp, "SSH response credential too large.", EMSGSIZE);
   XrdSecCredentials *ret = makeCredentialsFromString(out);
   if (!ret) return FatalC(erp, "Insufficient memory.", ENOMEM);
   return ret;
}

XrdSecCredentials *XrdSecProtocolssh::getCredentials(XrdSecParameters *parms, XrdOucErrInfo *einfo)
{
   if (!parms) return makeInitCred(einfo);
   return makeResponseCred(parms, einfo);
}

int XrdSecProtocolssh::Authenticate(XrdSecCredentials *cred, XrdSecParameters **parms, XrdOucErrInfo *erp)
{
   if (!cred || !cred->buffer || cred->size < static_cast<int>(sizeof(WireHdr)))
      return FatalS(erp, "Invalid SSH credentials.", EINVAL, false);
   if (cred->size > MaxCredSize)
      return FatalS(erp, "SSH credentials exceed configured max size.", EMSGSIZE, false);

   const char *p = cred->buffer;
   const char *e = cred->buffer + cred->size;
   const WireHdr *h = reinterpret_cast<const WireHdr *>(p);
   if (memcmp(h->id, "ssh", 4) != 0 || h->ver != kProtoVersion)
      return FatalS(erp, "SSH protocol id/version mismatch.", EINVAL, false);
   p += sizeof(WireHdr);

   if (h->op == OpInit)
      {
         // A new init invalidates any challenge issued earlier on this connection.
         pending.reset();

         uint16_t uLen = 0, bLen = 0;
         if (!readU16(p, e, uLen) || !readU16(p, e, bLen))
            return FatalS(erp, "Malformed SSH init request.", EINVAL, false);
         if (uLen == 0 || bLen == 0 || (e - p) != (uLen + bLen))
            return FatalS(erp, "Malformed SSH init lengths.", EINVAL, false);
         std::string reqUser(p, uLen);
         p += uLen;
         std::string blob(p, bLen);
         std::string fp;
         std::string blobAlg;
         if (!getSshBlobAlg(blob, blobAlg))
            return FatalS(erp, "Unable to parse SSH key/cert blob.", EINVAL, false);
         bool isCert = false;
         std::string mappedUser;
         std::string verifyAlg;
         std::string verifyBlob;
         std::string certBaseAlg;
         CertInfo certInfo;
         if (isSshUserCertAlg(blobAlg, certBaseAlg))
            {
               (void)certBaseAlg;
               isCert = true;
            }
         else
            {
               if (!sha256Base64(blob, fp))
                  return FatalS(erp, "Unable to fingerprint SSH key blob.", EINVAL, false);
            }

         std::string emsg;
         if (!ensurePrincipalMapFresh(emsg) || !ensureRevocationFresh(emsg))
            return rejectAuth(erp, emsg, "SSH server configuration error.");

         if (isCert)
            {
               if (!validateUserCert(blob, reqUser, mappedUser, verifyAlg, verifyBlob, fp,
                                     emsg, &certInfo))
                  {
                     // Do not disclose which CAs are trusted.
                     const bool sensitive = emsg.find("signer is not trusted") != std::string::npos;
                     return rejectAuth(erp, emsg, sensitive ? kGenericAuthFailure : emsg.c_str());
                  }
            }
         else
            {
               auto it = TrustedByFP.find(fp);
               if (it == TrustedByFP.end())
                  return rejectAuth(erp, "SSH public key not trusted (fp=" + fp + ")",
                                    kGenericAuthFailure);
               mappedUser = it->second.user;
               verifyBlob = it->second.sshBlob.empty() ? blob : it->second.sshBlob;
               verifyAlg = it->second.alg;
               if (verifyAlg.empty())
                  {
                     if (!getSshBlobAlg(verifyBlob, verifyAlg))
                        return FatalS(erp, "Unable to determine SSH key algorithm.", EAUTH, false);
                  }
               if (!reqUser.empty() && reqUser != mappedUser)
                  // Do not disclose the key -> account mapping.
                  return rejectAuth(erp, "SSH username/key mapping mismatch (requested='"
                                         + reqUser + "', mapped='" + mappedUser + "', fp="
                                         + fp + ")", kGenericAuthFailure);
            }

         // Revocation applies to the raw key, the certificate subject key, the
         // certificate itself, its serial and its key id.
         {
            std::string subjectFp;
            if (isCert && !verifyBlob.empty()) sha256Base64(verifyBlob, subjectFp);
            std::string why;
            if (isRevoked(isCert ? subjectFp : fp, fp, isCert, certInfo.serial,
                          certInfo.keyId, why))
               return rejectAuth(erp, why + " (fp=" + fp + ")", "SSH key or certificate is revoked.");
         }

         if (!isValidMappedUsername(mappedUser))
            return rejectAuth(erp, "invalid mapped username '" + mappedUser + "'",
                              "Invalid mapped username for SSH authentication.");
         if (isDeniedUser(mappedUser))
            return rejectAuth(erp, "mapped user '" + mappedUser + "' is denied (-deny-users)",
                              "Mapped user is not permitted to authenticate via SSH.");

         debugLog("Auth", std::string("init")
                        + " auth_mode='" + (isCert ? "ssh-cert" : "raw-key") + "'"
                        + " key_alg='" + verifyAlg + "'"
                        + " fp='" + redactFp(fp) + "'");

         std::string nonce(32, '\0');
         if (RAND_bytes(reinterpret_cast<unsigned char *>(&nonce[0]), nonce.size()) != 1)
            return FatalS(erp, "Unable to generate SSH challenge nonce.", EIO, false);

         auto pc = std::make_unique<PendingChallenge>();
         pc->nonce = nonce;
         pc->fp = fp;
         pc->user = mappedUser;
         pc->verifyAlg = verifyAlg;
         pc->verifyBlob = verifyBlob;
         pc->expiresAt = time(0) + NonceTTL.load(std::memory_order_relaxed);

         std::string out;
         WireHdr ch;
         memcpy(ch.id, "ssh", 4);
         ch.ver = kProtoVersion;
         ch.op = OpChallenge;
         ch.rsvd[0] = ch.rsvd[1] = 0;
         out.append(reinterpret_cast<const char *>(&ch), sizeof(ch));
         putU16(out, static_cast<uint16_t>(nonce.size()));
         putU16(out, static_cast<uint16_t>(fp.size()));
         out.append(nonce);
         out.append(fp);
         *parms = makeParametersFromString(out);
         if (!*parms) return FatalS(erp, "Insufficient memory.", ENOMEM, false);
         pending = std::move(pc);
         return 1;
      }
   if (h->op == OpResponse)
      {
         uint16_t sLen = 0, hLen = 0;
         if (!readU16(p, e, sLen) || !readU16(p, e, hLen))
            return FatalS(erp, "Malformed SSH response.", EINVAL, false);
         if (sLen == 0 || hLen == 0 || (e - p) != (sLen + hLen))
            return FatalS(erp, "Malformed SSH response lengths.", EINVAL, false);
         std::string sig(p, sLen);
         p += sLen;
         std::string host(p, hLen);

         // Single use: the challenge is consumed whatever the outcome.
         std::unique_ptr<PendingChallenge> pc = std::move(pending);
         if (!pc)
            return FatalS(erp, "No pending SSH challenge.", EAUTH, false);
         if (pc->expiresAt < time(0))
            return FatalS(erp, "SSH challenge expired.", EAUTH, false);

         std::string normHost = normalizeHostname(host);
         if (normHost.empty() || normHost != host)
            return rejectAuth(erp, "response carries malformed target host '" + host + "'",
                              "Malformed SSH response host.");
         if (!isAcceptedHost(normHost))
            return rejectAuth(erp, "response bound to unknown target host '" + host
                                   + "' (fp=" + pc->fp + "); add it with -hostnames if legitimate",
                              "SSH challenge is bound to a different server.");

         EvpPkeyPtr verifyKey;
         if (!makePkeyFromSshBlob(pc->verifyAlg, pc->verifyBlob, verifyKey) || !verifyKey)
            return FatalS(erp, "Unable to reconstruct SSH verify key.", EAUTH, false);

         std::string payload = challengePayload(pc->nonce, pc->fp, normHost);
         if (!verifyData(verifyKey.get(), payload, sig))
            return rejectAuth(erp, "signature validation failed (fp=" + pc->fp + ")",
                              "SSH signature validation failed.");

         if (!isValidMappedUsername(pc->user) || isDeniedUser(pc->user))
            return FatalS(erp, "Invalid mapped username for SSH authentication.", EAUTH, false);

         if (Entity.name) free(Entity.name);
         Entity.name = strdup(pc->user.c_str());
         strncpy(Entity.prot, "ssh", sizeof(Entity.prot));
         debugLog("Auth", std::string("success")
                        + " key_alg='" + pc->verifyAlg + "'"
                        + " fp='" + redactFp(pc->fp) + "'"
                        + " host='" + normHost + "'");
         return 0;
      }

   return FatalS(erp, "Unsupported SSH operation code.", EINVAL, false);
}

extern "C"
{
char *XrdSecProtocolsshInit(const char mode, const char *parms, XrdOucErrInfo *erp)
{
   static char nilstr = 0;
   uint64_t opts = XrdSecProtocolssh::sshVersion;
   SSHLog.logger(&SSHLogger);
   const char *dbg = getenv("XrdSecDEBUG");
   if (dbg && *dbg && strcmp(dbg, "0") != 0) DebugSSH.store(true, std::memory_order_relaxed);
   if (mode == 'c') return &nilstr;

   std::vector<std::string> extraHosts;
   if (parms && *parms)
      {
         std::vector<char> cfgBuf(parms, parms + strlen(parms) + 1);
         XrdOucTokenizer cfg(cfgBuf.data());
         cfg.GetLine();
         char *val = 0, *endP = 0;
         while ((val = cfg.GetToken()))
               {
                  if (!strcmp(val, "-maxsz"))
                     {
                        if (!(val = cfg.GetToken()))
                           {FatalC(erp, "-maxsz argument missing", EINVAL); return 0;}
                        int msz = strtol(val, &endP, 10);
                        if (msz <= 0 || msz > 524288 || *endP)
                           {FatalC(erp, "-maxsz argument invalid", EINVAL); return 0;}
                        MaxCredSize.store(msz, std::memory_order_relaxed);
                     }
                  else if (!strcmp(val, "-keys-file"))
                     {
                        if (!(val = cfg.GetToken()))
                           {FatalC(erp, "-keys-file argument missing", EINVAL); return 0;}
                        KeysFile = val;
                     }
                  else if (!strcmp(val, "-ca-keys-file"))
                     {
                        if (!(val = cfg.GetToken()))
                           {FatalC(erp, "-ca-keys-file argument missing", EINVAL); return 0;}
                        CAKeysFile = val;
                     }
                  else if (!strcmp(val, "-principal-as-user"))
                     {
                        PrincipalAsUser = true;
                     }
                  else if (!strcmp(val, "-principal-map-file"))
                     {
                        if (!(val = cfg.GetToken()))
                           {FatalC(erp, "-principal-map-file argument missing", EINVAL); return 0;}
                        PrincipalMapFile = val;
                     }
                  else if (!strcmp(val, "-principal-map"))
                     {
                        PrincipalMapFile = "/etc/xrootd/ssh_principals.map";
                     }
                  else if (!strcmp(val, "-nonce-ttl"))
                     {
                        if (!(val = cfg.GetToken()))
                           {FatalC(erp, "-nonce-ttl argument missing", EINVAL); return 0;}
                        int ttl = strtol(val, &endP, 10);
                        if (ttl <= 0 || ttl > 600 || *endP)
                           {FatalC(erp, "-nonce-ttl argument invalid", EINVAL); return 0;}
                        NonceTTL.store(ttl, std::memory_order_relaxed);
                     }
                  else if (!strcmp(val, "-debug"))
                     {
                        DebugSSH.store(true, std::memory_order_relaxed);
                     }
                  else if (!strcmp(val, "-revoked-keys-file"))
                     {
                        if (!(val = cfg.GetToken()))
                           {FatalC(erp, "-revoked-keys-file argument missing", EINVAL); return 0;}
                        RevokedKeysFile = val;
                     }
                  else if (!strcmp(val, "-allow-empty-principals"))
                     {
                        AllowEmptyPrincipals = true;
                     }
                  else if (!strcmp(val, "-deny-users"))
                     {
                        if (!(val = cfg.GetToken()))
                           {FatalC(erp, "-deny-users argument missing", EINVAL); return 0;}
                        DenyUsers.clear();
                        if (strcmp(val, "none"))
                           {
                              std::string list(val), item;
                              std::istringstream ls(list);
                              while (std::getline(ls, item, ','))
                                 {
                                    item = trim(item);
                                    if (item.empty()) continue;
                                    if (!isValidMappedUsername(item))
                                       {FatalC(erp, "-deny-users contains an invalid username", EINVAL);
                                        return 0;
                                       }
                                    DenyUsers[item] = true;
                                 }
                           }
                     }
                  else if (!strcmp(val, "-hostnames"))
                     {
                        if (!(val = cfg.GetToken()))
                           {FatalC(erp, "-hostnames argument missing", EINVAL); return 0;}
                        std::string list(val), item;
                        std::istringstream ls(list);
                        while (std::getline(ls, item, ','))
                           {
                              item = trim(item);
                              if (item.empty()) continue;
                              if (normalizeHostname(item).empty())
                                 {FatalC(erp, "-hostnames contains an invalid hostname", EINVAL);
                                  return 0;
                                 }
                              extraHosts.push_back(item);
                           }
                     }
                  else {XrdOucString eTxt("Invalid parameter - "); eTxt += val;
                        FatalC(erp, eTxt.c_str(), EINVAL); return 0;
                       }
               }
      }

   std::string emsg;
   {
      std::lock_guard<std::mutex> lock(Gm);
      clearTrusted();
      {
         std::lock_guard<std::mutex> pl(PrincipalMapMu);
         PrincipalMap.clear();
         PrincipalMapState = HotFileState();
      }
      const bool allowEmptyKeysFile = !CAKeysFile.empty();
      if (!loadTrustedKeys(emsg, allowEmptyKeysFile))
         {FatalC(erp, emsg.c_str(), EINVAL);
          return 0;
         }
      if (!loadTrustedCAKeys(emsg))
         {FatalC(erp, emsg.c_str(), EINVAL);
          return 0;
         }
      if (TrustedByFP.empty() && TrustedCAByFP.empty())
         {FatalC(erp, "At least one of keys-file or ca-keys-file must provide usable keys.",
                 EINVAL);
          return 0;
         }
      if (!PrincipalMapFile.empty())
         {
            SafeFileResult pmSfr;
            if (!safeReadFile(PrincipalMapFile.c_str(), pmSfr, emsg))
               {FatalC(erp, emsg.c_str(), EACCES);
                return 0;
               }
            if (!pmSfr.found)
               {FatalC(erp, (std::string("principal-map-file not found: ") + PrincipalMapFile).c_str(), EACCES);
                return 0;
               }
            if (!loadPrincipalMap(pmSfr, emsg))
               {FatalC(erp, emsg.c_str(), EINVAL);
                return 0;
               }
         }
      {
         std::lock_guard<std::mutex> rl(RevokedMu);
         Revoked = RevocationList();
         RevokedState = HotFileState();
      }
      if (!RevokedKeysFile.empty())
         {
            SafeFileResult rkSfr;
            if (!safeReadFile(RevokedKeysFile.c_str(), rkSfr, emsg))
               {FatalC(erp, emsg.c_str(), EACCES);
                return 0;
               }
            if (!rkSfr.found)
               {FatalC(erp, (std::string("revoked-keys-file not found: ") + RevokedKeysFile).c_str(), EACCES);
                return 0;
               }
            if (!loadRevocationList(rkSfr, emsg))
               {FatalC(erp, emsg.c_str(), EINVAL);
                return 0;
               }
         }
      AcceptedHosts.clear();
      addDefaultAcceptedHosts();
      for (const auto &hst : extraHosts) addAcceptedHost(hst);
      if (DebugSSH.load(std::memory_order_relaxed))
         {
            std::string hl;
            for (const auto &kv : AcceptedHosts) {if (!hl.empty()) hl += ","; hl += kv.first;}
            debugLog("Init", "accepted target hostnames: " + hl);
         }
   }

   char buff[256];
   snprintf(buff, sizeof(buff), "TLS:%" PRIu64 ":%d:", opts, MaxCredSize.load(std::memory_order_relaxed));
   return strdup(buff);
}
}

extern "C"
{
XrdSecProtocol *XrdSecProtocolsshObject(const char mode,
                                        const char *hostname,
                                              XrdNetAddrInfo &endPoint,
                                        const char *parms,
                                              XrdOucErrInfo *erp)
{
   if (!endPoint.isUsingTLS())
      {FatalC(erp, "security protocol 'ssh' disallowed for non-TLS connections.",
              ENOTSUP, false);
       return 0;
      }

   if (mode == 'c')
      {
         bool aOK = false;
         auto prot = std::make_unique<XrdSecProtocolssh>(hostname, parms, erp, aOK);
         if (aOK) return prot.release();
         return nullptr;
      }

   return std::make_unique<XrdSecProtocolssh>(hostname, endPoint).release();
}
}
