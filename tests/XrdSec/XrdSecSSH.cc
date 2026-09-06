#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

// Exercise internal SSH helpers directly in one TU.
#include "../../src/XrdSecssh/XrdSecProtocolssh.cc"

namespace {

std::string B64Encode(const std::string &in)
{
  std::vector<unsigned char> out(((in.size() + 2) / 3) * 4 + 4);
  int n = EVP_EncodeBlock(out.data(),
                          reinterpret_cast<const unsigned char *>(in.data()),
                          static_cast<int>(in.size()));
  if (n <= 0) return std::string();
  return std::string(reinterpret_cast<char *>(out.data()), static_cast<size_t>(n));
}

EVP_PKEY *LoadTestRsaPrivateKey()
{
  static const char kTestPrivateKeyPem[] =
      "-----BEGIN PRIVATE KEY-----\n"
      "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDSvCX3WFZLsb60\n"
      "VfC7RT2lUF5zzg6ptteayFktnmnaZv2bF5phT2Hk1mEpQ2bFt2apTX/olfFoXDGM\n"
      "MdBDWCQzpd8Zf/AknwjRhNzFge+R8gvPsRiu/bAxe5kJlVbxN0cxzXeVlopqzppq\n"
      "e6eBl/6yZnhhhgtzFzFGMRrUyQMHYB9WZ8G5cFzHLIDsecYL/hejrSWYeWF95NZJ\n"
      "apogbw+ozpM8rqMbXYTwSjziy21HDH20mPrbOzTam//SoCkaR6trTMR8Lcu5Z1KK\n"
      "O8JaY0cCe13St9uuQtzBXyYxsk3oeWcslEStweeMb9xsXqAMecyZ4vo4OAn2RhsJ\n"
      "LFa9X4QVAgMBAAECggEBAKj1gzcyU1YzN6Sc9vsfA9MEggY1Yto9p9YI5j9GMLaU\n"
      "NqdfoD5/CA0SuSn9SWwipKP/aBtSBRDYQj2WPtWP2M60IhDu29pwzAA4l02f5TXN\n"
      "rBykcBb1fYve/g+J9gBGgsVyaHu+DFKgPXw8kku8UtA0ijYe5BUttisB+fI7DDuf\n"
      "JrNQizvuw8fTb4vlpKn5o2/zCiMyvjUIGf9In3UeoUtUfekBsdlaestvCZl7Jp4d\n"
      "1hpjp2BSkaiTnlu4zjSOWiutXznfybTYM0TkQ5D4bK9pW/aaxBEm091+WV0lICSI\n"
      "P7OfNejDsVxfOfL6AIlaXnOYBiTvvBOQOF3BNdcfzgECgYEA/7y9FuBGVPFcHRDM\n"
      "L8oM+o0FUKIXO25FCUYHH6gODsDYXYtv/mAqgiqbSTteRCe+2/67RDS94kHiuAbQ\n"
      "pP92WynsseM3V42/Zt5P2JcyUucOYFAxUMTkGFi0DOBs/BSPsZTB1aB1SlA806an\n"
      "i7QIxO9TbGtpaf2cPXyJqAkHbYECgYEA0vOS2az/1yG1jt3cHKipdDs6PEl2KTMg\n"
      "Bbn6mswb4xqrtKoUc9mflnt7wugvupqs0K4vIEBaV3fHIIVrQBW1g96NttJ6Dcb2\n"
      "K2HofaG/8Gj6bsoMTO4j8UORIbqhbbUFvySQsDDfLaurDgX7bSzY4fx8efOeQWYz\n"
      "GIZ0d+6AyJUCgYAxs7vIM2RX4+S0HWyhqRImq7uptSbwvPib2clOpLm+skoavBvT\n"
      "A1ufmqo9bFVgx5y3YuWAVwPEcmueumxYdPHKu/YtiGxcDsdxNams/Y2hQRixLUS4\n"
      "Xtu5w5uSmRd1UoWuzKtzWlERVVDNDamoZCAELkM3YhTFra1s4cRbLQ4NgQKBgBgI\n"
      "7oNkpaWyTohfmNOfF+eJVAJIcHETRSPB4969QLQodsXX1wq4EenIWfqED+QX+Xax\n"
      "Uec/zctHd2WqLeUYVB0ZP1ZENunY8HVS63Vc94yBuX7kAHDHdUro4uFP7VKdnHEB\n"
      "zoZ0mwkOFSg84D+5K2DcLqaKbk6PQWUa9KwxfXTdAoGATZfBgdSFZuT1/xbPA2iG\n"
      "7fvAc/Bn6yW4bIjAXRnk/79br6awLlbmKi7aHJlvk7Z7Gqrl1hrqy8tcsAwsbDgt\n"
      "DmUsHBOQzN1xtlnbKCj8wfzygXjORJFsahacp4/ouFVaVnttobykz6NdRnjxCVRx\n"
      "bMtLkbplio+ALbmbxTyeGdA=\n"
      "-----END PRIVATE KEY-----\n";

  BIO *bio = BIO_new_mem_buf(kTestPrivateKeyPem, -1);
  if (!bio) return nullptr;
  EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  return pkey;
}

std::string TempFilePath(const char *suffix)
{
  std::string p = "/tmp/xrdsecssh-test-";
  p += std::to_string(static_cast<long long>(getpid()));
  p += "-";
  p += suffix;
  return p;
}

void WriteFile(const std::string &path, const std::string &content, mode_t mode = 0600)
{
  std::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
  ASSERT_TRUE(out.is_open()) << "unable to open " << path;
  out << content;
  out.close();
  ASSERT_EQ(chmod(path.c_str(), mode), 0) << "chmod failed for " << path;
}

std::string BuildInitCred(const std::string &user, const std::string &blob)
{
  std::string out;
  WireHdr h;
  memcpy(h.id, "ssh", 4);
  h.ver = kProtoVersion;
  h.op = OpInit;
  h.rsvd[0] = h.rsvd[1] = 0;
  out.append(reinterpret_cast<const char *>(&h), sizeof(h));
  uint16_t ulen = htons(static_cast<uint16_t>(user.size()));
  uint16_t blen = htons(static_cast<uint16_t>(blob.size()));
  out.append(reinterpret_cast<const char *>(&ulen), sizeof(ulen));
  out.append(reinterpret_cast<const char *>(&blen), sizeof(blen));
  out.append(user);
  out.append(blob);
  return out;
}

// Host the simulated client "connected to"; always in the server's default
// accepted set.
static const char kTestHost[] = "localhost";

std::string BuildResponseCred(const std::string &sig, const std::string &host = kTestHost)
{
  std::string out;
  WireHdr h;
  memcpy(h.id, "ssh", 4);
  h.ver = kProtoVersion;
  h.op = OpResponse;
  h.rsvd[0] = h.rsvd[1] = 0;
  out.append(reinterpret_cast<const char *>(&h), sizeof(h));
  putU16(out, static_cast<uint16_t>(sig.size()));
  putU16(out, static_cast<uint16_t>(host.size()));
  out.append(sig);
  out.append(host);
  return out;
}

std::string BuildChallengeParams(const std::string &nonce, const std::string &fp)
{
  std::string out;
  WireHdr h;
  memcpy(h.id, "ssh", 4);
  h.ver = kProtoVersion;
  h.op = OpChallenge;
  h.rsvd[0] = h.rsvd[1] = 0;
  out.append(reinterpret_cast<const char *>(&h), sizeof(h));
  putU16(out, static_cast<uint16_t>(nonce.size()));
  putU16(out, static_cast<uint16_t>(fp.size()));
  out.append(nonce);
  out.append(fp);
  return out;
}

// Parses a server challenge; returns false if malformed.
bool ParseChallenge(const XrdSecParameters *challenge, std::string &nonce, std::string &fp)
{
  if (!challenge || challenge->size < static_cast<int>(sizeof(WireHdr) + 4)) return false;
  const char *cp = challenge->buffer;
  const char *ce = challenge->buffer + challenge->size;
  const WireHdr *wh = reinterpret_cast<const WireHdr *>(cp);
  if (memcmp(wh->id, "ssh", 4) != 0 || wh->op != OpChallenge || wh->ver != kProtoVersion)
    return false;
  cp += sizeof(WireHdr);
  uint16_t nLen = 0, fLen = 0;
  if (!readU16(cp, ce, nLen) || !readU16(cp, ce, fLen)) return false;
  if (nLen == 0 || fLen == 0 || (ce - cp) != nLen + fLen) return false;
  nonce.assign(cp, nLen);
  cp += nLen;
  fp.assign(cp, fLen);
  return true;
}

bool WritePrivateKeyPem(const std::string &path, EVP_PKEY *pkey)
{
  if (!pkey) return false;
  BIO *bio = BIO_new_file(path.c_str(), "w");
  if (!bio) return false;
  const int ok = PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
  BIO_free(bio);
  return ok == 1;
}

XrdSecCredentials *MakeCred(const std::string &payload)
{
  char *b = static_cast<char *>(malloc(payload.size()));
  if (!b) return nullptr;
  memcpy(b, payload.data(), payload.size());
  return new XrdSecCredentials(b, static_cast<int>(payload.size()));
}

void AppendU32BE(std::string &out, uint32_t v)
{
  unsigned char b[4] = {static_cast<unsigned char>((v >> 24) & 0xff),
                        static_cast<unsigned char>((v >> 16) & 0xff),
                        static_cast<unsigned char>((v >> 8) & 0xff),
                        static_cast<unsigned char>(v & 0xff)};
  out.append(reinterpret_cast<const char *>(b), 4);
}

void AppendU64BE(std::string &out, uint64_t v)
{
  for (int i = 7; i >= 0; --i)
    out.push_back(static_cast<char>((v >> (i * 8)) & 0xff));
}

// Deterministic ed25519 keypair from a seed byte; returns the private key and
// fills rawPub with the 32-byte raw public key.
EVP_PKEY *MakeEd25519FromSeed(unsigned char base, std::string &rawPub)
{
  std::string seed(32, '\0');
  for (size_t i = 0; i < seed.size(); ++i)
    seed[i] = static_cast<char>(base + i);
  EVP_PKEY *priv = EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(seed.data()), seed.size());
  if (!priv) return nullptr;
  size_t pubLen = 32;
  rawPub.assign(32, '\0');
  if (EVP_PKEY_get_raw_public_key(
          priv, reinterpret_cast<unsigned char *>(&rawPub[0]), &pubLen) != 1 ||
      pubLen != 32)
    {EVP_PKEY_free(priv); return nullptr;}
  rawPub.resize(pubLen);
  return priv;
}

// Build an OpenSSH-style ssh-ed25519 user certificate signed by caPriv.
std::string BuildEd25519UserCert(EVP_PKEY *caPriv,
                                 const std::string &caSshBlob,
                                 const std::string &userRawPub,
                                 const std::vector<std::string> &principals,
                                 uint64_t validAfter,
                                 uint64_t validBefore,
                                 uint32_t certType = 1,
                                 const std::string &keyId = "test-key-id",
                                 const std::string &criticalOpts = "",
                                 const std::string &caSigAlg = "ssh-ed25519")
{
  std::string cert;
  appendSshString(cert, "ssh-ed25519-cert-v01@openssh.com");
  appendSshString(cert, std::string(16, '\x01')); // cert nonce (ignored by server)
  appendSshString(cert, userRawPub);              // subject ed25519 raw pubkey
  AppendU64BE(cert, 1);                            // serial
  AppendU32BE(cert, certType);                     // type (1 == user)
  appendSshString(cert, keyId);
  std::string principalsBlob;
  for (const auto &p : principals) appendSshString(principalsBlob, p);
  appendSshString(cert, principalsBlob);
  AppendU64BE(cert, validAfter);
  AppendU64BE(cert, validBefore);
  appendSshString(cert, criticalOpts); // critical options
  appendSshString(cert, ""); // extensions
  appendSshString(cert, ""); // reserved
  appendSshString(cert, caSshBlob); // signature key (the CA public key blob)
  std::string sig;
  if (!signData(caPriv, cert, sig)) return std::string();
  std::string sigOuter;
  appendSshString(sigOuter, caSigAlg);
  appendSshString(sigOuter, sig);
  appendSshString(cert, sigOuter);
  return cert;
}

// Build an OpenSSH-style ssh-rsa user certificate signed by caPriv. The CA may
// be ed25519 or rsa; caSigAlg selects the signature-blob algorithm name.
std::string BuildRsaUserCert(EVP_PKEY *caPriv,
                             const std::string &caSshBlob,
                             EVP_PKEY *userPriv,
                             const std::vector<std::string> &principals,
                             uint64_t validAfter,
                             uint64_t validBefore,
                             const std::string &caSigAlg,
                             uint32_t certType = 1,
                             const std::string &keyId = "test-rsa-key-id")
{
  // Pull the subject e/n value strings straight from the ssh-rsa public blob.
  std::string userBlob;
  if (!makeSshRsaBlobFromPkey(userPriv, userBlob)) return std::string();
  size_t bat = 0;
  std::string subjAlg, eVal, nVal;
  if (!parseSshString(userBlob, bat, subjAlg) ||
      !parseSshString(userBlob, bat, eVal) ||
      !parseSshString(userBlob, bat, nVal))
    return std::string();

  std::string cert;
  appendSshString(cert, "ssh-rsa-cert-v01@openssh.com");
  appendSshString(cert, std::string(16, '\x02')); // cert nonce (ignored)
  appendSshString(cert, eVal);                      // subject RSA e
  appendSshString(cert, nVal);                      // subject RSA n
  AppendU64BE(cert, 1);                             // serial
  AppendU32BE(cert, certType);                      // type (1 == user)
  appendSshString(cert, keyId);
  std::string principalsBlob;
  for (const auto &p : principals) appendSshString(principalsBlob, p);
  appendSshString(cert, principalsBlob);
  AppendU64BE(cert, validAfter);
  AppendU64BE(cert, validBefore);
  appendSshString(cert, ""); // critical options
  appendSshString(cert, ""); // extensions
  appendSshString(cert, ""); // reserved
  appendSshString(cert, caSshBlob); // signature key (CA public key blob)
  std::string sig;
  if (!signData(caPriv, cert, sig)) return std::string();
  std::string sigOuter;
  appendSshString(sigOuter, caSigAlg);
  appendSshString(sigOuter, sig);
  appendSshString(cert, sigOuter);
  return cert;
}

// Register a trusted CA key; the inserted pkey is owned by the map and freed by
// clearTrusted() in TearDown.
void RegisterTrustedCA(const std::string &caSshBlob, EVP_PKEY *caPub,
                       const std::string &alg = "ssh-ed25519")
{
  std::string fp;
  ASSERT_TRUE(sha256Base64(caSshBlob, fp));
  TrustedKey k;
  k.alg = alg;
  k.fp = fp;
  k.sshBlob = caSshBlob;
  k.pkey.reset(caPub);
  TrustedCAByFP[fp] = std::move(k);
}

class XrdSecSSHTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    AcceptedHosts.clear();
    addDefaultAcceptedHosts();
  }

  void TearDown() override
  {
    clearTrusted();
    {
      std::lock_guard<std::mutex> lock(PrincipalMapMu);
      PrincipalMap.clear();
      PrincipalMapState = HotFileState();
    }
    {
      std::lock_guard<std::mutex> lock(RevokedMu);
      Revoked = RevocationList();
      RevokedState = HotFileState();
    }
    RevokedKeysFile.clear();
    PrincipalAsUser = false;
    AllowEmptyPrincipals = false;
    DenyUsers = {{"root", true}};
    PrincipalMapFile.clear();
    CAKeysFile.clear();
    unlink(keysPath.c_str());
    unlink(keysOpenPath.c_str());
    KeysFile = "/etc/xrootd/ssh_authorized_keys";
  }

  std::string keysPath = TempFilePath("keys");
  std::string keysOpenPath = TempFilePath("keys-open");
};

TEST_F(XrdSecSSHTest, SshBlobRoundTripEd25519)
{
  std::string raw(32, '\0');
  for (size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<char>(i + 1);

  std::string blob = makeEd25519SshBlob(raw);
  ASSERT_FALSE(blob.empty());

  std::string outRaw;
  ASSERT_TRUE(extractEd25519RawFromSshBlob(blob, outRaw));
  EXPECT_EQ(outRaw, raw);
}

TEST_F(XrdSecSSHTest, SafeReadFileRejectsGroupWritable)
{
  WriteFile(keysOpenPath, "dummy\n", 0660);
  std::string emsg;
  SafeFileResult sfr;
  EXPECT_FALSE(safeReadFile(keysOpenPath.c_str(), sfr, emsg));
  EXPECT_NE(emsg.find("must not be group/other writable"), std::string::npos);
}

TEST_F(XrdSecSSHTest, LoadTrustedKeysRejectsInvalidUsername)
{
  std::string path = TempFilePath("keys-invalid-user");
  const std::string line =
      "bad/user ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIHRlc3Qta2V5LWRhdGE=\n";
  WriteFile(path, line);

  std::unordered_map<std::string, TrustedKey> out;
  std::string emsg;
  EXPECT_FALSE(loadTrustedKeyFile(path, line, out, true, emsg));
  EXPECT_NE(emsg.find("invalid username"), std::string::npos);
  unlink(path.c_str());
}

TEST_F(XrdSecSSHTest, PrivateKeyFileRejectsGroupReadable)
{
  static const char kPem[] =
      "-----BEGIN PRIVATE KEY-----\n"
      "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDSvCX3WFZLsb60\n"
      "VfC7RT2lUF5zzg6ptteayFktnmnaZv2bF5phT2Hk1mEpQ2bFt2apTX/olfFoXDGM\n"
      "MdBDWCQzpd8Zf/AknwjRhNzFge+R8gvPsRiu/bAxe5kJlVbxN0cxzXeVlopqzppq\n"
      "e6eBl/6yZnhhhgtzFzFGMRrUyQMHYB9WZ8G5cFzHLIDsecYL/hejrSWYeWF95NZJ\n"
      "apogbw+ozpM8rqMbXYTwSjziy21HDH20mPrbOzTam//SoCkaR6trTMR8Lcu5Z1KK\n"
      "O8JaY0cCe13St9uuQtzBXyYxsk3oeWcslEStweeMb9xsXqAMecyZ4vo4OAn2RhsJ\n"
      "LFa9X4QVAgMBAAECggEBAKj1gzcyU1YzN6Sc9vsfA9MEggY1Yto9p9YI5j9GMLaU\n"
      "NqdfoD5/CA0SuSn9SWwipKP/aBtSBRDYQj2WPtWP2M60IhDu29pwzAA4l02f5TXN\n"
      "rBykcBb1fYve/g+J9gBGgsVyaHu+DFKgPXw8kku8UtA0ijYe5BUttisB+fI7DDuf\n"
      "JrNQizvuw8fTb4vlpKn5o2/zCiMyvjUIGf9In3UeoUtUfekBsdlaestvCZl7Jp4d\n"
      "1hpjp2BSkaiTnlu4zjSOWiutXznfybTYM0TkQ5D4bK9pW/aaxBEm091+WV0lICSI\n"
      "P7OfNejDsVxfOfL6AIlaXnOYBiTvvBOQOF3BNdcfzgECgYEA/7y9FuBGVPFcHRDM\n"
      "L8oM+o0FUKIXO25FCUYHH6gODsDYXYtv/mAqgiqbSTteRCe+2/67RDS94kHiuAbQ\n"
      "pP92WynsseM3V42/Zt5P2JcyUucOYFAxUMTkGFi0DOBs/BSPsZTB1aB1SlA806an\n"
      "i7QIxO9TbGtpaf2cPXyJqAkHbYECgYEA0vOS2az/1yG1jt3cHKipdDs6PEl2KTMg\n"
      "Bbn6mswb4xqrtKoUc9mflnt7wugvupqs0K4vIEBaV3fHIIVrQBW1g96NttJ6Dcb2\n"
      "K2HofaG/8Gj6bsoMTO4j8UORIbqhbbUFvySQsDDfLaurDgX7bSzY4fx8efOeQWYz\n"
      "GIZ0d+6AyJUCgYAxs7vIM2RX4+S0HWyhqRImq7uptSbwvPib2clOpLm+skoavBvT\n"
      "A1ufmqo9bFVgx5y3YuWAVwPEcmueumxYdPHKu/YtiGxcDsdxNams/Y2hQRixLUS4\n"
      "Xtu5w5uSmRd1UoWuzKtzWlERVVDNDamoZCAELkM3YhTFra1s4cRbLQ4NgQKBgBgI\n"
      "7oNkpaWyTohfmNOfF+eJVAJIcHETRSPB4969QLQodsXX1wq4EenIWfqED+QX+Xax\n"
      "Uec/zctHd2WqLeUYVB0ZP1ZENunY8HVS63Vc94yBuX7kAHDHdUro4uFP7VKdnHEB\n"
      "zoZ0mwkOFSg84D+5K2DcLqaKbk6PQWUa9KwxfXTdAoGATZfBgdSFZuT1/xbPA2iG\n"
      "7fvAc/Bn6yW4bIjAXRnk/79br6awLlbmKi7aHJlvk7Z7Gqrl1hrqy8tcsAwsbDgt\n"
      "DmUsHBOQzN1xtlnbKCj8wfzygXjORJFsahacp4/ouFVaVnttobykz6NdRnjxCVRx\n"
      "bMtLkbplio+ALbmbxTyeGdA=\n"
      "-----END PRIVATE KEY-----\n";
  std::string path = TempFilePath("key-group-readable.pem");
  WriteFile(path, kPem, 0644);
  std::string emsg;
  EXPECT_FALSE(safeStatPrivateKeyFile(path.c_str(), emsg));
  unlink(path.c_str());
}

TEST_F(XrdSecSSHTest, MappedUsernameValidation)
{
  EXPECT_TRUE(isValidMappedUsername("alice"));
  EXPECT_TRUE(isValidMappedUsername("user_1"));
  EXPECT_FALSE(isValidMappedUsername(""));
  EXPECT_FALSE(isValidMappedUsername("bad/user"));
  EXPECT_FALSE(isValidMappedUsername(std::string(kMaxLocalUsernameLen + 1, 'a')));
}

TEST_F(XrdSecSSHTest, LoadTrustedKeysParsesBothFormats)
{
  std::string rawA(32, 'A');
  std::string rawB(32, 'B');
  std::string blobA = makeEd25519SshBlob(rawA);
  std::string blobB = makeEd25519SshBlob(rawB);
  ASSERT_FALSE(blobA.empty());
  ASSERT_FALSE(blobB.empty());
  std::string b64A = B64Encode(blobA);
  std::string b64B = B64Encode(blobB);

  std::string content;
  content += "alice ssh-ed25519 " + b64A + "\n";
  content += "ssh-ed25519 " + b64B + " bob@host\n";
  WriteFile(keysPath, content, 0600);

  KeysFile = keysPath;
  std::string emsg;
  ASSERT_TRUE(loadTrustedKeys(emsg)) << emsg;
  ASSERT_EQ(TrustedByFP.size(), static_cast<size_t>(2));

  std::string fpA, fpB;
  ASSERT_TRUE(sha256Base64(blobA, fpA));
  ASSERT_TRUE(sha256Base64(blobB, fpB));
  ASSERT_TRUE(TrustedByFP.find(fpA) != TrustedByFP.end());
  ASSERT_TRUE(TrustedByFP.find(fpB) != TrustedByFP.end());
  EXPECT_EQ(TrustedByFP[fpA].user, "alice");
  EXPECT_EQ(TrustedByFP[fpB].user, "bob");
}

TEST_F(XrdSecSSHTest, LoadTrustedKeysParsesRsa)
{
  EVP_PKEY *rsa = LoadTestRsaPrivateKey();
  ASSERT_NE(rsa, nullptr);
  std::string blob;
  ASSERT_TRUE(makeSshRsaBlobFromPkey(rsa, blob));
  ASSERT_FALSE(blob.empty());
  std::string b64 = B64Encode(blob);

  std::string content;
  content += "alice ssh-rsa " + b64 + "\n";
  WriteFile(keysPath, content, 0600);

  KeysFile = keysPath;
  std::string emsg;
  ASSERT_TRUE(loadTrustedKeys(emsg)) << emsg;
  ASSERT_EQ(TrustedByFP.size(), static_cast<size_t>(1));
  std::string fp;
  ASSERT_TRUE(sha256Base64(blob, fp));
  ASSERT_TRUE(TrustedByFP.find(fp) != TrustedByFP.end());
  EXPECT_EQ(TrustedByFP[fp].user, "alice");
  EVP_PKEY_free(rsa);
}

TEST_F(XrdSecSSHTest, SignAndVerifyChallengePayload)
{
  std::string rawPriv(32, '\0');
  for (size_t i = 0; i < rawPriv.size(); ++i)
    rawPriv[i] = static_cast<char>(0x20 + i);

  EVP_PKEY *priv = EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(rawPriv.data()),
      rawPriv.size());
  ASSERT_NE(priv, nullptr);

  size_t pubLen = 32;
  std::string rawPub(32, '\0');
  ASSERT_EQ(EVP_PKEY_get_raw_public_key(
                priv,
                reinterpret_cast<unsigned char *>(&rawPub[0]),
                &pubLen),
            1);
  ASSERT_EQ(pubLen, static_cast<size_t>(32));
  rawPub.resize(pubLen);

  EVP_PKEY *pub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(rawPub.data()),
      rawPub.size());
  ASSERT_NE(pub, nullptr);

  std::string payload = challengePayload("nonce-123", "SHA256:abc", kTestHost);
  std::string sig;
  ASSERT_TRUE(signData(priv, payload, sig));
  ASSERT_FALSE(sig.empty());
  EXPECT_TRUE(verifyData(pub, payload, sig));
  EXPECT_FALSE(verifyData(pub, payload + "x", sig));

  EVP_PKEY_free(pub);
  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, SignAndVerifyChallengePayloadRsa)
{
  EVP_PKEY *priv = LoadTestRsaPrivateKey();
  ASSERT_NE(priv, nullptr);

  std::string blob;
  ASSERT_TRUE(makeSshRsaBlobFromPkey(priv, blob));
  std::string nBin, eBin;
  ASSERT_TRUE(extractRsaNEFromSshBlob(blob, nBin, eBin));
  EvpPkeyPtr pub = makeRSAPublicKeyFromNE(nBin, eBin);
  ASSERT_TRUE(pub);

  std::string payload = challengePayload("nonce-rsa", "SHA256:rsa", kTestHost);
  std::string sig;
  ASSERT_TRUE(signData(priv, payload, sig));
  ASSERT_FALSE(sig.empty());
  EXPECT_TRUE(verifyData(pub.get(), payload, sig));
  EXPECT_FALSE(verifyData(pub.get(), payload + "x", sig));

  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, AuthenticateRejectsMalformedCredentials)
{
  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-malformed");

  std::string bad = "xx";
  XrdSecCredentials *cred = MakeCred(bad);
  ASSERT_NE(cred, nullptr);
  XrdSecParameters *outParms = nullptr;
  XrdOucErrInfo err;
  EXPECT_LT(srv.Authenticate(cred, &outParms, &err), 0);
  EXPECT_EQ(outParms, nullptr);
  delete cred;
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
}

TEST_F(XrdSecSSHTest, AuthenticateInitThenReplayAndExpiryChecks)
{
  std::string raw(32, 'R');
  std::string blob = makeEd25519SshBlob(raw);
  ASSERT_FALSE(blob.empty());
  std::string fp;
  ASSERT_TRUE(sha256Base64(blob, fp));

  EVP_PKEY *pub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(raw.data()), raw.size());
  ASSERT_NE(pub, nullptr);

  TrustedKey tk;
  tk.user = "alice";
  tk.fp = fp;
  tk.sshBlob = blob;
  tk.pkey.reset(pub);
  TrustedByFP[fp] = std::move(tk);

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-replay");
  XrdSecParameters *challenge = nullptr;
  XrdOucErrInfo err;

  std::string initPayload = BuildInitCred("alice", blob);
  XrdSecCredentials *initCred = MakeCred(initPayload);
  ASSERT_NE(initCred, nullptr);
  ASSERT_EQ(srv.Authenticate(initCred, &challenge, &err), 1);
  ASSERT_NE(challenge, nullptr);
  delete initCred;

  // A bad response consumes the pending challenge ...
  std::string fakeSig(64, 'S');
  XrdSecCredentials *respBad = MakeCred(BuildResponseCred(fakeSig));
  ASSERT_NE(respBad, nullptr);
  EXPECT_LT(srv.Authenticate(respBad, &challenge, &err), 0);
  delete respBad;

  // ... so a second response finds no pending challenge.
  XrdSecCredentials *respNoPending = MakeCred(BuildResponseCred(fakeSig));
  ASSERT_NE(respNoPending, nullptr);
  EXPECT_LT(srv.Authenticate(respNoPending, &challenge, &err), 0);
  {
    int ec = 0;
    const char *txt = err.getErrText(ec);
    EXPECT_NE(std::string(txt ? txt : "").find("No pending"), std::string::npos);
  }
  delete respNoPending;

  // An expired challenge is rejected even with a correct signature.
  {
    NonceTTL.store(1, std::memory_order_relaxed);
    delete challenge; challenge = nullptr;
    XrdSecCredentials *init3 = MakeCred(initPayload);
    ASSERT_EQ(srv.Authenticate(init3, &challenge, &err), 1);
    delete init3;
    NonceTTL.store(kDefaultNonceTTL, std::memory_order_relaxed);
    sleep(2);
    XrdSecCredentials *respExpired = MakeCred(BuildResponseCred(fakeSig));
    EXPECT_LT(srv.Authenticate(respExpired, &challenge, &err), 0);
    int ec = 0;
    const char *txt = err.getErrText(ec);
    EXPECT_NE(std::string(txt ? txt : "").find("expired"), std::string::npos);
    delete respExpired;
  }

  if (challenge) {delete challenge; challenge = nullptr;}
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
}

// Helper: register an ed25519 key for `user` and return the private key.
EVP_PKEY *RegisterRawEd25519(unsigned char seed, const std::string &user,
                             std::string &blob, std::string &fp)
{
  std::string rawPub;
  EVP_PKEY *priv = MakeEd25519FromSeed(seed, rawPub);
  if (!priv) return nullptr;
  blob = makeEd25519SshBlob(rawPub);
  if (!sha256Base64(blob, fp)) {EVP_PKEY_free(priv); return nullptr;}
  TrustedKey tk;
  tk.user = user;
  tk.alg = "ssh-ed25519";
  tk.fp = fp;
  tk.sshBlob = blob;
  tk.pkey.reset(EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(rawPub.data()), rawPub.size()));
  TrustedByFP[fp] = std::move(tk);
  return priv;
}

TEST_F(XrdSecSSHTest, AuthenticateSecondInitReplacesPendingChallenge)
{
  std::string blob, fp;
  EVP_PKEY *priv = RegisterRawEd25519(0x61, "alice", blob, fp);
  ASSERT_NE(priv, nullptr);

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-dup-challenge");
  XrdSecParameters *challenge1 = nullptr, *challenge2 = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *init1 = MakeCred(BuildInitCred("alice", blob));
  ASSERT_EQ(srv.Authenticate(init1, &challenge1, &err), 1);
  ASSERT_NE(challenge1, nullptr);
  delete init1;

  // A second init on the same connection is allowed and invalidates the first
  // nonce (no global EBUSY state any more).
  XrdSecCredentials *init2 = MakeCred(BuildInitCred("alice", blob));
  ASSERT_EQ(srv.Authenticate(init2, &challenge2, &err), 1);
  ASSERT_NE(challenge2, nullptr);
  delete init2;

  std::string n1, f1, n2, f2;
  ASSERT_TRUE(ParseChallenge(challenge1, n1, f1));
  ASSERT_TRUE(ParseChallenge(challenge2, n2, f2));
  EXPECT_NE(n1, n2);

  std::string sigOld;
  ASSERT_TRUE(signData(priv, challengePayload(n1, f1, kTestHost), sigOld));
  XrdSecCredentials *respOld = MakeCred(BuildResponseCred(sigOld));
  EXPECT_LT(srv.Authenticate(respOld, &challenge2, &err), 0);
  delete respOld;

  // The failed response consumed the challenge; re-init and answer correctly.
  delete challenge2; challenge2 = nullptr;
  XrdSecCredentials *init3 = MakeCred(BuildInitCred("alice", blob));
  ASSERT_EQ(srv.Authenticate(init3, &challenge2, &err), 1);
  delete init3;
  ASSERT_TRUE(ParseChallenge(challenge2, n2, f2));
  std::string sigNew;
  ASSERT_TRUE(signData(priv, challengePayload(n2, f2, kTestHost), sigNew));
  XrdSecCredentials *respNew = MakeCred(BuildResponseCred(sigNew));
  EXPECT_EQ(srv.Authenticate(respNew, &challenge2, &err), 0);
  EXPECT_STREQ(srv.Entity.name, "alice");
  delete respNew;

  delete challenge1;
  delete challenge2;
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
  EVP_PKEY_free(priv);
}

// Challenge state is per protocol object: deleting the object (connection
// closed) leaves nothing behind that could block or be consumed by another
// connection with the same transport identifier.
TEST_F(XrdSecSSHTest, PendingChallengeDiesWithProtocolObject)
{
  std::string blob, fp;
  EVP_PKEY *priv = RegisterRawEd25519(0x62, "alice", blob, fp);
  ASSERT_NE(priv, nullptr);

  XrdNetAddrInfo endPoint;
  XrdOucErrInfo err;
  XrdSecParameters *ch1 = nullptr;
  {
    auto *p1 = new XrdSecProtocolssh("localhost", endPoint);
    p1->Entity.tident = "alice.1:23@client";
    XrdSecCredentials *c1 = MakeCred(BuildInitCred("alice", blob));
    ASSERT_EQ(p1->Authenticate(c1, &ch1, &err), 1);
    delete c1;
    p1->Delete();
  }
  std::string n1, f1;
  ASSERT_TRUE(ParseChallenge(ch1, n1, f1));

  XrdSecProtocolssh p2("localhost", endPoint);
  p2.Entity.tident = "alice.1:23@client";
  // Old challenge cannot be redeemed on the new connection ...
  std::string sig;
  ASSERT_TRUE(signData(priv, challengePayload(n1, f1, kTestHost), sig));
  XrdSecCredentials *stale = MakeCred(BuildResponseCred(sig));
  EXPECT_LT(p2.Authenticate(stale, &ch1, &err), 0);
  delete stale;
  // ... and a fresh init is not blocked by it.
  XrdSecParameters *ch2 = nullptr;
  XrdSecCredentials *c2 = MakeCred(BuildInitCred("alice", blob));
  EXPECT_EQ(p2.Authenticate(c2, &ch2, &err), 1);
  delete c2;
  delete ch1;
  delete ch2;
  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, AuthenticateFullHandshakeSuccess)
{
  std::string rawPriv(32, '\0');
  for (size_t i = 0; i < rawPriv.size(); ++i)
    rawPriv[i] = static_cast<char>(0x40 + i);

  EVP_PKEY *priv = EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(rawPriv.data()), rawPriv.size());
  ASSERT_NE(priv, nullptr);

  size_t pubLen = 32;
  std::string rawPub(32, '\0');
  ASSERT_EQ(EVP_PKEY_get_raw_public_key(
                priv,
                reinterpret_cast<unsigned char *>(&rawPub[0]),
                &pubLen),
            1);
  ASSERT_EQ(pubLen, static_cast<size_t>(32));
  rawPub.resize(pubLen);

  EVP_PKEY *pub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(rawPub.data()), rawPub.size());
  ASSERT_NE(pub, nullptr);

  std::string blob = makeEd25519SshBlob(rawPub);
  ASSERT_FALSE(blob.empty());
  std::string fp;
  ASSERT_TRUE(sha256Base64(blob, fp));

  TrustedKey tk;
  tk.user = "carol";
  tk.alg = "ssh-ed25519";
  tk.fp = fp;
  tk.sshBlob = blob;
  tk.pkey.reset(pub);
  TrustedByFP[fp] = std::move(tk);

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-success");
  XrdSecParameters *challenge = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *initCred = MakeCred(BuildInitCred("carol", blob));
  ASSERT_NE(initCred, nullptr);
  ASSERT_EQ(srv.Authenticate(initCred, &challenge, &err), 1);
  ASSERT_NE(challenge, nullptr);
  delete initCred;

  std::string nonce, chFp;
  ASSERT_TRUE(ParseChallenge(challenge, nonce, chFp));
  ASSERT_EQ(nonce.size(), static_cast<size_t>(32));
  ASSERT_EQ(chFp, fp);

  std::string payload = challengePayload(nonce, chFp, kTestHost);
  std::string sig;
  ASSERT_TRUE(signData(priv, payload, sig));
  ASSERT_FALSE(sig.empty());

  XrdSecCredentials *respCred = MakeCred(BuildResponseCred(sig));
  ASSERT_NE(respCred, nullptr);
  EXPECT_EQ(srv.Authenticate(respCred, &challenge, &err), 0);
  EXPECT_NE(srv.Entity.name, nullptr);
  EXPECT_STREQ(srv.Entity.name, "carol");
  delete respCred;

  if (challenge) {delete challenge; challenge = nullptr;}
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, AuthenticateRejectsUntrustedRawKey)
{
  std::string raw(32, 'Z');
  std::string blob = makeEd25519SshBlob(raw);
  ASSERT_FALSE(blob.empty());

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-untrusted");
  XrdSecParameters *outParms = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *initCred = MakeCred(BuildInitCred("eve", blob));
  ASSERT_NE(initCred, nullptr);
  EXPECT_LT(srv.Authenticate(initCred, &outParms, &err), 0);
  EXPECT_EQ(outParms, nullptr);
  delete initCred;

  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
}

TEST_F(XrdSecSSHTest, AuthenticateRejectsBadResponseSignature)
{
  std::string rawPub;
  EVP_PKEY *priv = MakeEd25519FromSeed(0x50, rawPub);
  ASSERT_NE(priv, nullptr);
  EVP_PKEY *pub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(rawPub.data()), rawPub.size());
  ASSERT_NE(pub, nullptr);

  std::string blob = makeEd25519SshBlob(rawPub);
  std::string fp;
  ASSERT_TRUE(sha256Base64(blob, fp));

  TrustedKey tk;
  tk.user = "carol";
  tk.alg = "ssh-ed25519";
  tk.fp = fp;
  tk.sshBlob = blob;
  tk.pkey.reset(pub); // owned by map / freed in TearDown
  TrustedByFP[fp] = std::move(tk);

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-badsig");
  XrdSecParameters *challenge = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *initCred = MakeCred(BuildInitCred("carol", blob));
  ASSERT_NE(initCred, nullptr);
  ASSERT_EQ(srv.Authenticate(initCred, &challenge, &err), 1);
  ASSERT_NE(challenge, nullptr);
  delete initCred;

  // Sign the wrong payload: a valid ed25519 signature that does not match the
  // server's challenge must be rejected.
  std::string wrongPayload = challengePayload("not-the-real-nonce", fp, kTestHost);
  std::string sig;
  ASSERT_TRUE(signData(priv, wrongPayload, sig));
  XrdSecCredentials *respCred = MakeCred(BuildResponseCred(sig));
  ASSERT_NE(respCred, nullptr);
  EXPECT_LT(srv.Authenticate(respCred, &challenge, &err), 0);
  EXPECT_STRNE(srv.Entity.name, "carol");
  delete respCred;

  if (challenge) {delete challenge; challenge = nullptr;}
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, SafeReadFileRejectsSymlink)
{
  std::string target = TempFilePath("symlink-target");
  std::string link = TempFilePath("symlink");
  WriteFile(target, "trusted\n", 0600);
  unlink(link.c_str());
  ASSERT_EQ(symlink(target.c_str(), link.c_str()), 0);

  SafeFileResult sfr;
  std::string emsg;
  EXPECT_FALSE(safeReadFile(link.c_str(), sfr, emsg));
  EXPECT_FALSE(sfr.found);

  unlink(link.c_str());
  unlink(target.c_str());
}

TEST_F(XrdSecSSHTest, SafeReadFileRejectsNonRegular)
{
  SafeFileResult sfr;
  std::string emsg;
  EXPECT_FALSE(safeReadFile("/", sfr, emsg));
  EXPECT_NE(emsg.find("not regular"), std::string::npos);
}

TEST_F(XrdSecSSHTest, CertValidateAcceptsTrustedCa)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  ASSERT_FALSE(caSshBlob.empty());
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, 0,
      0xFFFFFFFFFFFFFFFFULL);
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_TRUE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                               fp, emsg)) << emsg;
  EXPECT_EQ(mappedUser, "dave");
  EXPECT_EQ(verifyAlg, "ssh-ed25519");
  EXPECT_FALSE(verifyBlob.empty());

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, CertValidateRejectsUntrustedCa)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);

  // Register a *different* CA so the trust store is non-empty but does not
  // contain the signer.
  std::string decoyRawPub;
  EVP_PKEY *decoyPriv = MakeEd25519FromSeed(0x33, decoyRawPub);
  ASSERT_NE(decoyPriv, nullptr);
  std::string decoySshBlob = makeEd25519SshBlob(decoyRawPub);
  EVP_PKEY *decoyPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(decoyRawPub.data()),
      decoyRawPub.size());
  ASSERT_NE(decoyPub, nullptr);
  RegisterTrustedCA(decoySshBlob, decoyPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, 0, 0xFFFFFFFFFFFFFFFFULL);
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                                fp, emsg));
  EXPECT_NE(emsg.find("not trusted"), std::string::npos);

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(decoyPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, CertValidateRejectsExpired)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  uint64_t now = static_cast<uint64_t>(time(nullptr));
  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, now - 7200, now - 3600);
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                                fp, emsg));
  EXPECT_NE(emsg.find("expired"), std::string::npos);

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, CertValidateRejectsWrongType)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  // certType 2 == host certificate, must be rejected for user auth.
  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, 0, 0xFFFFFFFFFFFFFFFFULL, 2);
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                                fp, emsg));
  EXPECT_NE(emsg.find("user certificate"), std::string::npos);

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, CertValidateRejectsTamperedSignature)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, 0, 0xFFFFFFFFFFFFFFFFULL);
  ASSERT_FALSE(cert.empty());
  // Flip a bit in the trailing signature.
  cert[cert.size() - 1] ^= 0x01;

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                                fp, emsg));
  EXPECT_NE(emsg.find("signature validation failed"), std::string::npos);

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, CertValidateRejectsUserNotInPrincipals)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, 0, 0xFFFFFFFFFFFFFFFFULL);
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "mallory", mappedUser, verifyAlg,
                                verifyBlob, fp, emsg));
  EXPECT_NE(emsg.find("principals"), std::string::npos);

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, AuthenticateFullCertHandshakeSuccess)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, 0, 0xFFFFFFFFFFFFFFFFULL);
  ASSERT_FALSE(cert.empty());

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-cert");
  XrdSecParameters *challenge = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *initCred = MakeCred(BuildInitCred("dave", cert));
  ASSERT_NE(initCred, nullptr);
  ASSERT_EQ(srv.Authenticate(initCred, &challenge, &err), 1);
  ASSERT_NE(challenge, nullptr);
  delete initCred;

  std::string nonce, chFp;
  ASSERT_TRUE(ParseChallenge(challenge, nonce, chFp));

  std::string payload = challengePayload(nonce, chFp, kTestHost);
  std::string sig;
  ASSERT_TRUE(signData(userPriv, payload, sig));

  XrdSecCredentials *respCred = MakeCred(BuildResponseCred(sig));
  ASSERT_NE(respCred, nullptr);
  EXPECT_EQ(srv.Authenticate(respCred, &challenge, &err), 0);
  EXPECT_NE(srv.Entity.name, nullptr);
  EXPECT_STREQ(srv.Entity.name, "dave");
  delete respCred;

  if (challenge) {delete challenge; challenge = nullptr;}
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, Ed25519BlobRejectsMalformed)
{
  std::string out;

  // Too short to even hold the first length prefix.
  EXPECT_FALSE(extractEd25519RawFromSshBlob("ab", out));

  // Oversized algorithm-name length must be rejected without wrapping (the
  // 32-bit length 0xFFFFFFFF would wrap to a small value under 32-bit math).
  std::string overflow;
  AppendU32BE(overflow, 0xFFFFFFFFu);
  overflow += "ssh-ed25519";
  EXPECT_FALSE(extractEd25519RawFromSshBlob(overflow, out));

  // Correct framing but a non-ed25519 algorithm name.
  std::string wrongAlg;
  appendSshString(wrongAlg, "ssh-rsa");
  appendSshString(wrongAlg, std::string(32, 'A'));
  EXPECT_FALSE(extractEd25519RawFromSshBlob(wrongAlg, out));

  // Correct algorithm but a key body that is not exactly 32 bytes.
  std::string wrongLen;
  appendSshString(wrongLen, "ssh-ed25519");
  appendSshString(wrongLen, std::string(31, 'A'));
  EXPECT_FALSE(extractEd25519RawFromSshBlob(wrongLen, out));

  // Well-formed blob round-trips.
  std::string good = makeEd25519SshBlob(std::string(32, 'K'));
  ASSERT_FALSE(good.empty());
  EXPECT_TRUE(extractEd25519RawFromSshBlob(good, out));
  EXPECT_EQ(out, std::string(32, 'K'));
}

TEST_F(XrdSecSSHTest, AuthenticateFullHandshakeSuccessRsa)
{
  EVP_PKEY *priv = LoadTestRsaPrivateKey();
  ASSERT_NE(priv, nullptr);

  std::string blob;
  ASSERT_TRUE(makeSshRsaBlobFromPkey(priv, blob));
  std::string nBin, eBin;
  ASSERT_TRUE(extractRsaNEFromSshBlob(blob, nBin, eBin));
  EvpPkeyPtr pub = makeRSAPublicKeyFromNE(nBin, eBin);
  ASSERT_TRUE(pub);

  std::string fp;
  ASSERT_TRUE(sha256Base64(blob, fp));

  TrustedKey tk;
  tk.user = "rsauser";
  tk.alg = "ssh-rsa";
  tk.fp = fp;
  tk.sshBlob = blob;
  tk.pkey = std::move(pub);
  TrustedByFP[fp] = std::move(tk);

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-rsa");
  XrdSecParameters *challenge = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *initCred = MakeCred(BuildInitCred("rsauser", blob));
  ASSERT_NE(initCred, nullptr);
  ASSERT_EQ(srv.Authenticate(initCred, &challenge, &err), 1);
  ASSERT_NE(challenge, nullptr);
  delete initCred;

  std::string nonce, chFp;
  ASSERT_TRUE(ParseChallenge(challenge, nonce, chFp));

  std::string payload = challengePayload(nonce, chFp, kTestHost);
  std::string sig;
  ASSERT_TRUE(signData(priv, payload, sig));

  XrdSecCredentials *respCred = MakeCred(BuildResponseCred(sig));
  ASSERT_NE(respCred, nullptr);
  EXPECT_EQ(srv.Authenticate(respCred, &challenge, &err), 0);
  EXPECT_NE(srv.Entity.name, nullptr);
  EXPECT_STREQ(srv.Entity.name, "rsauser");
  delete respCred;

  if (challenge) {delete challenge; challenge = nullptr;}
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, AuthenticateRejectsOversizedCredential)
{
  const int savedMax = MaxCredSize.load();
  MaxCredSize.store(16); // smaller than any well-formed init credential

  std::string blob = makeEd25519SshBlob(std::string(32, 'R'));
  ASSERT_FALSE(blob.empty());

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-oversize");
  XrdSecParameters *outParms = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *initCred = MakeCred(BuildInitCred("alice", blob));
  ASSERT_NE(initCred, nullptr);
  ASSERT_GT(initCred->size, MaxCredSize.load());
  EXPECT_LT(srv.Authenticate(initCred, &outParms, &err), 0);
  EXPECT_EQ(outParms, nullptr);
  delete initCred;

  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
  MaxCredSize.store(savedMax);
}

TEST_F(XrdSecSSHTest, CertValidateRejectsNotYetValid)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  uint64_t now = static_cast<uint64_t>(time(nullptr));
  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, now + 3600,
      0xFFFFFFFFFFFFFFFFULL);
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                                fp, emsg));
  EXPECT_NE(emsg.find("not yet valid"), std::string::npos);

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, CertValidateRejectsCriticalOptions)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  // Build a critical-options blob like OpenSSH (force-command) and confirm the
  // server fails closed on any unsupported critical option.
  std::string critical;
  appendSshString(critical, "force-command");
  std::string optVal;
  appendSshString(optVal, "/bin/false");
  appendSshString(critical, optVal);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, 0, 0xFFFFFFFFFFFFFFFFULL, 1,
      "test-key-id", critical);
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                                fp, emsg));
  EXPECT_NE(emsg.find("critical options"), std::string::npos);

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, CertValidateEmptyPrincipalsActsAsWildcardWhenAllowed)
{
  // A certificate with an empty principals list is, per OpenSSH semantics,
  // valid for any requested user. That behaviour is opt-in via
  // -allow-empty-principals (the default rejects such certificates, see
  // CertEmptyPrincipalsRejectedByDefault).
  AllowEmptyPrincipals = true;
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {}, 0, 0xFFFFFFFFFFFFFFFFULL);
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_TRUE(validateUserCert(cert, "anyone", mappedUser, verifyAlg, verifyBlob,
                               fp, emsg)) << emsg;
  EXPECT_EQ(mappedUser, "anyone");

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, PrincipalMappingResolvesLocalUser)
{
  // Resolve a known-valid local account (the user running the test).
  std::string localUser;
  ASSERT_TRUE(resolveLocalUser(std::to_string(static_cast<unsigned long long>(
                                   geteuid())),
                               localUser));
  ASSERT_FALSE(localUser.empty());

  // -principal-as-user: principal that names a real local account maps to it.
  PrincipalAsUser = true;
  {
    std::string mapped, method, emsg;
    EXPECT_TRUE(mapPrincipalsToUser({"nosuchprincipal", localUser}, "", mapped,
                                    method, emsg))
        << emsg;
    EXPECT_EQ(mapped, localUser);
    EXPECT_EQ(method, "principal-as-user");
  }
  PrincipalAsUser = false;

  // principal-map-file: explicit principal -> local user mapping wins.
  PrincipalMap["cert-principal"] = localUser;
  {
    std::string mapped, method, emsg;
    EXPECT_TRUE(mapPrincipalsToUser({"cert-principal"}, "", mapped, method, emsg))
        << emsg;
    EXPECT_EQ(mapped, localUser);
    EXPECT_EQ(method, "principal-map-file");
  }

  // A principal that matches nothing is rejected.
  {
    std::string mapped, method, emsg;
    EXPECT_FALSE(mapPrincipalsToUser({"unknown-principal"}, "", mapped, method,
                                     emsg));
    EXPECT_FALSE(emsg.empty());
  }
}

TEST_F(XrdSecSSHTest, LoadTrustedCAKeysParsesPlainAndCertAuthority)
{
  // Two CA keys: one plain "<alg> <key>" line and one prefixed with the
  // OpenSSH "cert-authority" marker that the docs say is accepted.
  std::string blobA = makeEd25519SshBlob(std::string(32, 'C'));
  std::string blobB = makeEd25519SshBlob(std::string(32, 'D'));
  ASSERT_FALSE(blobA.empty());
  ASSERT_FALSE(blobB.empty());

  std::string content;
  content += "ssh-ed25519 " + B64Encode(blobA) + " ca-one\n";
  content += "cert-authority ssh-ed25519 " + B64Encode(blobB) + " ca-two\n";
  std::string caPath = TempFilePath("ca-keys");
  WriteFile(caPath, content, 0600);

  CAKeysFile = caPath;
  std::string emsg;
  ASSERT_TRUE(loadTrustedCAKeys(emsg)) << emsg;
  EXPECT_EQ(TrustedCAByFP.size(), static_cast<size_t>(2));

  std::string fpA, fpB;
  ASSERT_TRUE(sha256Base64(blobA, fpA));
  ASSERT_TRUE(sha256Base64(blobB, fpB));
  EXPECT_TRUE(TrustedCAByFP.find(fpA) != TrustedCAByFP.end());
  EXPECT_TRUE(TrustedCAByFP.find(fpB) != TrustedCAByFP.end());

  unlink(caPath.c_str());
}

TEST_F(XrdSecSSHTest, AuthenticateRejectsRawKeyUserMismatch)
{
  // A trusted key is mapped to "alice"; a client asking to be "bob" with that
  // same key must be rejected (key/username binding is enforced).
  std::string raw(32, 'M');
  std::string blob = makeEd25519SshBlob(raw);
  ASSERT_FALSE(blob.empty());
  std::string fp;
  ASSERT_TRUE(sha256Base64(blob, fp));

  EVP_PKEY *pub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(raw.data()), raw.size());
  ASSERT_NE(pub, nullptr);

  TrustedKey tk;
  tk.user = "alice";
  tk.alg = "ssh-ed25519";
  tk.fp = fp;
  tk.sshBlob = blob;
  tk.pkey.reset(pub);
  TrustedByFP[fp] = std::move(tk);

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-usermismatch");
  XrdSecParameters *outParms = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *initCred = MakeCred(BuildInitCred("bob", blob));
  ASSERT_NE(initCred, nullptr);
  EXPECT_LT(srv.Authenticate(initCred, &outParms, &err), 0);
  EXPECT_EQ(outParms, nullptr);
  delete initCred;

  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
}

TEST_F(XrdSecSSHTest, AuthenticateRejectsUnsupportedOpCode)
{
  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-badop");

  std::string buf;
  WireHdr h;
  memcpy(h.id, "ssh", 4);
  h.ver = kProtoVersion;
  h.op = 'X'; // neither OpInit, OpChallenge nor OpResponse
  h.rsvd[0] = h.rsvd[1] = 0;
  buf.append(reinterpret_cast<const char *>(&h), sizeof(h));

  XrdSecCredentials *cred = MakeCred(buf);
  ASSERT_NE(cred, nullptr);
  XrdSecParameters *outParms = nullptr;
  XrdOucErrInfo err;
  EXPECT_LT(srv.Authenticate(cred, &outParms, &err), 0);
  EXPECT_EQ(outParms, nullptr);
  delete cred;

  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
}

TEST_F(XrdSecSSHTest, ReadBlobRejectsTruncatedAndOversizedLength)
{
  // Missing the 4-byte length prefix entirely.
  {
    std::string buf("ab");
    size_t at = 0;
    std::string out;
    EXPECT_FALSE(readBlob(buf, at, out));
  }
  // Per-field wire limit (64 KiB) is enforced before allocation.
  {
    std::string buf;
    AppendU32BE(buf, static_cast<uint32_t>(kMaxSshWireFieldLen + 1));
    buf.append(8, 'x');
    size_t at = 0;
    std::string out;
    EXPECT_FALSE(readBlob(buf, at, out));
  }
  // Length prefix promises more bytes than are present; must not over-read.
  {
    std::string buf;
    AppendU32BE(buf, 0xFFFFFFFFu);
    buf += "short";
    size_t at = 0;
    std::string out;
    EXPECT_FALSE(readBlob(buf, at, out));
  }
  // Well-formed blob round-trips and advances the cursor.
  {
    std::string buf;
    appendSshString(buf, "hello");
    size_t at = 0;
    std::string out;
    EXPECT_TRUE(readBlob(buf, at, out));
    EXPECT_EQ(out, "hello");
    EXPECT_EQ(at, buf.size());
  }
}

TEST_F(XrdSecSSHTest, CertValidateAcceptsRsaSubjectWithEd25519Ca)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  EVP_PKEY *userPriv = LoadTestRsaPrivateKey();
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildRsaUserCert(caPriv, caSshBlob, userPriv, {"dave"}, 0,
                                      0xFFFFFFFFFFFFFFFFULL, "ssh-ed25519");
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_TRUE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                               fp, emsg)) << emsg;
  EXPECT_EQ(mappedUser, "dave");
  EXPECT_EQ(verifyAlg, "ssh-rsa");
  EXPECT_FALSE(verifyBlob.empty());

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, AuthenticateFullCertHandshakeSuccessRsaSubject)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x11, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  EVP_PKEY *userPriv = LoadTestRsaPrivateKey();
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildRsaUserCert(caPriv, caSshBlob, userPriv, {"erin"}, 0,
                                      0xFFFFFFFFFFFFFFFFULL, "ssh-ed25519");
  ASSERT_FALSE(cert.empty());

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-rsa-cert");
  XrdSecParameters *challenge = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *initCred = MakeCred(BuildInitCred("erin", cert));
  ASSERT_NE(initCred, nullptr);
  ASSERT_EQ(srv.Authenticate(initCred, &challenge, &err), 1);
  ASSERT_NE(challenge, nullptr);
  delete initCred;

  std::string nonce, chFp;
  ASSERT_TRUE(ParseChallenge(challenge, nonce, chFp));

  std::string payload = challengePayload(nonce, chFp, kTestHost);
  std::string sig;
  ASSERT_TRUE(signData(userPriv, payload, sig));

  XrdSecCredentials *respCred = MakeCred(BuildResponseCred(sig));
  ASSERT_NE(respCred, nullptr);
  EXPECT_EQ(srv.Authenticate(respCred, &challenge, &err), 0);
  EXPECT_NE(srv.Entity.name, nullptr);
  EXPECT_STREQ(srv.Entity.name, "erin");
  delete respCred;

  if (challenge) {delete challenge; challenge = nullptr;}
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, CertPrincipalAsUserMappingAndMismatch)
{
  std::string localUser;
  ASSERT_TRUE(resolveLocalUser(
      std::to_string(static_cast<unsigned long long>(geteuid())), localUser));
  ASSERT_FALSE(localUser.empty());

  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {localUser}, 0, 0xFFFFFFFFFFFFFFFFULL);
  ASSERT_FALSE(cert.empty());

  PrincipalAsUser = true;

  // With no requested user, the principal maps to the local account.
  {
    std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
    EXPECT_TRUE(validateUserCert(cert, "", mappedUser, verifyAlg, verifyBlob, fp,
                                 emsg)) << emsg;
    EXPECT_EQ(mappedUser, localUser);
  }

  // A requested user that does not match the mapped principal is rejected.
  {
    std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
    EXPECT_FALSE(validateUserCert(cert, "definitely-not-a-user", mappedUser,
                                  verifyAlg, verifyBlob, fp, emsg));
    EXPECT_NE(emsg.find("does not match mapped principal"), std::string::npos);
  }

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, PrincipalMapFileLoadsAndHotReloads)
{
  std::string localUser;
  ASSERT_TRUE(resolveLocalUser(
      std::to_string(static_cast<unsigned long long>(geteuid())), localUser));
  ASSERT_FALSE(localUser.empty());

  std::string mapPath = TempFilePath("principal-map");
  WriteFile(mapPath, "# comment\nalpha " + localUser + "\n", 0600);
  PrincipalMapFile = mapPath;

  std::string emsg;
  ASSERT_TRUE(ensurePrincipalMapFresh(emsg)) << emsg;
  {
    std::string mapped, method, mErr;
    EXPECT_TRUE(mapPrincipalsToUser({"alpha"}, "", mapped, method, mErr)) << mErr;
    EXPECT_EQ(mapped, localUser);
    EXPECT_EQ(method, "principal-map-file");
  }
  // "beta" is not mapped yet.
  {
    std::string mapped, method, mErr;
    EXPECT_FALSE(mapPrincipalsToUser({"beta"}, "", mapped, method, mErr));
  }

  // Rewrite the file with a new principal and force a newer mtime so the
  // inode/mtime freshness check triggers a reload.
  WriteFile(mapPath, "beta " + localUser + "\n", 0600);
  struct stat st;
  ASSERT_EQ(stat(mapPath.c_str(), &st), 0);
  struct timeval times[2];
  times[0].tv_sec = st.st_atime; times[0].tv_usec = 0;
  times[1].tv_sec = st.st_mtime + 10; times[1].tv_usec = 0;
  ASSERT_EQ(utimes(mapPath.c_str(), times), 0);

  ASSERT_TRUE(ensurePrincipalMapFresh(emsg)) << emsg;
  {
    std::string mapped, method, mErr;
    EXPECT_TRUE(mapPrincipalsToUser({"beta"}, "", mapped, method, mErr)) << mErr;
    EXPECT_EQ(mapped, localUser);
  }
  // The old principal is gone after reload.
  {
    std::string mapped, method, mErr;
    EXPECT_FALSE(mapPrincipalsToUser({"alpha"}, "", mapped, method, mErr));
  }

  unlink(mapPath.c_str());
}

TEST_F(XrdSecSSHTest, CertValidateAcceptsRsaCaWithRsaSha2)
{
  // CA key is RSA; certificates signed by an RSA CA use rsa-sha2-256. This
  // exercises the ssh-rsa CA branch (and the rsa-sha2-256 acceptance) that the
  // ed25519-CA tests never reach.
  EVP_PKEY *caPriv = LoadTestRsaPrivateKey();
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob;
  ASSERT_TRUE(makeSshRsaBlobFromPkey(caPriv, caSshBlob));
  std::string caN, caE;
  ASSERT_TRUE(extractRsaNEFromSshBlob(caSshBlob, caN, caE));
  EvpPkeyPtr caPub = makeRSAPublicKeyFromNE(caN, caE);
  ASSERT_TRUE(caPub);
  RegisterTrustedCA(caSshBlob, caPub.release(), "ssh-rsa");

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, 0, 0xFFFFFFFFFFFFFFFFULL, 1,
      "test-key-id", "", "rsa-sha2-256");
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_TRUE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                               fp, emsg)) << emsg;
  EXPECT_EQ(mappedUser, "dave");
  EXPECT_EQ(verifyAlg, "ssh-ed25519");

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, CertValidateRejectsSignatureAlgMismatch)
{
  // An ed25519 CA whose certificate signature blob advertises "ssh-rsa" must be
  // rejected before any signature math runs.
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {"dave"}, 0, 0xFFFFFFFFFFFFFFFFULL, 1,
      "test-key-id", "", "ssh-rsa");
  ASSERT_FALSE(cert.empty());

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                                fp, emsg));
  EXPECT_NE(emsg.find("signature algorithm mismatch"), std::string::npos);

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, CertValidateRejectsUnsupportedCertAlg)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  // A well-formed blob that is not one of the supported certificate algorithms.
  std::string cert;
  appendSshString(cert, "ssh-dss-cert-v01@openssh.com");
  appendSshString(cert, std::string(16, '\x01'));

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                                fp, emsg));
  EXPECT_NE(emsg.find("Unsupported SSH certificate algorithm"),
            std::string::npos);

  EVP_PKEY_free(caPriv);
}

TEST_F(XrdSecSSHTest, CertValidateRejectsTruncatedBody)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  // Only the algorithm, nonce and subject public key are present; the body
  // fields (serial, type, ...) are missing.
  std::string cert;
  appendSshString(cert, "ssh-ed25519-cert-v01@openssh.com");
  appendSshString(cert, std::string(16, '\x01'));
  appendSshString(cert, userRawPub);

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "dave", mappedUser, verifyAlg, verifyBlob,
                                fp, emsg));
  EXPECT_NE(emsg.find("Malformed SSH certificate body"), std::string::npos);

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, B64DecodeEdgeCases)
{
  std::string out;

  // Empty input is rejected.
  EXPECT_FALSE(b64Decode("", out));

  // Padding-only input carries no data and is rejected.
  EXPECT_FALSE(b64Decode("====", out));

  // Invalid base64 alphabet is rejected.
  EXPECT_FALSE(b64Decode("@@@@", out));

  // Round-trips real data, including a length that needs padding.
  std::string encoded = B64Encode("hello");
  ASSERT_FALSE(encoded.empty());
  ASSERT_TRUE(b64Decode(encoded, out));
  EXPECT_EQ(out, "hello");
}

TEST_F(XrdSecSSHTest, ExtractRsaNERejectsMalformed)
{
  std::string nBin, eBin;

  // Wrong algorithm name.
  {
    std::string blob;
    appendSshString(blob, "ssh-ed25519");
    appendSshString(blob, std::string(3, '\x01'));
    appendSshString(blob, std::string(8, '\x02'));
    EXPECT_FALSE(extractRsaNEFromSshBlob(blob, nBin, eBin));
  }

  // Trailing garbage after a well-formed ssh-rsa blob.
  {
    EVP_PKEY *rsa = LoadTestRsaPrivateKey();
    ASSERT_NE(rsa, nullptr);
    std::string blob;
    ASSERT_TRUE(makeSshRsaBlobFromPkey(rsa, blob));
    blob.push_back('\x00');
    EXPECT_FALSE(extractRsaNEFromSshBlob(blob, nBin, eBin));
    EVP_PKEY_free(rsa);
  }

  // A well-formed blob parses cleanly.
  {
    EVP_PKEY *rsa = LoadTestRsaPrivateKey();
    ASSERT_NE(rsa, nullptr);
    std::string blob;
    ASSERT_TRUE(makeSshRsaBlobFromPkey(rsa, blob));
    EXPECT_TRUE(extractRsaNEFromSshBlob(blob, nBin, eBin));
    EXPECT_FALSE(nBin.empty());
    EXPECT_FALSE(eBin.empty());
    EVP_PKEY_free(rsa);
  }
}

TEST_F(XrdSecSSHTest, AuthenticateRejectsReplayedResponse)
{
  std::string rawPriv(32, '\0');
  for (size_t i = 0; i < rawPriv.size(); ++i)
    rawPriv[i] = static_cast<char>(0x70 + i);

  EVP_PKEY *priv = EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(rawPriv.data()), rawPriv.size());
  ASSERT_NE(priv, nullptr);

  size_t pubLen = 32;
  std::string rawPub(32, '\0');
  ASSERT_EQ(EVP_PKEY_get_raw_public_key(
                priv, reinterpret_cast<unsigned char *>(&rawPub[0]), &pubLen),
            1);
  rawPub.resize(pubLen);

  EVP_PKEY *pub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(rawPub.data()), rawPub.size());
  ASSERT_NE(pub, nullptr);

  std::string blob = makeEd25519SshBlob(rawPub);
  ASSERT_FALSE(blob.empty());
  std::string fp;
  ASSERT_TRUE(sha256Base64(blob, fp));

  TrustedKey tk;
  tk.user = "carol";
  tk.alg = "ssh-ed25519";
  tk.fp = fp;
  tk.sshBlob = blob;
  tk.pkey.reset(pub); // owned by map / freed in TearDown
  TrustedByFP[fp] = std::move(tk);

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = strdup("tid-replay-e2e");
  XrdSecParameters *challenge = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *initCred = MakeCred(BuildInitCred("carol", blob));
  ASSERT_NE(initCred, nullptr);
  ASSERT_EQ(srv.Authenticate(initCred, &challenge, &err), 1);
  ASSERT_NE(challenge, nullptr);
  delete initCred;

  std::string nonce, chFp;
  ASSERT_TRUE(ParseChallenge(challenge, nonce, chFp));

  std::string payload = challengePayload(nonce, chFp, kTestHost);
  std::string sig;
  ASSERT_TRUE(signData(priv, payload, sig));

  // First use succeeds.
  std::string respBytes = BuildResponseCred(sig);
  XrdSecCredentials *respCred = MakeCred(respBytes);
  ASSERT_NE(respCred, nullptr);
  EXPECT_EQ(srv.Authenticate(respCred, &challenge, &err), 0);
  EXPECT_STREQ(srv.Entity.name, "carol");
  delete respCred;

  // Replaying the exact same response must fail: the pending challenge was
  // consumed (single-use), so there is nothing to verify against.
  XrdSecCredentials *replayCred = MakeCred(respBytes);
  ASSERT_NE(replayCred, nullptr);
  EXPECT_LT(srv.Authenticate(replayCred, &challenge, &err), 0);
  delete replayCred;

  if (challenge) {delete challenge; challenge = nullptr;}
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, InitRejectsInvalidOptionRanges)
{
  XrdOucErrInfo err;

  EXPECT_EQ(XrdSecProtocolsshInit('s', "-maxsz 0", &err), nullptr);
  EXPECT_EQ(XrdSecProtocolsshInit('s', "-maxsz 524289", &err), nullptr);
  EXPECT_EQ(XrdSecProtocolsshInit('s', "-nonce-ttl 0", &err), nullptr);
  EXPECT_EQ(XrdSecProtocolsshInit('s', "-nonce-ttl 601", &err), nullptr);
  EXPECT_EQ(XrdSecProtocolsshInit('s', "-bogus-option", &err), nullptr);
}

TEST_F(XrdSecSSHTest, InitParsesValidOptions)
{
  const int savedMax = MaxCredSize.load();
  const int savedTtl = NonceTTL.load();

  // A valid keys-file is required for server-mode init to succeed.
  std::string raw(32, 'I');
  std::string blob = makeEd25519SshBlob(raw);
  ASSERT_FALSE(blob.empty());
  WriteFile(keysPath, "ivan ssh-ed25519 " + B64Encode(blob) + "\n", 0600);
  KeysFile = keysPath;

  XrdOucErrInfo err;
  char *info = XrdSecProtocolsshInit('s', "-maxsz 4096 -nonce-ttl 45", &err);
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(MaxCredSize.load(), 4096);
  EXPECT_EQ(NonceTTL.load(), 45);
  free(info);

  MaxCredSize.store(savedMax);
  NonceTTL.store(savedTtl);
}

TEST_F(XrdSecSSHTest, InitAllowsCertOnlyWithoutKeysFile)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x20, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caBlob = makeEd25519SshBlob(caRawPub);
  ASSERT_FALSE(caBlob.empty());

  const std::string caPath = TempFilePath("ca-only");
  WriteFile(caPath, "ssh-ed25519 " + B64Encode(caBlob) + "\n", 0600);

  KeysFile = TempFilePath("missing-keys");
  CAKeysFile.clear();

  XrdOucErrInfo err;
  const std::string parms = "-ca-keys-file " + caPath;
  char *info = XrdSecProtocolsshInit('s', parms.c_str(), &err);
  ASSERT_NE(info, nullptr) << (err.getErrText() ? err.getErrText() : "init failed");
  EXPECT_TRUE(TrustedByFP.empty());
  EXPECT_EQ(TrustedCAByFP.size(), 1u);
  free(info);

  EVP_PKEY_free(caPriv);
  unlink(caPath.c_str());
}

TEST_F(XrdSecSSHTest, InitAllowsEmptyKeysFileWhenCaKeysConfigured)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x21, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caBlob = makeEd25519SshBlob(caRawPub);
  const std::string caPath = TempFilePath("ca-keys");
  WriteFile(caPath, "ssh-ed25519 " + B64Encode(caBlob) + "\n", 0600);
  WriteFile(keysPath, "# no raw keys here\n", 0600);

  KeysFile = keysPath;
  CAKeysFile.clear();

  XrdOucErrInfo err;
  const std::string parms = "-ca-keys-file " + caPath;
  char *info = XrdSecProtocolsshInit('s', parms.c_str(), &err);
  ASSERT_NE(info, nullptr);
  EXPECT_TRUE(TrustedByFP.empty());
  EXPECT_EQ(TrustedCAByFP.size(), 1u);
  free(info);

  EVP_PKEY_free(caPriv);
  unlink(caPath.c_str());
}

TEST_F(XrdSecSSHTest, CertValidateRejectsEmptyPrincipalsWithPrincipalAsUser)
{
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);

  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  ASSERT_NE(userPriv, nullptr);

  std::string cert = BuildEd25519UserCert(
      caPriv, caSshBlob, userRawPub, {}, 0, 0xFFFFFFFFFFFFFFFFULL);
  ASSERT_FALSE(cert.empty());

  PrincipalAsUser = true;
  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "alice", mappedUser, verifyAlg, verifyBlob,
                              fp, emsg));
  EXPECT_NE(emsg.find("principals are required"), std::string::npos);

  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, ClientRejectsChallengeFingerprintMismatch)
{
  const std::string keyPath = TempFilePath("client.pem");
  EVP_PKEY *priv = LoadTestRsaPrivateKey();
  ASSERT_NE(priv, nullptr);
  ASSERT_TRUE(WritePrivateKeyPem(keyPath, priv));
  ASSERT_EQ(chmod(keyPath.c_str(), 0600), 0);

  const char *oldKey = getenv("XRD_SSH_KEY_FILE");
  std::string savedKey = oldKey ? oldKey : "";
  setenv("XRD_SSH_KEY_FILE", keyPath.c_str(), 1);
  setenv("XRD_SSH_USER", "alice", 1);

  bool aOK = false;
  XrdOucErrInfo err;
  XrdSecProtocolssh cli(kTestHost, "0:8192:", &err, aOK);
  ASSERT_TRUE(aOK);

  const std::string challengeBytes =
      BuildChallengeParams(std::string(32, 'N'), "SHA256:wrongfingerprint");
  XrdSecParameters *challenge = makeParametersFromString(challengeBytes);
  ASSERT_NE(challenge, nullptr);
  EXPECT_EQ(cli.getCredentials(challenge, &err), nullptr);
  delete challenge;

  if (!savedKey.empty()) setenv("XRD_SSH_KEY_FILE", savedKey.c_str(), 1);
  else unsetenv("XRD_SSH_KEY_FILE");
  unsetenv("XRD_SSH_USER");
  unlink(keyPath.c_str());
  EVP_PKEY_free(priv);
}

// ---------------------------------------------------------------------------
// Host binding: a signature is only valid for the host the client connected to.
// ---------------------------------------------------------------------------

TEST_F(XrdSecSSHTest, HostBindingRejectsRelayedSignature)
{
  std::string blob, fp;
  EVP_PKEY *priv = RegisterRawEd25519(0x70, "alice", blob, fp);
  ASSERT_NE(priv, nullptr);

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = "tid-hostbind";
  XrdOucErrInfo err;
  XrdSecParameters *challenge = nullptr;

  auto initAndParse = [&](std::string &nonce, std::string &chFp)
    {
      delete challenge; challenge = nullptr;
      XrdSecCredentials *init = MakeCred(BuildInitCred("alice", blob));
      ASSERT_EQ(srv.Authenticate(init, &challenge, &err), 1);
      delete init;
      ASSERT_TRUE(ParseChallenge(challenge, nonce, chFp));
    };

  // Client believed it was talking to rogue.example.org: the relayed signature
  // is rejected even though it is cryptographically valid for that host.
  {
    std::string nonce, chFp;
    initAndParse(nonce, chFp);
    std::string sig;
    ASSERT_TRUE(signData(priv, challengePayload(nonce, chFp, "rogue.example.org"), sig));
    XrdSecCredentials *resp = MakeCred(BuildResponseCred(sig, "rogue.example.org"));
    EXPECT_LT(srv.Authenticate(resp, &challenge, &err), 0);
    int ec = 0;
    std::string txt = err.getErrText(ec);
    EXPECT_NE(txt.find("different server"), std::string::npos) << txt;
    delete resp;
  }
  // Host in the response that does not match the host that was signed.
  {
    std::string nonce, chFp;
    initAndParse(nonce, chFp);
    std::string sig;
    ASSERT_TRUE(signData(priv, challengePayload(nonce, chFp, "rogue.example.org"), sig));
    XrdSecCredentials *resp = MakeCred(BuildResponseCred(sig, kTestHost));
    EXPECT_LT(srv.Authenticate(resp, &challenge, &err), 0);
    delete resp;
  }
  // Non-normalised host strings are refused.
  {
    std::string nonce, chFp;
    initAndParse(nonce, chFp);
    std::string sig;
    ASSERT_TRUE(signData(priv, challengePayload(nonce, chFp, "LocalHost"), sig));
    XrdSecCredentials *resp = MakeCred(BuildResponseCred(sig, "LocalHost"));
    EXPECT_LT(srv.Authenticate(resp, &challenge, &err), 0);
    delete resp;
  }
  // A -hostnames alias is accepted once configured.
  {
    addAcceptedHost("Alias.Example.Org.");
    std::string nonce, chFp;
    initAndParse(nonce, chFp);
    std::string sig;
    ASSERT_TRUE(signData(priv, challengePayload(nonce, chFp, "alias.example.org"), sig));
    XrdSecCredentials *resp = MakeCred(BuildResponseCred(sig, "alias.example.org"));
    EXPECT_EQ(srv.Authenticate(resp, &challenge, &err), 0);
    EXPECT_STREQ(srv.Entity.name, "alice");
    delete resp;
  }
  delete challenge;
  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, ClientBindsConnectedHostIntoResponse)
{
  const std::string keyPath = TempFilePath("client-ed.pem");
  std::string rawPub;
  EVP_PKEY *priv = MakeEd25519FromSeed(0x71, rawPub);
  ASSERT_NE(priv, nullptr);
  ASSERT_TRUE(WritePrivateKeyPem(keyPath, priv));
  ASSERT_EQ(chmod(keyPath.c_str(), 0600), 0);
  std::string blob = makeEd25519SshBlob(rawPub);
  std::string fp;
  ASSERT_TRUE(sha256Base64(blob, fp));

  setenv("XRD_SSH_KEY_FILE", keyPath.c_str(), 1);
  setenv("XRD_SSH_USER", "alice", 1);
  bool aOK = false;
  XrdOucErrInfo err;
  XrdSecProtocolssh cli("Data1.Example.ORG.", "0:8192:", &err, aOK);
  ASSERT_TRUE(aOK);

  XrdSecCredentials *init = cli.getCredentials(nullptr, &err);
  ASSERT_NE(init, nullptr);
  delete init;

  const std::string nonce(32, 'N');
  XrdSecParameters *challenge = makeParametersFromString(BuildChallengeParams(nonce, fp));
  XrdSecCredentials *resp = cli.getCredentials(challenge, &err);
  ASSERT_NE(resp, nullptr);

  const char *p = resp->buffer + sizeof(WireHdr);
  const char *e = resp->buffer + resp->size;
  uint16_t sLen = 0, hLen = 0;
  ASSERT_TRUE(readU16(p, e, sLen));
  ASSERT_TRUE(readU16(p, e, hLen));
  ASSERT_EQ(e - p, sLen + hLen);
  std::string sig(p, sLen);
  std::string host(p + sLen, hLen);
  EXPECT_EQ(host, "data1.example.org");
  EvpPkeyPtr pub(EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(rawPub.data()), rawPub.size()));
  EXPECT_TRUE(verifyData(pub.get(), challengePayload(nonce, fp, host), sig));

  delete resp;
  delete challenge;
  unsetenv("XRD_SSH_KEY_FILE");
  unsetenv("XRD_SSH_USER");
  unlink(keyPath.c_str());
  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, HostnameNormalisation)
{
  EXPECT_EQ(normalizeHostname("Host.Example.ORG."), "host.example.org");
  EXPECT_EQ(normalizeHostname("[::1]"), "::1");
  EXPECT_EQ(normalizeHostname("10.0.0.1"), "10.0.0.1");
  EXPECT_EQ(normalizeHostname(""), "");
  EXPECT_EQ(normalizeHostname("bad host"), "");
  EXPECT_EQ(normalizeHostname("a|b"), "");
  EXPECT_EQ(normalizeHostname(std::string(300, 'a')), "");
  EXPECT_TRUE(isAcceptedHost("localhost"));
  EXPECT_TRUE(isAcceptedHost("127.0.0.1"));
  EXPECT_FALSE(isAcceptedHost("rogue.example.org"));
}

// ---------------------------------------------------------------------------
// Account policy: empty principals, deny list, requested-user preference.
// ---------------------------------------------------------------------------

void SetupEd25519Ca(EVP_PKEY *&caPriv, std::string &caSshBlob)
{
  std::string caRawPub;
  caPriv = MakeEd25519FromSeed(0x10, caRawPub);
  ASSERT_NE(caPriv, nullptr);
  caSshBlob = makeEd25519SshBlob(caRawPub);
  EVP_PKEY *caPub = EVP_PKEY_new_raw_public_key(
      EVP_PKEY_ED25519, nullptr,
      reinterpret_cast<const unsigned char *>(caRawPub.data()), caRawPub.size());
  ASSERT_NE(caPub, nullptr);
  RegisterTrustedCA(caSshBlob, caPub);
}

TEST_F(XrdSecSSHTest, CertEmptyPrincipalsRejectedByDefault)
{
  EVP_PKEY *caPriv = nullptr;
  std::string caSshBlob;
  SetupEd25519Ca(caPriv, caSshBlob);
  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x90, userRawPub);
  std::string cert = BuildEd25519UserCert(caPriv, caSshBlob, userRawPub, {}, 0,
                                          0xFFFFFFFFFFFFFFFFULL);

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_FALSE(validateUserCert(cert, "alice", mappedUser, verifyAlg, verifyBlob, fp, emsg));
  EXPECT_NE(emsg.find("no principals"), std::string::npos) << emsg;

  // Opt-in restores the OpenSSH wildcard behaviour ...
  AllowEmptyPrincipals = true;
  EXPECT_TRUE(validateUserCert(cert, "alice", mappedUser, verifyAlg, verifyBlob, fp, emsg)) << emsg;
  EXPECT_EQ(mappedUser, "alice");

  // ... but the deny list still blocks root at the Authenticate level.
  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = "tid-root";
  XrdOucErrInfo err;
  XrdSecParameters *challenge = nullptr;
  XrdSecCredentials *init = MakeCred(BuildInitCred("root", cert));
  EXPECT_LT(srv.Authenticate(init, &challenge, &err), 0);
  int ec = 0;
  std::string txt = err.getErrText(ec);
  EXPECT_NE(txt.find("not permitted"), std::string::npos) << txt;
  delete init;
  delete challenge;
  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, DenyUsersAppliesToRawKeys)
{
  std::string blob, fp;
  EVP_PKEY *priv = RegisterRawEd25519(0x72, "svc", blob, fp);
  ASSERT_NE(priv, nullptr);
  DenyUsers = {{"svc", true}};

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = "tid-deny";
  XrdOucErrInfo err;
  XrdSecParameters *challenge = nullptr;
  XrdSecCredentials *init = MakeCred(BuildInitCred("svc", blob));
  EXPECT_LT(srv.Authenticate(init, &challenge, &err), 0);
  delete init;

  DenyUsers.clear();
  XrdSecCredentials *init2 = MakeCred(BuildInitCred("svc", blob));
  EXPECT_EQ(srv.Authenticate(init2, &challenge, &err), 1);
  delete init2;
  delete challenge;
  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, PrincipalAsUserPrefersRequestedPrincipal)
{
  std::string localUser;
  ASSERT_TRUE(resolveLocalUser(
      std::to_string(static_cast<unsigned long long>(geteuid())), localUser));
  EVP_PKEY *caPriv = nullptr;
  std::string caSshBlob;
  SetupEd25519Ca(caPriv, caSshBlob);
  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x91, userRawPub);
  // "root" resolves on every system and is listed first.
  std::string cert = BuildEd25519UserCert(caPriv, caSshBlob, userRawPub,
                                          {"root", localUser}, 0, 0xFFFFFFFFFFFFFFFFULL);
  PrincipalAsUser = true;

  std::string mappedUser, verifyAlg, verifyBlob, fp, emsg;
  EXPECT_TRUE(validateUserCert(cert, localUser, mappedUser, verifyAlg, verifyBlob, fp, emsg)) << emsg;
  EXPECT_EQ(mappedUser, localUser);

  // Without a requested user the first mappable principal is still chosen.
  EXPECT_TRUE(validateUserCert(cert, "", mappedUser, verifyAlg, verifyBlob, fp, emsg)) << emsg;
  EXPECT_EQ(mappedUser, "root");

  // Requesting a user that is not a principal is rejected.
  EXPECT_FALSE(validateUserCert(cert, "definitely-not-a-user", mappedUser, verifyAlg,
                                verifyBlob, fp, emsg));
  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

TEST_F(XrdSecSSHTest, PrincipalMapFilePrefersRequestedPrincipal)
{
  std::string localUser;
  ASSERT_TRUE(resolveLocalUser(
      std::to_string(static_cast<unsigned long long>(geteuid())), localUser));
  std::string mapPath = TempFilePath("principal-map-pref");
  WriteFile(mapPath, "p-root root\np-me " + localUser + "\n", 0600);
  PrincipalMapFile = mapPath;
  std::string emsg;
  ASSERT_TRUE(ensurePrincipalMapFresh(emsg)) << emsg;

  std::string mapped, method;
  EXPECT_TRUE(mapPrincipalsToUser({"p-root", "p-me"}, localUser, mapped, method, emsg)) << emsg;
  EXPECT_EQ(mapped, localUser);
  EXPECT_EQ(method, "principal-map-file");
  EXPECT_TRUE(mapPrincipalsToUser({"p-root", "p-me"}, "", mapped, method, emsg)) << emsg;
  EXPECT_EQ(mapped, "root");
  unlink(mapPath.c_str());
}

// ---------------------------------------------------------------------------
// Information disclosure: client-facing errors are generic, detail is logged.
// ---------------------------------------------------------------------------

TEST_F(XrdSecSSHTest, ClientErrorsDoNotLeakMapping)
{
  std::string blob, fp;
  EVP_PKEY *priv = RegisterRawEd25519(0x73, "secretsvc", blob, fp);
  ASSERT_NE(priv, nullptr);

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = "tid-leak";
  XrdOucErrInfo err;
  XrdSecParameters *challenge = nullptr;

  XrdSecCredentials *init = MakeCred(BuildInitCred("guess", blob));
  EXPECT_LT(srv.Authenticate(init, &challenge, &err), 0);
  int ec = 0;
  std::string txt = err.getErrText(ec);
  EXPECT_EQ(txt.find("secretsvc"), std::string::npos) << txt;
  EXPECT_EQ(txt.find("guess"), std::string::npos) << txt;
  EXPECT_EQ(txt, kGenericAuthFailure);
  delete init;

  std::string otherBlob = makeEd25519SshBlob(std::string(32, 'U'));
  XrdSecCredentials *init2 = MakeCred(BuildInitCred("guess", otherBlob));
  EXPECT_LT(srv.Authenticate(init2, &challenge, &err), 0);
  txt = err.getErrText(ec);
  EXPECT_EQ(txt, kGenericAuthFailure);
  EXPECT_EQ(txt.find("SHA256"), std::string::npos);
  delete init2;

  // An untrusted CA is likewise reported generically.
  std::string caRawPub;
  EVP_PKEY *caPriv = MakeEd25519FromSeed(0x11, caRawPub);
  std::string caSshBlob = makeEd25519SshBlob(caRawPub);
  std::string dummyRaw;
  EVP_PKEY *dummyCa = MakeEd25519FromSeed(0x12, dummyRaw);
  RegisterTrustedCA(makeEd25519SshBlob(dummyRaw),
                    EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
                        reinterpret_cast<const unsigned char *>(dummyRaw.data()), 32));
  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x92, userRawPub);
  std::string cert = BuildEd25519UserCert(caPriv, caSshBlob, userRawPub, {"alice"}, 0,
                                          0xFFFFFFFFFFFFFFFFULL);
  XrdSecCredentials *init3 = MakeCred(BuildInitCred("alice", cert));
  EXPECT_LT(srv.Authenticate(init3, &challenge, &err), 0);
  txt = err.getErrText(ec);
  EXPECT_EQ(txt, kGenericAuthFailure);
  delete init3;

  delete challenge;
  EVP_PKEY_free(priv);
  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(dummyCa);
  EVP_PKEY_free(userPriv);
}

// ---------------------------------------------------------------------------
// Key strength and formats.
// ---------------------------------------------------------------------------

TEST_F(XrdSecSSHTest, RejectsSmallRsaKeys)
{
  // 1024-bit key (the previous test key).
  static const char kSmallPem[] =
      "-----BEGIN PRIVATE KEY-----\n"
      "MIICdQIBADANBgkqhkiG9w0BAQEFAASCAl8wggJbAgEAAoGBAM0Dmz/xOiViLIN9\n"
      "0ySmQzYb/IyeBs3EYoXOQ1t7HyvxpTKYLOE4Iwk6i6gW11HhYhfjOVR+LUChUTl/\n"
      "t7zP5RW5a55e/KsCwOL947e99ryCyZbjF/REJN9pnfvrxXekB0UzGbtqdCFEhHPJ\n"
      "WLao7q4u/eaeNAts7iYaT1TT5pZJAgMBAAECgYAHdfcjZ5L3I1B9ZInXjplpkbEq\n"
      "KOIUgO4Y8n2vCZcD0WJyqekQNSvJPTEx58rkNvCL7//5HDJnZLeBAS3dmC88/3cf\n"
      "+U2skdkNLlwY0x0sqqLXU41rnfnbi51J/QhGZYZcgN85gbMRMdJeKwVqUj609wWY\n"
      "xkFUEnajJmUgxuSeVQJBAO7Ow1Q/3GhgfqwoBFyk0PjRrMVgfD2AT39cJRo3nY2+\n"
      "9PrXu+RFDfpdtlvuAkKLgn+liJmf7GX0JEMQAnC/Z+cCQQDbxgaurgIRq09znzyI\n"
      "7XnID/ZPcO4N/4dHA7u8KmlL+ispy+LfiNIlz5U9zb2PYyXq9u0410eEDlXh84Xo\n"
      "NYpPAkBtG6zk9lSGn+fgUlxD083ikTIF8CJzmwc3YmtVQinLFH8riJvBHMfZJy3l\n"
      "bKY9ry4Nkh0KS6Yfot9agJsM1nbrAkAkyzZ7MC6wfpnCpbogwoFM+T8ndaSlO06O\n"
      "mRVpH0CZs7xeNwA4pFNqeSJnQnal9td2SvjUN1aFyVCfj4GvqqcJAkA4vrMx5sv5\n"
      "/oVqPyU6sZnV7btUDj41Xn8gjJFHA/Kadg0mVBURLKpMa82UiZo9dPg+VLqlQU8x\n"
      "FosiOE9wi0pd\n"
      "-----END PRIVATE KEY-----\n";
  BIO *bio = BIO_new_mem_buf(kSmallPem, -1);
  EVP_PKEY *small = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  ASSERT_NE(small, nullptr);
  ASSERT_LT(EVP_PKEY_bits(small), kMinRsaBits);

  std::string blob;
  ASSERT_TRUE(makeSshRsaBlobFromPkey(small, blob));
  EvpPkeyPtr pk;
  EXPECT_FALSE(makePkeyFromSshBlob("ssh-rsa", blob, pk));

  // Server keys-file: the line is skipped.
  WriteFile(keysPath, "alice ssh-rsa " + B64Encode(blob) + "\n", 0600);
  KeysFile = keysPath;
  std::string emsg;
  EXPECT_FALSE(loadTrustedKeys(emsg));

  // Client side: refused with a clear message.
  const std::string keyPath = TempFilePath("small-rsa.pem");
  ASSERT_TRUE(WritePrivateKeyPem(keyPath, small));
  ASSERT_EQ(chmod(keyPath.c_str(), 0600), 0);
  setenv("XRD_SSH_KEY_FILE", keyPath.c_str(), 1);
  setenv("XRD_SSH_USER", "alice", 1);
  bool aOK = false;
  XrdOucErrInfo err;
  XrdSecProtocolssh cli(kTestHost, "0:8192:", &err, aOK);
  ASSERT_TRUE(aOK);
  EXPECT_EQ(cli.getCredentials(nullptr, &err), nullptr);
  int ec = 0;
  std::string txt = err.getErrText(ec);
  EXPECT_NE(txt.find("2048"), std::string::npos) << txt;
  unsetenv("XRD_SSH_KEY_FILE");
  unsetenv("XRD_SSH_USER");
  unlink(keyPath.c_str());
  EVP_PKEY_free(small);
}

// Unencrypted OpenSSH-format keys (ssh-keygen default) are loadable.
TEST_F(XrdSecSSHTest, ClientLoadsOpenSshFormatKeys)
{
  if (system("command -v ssh-keygen >/dev/null 2>&1") != 0)
    GTEST_SKIP() << "ssh-keygen not available";

  for (const char *type : {"ed25519", "rsa"})
    {
      const std::string keyPath = TempFilePath((std::string("openssh-") + type).c_str());
      unlink(keyPath.c_str());
      unlink((keyPath + ".pub").c_str());
      std::string cmd = std::string("ssh-keygen -q -t ") + type + " -N '' -f " + keyPath
                      + " >/dev/null 2>&1";
      ASSERT_EQ(system(cmd.c_str()), 0);

      // Expected public key blob from the .pub file.
      std::ifstream pub((keyPath + ".pub").c_str());
      std::string alg, b64;
      pub >> alg >> b64;
      std::string wantBlob;
      ASSERT_TRUE(b64Decode(b64, wantBlob));
      std::string wantFp;
      ASSERT_TRUE(sha256Base64(wantBlob, wantFp));

      setenv("XRD_SSH_KEY_FILE", keyPath.c_str(), 1);
      setenv("XRD_SSH_USER", "alice", 1);
      bool aOK = false;
      XrdOucErrInfo err;
      XrdSecProtocolssh cli(kTestHost, "0:65536:", &err, aOK);
      ASSERT_TRUE(aOK);
      XrdSecCredentials *init = cli.getCredentials(nullptr, &err);
      int ec = 0;
      ASSERT_NE(init, nullptr) << type << ": " << err.getErrText(ec);

      // The init credential carries exactly the blob ssh-keygen wrote.
      const char *p = init->buffer + sizeof(WireHdr);
      const char *e = init->buffer + init->size;
      uint16_t uLen = 0, bLen = 0;
      ASSERT_TRUE(readU16(p, e, uLen));
      ASSERT_TRUE(readU16(p, e, bLen));
      std::string gotBlob(p + uLen, bLen);
      EXPECT_EQ(gotBlob, wantBlob) << type;

      // And the key can sign a challenge that the public blob verifies.
      XrdSecParameters *challenge =
          makeParametersFromString(BuildChallengeParams(std::string(32, 'N'), wantFp));
      XrdSecCredentials *resp = cli.getCredentials(challenge, &err);
      ASSERT_NE(resp, nullptr) << type << ": " << err.getErrText(ec);
      p = resp->buffer + sizeof(WireHdr);
      e = resp->buffer + resp->size;
      uint16_t sLen = 0, hLen = 0;
      ASSERT_TRUE(readU16(p, e, sLen));
      ASSERT_TRUE(readU16(p, e, hLen));
      std::string sig(p, sLen);
      EvpPkeyPtr pk;
      ASSERT_TRUE(makePkeyFromSshBlob(alg, wantBlob, pk));
      EXPECT_TRUE(verifyData(pk.get(), challengePayload(std::string(32, 'N'), wantFp, kTestHost), sig));

      delete init;
      delete resp;
      delete challenge;
      unlink(keyPath.c_str());
      unlink((keyPath + ".pub").c_str());
    }
  unsetenv("XRD_SSH_KEY_FILE");
  unsetenv("XRD_SSH_USER");
}

TEST_F(XrdSecSSHTest, EncryptedOpenSshKeyIsRefused)
{
  if (system("command -v ssh-keygen >/dev/null 2>&1") != 0)
    GTEST_SKIP() << "ssh-keygen not available";
  const std::string keyPath = TempFilePath("openssh-enc");
  unlink(keyPath.c_str());
  std::string cmd = "ssh-keygen -q -t ed25519 -N 'secret' -f " + keyPath + " >/dev/null 2>&1";
  ASSERT_EQ(system(cmd.c_str()), 0);
  setenv("XRD_SSH_KEY_FILE", keyPath.c_str(), 1);
  setenv("XRD_SSH_USER", "alice", 1);
  bool aOK = false;
  XrdOucErrInfo err;
  XrdSecProtocolssh cli(kTestHost, "0:8192:", &err, aOK);
  ASSERT_TRUE(aOK);
  EXPECT_EQ(cli.getCredentials(nullptr, &err), nullptr);
  int ec = 0;
  std::string txt = err.getErrText(ec);
  EXPECT_NE(txt.find("encrypted"), std::string::npos) << txt;
  unsetenv("XRD_SSH_KEY_FILE");
  unsetenv("XRD_SSH_USER");
  unlink(keyPath.c_str());
  unlink((keyPath + ".pub").c_str());
}

// ---------------------------------------------------------------------------
// Revocation.
// ---------------------------------------------------------------------------

TEST_F(XrdSecSSHTest, RevocationListParsing)
{
  std::string raw(32, 'V');
  std::string blob = makeEd25519SshBlob(raw);
  std::string fp;
  ASSERT_TRUE(sha256Base64(blob, fp));
  RevocationList rl;
  std::string emsg;
  std::string content = "# comment\nssh-ed25519 " + B64Encode(blob) + " someone@host\n"
                        "serial: 42\nserial:43\nid: my-key-id\nSHA256:abc/def+ghi=\n";
  ASSERT_TRUE(parseRevocationList(content, rl, emsg)) << emsg;
  EXPECT_TRUE(rl.keyFps.count(fp));
  EXPECT_TRUE(rl.serials.count(42));
  EXPECT_TRUE(rl.serials.count(43));
  EXPECT_TRUE(rl.keyIds.count("my-key-id"));
  EXPECT_TRUE(rl.keyFps.count("SHA256:abc/def+ghi"));
  EXPECT_FALSE(parseRevocationList("serial: notanumber\n", rl, emsg));
  EXPECT_FALSE(parseRevocationList("garbage line\n", rl, emsg));
}

TEST_F(XrdSecSSHTest, RevokedRawKeyAndCertAreRejectedWithHotReload)
{
  std::string blob, fp;
  EVP_PKEY *priv = RegisterRawEd25519(0x74, "alice", blob, fp);
  ASSERT_NE(priv, nullptr);

  const std::string revPath = TempFilePath("revoked");
  WriteFile(revPath, "# nothing revoked yet\n", 0600);
  RevokedKeysFile = revPath;
  std::string emsg;
  ASSERT_TRUE(ensureRevocationFresh(emsg)) << emsg;

  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = "tid-revoked";
  XrdOucErrInfo err;
  XrdSecParameters *challenge = nullptr;

  XrdSecCredentials *init = MakeCred(BuildInitCred("alice", blob));
  EXPECT_EQ(srv.Authenticate(init, &challenge, &err), 1);
  delete init;

  // Revoke by fingerprint; bump mtime so the stat probe notices.
  sleep(1);
  WriteFile(revPath, fp + "\n", 0600);
  {
    struct stat st;
    ASSERT_EQ(stat(revPath.c_str(), &st), 0);
    struct timeval tv[2];
    tv[0].tv_sec = tv[1].tv_sec = st.st_mtime + 5;
    tv[0].tv_usec = tv[1].tv_usec = 0;
    ASSERT_EQ(utimes(revPath.c_str(), tv), 0);
  }
  XrdSecCredentials *init2 = MakeCred(BuildInitCred("alice", blob));
  EXPECT_LT(srv.Authenticate(init2, &challenge, &err), 0);
  int ec = 0;
  std::string txt = err.getErrText(ec);
  EXPECT_NE(txt.find("revoked"), std::string::npos) << txt;
  delete init2;

  // Certificates: by serial and by subject key.
  EVP_PKEY *caPriv = nullptr;
  std::string caSshBlob;
  SetupEd25519Ca(caPriv, caSshBlob);
  std::string userRawPub;
  EVP_PKEY *userPriv = MakeEd25519FromSeed(0x93, userRawPub);
  std::string cert = BuildEd25519UserCert(caPriv, caSshBlob, userRawPub, {"alice"}, 0,
                                          0xFFFFFFFFFFFFFFFFULL); // serial 1
  {
    std::lock_guard<std::mutex> lock(RevokedMu);
    Revoked = RevocationList();
    Revoked.serials[1] = true;
  }
  XrdSecCredentials *init3 = MakeCred(BuildInitCred("alice", cert));
  EXPECT_LT(srv.Authenticate(init3, &challenge, &err), 0);
  txt = err.getErrText(ec);
  EXPECT_NE(txt.find("revoked"), std::string::npos) << txt;
  delete init3;
  {
    std::string subjFp;
    ASSERT_TRUE(sha256Base64(makeEd25519SshBlob(userRawPub), subjFp));
    std::lock_guard<std::mutex> lock(RevokedMu);
    Revoked = RevocationList();
    Revoked.keyFps[subjFp] = true;
  }
  XrdSecCredentials *init4 = MakeCred(BuildInitCred("alice", cert));
  EXPECT_LT(srv.Authenticate(init4, &challenge, &err), 0);
  delete init4;
  {
    std::lock_guard<std::mutex> lock(RevokedMu);
    Revoked = RevocationList();
    Revoked.keyIds["test-key-id"] = true;
  }
  XrdSecCredentials *init5 = MakeCred(BuildInitCred("alice", cert));
  EXPECT_LT(srv.Authenticate(init5, &challenge, &err), 0);
  delete init5;

  delete challenge;
  unlink(revPath.c_str());
  RevokedKeysFile.clear();
  EVP_PKEY_free(priv);
  EVP_PKEY_free(caPriv);
  EVP_PKEY_free(userPriv);
}

// ---------------------------------------------------------------------------
// Wire strictness.
// ---------------------------------------------------------------------------

TEST_F(XrdSecSSHTest, RejectsTrailingBytes)
{
  std::string blob, fp;
  EVP_PKEY *priv = RegisterRawEd25519(0x75, "alice", blob, fp);
  ASSERT_NE(priv, nullptr);
  XrdNetAddrInfo endPoint;
  XrdSecProtocolssh srv("localhost", endPoint);
  srv.Entity.tident = "tid-trailing";
  XrdOucErrInfo err;
  XrdSecParameters *challenge = nullptr;

  XrdSecCredentials *init = MakeCred(BuildInitCred("alice", blob) + "X");
  EXPECT_LT(srv.Authenticate(init, &challenge, &err), 0);
  delete init;

  XrdSecCredentials *init2 = MakeCred(BuildInitCred("alice", blob));
  ASSERT_EQ(srv.Authenticate(init2, &challenge, &err), 1);
  delete init2;
  std::string nonce, chFp;
  ASSERT_TRUE(ParseChallenge(challenge, nonce, chFp));
  std::string sig;
  ASSERT_TRUE(signData(priv, challengePayload(nonce, chFp, kTestHost), sig));
  XrdSecCredentials *resp = MakeCred(BuildResponseCred(sig) + "X");
  EXPECT_LT(srv.Authenticate(resp, &challenge, &err), 0);
  delete resp;
  delete challenge;
  EVP_PKEY_free(priv);
}

TEST_F(XrdSecSSHTest, InitParsesNewOptions)
{
  std::string blob, fp;
  EVP_PKEY *priv = RegisterRawEd25519(0x76, "alice", blob, fp);
  WriteFile(keysPath, "alice ssh-ed25519 " + B64Encode(blob) + "\n", 0600);
  const std::string revPath = TempFilePath("revoked-init");
  WriteFile(revPath, "serial: 7\n", 0600);

  std::string cfg = "-keys-file " + keysPath + " -revoked-keys-file " + revPath
                  + " -deny-users root,daemon -hostnames Alias.Example.Org,10.1.2.3"
                  + " -allow-empty-principals";
  XrdOucErrInfo err;
  char *tok = XrdSecProtocolsshInit('s', cfg.c_str(), &err);
  int ec = 0;
  ASSERT_NE(tok, nullptr) << err.getErrText(ec);
  free(tok);
  EXPECT_TRUE(isDeniedUser("root"));
  EXPECT_TRUE(isDeniedUser("daemon"));
  EXPECT_FALSE(isDeniedUser("alice"));
  EXPECT_TRUE(isAcceptedHost("alias.example.org"));
  EXPECT_TRUE(isAcceptedHost("10.1.2.3"));
  EXPECT_TRUE(isAcceptedHost("localhost"));
  EXPECT_TRUE(AllowEmptyPrincipals);
  {
    std::lock_guard<std::mutex> lock(RevokedMu);
    EXPECT_TRUE(Revoked.serials.count(7));
  }

  std::string cfgNone = "-keys-file " + keysPath + " -deny-users none";
  tok = XrdSecProtocolsshInit('s', cfgNone.c_str(), &err);
  ASSERT_NE(tok, nullptr);
  free(tok);
  EXPECT_FALSE(isDeniedUser("root"));

  std::string cfgBad = "-keys-file " + keysPath + " -hostnames 'bad host'";
  EXPECT_EQ(XrdSecProtocolsshInit('s', cfgBad.c_str(), &err), nullptr);

  unlink(revPath.c_str());
  RevokedKeysFile.clear();
  EVP_PKEY_free(priv);
}

} // namespace
