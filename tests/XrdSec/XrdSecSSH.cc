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

std::string BuildResponseCred(const std::string &sig)
{
  std::string out;
  WireHdr h;
  memcpy(h.id, "ssh", 4);
  h.ver = kProtoVersion;
  h.op = OpResponse;
  h.rsvd[0] = h.rsvd[1] = 0;
  out.append(reinterpret_cast<const char *>(&h), sizeof(h));
  uint16_t slen = htons(static_cast<uint16_t>(sig.size()));
  out.append(reinterpret_cast<const char *>(&slen), sizeof(slen));
  out.append(sig);
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
  putU32(out, static_cast<uint32_t>(time(nullptr)));
  putU16(out, static_cast<uint16_t>(nonce.size()));
  putU16(out, static_cast<uint16_t>(fp.size()));
  out.append(nonce);
  out.append(fp);
  return out;
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
  void TearDown() override
  {
    clearTrusted();
    PendingByTid.clear();
    PrincipalMap.clear();
    PrincipalAsUser = false;
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

  std::string payload = challengePayload("nonce-123", "SHA256:abc");
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

  std::string payload = challengePayload("nonce-rsa", "SHA256:rsa");
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

  // Missing pending challenge after erase should fail.
  PendingByTid.erase("tid-replay");
  std::string fakeSig(64, 'S');
  XrdSecCredentials *respNoPending = MakeCred(BuildResponseCred(fakeSig));
  ASSERT_NE(respNoPending, nullptr);
  EXPECT_LT(srv.Authenticate(respNoPending, &challenge, &err), 0);
  delete respNoPending;

  // Expired pending challenge should fail.
  PendingChallenge pc;
  pc.nonce = "nonce";
  pc.fp = fp;
  pc.user = "alice";
  pc.expiresAt = time(nullptr) - 1;
  PendingByTid["tid-replay"] = pc;
  XrdSecCredentials *respExpired = MakeCred(BuildResponseCred(fakeSig));
  ASSERT_NE(respExpired, nullptr);
  EXPECT_LT(srv.Authenticate(respExpired, &challenge, &err), 0);
  delete respExpired;

  if (challenge) {delete challenge; challenge = nullptr;}
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
}

TEST_F(XrdSecSSHTest, AuthenticateRejectsDuplicatePendingChallenge)
{
  std::string raw(32, 'D');
  std::string blob = makeEd25519SshBlob(raw);
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
  srv.Entity.tident = strdup("tid-dup-challenge");
  XrdSecParameters *challenge = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *init1 = MakeCred(BuildInitCred("alice", blob));
  ASSERT_NE(init1, nullptr);
  ASSERT_EQ(srv.Authenticate(init1, &challenge, &err), 1);
  ASSERT_NE(challenge, nullptr);
  delete init1;

  XrdSecCredentials *init2 = MakeCred(BuildInitCred("alice", blob));
  ASSERT_NE(init2, nullptr);
  EXPECT_LT(srv.Authenticate(init2, &challenge, &err), 0);
  delete init2;

  if (challenge) delete challenge;
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
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

  const char *cp = challenge->buffer;
  const char *ce = challenge->buffer + challenge->size;
  ASSERT_GE(challenge->size, static_cast<int>(sizeof(WireHdr) + 8));
  const WireHdr *wh = reinterpret_cast<const WireHdr *>(cp);
  ASSERT_EQ(memcmp(wh->id, "ssh", 4), 0);
  ASSERT_EQ(wh->op, OpChallenge);
  cp += sizeof(WireHdr);

  uint32_t ts = 0;
  uint16_t nLen = 0, fLen = 0;
  ASSERT_TRUE(readU32(cp, ce, ts));
  ASSERT_TRUE(readU16(cp, ce, nLen));
  ASSERT_TRUE(readU16(cp, ce, fLen));
  ASSERT_GT(ts, static_cast<uint32_t>(0));
  ASSERT_GT(nLen, static_cast<uint16_t>(0));
  ASSERT_GT(fLen, static_cast<uint16_t>(0));
  ASSERT_GE(ce - cp, nLen + fLen);
  std::string nonce(cp, nLen);
  cp += nLen;
  std::string chFp(cp, fLen);
  ASSERT_EQ(chFp, fp);

  std::string payload = challengePayload(nonce, chFp);
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
  std::string wrongPayload = challengePayload("not-the-real-nonce", fp);
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

  const char *cp = challenge->buffer;
  const char *ce = challenge->buffer + challenge->size;
  cp += sizeof(WireHdr);
  uint32_t ts = 0;
  uint16_t nLen = 0, fLen = 0;
  ASSERT_TRUE(readU32(cp, ce, ts));
  ASSERT_TRUE(readU16(cp, ce, nLen));
  ASSERT_TRUE(readU16(cp, ce, fLen));
  ASSERT_GE(ce - cp, nLen + fLen);
  std::string nonce(cp, nLen);
  cp += nLen;
  std::string chFp(cp, fLen);

  std::string payload = challengePayload(nonce, chFp);
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

  const char *cp = challenge->buffer;
  const char *ce = challenge->buffer + challenge->size;
  cp += sizeof(WireHdr);
  uint32_t ts = 0;
  uint16_t nLen = 0, fLen = 0;
  ASSERT_TRUE(readU32(cp, ce, ts));
  ASSERT_TRUE(readU16(cp, ce, nLen));
  ASSERT_TRUE(readU16(cp, ce, fLen));
  ASSERT_GE(ce - cp, nLen + fLen);
  std::string nonce(cp, nLen);
  cp += nLen;
  std::string chFp(cp, fLen);

  std::string payload = challengePayload(nonce, chFp);
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

TEST_F(XrdSecSSHTest, CertValidateEmptyPrincipalsActsAsWildcard)
{
  // A certificate with an empty principals list is, per OpenSSH semantics,
  // valid for any requested user. This test pins that (security-sensitive)
  // behavior so it cannot change silently.
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
    EXPECT_TRUE(mapPrincipalsToUser({"nosuchprincipal", localUser}, mapped,
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
    EXPECT_TRUE(mapPrincipalsToUser({"cert-principal"}, mapped, method, emsg))
        << emsg;
    EXPECT_EQ(mapped, localUser);
    EXPECT_EQ(method, "principal-map-file");
  }

  // A principal that matches nothing is rejected.
  {
    std::string mapped, method, emsg;
    EXPECT_FALSE(mapPrincipalsToUser({"unknown-principal"}, mapped, method,
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

  const char *cp = challenge->buffer;
  const char *ce = challenge->buffer + challenge->size;
  cp += sizeof(WireHdr);
  uint32_t ts = 0;
  uint16_t nLen = 0, fLen = 0;
  ASSERT_TRUE(readU32(cp, ce, ts));
  ASSERT_TRUE(readU16(cp, ce, nLen));
  ASSERT_TRUE(readU16(cp, ce, fLen));
  ASSERT_GE(ce - cp, nLen + fLen);
  std::string nonce(cp, nLen);
  cp += nLen;
  std::string chFp(cp, fLen);

  std::string payload = challengePayload(nonce, chFp);
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
    EXPECT_TRUE(mapPrincipalsToUser({"alpha"}, mapped, method, mErr)) << mErr;
    EXPECT_EQ(mapped, localUser);
    EXPECT_EQ(method, "principal-map-file");
  }
  // "beta" is not mapped yet.
  {
    std::string mapped, method, mErr;
    EXPECT_FALSE(mapPrincipalsToUser({"beta"}, mapped, method, mErr));
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
    EXPECT_TRUE(mapPrincipalsToUser({"beta"}, mapped, method, mErr)) << mErr;
    EXPECT_EQ(mapped, localUser);
  }
  // The old principal is gone after reload.
  {
    std::string mapped, method, mErr;
    EXPECT_FALSE(mapPrincipalsToUser({"alpha"}, mapped, method, mErr));
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

  const char *cp = challenge->buffer;
  const char *ce = challenge->buffer + challenge->size;
  cp += sizeof(WireHdr);
  uint32_t ts = 0;
  uint16_t nLen = 0, fLen = 0;
  ASSERT_TRUE(readU32(cp, ce, ts));
  ASSERT_TRUE(readU16(cp, ce, nLen));
  ASSERT_TRUE(readU16(cp, ce, fLen));
  ASSERT_GE(ce - cp, nLen + fLen);
  std::string nonce(cp, nLen);
  cp += nLen;
  std::string chFp(cp, fLen);

  std::string payload = challengePayload(nonce, chFp);
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
  XrdSecProtocolssh cli("0:8192:", &err, aOK);
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

TEST_F(XrdSecSSHTest, AuthenticateRejectsTooManyPendingChallenges)
{
  PendingByTid.clear();
  for (size_t i = 0; i < kMaxPendingChallenges; ++i)
    {
     PendingChallenge pc;
     pc.nonce = "n";
     pc.fp = "fp";
     pc.user = "u";
     pc.expiresAt = time(nullptr) + 60;
     PendingByTid["tid-fill-" + std::to_string(i)] = pc;
    }
  ASSERT_EQ(PendingByTid.size(), kMaxPendingChallenges);

  std::string raw(32, 'P');
  std::string blob = makeEd25519SshBlob(raw);
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
  srv.Entity.tident = strdup("tid-overflow");
  XrdSecParameters *challenge = nullptr;
  XrdOucErrInfo err;

  XrdSecCredentials *init = MakeCred(BuildInitCred("alice", blob));
  ASSERT_NE(init, nullptr);
  EXPECT_LT(srv.Authenticate(init, &challenge, &err), 0);
  delete init;

  if (challenge) delete challenge;
  free(const_cast<char *>(srv.Entity.tident));
  srv.Entity.tident = nullptr;
  PendingByTid.clear();
}

} // namespace
