#include <ctime>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

// Exercise internal OIDC/JWT helpers directly in one TU.
#include "../../src/XrdSecoidc/XrdSecProtocoloidc.cc"

namespace {

std::string Base64URLEncode(const unsigned char *data, size_t len)
{
  static const char tbl[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string out;
  out.reserve(((len + 2) / 3) * 4);
  size_t i = 0;
  while (i + 2 < len) {
    uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) |
                 uint32_t(data[i + 2]);
    out.push_back(tbl[(v >> 18) & 0x3F]);
    out.push_back(tbl[(v >> 12) & 0x3F]);
    out.push_back(tbl[(v >> 6) & 0x3F]);
    out.push_back(tbl[v & 0x3F]);
    i += 3;
  }
  if (i < len) {
    uint32_t v = uint32_t(data[i]) << 16;
    out.push_back(tbl[(v >> 18) & 0x3F]);
    if (i + 1 < len) {
      v |= uint32_t(data[i + 1]) << 8;
      out.push_back(tbl[(v >> 12) & 0x3F]);
      out.push_back(tbl[(v >> 6) & 0x3F]);
    } else {
      out.push_back(tbl[(v >> 12) & 0x3F]);
    }
  }
  return out;
}

std::string Base64URLEncode(const std::string &s)
{
  return Base64URLEncode(reinterpret_cast<const unsigned char *>(s.data()),
                         s.size());
}

std::string BigNumToBase64URL(const BIGNUM *bn)
{
  int nbytes = BN_num_bytes(bn);
  std::vector<unsigned char> buf(nbytes);
  BN_bn2bin(bn, buf.data());
  return Base64URLEncode(buf.data(), buf.size());
}

struct KeyMaterial {
  EVP_PKEY *pkey{nullptr};
  std::string jwks;
};

KeyMaterial MakeKeyAndJWKS()
{
  KeyMaterial km;
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
  if (!bio) {
    ADD_FAILURE() << "failed to allocate PEM BIO";
    return km;
  }
  km.pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
  BIO_free(bio);
  if (!km.pkey) {
    ADD_FAILURE() << "failed to parse embedded private key";
    return km;
  }

  BIGNUM *n = nullptr;
  BIGNUM *e = nullptr;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  if (EVP_PKEY_get_bn_param(km.pkey, "n", &n) != 1 ||
      EVP_PKEY_get_bn_param(km.pkey, "e", &e) != 1) {
    ADD_FAILURE() << "failed to extract RSA n/e";
    if (n) BN_free(n);
    if (e) BN_free(e);
    EVP_PKEY_free(km.pkey);
    km.pkey = nullptr;
    return km;
  }
#else
  RSA *rsa = EVP_PKEY_get1_RSA(km.pkey);
  if (!rsa) {
    ADD_FAILURE() << "failed to extract RSA key";
    EVP_PKEY_free(km.pkey);
    km.pkey = nullptr;
    return km;
  }
  const BIGNUM *nRef = nullptr;
  const BIGNUM *eRef = nullptr;
  RSA_get0_key(rsa, &nRef, &eRef, nullptr);
  if (!nRef || !eRef) {
    ADD_FAILURE() << "failed to extract RSA n/e";
    RSA_free(rsa);
    EVP_PKEY_free(km.pkey);
    km.pkey = nullptr;
    return km;
  }
  n = BN_dup(nRef);
  e = BN_dup(eRef);
  RSA_free(rsa);
  if (!n || !e) {
    ADD_FAILURE() << "failed to duplicate RSA n/e";
    if (n) BN_free(n);
    if (e) BN_free(e);
    EVP_PKEY_free(km.pkey);
    km.pkey = nullptr;
    return km;
  }
#endif
  std::string n64 = BigNumToBase64URL(n);
  std::string e64 = BigNumToBase64URL(e);
  km.jwks = "{\"keys\":[{\"kty\":\"RSA\",\"kid\":\"k1\",\"n\":\"" + n64 +
            "\",\"e\":\"" + e64 + "\"}]}";
  BN_free(n);
  BN_free(e);
  return km;
}

std::string SignRS256(EVP_PKEY *pkey, const std::string &data)
{
  EVP_MD_CTX *mctx = EVP_MD_CTX_new();
  if (!mctx) {
    ADD_FAILURE() << "failed to allocate signing context";
    return std::string();
  }
  if (EVP_DigestSignInit(mctx, nullptr, EVP_sha256(), nullptr, pkey) != 1 ||
      EVP_DigestSignUpdate(mctx, data.data(), data.size()) != 1) {
    ADD_FAILURE() << "failed to initialize signature";
    EVP_MD_CTX_free(mctx);
    return std::string();
  }
  size_t siglen = 0;
  if (EVP_DigestSignFinal(mctx, nullptr, &siglen) != 1) {
    ADD_FAILURE() << "failed to get signature length";
    EVP_MD_CTX_free(mctx);
    return std::string();
  }
  std::vector<unsigned char> sig(siglen);
  if (EVP_DigestSignFinal(mctx, sig.data(), &siglen) != 1) {
    ADD_FAILURE() << "failed to sign payload";
    EVP_MD_CTX_free(mctx);
    return std::string();
  }
  EVP_MD_CTX_free(mctx);
  return Base64URLEncode(sig.data(), siglen);
}

std::string MakeToken(EVP_PKEY *pkey, const std::string &issuer,
                      const std::string &audience, time_t exp,
                      const std::string &sub = "alice")
{
  const std::string hdr = "{\"alg\":\"RS256\",\"kid\":\"k1\",\"typ\":\"JWT\"}";
  const std::string payload =
      "{\"iss\":\"" + issuer + "\",\"aud\":\"" + audience + "\",\"exp\":" +
      std::to_string(static_cast<long long>(exp)) + ",\"sub\":\"" + sub + "\"}";

  const std::string signedData =
      Base64URLEncode(hdr) + "." + Base64URLEncode(payload);
  const std::string sig = SignRS256(pkey, signedData);
  return signedData + "." + sig;
}

void ResetGlobalsForTest()
{
  expiry = 1;
  MaxTokSize = 8192;
  ClockSkew = 0;
  JwksRefresh = 3600;
  customIdentityClaims = false;
  clearIssuerPolicies();
  auto p = std::make_shared<IssuerPolicy>();
  p->issuer = "https://issuer.example";
  p->audiences.push_back("xrootd");
  IssuerPolicies.push_back(p);
  IssuerPolicyByIssuer[p->issuer] = p;
  IdentityClaims.clear();
  IdentityClaims.push_back("preferred_username");
  IdentityClaims.push_back("sub");
  EmailIdentityMap.clear();
  TokenCache.clear();
  TokenCacheHits = 0;
  TokenCacheMisses = 0;
}

} // namespace

TEST(XrdSecOIDCTest, ValidTokenPassesAndExtractsIdentity)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  ASSERT_EQ(EVP_PKEY_up_ref(km.pkey), 1);
  IssuerPolicies[0]->jwksKeys["k1"] = km.pkey;
  IssuerPolicies[0]->lastJwksLoad = time(0);

  std::string emsg;
  std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                time(0) + 600, "alice");
  std::string header;
  std::string payload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_TRUE(parseAndValidateJWT(token.c_str(), payload, header, identity, expOut, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, AudienceMismatchFails)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  ASSERT_EQ(EVP_PKEY_up_ref(km.pkey), 1);
  IssuerPolicies[0]->jwksKeys["k1"] = km.pkey;
  IssuerPolicies[0]->lastJwksLoad = time(0);

  std::string emsg;
  std::string token =
      MakeToken(km.pkey, "https://issuer.example", "other-aud", time(0) + 600);
  std::string header;
  std::string payload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), payload, header, identity, expOut, emsg));
  EXPECT_NE(emsg.find("audience"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, ExpiredTokenFails)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  ASSERT_EQ(EVP_PKEY_up_ref(km.pkey), 1);
  IssuerPolicies[0]->jwksKeys["k1"] = km.pkey;
  IssuerPolicies[0]->lastJwksLoad = time(0);

  std::string emsg;
  std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                time(0) - 10);
  std::string header;
  std::string payload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), payload, header, identity, expOut, emsg));
  EXPECT_NE(emsg.find("expired"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, MalformedTokenFails)
{
  ResetGlobalsForTest();
  std::string header;
  std::string payload;
  std::string identity;
  std::string emsg;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT("not.a.jwt", payload, header, identity, expOut, emsg));
}

TEST(XrdSecOIDCTest, BadSignatureFails)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  ASSERT_EQ(EVP_PKEY_up_ref(km.pkey), 1);
  IssuerPolicies[0]->jwksKeys["k1"] = km.pkey;
  IssuerPolicies[0]->lastJwksLoad = time(0);

  std::string emsg;
  std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                time(0) + 600);
  const size_t dot2 = token.rfind('.');
  ASSERT_NE(dot2, std::string::npos);
  ASSERT_GT(token.size(), dot2 + 3);
  const size_t tamperPos = dot2 + 2; // non-trailing signature char
  token[tamperPos] = (token[tamperPos] == 'A' ? 'B' : 'A');

  std::string payload;
  std::string header;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), payload, header, identity, expOut, emsg));
  EXPECT_NE(emsg.find("signature"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, UnknownIssuerFails)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  ASSERT_EQ(EVP_PKEY_up_ref(km.pkey), 1);
  IssuerPolicies[0]->jwksKeys["k1"] = km.pkey;
  IssuerPolicies[0]->lastJwksLoad = time(0);

  std::string emsg;
  std::string token =
      MakeToken(km.pkey, "https://unknown-issuer.example", "xrootd", time(0) + 600);
  std::string payload;
  std::string header;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), payload, header, identity, expOut, emsg));
  EXPECT_NE(emsg.find("issuer"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, MultiIssuerAudiencePolicyMismatchFails)
{
  ResetGlobalsForTest();
  auto p2 = std::make_shared<IssuerPolicy>();
  p2->issuer = "https://issuer-b.example";
  p2->audiences.push_back("service-b");
  IssuerPolicies.push_back(p2);
  IssuerPolicyByIssuer[p2->issuer] = p2;

  KeyMaterial kmA = MakeKeyAndJWKS();
  ASSERT_NE(kmA.pkey, nullptr);
  ASSERT_EQ(EVP_PKEY_up_ref(kmA.pkey), 1);
  IssuerPolicies[0]->jwksKeys["k1"] = kmA.pkey;
  IssuerPolicies[0]->lastJwksLoad = time(0);

  std::string emsg;
  std::string token = MakeToken(kmA.pkey, "https://issuer.example", "service-b", time(0) + 600);
  std::string payload;
  std::string header;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), payload, header, identity, expOut, emsg));
  EXPECT_NE(emsg.find("audience"), std::string::npos);

  EVP_PKEY_free(kmA.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, ForcedIdentityClaimPerIssuerPasses)
{
  ResetGlobalsForTest();
  IssuerPolicies[0]->forcedIdentityClaim = "cern_upn";

  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  ASSERT_EQ(EVP_PKEY_up_ref(km.pkey), 1);
  IssuerPolicies[0]->jwksKeys["k1"] = km.pkey;
  IssuerPolicies[0]->lastJwksLoad = time(0);

  const std::string hdr = "{\"alg\":\"RS256\",\"kid\":\"k1\",\"typ\":\"JWT\"}";
  const std::string payload =
      "{\"iss\":\"https://issuer.example\",\"aud\":\"xrootd\",\"exp\":" +
      std::to_string(static_cast<long long>(time(0) + 600)) +
      ",\"sub\":\"alice\",\"cern_upn\":\"apeters\"}";
  const std::string signedData =
      Base64URLEncode(hdr) + "." + Base64URLEncode(payload);
  const std::string token = signedData + "." + SignRS256(km.pkey, signedData);

  std::string emsg, outHdr, outPayload, identity;
  uint64_t expOut = 0;
  ASSERT_TRUE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg)) << emsg;
  EXPECT_EQ(identity, "apeters");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, ForcedIdentityClaimMissingFails)
{
  ResetGlobalsForTest();
  IssuerPolicies[0]->forcedIdentityClaim = "cern_upn";

  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  ASSERT_EQ(EVP_PKEY_up_ref(km.pkey), 1);
  IssuerPolicies[0]->jwksKeys["k1"] = km.pkey;
  IssuerPolicies[0]->lastJwksLoad = time(0);

  std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                time(0) + 600, "alice");
  std::string emsg, outHdr, outPayload, identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("cern_upn"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, ForcedEmailIdentityClaimUsesMap)
{
  ResetGlobalsForTest();
  IssuerPolicies[0]->forcedIdentityClaim = "email";
  EmailIdentityMap["andreas.joachim.peters@cern.ch"] = "apeters";

  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  ASSERT_EQ(EVP_PKEY_up_ref(km.pkey), 1);
  IssuerPolicies[0]->jwksKeys["k1"] = km.pkey;
  IssuerPolicies[0]->lastJwksLoad = time(0);

  const std::string hdr = "{\"alg\":\"RS256\",\"kid\":\"k1\",\"typ\":\"JWT\"}";
  const std::string payload =
      "{\"iss\":\"https://issuer.example\",\"aud\":\"xrootd\",\"exp\":" +
      std::to_string(static_cast<long long>(time(0) + 600)) +
      ",\"email\":\"andreas.joachim.peters@cern.ch\"}";
  const std::string signedData =
      Base64URLEncode(hdr) + "." + Base64URLEncode(payload);
  const std::string token = signedData + "." + SignRS256(km.pkey, signedData);

  std::string emsg, outHdr, outPayload, identity;
  uint64_t expOut = 0;
  ASSERT_TRUE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg)) << emsg;
  EXPECT_EQ(identity, "apeters");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, ForcedEmailIdentityClaimMissingMapFails)
{
  ResetGlobalsForTest();
  IssuerPolicies[0]->forcedIdentityClaim = "email";

  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  ASSERT_EQ(EVP_PKEY_up_ref(km.pkey), 1);
  IssuerPolicies[0]->jwksKeys["k1"] = km.pkey;
  IssuerPolicies[0]->lastJwksLoad = time(0);

  const std::string hdr = "{\"alg\":\"RS256\",\"kid\":\"k1\",\"typ\":\"JWT\"}";
  const std::string payload =
      "{\"iss\":\"https://issuer.example\",\"aud\":\"xrootd\",\"exp\":" +
      std::to_string(static_cast<long long>(time(0) + 600)) +
      ",\"email\":\"unknown.user@cern.ch\"}";
  const std::string signedData =
      Base64URLEncode(hdr) + "." + Base64URLEncode(payload);
  const std::string token = signedData + "." + SignRS256(km.pkey, signedData);

  std::string emsg, outHdr, outPayload, identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("not mapped"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}
