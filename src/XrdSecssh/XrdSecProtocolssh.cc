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

#include <arpa/inet.h>
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
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <strings.h>
#include <pwd.h>

#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/param_build.h>
#include <openssl/params.h>
#endif

#include "XrdVersion.hh"
#include "XrdNet/XrdNetAddrInfo.hh"
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

static const uint8_t kProtoVersion = 0;
static const int kDefaultMaxCredSize = 8192;
static const int kDefaultNonceTTL = 30;

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
   if (in.empty()) return false;
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
   EVP_MD_CTX *ctx = EVP_MD_CTX_new();
   if (!ctx) return false;
   unsigned char md[EVP_MAX_MD_SIZE];
   unsigned int mdLen = 0;
   bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), 0) == 1
          && EVP_DigestUpdate(ctx, data.data(), data.size()) == 1
          && EVP_DigestFinal_ex(ctx, md, &mdLen) == 1;
   EVP_MD_CTX_free(ctx);
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
   const unsigned char *p = reinterpret_cast<const unsigned char *>(blob.data());
   uint32_t n1net = 0;
   memcpy(&n1net, p, 4);
   uint32_t n1 = ntohl(n1net);
   p += 4;
   if (4 + n1 + 4 > blob.size()) return false;
   std::string alg(reinterpret_cast<const char *>(p), n1);
   p += n1;
   if (alg != "ssh-ed25519") return false;
   uint32_t n2net = 0;
   memcpy(&n2net, p, 4);
   uint32_t n2 = ntohl(n2net);
   p += 4;
   if (4 + n1 + 4 + n2 != blob.size()) return false;
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
   if (at + n > blob.size()) return false;
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

EVP_PKEY *makeRSAPublicKeyFromNE(const std::string &nBin, const std::string &eBin)
{
   BIGNUM *bnN = BN_bin2bn(reinterpret_cast<const unsigned char *>(nBin.data()),
                           static_cast<int>(nBin.size()), 0);
   BIGNUM *bnE = BN_bin2bn(reinterpret_cast<const unsigned char *>(eBin.data()),
                           static_cast<int>(eBin.size()), 0);
   if (!bnN || !bnE)
      {BN_free(bnN); BN_free(bnE); return 0;}

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
   EVP_PKEY *pkey = 0;
   OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
   OSSL_PARAM *params = 0;
   EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(0, "RSA", 0);
   if (!bld || !ctx) goto done3;
   if (OSSL_PARAM_BLD_push_BN(bld, "n", bnN) <= 0
   ||  OSSL_PARAM_BLD_push_BN(bld, "e", bnE) <= 0) goto done3;
   params = OSSL_PARAM_BLD_to_param(bld);
   if (!params) goto done3;
   if (EVP_PKEY_fromdata_init(ctx) <= 0
   ||  EVP_PKEY_fromdata(ctx, &pkey, EVP_PKEY_PUBLIC_KEY, params) <= 0)
      pkey = 0;
done3:
   BN_free(bnN);
   BN_free(bnE);
   OSSL_PARAM_free(params);
   OSSL_PARAM_BLD_free(bld);
   EVP_PKEY_CTX_free(ctx);
   return pkey;
#else
   RSA *rsa = RSA_new();
   EVP_PKEY *pkey = 0;
   if (!rsa) goto done11;
   if (RSA_set0_key(rsa, bnN, bnE, 0) != 1) goto done11;
   bnN = 0;
   bnE = 0;
   pkey = EVP_PKEY_new();
   if (!pkey) goto done11;
   if (EVP_PKEY_assign_RSA(pkey, rsa) != 1)
      {EVP_PKEY_free(pkey); pkey = 0; goto done11;}
   rsa = 0;
done11:
   BN_free(bnN);
   BN_free(bnE);
   RSA_free(rsa);
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
   BIGNUM *bnN = 0, *bnE = 0;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
   if (EVP_PKEY_get_bn_param(pkey, "n", &bnN) != 1
   ||  EVP_PKEY_get_bn_param(pkey, "e", &bnE) != 1)
      {BN_free(bnN); BN_free(bnE); return false;}
#else
   RSA *rsa = EVP_PKEY_get1_RSA(pkey);
   if (!rsa) return false;
   const BIGNUM *nRef = 0, *eRef = 0;
   RSA_get0_key(rsa, &nRef, &eRef, 0);
   if (!nRef || !eRef)
      {RSA_free(rsa); return false;}
   bnN = BN_dup(nRef);
   bnE = BN_dup(eRef);
   RSA_free(rsa);
   if (!bnN || !bnE)
      {BN_free(bnN); BN_free(bnE); return false;}
#endif
   std::string mpE = bnToMpint(bnE);
   std::string mpN = bnToMpint(bnN);
   BN_free(bnN);
   BN_free(bnE);
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
   char *b = (char *)malloc(s.size());
   if (!b) return 0;
   memcpy(b, s.data(), s.size());
   return new XrdSecCredentials(b, static_cast<int>(s.size()));
}

XrdSecParameters *makeParametersFromString(const std::string &s)
{
   char *b = (char *)malloc(s.size());
   if (!b) return 0;
   memcpy(b, s.data(), s.size());
   return new XrdSecParameters(b, static_cast<int>(s.size()));
}

bool readU16(const char *&p, const char *e, uint16_t &v)
{
   if (e - p < 2) return false;
   memcpy(&v, p, 2);
   p += 2;
   v = ntohs(v);
   return true;
}

bool readU32(const char *&p, const char *e, uint32_t &v)
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
   if (at + n > buf.size()) return false;
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
   std::string rawPub;
   EVP_PKEY   *pkey = 0;
};

struct PendingChallenge
{
   std::string nonce;
   std::string fp;
   std::string user;
   std::string verifyAlg;
   std::string verifyBlob;
   time_t      expiresAt = 0;
};

std::mutex Gm;
std::unordered_map<std::string, TrustedKey> TrustedByFP;
std::unordered_map<std::string, TrustedKey> TrustedCAByFP;
std::unordered_map<std::string, std::string> PrincipalMap;
std::unordered_map<std::string, PendingChallenge> PendingByTid;
std::string KeysFile = "/etc/xrootd/ssh_authorized_keys";
std::string CAKeysFile;
std::string PrincipalMapFile;
bool PrincipalAsUser = false;
bool PrincipalMapStatValid = false;
ino_t PrincipalMapIno = 0;
time_t PrincipalMapMTime = 0;
int MaxCredSize = kDefaultMaxCredSize;
int NonceTTL = kDefaultNonceTTL;
bool DebugSSH = false;
XrdSysLogger SSHLogger;
XrdSysError SSHLog(0, "secssh_");

void debugLog(const char *where, const std::string &msg)
{
   if (!DebugSSH) return;
   SSHLog.Emsg(where, "ssh", msg.c_str());
}

int connectAgentSocket(const char *sockPath)
{
   if (!sockPath || !*sockPath) return -1;
   int fd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (fd < 0) return -1;
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

bool makePkeyFromSshBlob(const std::string &alg, const std::string &blob, EVP_PKEY *&pk)
{
   pk = 0;
   if (alg == "ssh-ed25519")
      {
         std::string rawPub;
         if (!extractEd25519RawFromSshBlob(blob, rawPub)) return false;
         pk = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, 0,
                  reinterpret_cast<const unsigned char *>(rawPub.data()), rawPub.size());
         return pk != 0;
      }
   if (alg == "ssh-rsa")
      {
         std::string nBin, eBin;
         if (!extractRsaNEFromSshBlob(blob, nBin, eBin)) return false;
         pk = makeRSAPublicKeyFromNE(nBin, eBin);
         return pk != 0;
      }
   return false;
}

void clearTrustedMap(std::unordered_map<std::string, TrustedKey> &m)
{
   for (auto &it : m) if (it.second.pkey) EVP_PKEY_free(it.second.pkey);
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
bool checkSecureFile(const char *path, std::string &emsg);

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
   if ((!home || !*home))
      {
         long bufSz = sysconf(_SC_GETPW_R_SIZE_MAX);
         if (bufSz < 1024) bufSz = 4096;
         std::vector<char> buf(static_cast<size_t>(bufSz));
         struct passwd pw;
         struct passwd *res = 0;
         if (getpwuid_r(geteuid(), &pw, buf.data(), buf.size(), &res) == 0
         &&  res && res->pw_dir && *res->pw_dir) home = res->pw_dir;
      }
   if (!home || !*home) return;
   std::string h(home);
   out.push_back(h + "/.ssh/id_ed25519");
   out.push_back(h + "/.ssh/id_rsa");
}

bool loadPrincipalMap(std::string &emsg)
{
   if (PrincipalMapFile.empty()) return true;
   struct stat st;
   if (stat(PrincipalMapFile.c_str(), &st) != 0)
      {emsg = std::string("unable to stat principal-map-file: ") + PrincipalMapFile;
       return false;
      }
   std::ifstream in(PrincipalMapFile.c_str());
   if (!in.is_open())
      {emsg = std::string("unable to open principal-map-file: ") + PrincipalMapFile;
       return false;
      }
   std::unordered_map<std::string, std::string> newMap;
   std::string line;
   int lineNo = 0;
   while (std::getline(in, line))
      {
         lineNo++;
         std::string t = trim(line);
         if (t.empty() || t[0] == '#') continue;
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
         newMap[principal] = resolved;
      }
   PrincipalMap.swap(newMap);
   PrincipalMapIno = st.st_ino;
   PrincipalMapMTime = st.st_mtime;
   PrincipalMapStatValid = true;
   debugLog("Init", std::string("loaded principal map entries=")
                    + std::to_string(PrincipalMap.size()));
   return true;
}

bool ensurePrincipalMapFresh(std::string &emsg)
{
   // Caller must hold Gm to serialize map refresh with principal lookups.
   if (PrincipalMapFile.empty()) return true;
   struct stat st;
   if (stat(PrincipalMapFile.c_str(), &st) != 0)
      {emsg = std::string("unable to stat principal-map-file: ") + PrincipalMapFile;
       return false;
      }
   if (!PrincipalMapStatValid
   ||  st.st_ino != PrincipalMapIno
   ||  st.st_mtime != PrincipalMapMTime)
      {
         if (!checkSecureFile(PrincipalMapFile.c_str(), emsg)) return false;
         if (!loadPrincipalMap(emsg)) return false;
         debugLog("Auth", std::string("reloaded principal map file='") + PrincipalMapFile + "'");
      }
   return true;
}

bool mapPrincipalsToUser(const std::vector<std::string> &principals,
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
   if (PrincipalAsUser)
      {
         for (const auto &p : principals)
            {
               std::string resolved;
               if (!resolveLocalUser(p, resolved)) continue;
               mappedUser = resolved;
               mapMethod = "principal-as-user";
               return true;
            }
      }
   if (!PrincipalMap.empty())
      {
         for (const auto &p : principals)
            {
               auto it = PrincipalMap.find(p);
               if (it == PrincipalMap.end()) continue;
               mappedUser = it->second;
               mapMethod = "principal-map-file";
               return true;
            }
      }
   emsg = "No principal could be mapped to a valid local user";
   return false;
}

bool checkSecureFile(const char *path, std::string &emsg)
{
   struct stat st;
   if (stat(path, &st) != 0)
      {emsg = std::string("unable to stat file: ") + path + " (" + strerror(errno) + ")";
       return false;
      }
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
   return true;
}

bool loadTrustedKeyFile(const std::string &path,
                        std::unordered_map<std::string, TrustedKey> &outMap,
                        bool requireUser,
                        std::string &emsg)
{
   std::ifstream in(path.c_str());
   if (!in.is_open())
      {emsg = std::string("unable to open keys-file: ") + path;
       return false;
      }

   clearTrustedMap(outMap);
   std::string line;
   int lineNo = 0;
   while (std::getline(in, line))
      {
       lineNo++;
       std::string t = trim(line);
       if (t.empty() || t[0] == '#') continue;

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

       if (alg != "ssh-ed25519" && alg != "ssh-rsa") continue;

       std::string blob, rawPub;
       if (!b64Decode(keyb64, blob)) continue;
       EVP_PKEY *pk = 0;
       if (!makePkeyFromSshBlob(alg, blob, pk)) continue;
       if (!pk) continue;

       std::string fp;
       if (!sha256Base64(blob, fp))
          {EVP_PKEY_free(pk);
           continue;
          }

       TrustedKey k;
       k.user = user;
       k.alg = alg;
       k.fp = fp;
       k.sshBlob = blob;
       k.rawPub = rawPub;
       k.pkey = pk;
       outMap[fp] = k;
       debugLog("Init", std::string("accepted ")
                        + (requireUser ? "user key" : "ca key")
                        + " alg='" + k.alg + "'"
                        + (requireUser ? (" user='" + k.user + "'") : "")
                        + " fp='" + k.fp + "'");
      }
   return true;
}

bool loadTrustedKeys(std::string &emsg)
{
   debugLog("Init", std::string("loading keys-file='") + KeysFile + "'");
   if (!loadTrustedKeyFile(KeysFile, TrustedByFP, true, emsg)) return false;
   if (TrustedByFP.empty())
      {emsg = std::string("no usable ssh keys (ssh-ed25519/ssh-rsa) loaded from keys-file: ")
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
   if (!loadTrustedKeyFile(CAKeysFile, TrustedCAByFP, false, emsg)) return false;
   if (TrustedCAByFP.empty())
      {emsg = std::string("no usable CA keys (ssh-ed25519/ssh-rsa) loaded from ca-keys-file: ")
           + CAKeysFile;
       return false;
      }
   debugLog("Init", std::string("loaded ca keys count=")
                    + std::to_string(TrustedCAByFP.size()));
   return true;
}

bool validateUserCert(const std::string &certBlob,
                      const std::string &reqUser,
                      std::string &mappedUser,
                      std::string &verifyAlg,
                      std::string &verifyBlob,
                      std::string &fp,
                      std::string &emsg)
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
   if (validAfter && static_cast<uint64_t>(now) < validAfter)
      {emsg = "SSH certificate is not yet valid";
       return false;
      }
   if (validBefore && validBefore != static_cast<uint64_t>(-1) && static_cast<uint64_t>(now) > validBefore)
      {emsg = "SSH certificate expired";
       return false;
      }

   std::vector<std::string> principals;
   if (!splitPrincipalList(principalsBlob, principals))
      {emsg = "Malformed SSH certificate principals list";
       return false;
      }

   if (reqUser.empty())
      {
         if (PrincipalAsUser || !PrincipalMap.empty())
            {
               std::string ignoredMethod;
               if (!mapPrincipalsToUser(principals, mappedUser, ignoredMethod, emsg)) return false;
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
         if (PrincipalAsUser || !PrincipalMap.empty())
            {
               std::string ignoredMethod;
               if (!mapPrincipalsToUser(principals, mappedUser, ignoredMethod, emsg)) return false;
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
         if (sigAlg != "ssh-rsa" && sigAlg != "rsa-sha2-256")
            {emsg = "Unsupported RSA CA signature algorithm in certificate: " + sigAlg;
             return false;
            }
      }
   else if (sigAlg != caIt->second.alg)
      {emsg = "SSH certificate signature algorithm mismatch";
       return false;
      }

   std::string signedData = certBlob.substr(0, sigFieldStart);
   if (!verifyData(caIt->second.pkey, signedData, sigRaw))
      {emsg = "SSH certificate signature validation failed";
       return false;
      }

   if (!sha256Base64(certBlob, fp))
      {emsg = "Unable to fingerprint SSH certificate";
       return false;
      }

   debugLog("Auth", std::string("accepted user cert")
                    + " serial='" + std::to_string(serial) + "'"
                    + " key_id='" + keyId + "'"
                    + " signer_fp='" + signerFp + "'"
                    + " subject_alg='" + verifyAlg + "'"
                    + " cert_fp='" + fp + "'");
   return true;
}

bool signData(EVP_PKEY *priv, const std::string &msg, std::string &sig)
{
   sig.clear();
   if (!priv) return false;
   int ktype = EVP_PKEY_base_id(priv);
   EVP_MD_CTX *ctx = EVP_MD_CTX_new();
   if (!ctx) return false;
   bool ok = false;
   size_t sigLen = 0;
   if (ktype == EVP_PKEY_ED25519)
      {
         ok = EVP_DigestSignInit(ctx, 0, 0, 0, priv) == 1;
         if (ok) ok = EVP_DigestSign(ctx, 0, &sigLen,
                                     reinterpret_cast<const unsigned char *>(msg.data()),
                                     msg.size()) == 1;
         if (ok && sigLen > 0)
            {
               sig.resize(sigLen);
               ok = EVP_DigestSign(ctx, reinterpret_cast<unsigned char *>(&sig[0]), &sigLen,
                                   reinterpret_cast<const unsigned char *>(msg.data()),
                                   msg.size()) == 1;
               if (ok) sig.resize(sigLen);
            }
      }
   else if (ktype == EVP_PKEY_RSA)
      {
         ok = EVP_DigestSignInit(ctx, 0, EVP_sha256(), 0, priv) == 1
           && EVP_DigestSignUpdate(ctx, msg.data(), msg.size()) == 1
           && EVP_DigestSignFinal(ctx, 0, &sigLen) == 1;
         if (ok && sigLen > 0)
            {
               sig.resize(sigLen);
               ok = EVP_DigestSignFinal(ctx,
                       reinterpret_cast<unsigned char *>(&sig[0]), &sigLen) == 1;
               if (ok) sig.resize(sigLen);
            }
      }
   EVP_MD_CTX_free(ctx);
   return ok;
}

bool verifyData(EVP_PKEY *pub, const std::string &msg, const std::string &sig)
{
   if (!pub || sig.empty()) return false;
   int ktype = EVP_PKEY_base_id(pub);
   EVP_MD_CTX *ctx = EVP_MD_CTX_new();
   if (!ctx) return false;
   bool ok = false;
   if (ktype == EVP_PKEY_ED25519)
      {
         ok = EVP_DigestVerifyInit(ctx, 0, 0, 0, pub) == 1
           && EVP_DigestVerify(ctx,
                 reinterpret_cast<const unsigned char *>(sig.data()), sig.size(),
                 reinterpret_cast<const unsigned char *>(msg.data()), msg.size()) == 1;
      }
   else if (ktype == EVP_PKEY_RSA)
      {
         ok = EVP_DigestVerifyInit(ctx, 0, EVP_sha256(), 0, pub) == 1
           && EVP_DigestVerifyUpdate(ctx, msg.data(), msg.size()) == 1
           && EVP_DigestVerifyFinal(ctx,
                 reinterpret_cast<const unsigned char *>(sig.data()), sig.size()) == 1;
      }
   EVP_MD_CTX_free(ctx);
   return ok;
}

std::string challengePayload(const std::string &nonce, const std::string &fp)
{
   std::string p("xrdsec-ssh-v1|");
   p += nonce;
   p += "|";
   p += fp;
   return p;
}
}

class XrdSecProtocolssh : public XrdSecProtocol
{
public:
   int Authenticate(XrdSecCredentials *cred, XrdSecParameters **parms, XrdOucErrInfo *einfo=0);
   void Delete() {delete this;}
   XrdSecCredentials *getCredentials(XrdSecParameters *parms, XrdOucErrInfo *einfo=0);
   bool needTLS() {return true;}

   XrdSecProtocolssh(const char *parms, XrdOucErrInfo *erp, bool &aOK);
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
      if (privKey) EVP_PKEY_free(privKey);
   }

   static const int sshVersion = 0;

private:
   XrdSecCredentials *makeInitCred(XrdOucErrInfo *erp);
   XrdSecCredentials *makeResponseCred(XrdSecParameters *parms, XrdOucErrInfo *erp);
   bool ensureClientKeyLoaded(XrdOucErrInfo *erp);
   bool loadClientKeyFromFile(const char *kPath, XrdOucErrInfo *erp);
   bool loadClientKeyFromAgent(const char *sockPath, XrdOucErrInfo *erp);
   bool signWithAgent(const std::string &msg, std::string &sigOut, XrdOucErrInfo *erp);

   int      maxCredSize = 0;
   EVP_PKEY *privKey = 0;
   bool useAgent = false;
   std::string clientUser;
   std::string clientSshBlob;
   std::string clientFingerprint;
   std::string clientKeyAlg;
   std::string agentSock;
};

XrdSecProtocolssh::XrdSecProtocolssh(const char *parms, XrdOucErrInfo *erp, bool &aOK)
   : XrdSecProtocol("ssh"), maxCredSize(0), privKey(0)
{
   aOK = false;
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
      debugLog("ensureClientKeyLoaded", std::string("SSH_AUTH_SOCK=") + sock);
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
   BIO *bio = BIO_new_file(kPath, "r");
   if (!bio)
      {FatalC(erp, "Unable to open private key file.", errno ? errno : EINVAL);
       return false;
      }
   EVP_PKEY *k = PEM_read_bio_PrivateKey(bio, 0, 0, 0);
   BIO_free(bio);
   if (!k)
      {FatalC(erp, "Unable to parse private key; use PEM/PKCS8 ed25519 or rsa key.", EINVAL);
       return false;
      }

   std::string blob;
   std::string alg;
   int ktype = EVP_PKEY_base_id(k);
   if (ktype == EVP_PKEY_ED25519)
      {
         size_t pubLen = 32;
         std::string rawPub(32, '\0');
         if (EVP_PKEY_get_raw_public_key(k,
               reinterpret_cast<unsigned char *>(&rawPub[0]), &pubLen) != 1 || pubLen != 32)
            {EVP_PKEY_free(k);
             FatalC(erp, "Private key is not usable as ed25519 key.", EINVAL);
             return false;
            }
         rawPub.resize(pubLen);
         blob = makeEd25519SshBlob(rawPub);
         alg = "ssh-ed25519";
      }
   else if (ktype == EVP_PKEY_RSA)
      {
         if (!makeSshRsaBlobFromPkey(k, blob))
            {EVP_PKEY_free(k);
             FatalC(erp, "Private key is not usable as ssh-rsa key.", EINVAL);
             return false;
            }
         alg = "ssh-rsa";
      }
   else
      {
         EVP_PKEY_free(k);
         FatalC(erp, "Unsupported SSH private key type (supported: ed25519, rsa).", EINVAL);
         return false;
      }

   std::string fp;
   if (blob.empty() || !sha256Base64(blob, fp))
      {EVP_PKEY_free(k);
       FatalC(erp, "Unable to compute key fingerprint.", EINVAL);
       return false;
      }

   if (privKey) EVP_PKEY_free(privKey);
   privKey = k;
   useAgent = false;
   clientSshBlob = blob;
   clientFingerprint = fp;
   clientKeyAlg = alg;
   debugLog("loadClientKeyFromFile", std::string("loaded key alg=") + clientKeyAlg
      + " fp=" + clientFingerprint + " path=" + kPath);
   return true;
}

bool XrdSecProtocolssh::loadClientKeyFromAgent(const char *sockPath, XrdOucErrInfo *erp)
{
   debugLog("loadClientKeyFromAgent", std::string("connecting to ssh-agent socket: ") + sockPath);
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
         if (privKey) {EVP_PKEY_free(privKey); privKey = 0;}
         agentSock = sockPath;
         clientSshBlob = blob;
         clientFingerprint = fp;
         clientKeyAlg = alg;
         debugLog("loadClientKeyFromAgent", std::string("selected agent key alg=") + alg
            + " fp=" + fp + " comment=" + comment);
         debugLog("loadClientKeyFromAgent", std::string("client auth mode=agent socket=")
            + agentSock + " key=" + clientKeyAlg + " " + clientFingerprint);
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
   debugLog("signWithAgent", std::string("signing challenge via agent socket=") + agentSock
      + " key=" + clientKeyAlg + " " + clientFingerprint);

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
         if (sigAlg != "ssh-rsa" && sigAlg != "rsa-sha2-256")
            {
               std::string emsg("ssh-agent returned unsupported RSA signature algorithm: ");
               emsg += sigAlg;
               emsg += " (expected ssh-rsa or rsa-sha2-256)";
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
   uint32_t ts = 0;
   uint16_t nLen = 0, fLen = 0;
   if (!readU32(p, e, ts) || !readU16(p, e, nLen) || !readU16(p, e, fLen))
      return FatalC(erp, "Malformed SSH challenge.", EINVAL);
   if (nLen == 0 || fLen == 0 || (e - p) < (nLen + fLen))
      return FatalC(erp, "Malformed SSH challenge lengths.", EINVAL);
   std::string nonce(p, nLen);
   p += nLen;
   std::string fp(p, fLen);
   (void)ts;
   if (fp != clientFingerprint)
      return FatalC(erp, "Server challenge key fingerprint mismatch.", EAUTH);

   std::string payload = challengePayload(nonce, fp);
   std::string sig;
   bool okSign = useAgent ? signWithAgent(payload, sig, erp) : signData(privKey, payload, sig);
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
   out.append(sig);
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
         uint16_t uLen = 0, bLen = 0;
         if (!readU16(p, e, uLen) || !readU16(p, e, bLen))
            return FatalS(erp, "Malformed SSH init request.", EINVAL, false);
         if (uLen == 0 || bLen == 0 || (e - p) < (uLen + bLen))
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

         std::lock_guard<std::mutex> lock(Gm);
         if (isCert)
            {
               std::string emsg;
               if (!ensurePrincipalMapFresh(emsg))
                  {
                     debugLog("Auth", std::string("reject ") + emsg);
                     return FatalS(erp, emsg.c_str(), EAUTH, false);
                  }
               if (!validateUserCert(blob, reqUser, mappedUser, verifyAlg, verifyBlob, fp, emsg))
                  {
                     debugLog("Auth", std::string("reject ") + emsg);
                     return FatalS(erp, emsg.c_str(), EAUTH, false);
                  }
            }
         else
            {
               auto it = TrustedByFP.find(fp);
               if (it == TrustedByFP.end())
                  {
                     std::string m = "SSH public key not trusted (fp=" + fp + ")";
                     debugLog("Auth", std::string("reject ") + m);
                     return FatalS(erp, m.c_str(), EAUTH, false);
                  }
               mappedUser = it->second.user;
               verifyBlob = it->second.sshBlob.empty() ? blob : it->second.sshBlob;
               verifyAlg = it->second.alg;
               if (verifyAlg.empty())
                  {
                     if (!getSshBlobAlg(verifyBlob, verifyAlg))
                        return FatalS(erp, "Unable to determine SSH key algorithm.", EAUTH, false);
                  }
               if (!reqUser.empty() && reqUser != mappedUser)
                  {
                     std::string m = "SSH username/key mapping mismatch"
                                     " (requested='" + reqUser
                                   + "', mapped='" + mappedUser + "')";
                     debugLog("Auth", std::string("reject ") + m);
                     return FatalS(erp, m.c_str(), EAUTH, false);
                  }
            }
         debugLog("Auth", std::string("init")
                        + " tident='" + (Entity.tident ? Entity.tident : "") + "'"
                        + " req_user='" + reqUser + "'"
                        + " mapped_user='" + mappedUser + "'"
                        + " key_alg='" + verifyAlg + "'"
                        + " auth_mode='" + (isCert ? "ssh-cert" : "raw-key") + "'"
                        + " fp='" + fp + "'");

         std::string nonce(32, '\0');
         if (RAND_bytes(reinterpret_cast<unsigned char *>(&nonce[0]), nonce.size()) != 1)
            return FatalS(erp, "Unable to generate SSH challenge nonce.", EIO, false);

         if (!Entity.tident || !*Entity.tident)
            return FatalS(erp, "Missing transport identity for SSH challenge state.",
                          EAUTH, false);

         PendingChallenge pc;
         pc.nonce = nonce;
         pc.fp = fp;
         pc.user = mappedUser;
         pc.verifyAlg = verifyAlg;
         pc.verifyBlob = verifyBlob;
         pc.expiresAt = time(0) + NonceTTL;
         PendingByTid[Entity.tident] = pc;

         std::string out;
         WireHdr ch;
         memcpy(ch.id, "ssh", 4);
         ch.ver = kProtoVersion;
         ch.op = OpChallenge;
         ch.rsvd[0] = ch.rsvd[1] = 0;
         out.append(reinterpret_cast<const char *>(&ch), sizeof(ch));
         putU32(out, static_cast<uint32_t>(time(0)));
         putU16(out, static_cast<uint16_t>(nonce.size()));
         putU16(out, static_cast<uint16_t>(fp.size()));
         out.append(nonce);
         out.append(fp);
         *parms = makeParametersFromString(out);
         if (!*parms) return FatalS(erp, "Insufficient memory.", ENOMEM, false);
         return 1;
      }
   if (h->op == OpResponse)
      {
         uint16_t sLen = 0;
         if (!readU16(p, e, sLen))
            return FatalS(erp, "Malformed SSH response.", EINVAL, false);
         if (sLen == 0 || (e - p) < sLen)
            return FatalS(erp, "Malformed SSH signature length.", EINVAL, false);
         std::string sig(p, sLen);

         if (!Entity.tident || !*Entity.tident)
            return FatalS(erp, "Missing transport identity for SSH challenge state.",
                          EAUTH, false);
         std::string tid = Entity.tident;
         PendingChallenge pc;
         EVP_PKEY *verifyKey = 0;
         {
            std::lock_guard<std::mutex> lock(Gm);
            auto pit = PendingByTid.find(tid);
            if (pit == PendingByTid.end())
               return FatalS(erp, "No pending SSH challenge.", EAUTH, false);
            if (pit->second.expiresAt < time(0))
               {PendingByTid.erase(pit);
                return FatalS(erp, "SSH challenge expired.", EAUTH, false);
               }
            pc = pit->second;
            PendingByTid.erase(pit); // single-use
         }
         if (!makePkeyFromSshBlob(pc.verifyAlg, pc.verifyBlob, verifyKey) || !verifyKey)
            return FatalS(erp, "Unable to reconstruct SSH verify key.", EAUTH, false);

         std::string payload = challengePayload(pc.nonce, pc.fp);
         bool okVerify = verifyData(verifyKey, payload, sig);
         EVP_PKEY_free(verifyKey);
         if (!okVerify) return FatalS(erp, "SSH signature validation failed.", EAUTH, false);

         if (Entity.name) free(Entity.name);
         Entity.name = strdup(pc.user.c_str());
         strncpy(Entity.prot, "ssh", sizeof(Entity.prot));
         debugLog("Auth", std::string("success")
                        + " tident='" + (Entity.tident ? Entity.tident : "") + "'"
                        + " user='" + pc.user + "'"
                        + " key_alg='" + pc.verifyAlg + "'"
                        + " fp='" + pc.fp + "'");
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
   if (dbg && *dbg && strcmp(dbg, "0") != 0) DebugSSH = true;
   if (mode == 'c') return &nilstr;

   if (parms && *parms)
      {
         XrdOucString cfgParms(parms);
         XrdOucTokenizer cfg(const_cast<char *>(cfgParms.c_str()));
         cfg.GetLine();
         char *val = 0, *endP = 0;
         while ((val = cfg.GetToken()))
               {
                  if (!strcmp(val, "-maxsz"))
                     {
                        if (!(val = cfg.GetToken()))
                           {FatalC(erp, "-maxsz argument missing", EINVAL); return 0;}
                        MaxCredSize = strtol(val, &endP, 10);
                        if (MaxCredSize <= 0 || MaxCredSize > 524288 || *endP)
                           {FatalC(erp, "-maxsz argument invalid", EINVAL); return 0;}
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
                        NonceTTL = strtol(val, &endP, 10);
                        if (NonceTTL <= 0 || NonceTTL > 600 || *endP)
                           {FatalC(erp, "-nonce-ttl argument invalid", EINVAL); return 0;}
                     }
                  else if (!strcmp(val, "-debug"))
                     {
                        DebugSSH = true;
                     }
                  else {XrdOucString eTxt("Invalid parameter - "); eTxt += val;
                        FatalC(erp, eTxt.c_str(), EINVAL); return 0;
                       }
               }
      }

   std::string emsg;
   if (!checkSecureFile(KeysFile.c_str(), emsg))
      {FatalC(erp, emsg.c_str(), EACCES);
       return 0;
      }
   if (!CAKeysFile.empty() && !checkSecureFile(CAKeysFile.c_str(), emsg))
      {FatalC(erp, emsg.c_str(), EACCES);
       return 0;
      }
   if (!PrincipalMapFile.empty() && !checkSecureFile(PrincipalMapFile.c_str(), emsg))
      {FatalC(erp, emsg.c_str(), EACCES);
       return 0;
      }
   {
      std::lock_guard<std::mutex> lock(Gm);
      clearTrusted();
      PrincipalMap.clear();
      if (!loadTrustedKeys(emsg))
         {FatalC(erp, emsg.c_str(), EINVAL);
          return 0;
         }
      if (!loadTrustedCAKeys(emsg))
         {FatalC(erp, emsg.c_str(), EINVAL);
          return 0;
         }
      if (!loadPrincipalMap(emsg))
         {FatalC(erp, emsg.c_str(), EINVAL);
          return 0;
         }
      PendingByTid.clear();
   }

   char buff[256];
   snprintf(buff, sizeof(buff), "TLS:%" PRIu64 ":%d:", opts, MaxCredSize);
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
         XrdSecProtocolssh *prot = new XrdSecProtocolssh(parms, erp, aOK);
         if (aOK) return prot;
         delete prot;
         return 0;
      }

   XrdSecProtocolssh *prot = new XrdSecProtocolssh(hostname, endPoint);
   if (!prot)
      {FatalC(erp, "insufficient memory for protocol.", ENOMEM, false);
       return 0;
      }
   return prot;
}
}
