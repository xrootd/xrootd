#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <sys/stat.h>
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

XrdSecCredentials *MakeCred(const std::string &payload)
{
  char *b = static_cast<char *>(malloc(payload.size()));
  if (!b) return nullptr;
  memcpy(b, payload.data(), payload.size());
  return new XrdSecCredentials(b, static_cast<int>(payload.size()));
}

class XrdSecSSHTest : public ::testing::Test
{
protected:
  void TearDown() override
  {
    clearTrusted();
    PendingByTid.clear();
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

TEST_F(XrdSecSSHTest, CheckSecureFileRejectsGroupWritable)
{
  WriteFile(keysOpenPath, "dummy\n", 0660);
  std::string emsg;
  EXPECT_FALSE(checkSecureFile(keysOpenPath.c_str(), emsg));
  EXPECT_NE(emsg.find("must not be group/other writable"), std::string::npos);
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
  ASSERT_TRUE(checkSecureFile(KeysFile.c_str(), emsg)) << emsg;
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
  EVP_PKEY *pub = makeRSAPublicKeyFromNE(nBin, eBin);
  ASSERT_NE(pub, nullptr);

  std::string payload = challengePayload("nonce-rsa", "SHA256:rsa");
  std::string sig;
  ASSERT_TRUE(signData(priv, payload, sig));
  ASSERT_FALSE(sig.empty());
  EXPECT_TRUE(verifyData(pub, payload, sig));
  EXPECT_FALSE(verifyData(pub, payload + "x", sig));

  EVP_PKEY_free(pub);
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
  tk.rawPub = raw;
  tk.pkey = pub;
  TrustedByFP[fp] = tk;

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
  tk.rawPub = rawPub;
  tk.pkey = pub;
  TrustedByFP[fp] = tk;

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

} // namespace
