#include <chrono>
#include <ctime>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

// Exercise internal OIDC/JWT helpers directly in one TU.
#include "../../src/XrdOuc/XrdOucOIDC.cc"

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "../../src/XrdSecoidc/XrdSecProtocoloidc.cc"

using namespace XrdOucOIDC::detail;

namespace {

// Current wall-clock time in epoch seconds, expressed via std::chrono.
time_t NowSeconds()
{
  return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

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
  EvpMdCtxPtr mctx(EVP_MD_CTX_new());
  if (!mctx) {
    ADD_FAILURE() << "failed to allocate signing context";
    return std::string();
  }
  if (EVP_DigestSignInit(mctx.get(), nullptr, EVP_sha256(), nullptr, pkey) != 1 ||
      EVP_DigestSignUpdate(mctx.get(), data.data(), data.size()) != 1) {
    ADD_FAILURE() << "failed to initialize signature";
    return std::string();
  }
  size_t siglen = 0;
  if (EVP_DigestSignFinal(mctx.get(), nullptr, &siglen) != 1) {
    ADD_FAILURE() << "failed to get signature length";
    return std::string();
  }
  std::vector<unsigned char> sig(siglen);
  if (EVP_DigestSignFinal(mctx.get(), sig.data(), &siglen) != 1) {
    ADD_FAILURE() << "failed to sign payload";
    return std::string();
  }
  return Base64URLEncode(sig.data(), siglen);
}

std::string MakeToken(EVP_PKEY *pkey, const std::string &issuer,
                      const std::string &audience, time_t exp,
                      const std::string &sub = "alice")
{
  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
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
  EntityClaimMappings.clear();
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
  IssuerPolicies[0]->jwksKeys["k1"] = EvpPkeyPtr(km.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  std::string emsg;
  std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                NowSeconds() + 600, "alice");
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
  IssuerPolicies[0]->jwksKeys["k1"] = EvpPkeyPtr(km.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  std::string emsg;
  std::string token =
      MakeToken(km.pkey, "https://issuer.example", "other-aud", NowSeconds() + 600);
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
  IssuerPolicies[0]->jwksKeys["k1"] = EvpPkeyPtr(km.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  std::string emsg;
  std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                NowSeconds() - 10);
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
  IssuerPolicies[0]->jwksKeys["k1"] = EvpPkeyPtr(km.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  std::string emsg;
  std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                NowSeconds() + 600);
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
  IssuerPolicies[0]->jwksKeys["k1"] = EvpPkeyPtr(km.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  std::string emsg;
  std::string token =
      MakeToken(km.pkey, "https://unknown-issuer.example", "xrootd", NowSeconds() + 600);
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
  IssuerPolicies[0]->jwksKeys["k1"] = EvpPkeyPtr(kmA.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  std::string emsg;
  std::string token = MakeToken(kmA.pkey, "https://issuer.example", "service-b", NowSeconds() + 600);
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
  IssuerPolicies[0]->jwksKeys["k1"] = EvpPkeyPtr(km.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload =
      R"({"iss":"https://issuer.example","aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"sub":"alice","cern_upn":"apeters"})";
  const std::string signedData =
      Base64URLEncode(hdr) + "." + Base64URLEncode(payload);
  const std::string token = signedData + "." + SignRS256(km.pkey, signedData);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
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
  IssuerPolicies[0]->jwksKeys["k1"] = EvpPkeyPtr(km.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                NowSeconds() + 600, "alice");
  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
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
  IssuerPolicies[0]->jwksKeys["k1"] = EvpPkeyPtr(km.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload =
      R"({"iss":"https://issuer.example","aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"email":"andreas.joachim.peters@cern.ch"})";
  const std::string signedData =
      Base64URLEncode(hdr) + "." + Base64URLEncode(payload);
  const std::string token = signedData + "." + SignRS256(km.pkey, signedData);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
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
  IssuerPolicies[0]->jwksKeys["k1"] = EvpPkeyPtr(km.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload =
      R"({"iss":"https://issuer.example","aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"email":"unknown.user@cern.ch"})";
  const std::string signedData =
      Base64URLEncode(hdr) + "." + Base64URLEncode(payload);
  const std::string token = signedData + "." + SignRS256(km.pkey, signedData);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("not mapped"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

namespace {

// Build a JWT from explicit header/payload JSON. The signature is always a real
// RS256 signature over the encoded header+payload, regardless of the "alg"
// claimed in the header, so we can craft algorithm-confusion test vectors.
std::string MakeJWT(EVP_PKEY *pkey, const std::string &hdrJson,
                    const std::string &payloadJson)
{
  const std::string signedData =
      Base64URLEncode(hdrJson) + "." + Base64URLEncode(payloadJson);
  return signedData + "." + SignRS256(pkey, signedData);
}

// Register the test key under kid in the first issuer policy and mark JWKS as
// freshly loaded so no network refresh is attempted during validation.
void InstallTestKey(KeyMaterial &km, const char *kid = "k1")
{
  ASSERT_EQ(EVP_PKEY_up_ref(km.pkey), 1);
  IssuerPolicies[0]->jwksKeys[kid] = EvpPkeyPtr(km.pkey);
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();
}

std::string IssuerClaim() { return "\"iss\":\"https://issuer.example\""; }

} // namespace

// --- Algorithm-confusion guards (the classic JWT vulnerability class) --------

TEST(XrdSecOIDCTest, AlgNoneRejected)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = "{\"alg\":\"none\",\"kid\":\"k1\",\"typ\":\"JWT\"}";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) + ",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("alg"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, AlgHS256Rejected)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = "{\"alg\":\"HS256\",\"kid\":\"k1\",\"typ\":\"JWT\"}";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) + ",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("alg"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, MissingAlgRejected)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = "{\"kid\":\"k1\",\"typ\":\"JWT\"}";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) + ",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("alg"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

// --- Time-based claims -------------------------------------------------------

TEST(XrdSecOIDCTest, NotYetValidTokenFails)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 1200)) + ",\"nbf\":" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) + ",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("not yet valid"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, ClockSkewAllowsRecentlyExpiredToken)
{
  ResetGlobalsForTest();
  ClockSkew = 120;
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                NowSeconds() - 60, "alice");
  ASSERT_TRUE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, ExpiryOptionalAllowsMissingExp)
{
  ResetGlobalsForTest();
  expiry = -1;
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() + ",\"aud\":\"xrootd\",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_TRUE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");
  EXPECT_EQ(expOut, 0u);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, ExpiryRequiredRejectsMissingExp)
{
  ResetGlobalsForTest();
  expiry = 1;
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() + ",\"aud\":\"xrootd\",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("expiry"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, ExpiryIgnoreAllowsExpiredToken)
{
  ResetGlobalsForTest();
  expiry = 0;
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  // exp is in the past, but "ignore" mode must not enforce it.
  std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                NowSeconds() - 3600, "alice");
  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_TRUE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

// --- Audience as JSON array --------------------------------------------------

TEST(XrdSecOIDCTest, AudienceArrayMatchPasses)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      ",\"aud\":[\"other-service\",\"xrootd\"],\"exp\":" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) + ",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_TRUE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, AudienceArrayMismatchFails)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      ",\"aud\":[\"svc-a\",\"svc-b\"],\"exp\":" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) + ",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("audience"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

// A token that carries no "aud" claim at all must be rejected whenever the
// issuer policy configures an expected audience.
TEST(XrdSecOIDCTest, AudienceConfiguredButTokenMissingAudFails)
{
  ResetGlobalsForTest(); // ResetGlobalsForTest configures audience "xrootd"
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() + R"(,"exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) + ",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("audience"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

// When an issuer policy configures no audience, audience binding is disabled
// by design: a token minted for an unrelated relying party is still accepted.
TEST(XrdSecOIDCTest, NoConfiguredAudienceSkipsAudienceCheck)
{
  ResetGlobalsForTest();
  IssuerPolicies[0]->audiences.clear(); // opt out of audience binding
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  std::string token = MakeToken(km.pkey, "https://issuer.example",
                                "some-other-service", NowSeconds() + 600, "alice");
  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_TRUE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

// --- Key id (kid) handling ---------------------------------------------------

TEST(XrdSecOIDCTest, KidMismatchFails)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km, "k1");

  const std::string hdr = "{\"alg\":\"RS256\",\"kid\":\"k2\",\"typ\":\"JWT\"}";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) + ",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("signature"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, KidlessTokenVerifiedAgainstAllKeys)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km, "k1");

  const std::string hdr = "{\"alg\":\"RS256\",\"typ\":\"JWT\"}";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) + ",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_TRUE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

// --- Default identity-claim fallback order -----------------------------------

TEST(XrdSecOIDCTest, PreferredUsernameWinsOverSub)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      ",\"preferred_username\":\"alice\",\"sub\":\"uuid-1234\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_TRUE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, EntityClaimsExportScopeJsonArray)
{
  ResetGlobalsForTest();
  std::string emsg;
  ASSERT_TRUE(storeEntityClaimEntry("scope", emsg)) << emsg;

  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"sub":"alice","scope":["storage.read:/public","storage.modify:/alice"],"wlcg.ver":"1.0"})";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string identity;
  std::map<std::string, std::string> entityAttrs;
  ASSERT_TRUE(validateToken(token.c_str(), identity, emsg, nullptr, &entityAttrs)) << emsg;
  EXPECT_EQ(entityAttrs["token.scope"],
            "storage.read:/public storage.modify:/alice");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, EntityClaimsPopulateMappedAttributes)
{
  ResetGlobalsForTest();
  std::string emsg;
  ASSERT_TRUE(storeEntityClaimEntry("sub=token.subject", emsg)) << emsg;
  ASSERT_TRUE(storeEntityClaimEntry("iss", emsg)) << emsg;

  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"sub":"uuid-1234","iss":"https://issuer.example"})";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string identity;
  std::map<std::string, std::string> entityAttrs;
  ASSERT_TRUE(validateToken(token.c_str(), identity, emsg, nullptr, &entityAttrs)) << emsg;
  EXPECT_EQ(entityAttrs["token.subject"], "uuid-1234");
  EXPECT_EQ(entityAttrs["token.iss"], "https://issuer.example");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, AppendIssuerPathOptionAcceptsAbsolutePaths)
{
  std::vector<std::string> paths;
  std::string emsg;
  ASSERT_TRUE(appendIssuerPathOption("/tree1/", paths, emsg)) << emsg;
  EXPECT_EQ(paths, std::vector<std::string>{"/tree1"});
  ASSERT_TRUE(appendIssuerPathOption("/tree2/", paths, emsg)) << emsg;
  EXPECT_EQ(paths, (std::vector<std::string>{"/tree1", "/tree2"}));

  EXPECT_FALSE(appendIssuerPathOption("relative/path", paths, emsg));
  EXPECT_FALSE(appendIssuerPathOption("", paths, emsg));
}

TEST(XrdSecOIDCTest, ParseInitParmsIssuerPathOptions)
{
  clearIssuerPolicies();
  std::shared_ptr<IssuerPolicy> curPolicy;
  const char *parms =
      "-issuer https://issuer.example "
      "-base-path /tree1/ "
      "-restricted-path /restricted/ -restricted-path /shared/";
  ASSERT_TRUE(parseInitParms(parms, curPolicy, nullptr));
  ASSERT_EQ(IssuerPolicies.size(), 1u);
  EXPECT_EQ(IssuerPolicies[0]->basePath, "/tree1");
  EXPECT_EQ(IssuerPolicies[0]->restrictedPaths,
            (std::vector<std::string>{"/restricted", "/shared"}));
}

TEST(XrdSecOIDCTest, ParseInitParmsIssuerBasePathReplacesPrevious)
{
  clearIssuerPolicies();
  std::shared_ptr<IssuerPolicy> curPolicy;
  ASSERT_TRUE(parseInitParms(
      "-issuer https://issuer.example -base-path /tree1/ -base-path /tree2/",
      curPolicy, nullptr));
  EXPECT_EQ(IssuerPolicies[0]->basePath, "/tree2");
}

TEST(XrdSecOIDCTest, ParseInitParmsIssuerBasePathAllowsCommaInPath)
{
  clearIssuerPolicies();
  std::shared_ptr<IssuerPolicy> curPolicy;
  ASSERT_TRUE(parseInitParms(
      "-issuer https://issuer.example -base-path /tree,one/",
      curPolicy, nullptr));
  EXPECT_EQ(IssuerPolicies[0]->basePath, "/tree,one");
}

TEST(XrdSecOIDCTest, ParseInitParmsIssuerPathOptionsRejectRelative)
{
  clearIssuerPolicies();
  std::shared_ptr<IssuerPolicy> curPolicy;
  EXPECT_FALSE(parseInitParms("-issuer https://issuer.example -base-path rel",
                              curPolicy, nullptr));
}

TEST(XrdSecOIDCTest, IssuerPathOptionsPopulateEntityAttributes)
{
  ResetGlobalsForTest();
  IssuerPolicies[0]->basePath = "/stash";
  IssuerPolicies[0]->restrictedPaths = {"/public", "/shared"};

  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"preferred_username":"alice"})";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string identity, emsg;
  std::map<std::string, std::string> entityAttrs;
  ASSERT_TRUE(validateToken(token.c_str(), identity, emsg, nullptr, &entityAttrs)) << emsg;
  EXPECT_EQ(entityAttrs["base_path"], "/stash");
  EXPECT_EQ(entityAttrs["restricted_path"], "[\"/public\",\"/shared\"]");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecOIDCTest, EntityClaimsSkipMissingOrUnsafeValues)
{
  ResetGlobalsForTest();
  std::string emsg;
  ASSERT_TRUE(storeEntityClaimEntry("sub", emsg)) << emsg;
  ASSERT_TRUE(storeEntityClaimEntry("azp", emsg)) << emsg;

  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"sub":"bad\nvalue","azp":"client-app"})";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string identity;
  std::map<std::string, std::string> entityAttrs;
  ASSERT_TRUE(validateToken(token.c_str(), identity, emsg, nullptr, &entityAttrs)) << emsg;
  EXPECT_EQ(entityAttrs.count("token.sub"), 0u);
  EXPECT_EQ(entityAttrs["token.azp"], "client-app");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

// --- Strip(): Bearer-prefix and whitespace handling --------------------------

TEST(XrdSecOIDCTest, StripHandlesBearerAndWhitespace)
{
  int sz = 0;
  const char *r = Strip("  Bearer token123  ", sz);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(std::string(r, sz), "token123");

  r = Strip("Bearer%20abc", sz);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(std::string(r, sz), "abc");

  r = Strip("plain", sz);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(std::string(r, sz), "plain");

  // Length-bounded: only the first maxLen bytes are considered.
  const char buf[] = "abcGARBAGE";
  r = Strip(buf, sz, 3);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(std::string(r, sz), "abc");

  // Whitespace-only input yields nothing.
  r = Strip("     ", sz);
  EXPECT_EQ(r, nullptr);
}

// --- base64url round trip ----------------------------------------------------

TEST(XrdSecOIDCTest, Base64URLRoundTrip)
{
  std::string data;
  for (int i = 0; i < 256; ++i) data.push_back(static_cast<char>(i));
  const std::string enc = encodeBase64URL(data);
  EXPECT_EQ(enc.find('+'), std::string::npos);
  EXPECT_EQ(enc.find('/'), std::string::npos);
  EXPECT_EQ(enc.find('='), std::string::npos);

  std::string dec;
  ASSERT_TRUE(decodeBase64URL(enc, dec));
  EXPECT_EQ(dec, data);

  std::string bad;
  EXPECT_FALSE(decodeBase64URL("@@@@", bad));
}

// --- JWKS parsing ------------------------------------------------------------

TEST(XrdSecOIDCTest, LoadJWKSParsesRSAKey)
{
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);

  std::map<std::string, EvpPkeyPtr> keys;
  std::string emsg;
  ASSERT_TRUE(loadJWKS(km.jwks, keys, emsg)) << emsg;
  EXPECT_EQ(keys.size(), 1u);
  EXPECT_NE(keys.find("k1"), keys.end());

  freeKeys(keys);
  EVP_PKEY_free(km.pkey);
}

TEST(XrdSecOIDCTest, LoadJWKSRejectsNonRSAAndMalformed)
{
  std::map<std::string, EvpPkeyPtr> keys;
  std::string emsg;

  const std::string ecOnly =
      "{\"keys\":[{\"kty\":\"EC\",\"kid\":\"e1\",\"crv\":\"P-256\","
      "\"x\":\"AA\",\"y\":\"BB\"}]}";
  EXPECT_FALSE(loadJWKS(ecOnly, keys, emsg));
  EXPECT_NE(emsg.find("no usable RSA"), std::string::npos);

  EXPECT_FALSE(loadJWKS("{}", keys, emsg));
  EXPECT_FALSE(loadJWKS("not json", keys, emsg));
}

// --- Server-side token cache -------------------------------------------------

TEST(XrdSecOIDCTest, TokenCacheStoreLookupAndExpiry)
{
  ResetGlobalsForTest();
  TokenCache.clear();
  TokenCacheMax = 10;
  const uint64_t now = static_cast<uint64_t>(NowSeconds());

  CachedTokenEntry e;
  e.identity = "alice";
  e.expiresAt = now + 100;
  tokenCacheStore("key-a", e, now);

  CachedTokenEntry out;
  ASSERT_TRUE(tokenCacheLookup("key-a", now, out));
  EXPECT_EQ(out.identity, "alice");

  // An entry past its expiry is treated as a miss and dropped on lookup.
  CachedTokenEntry e2;
  e2.identity = "bob";
  e2.expiresAt = now + 10;
  tokenCacheStore("key-b", e2, now);
  EXPECT_FALSE(tokenCacheLookup("key-b", now + 20, out));
  EXPECT_EQ(tokenCacheSize(), 1u); // key-b removed, key-a remains

  // Caching disabled => never stores nor returns.
  TokenCacheMax = 0;
  tokenCacheStore("key-c", e, now);
  EXPECT_FALSE(tokenCacheLookup("key-c", now, out));

  TokenCacheMax = 10;
  TokenCache.clear();
}

TEST(XrdSecOIDCTest, TokenCacheEvictsSoonestToExpireWhenFull)
{
  ResetGlobalsForTest();
  TokenCache.clear();
  TokenCacheMax = 2;
  const uint64_t now = static_cast<uint64_t>(NowSeconds());

  CachedTokenEntry a; a.identity = "a"; a.expiresAt = now + 10;   // soonest
  CachedTokenEntry b; b.identity = "b"; b.expiresAt = now + 1000;
  CachedTokenEntry c; c.identity = "c"; c.expiresAt = now + 2000;
  tokenCacheStore("a", a, now);
  tokenCacheStore("b", b, now);
  ASSERT_EQ(tokenCacheSize(), 2u);

  tokenCacheStore("c", c, now); // at capacity: evict soonest-to-expire ("a")
  EXPECT_EQ(tokenCacheSize(), 2u);

  CachedTokenEntry out;
  EXPECT_FALSE(tokenCacheLookup("a", now, out));
  EXPECT_TRUE(tokenCacheLookup("b", now, out));
  EXPECT_TRUE(tokenCacheLookup("c", now, out));

  TokenCache.clear();
  TokenCacheMax = 10;
}

// --- INI config parsing / reload ---------------------------------------------

TEST(XrdSecOIDCTest, ParseReloadableIniSectionsParsesIssuersAndEmailMap)
{
  const std::string cfg =
      "[global]\n"
      "expiry = required\n"               // global keys are ignored on reload
      "[issuer \"https://issuer-a.example\"]\n"
      "audience = xrootd, xrootd-admin\n"
      "forced-identity-claim = preferred_username\n"
      "[email-map]\n"
      "Alice@Example.ORG = alice\n";

  std::vector<std::shared_ptr<IssuerPolicy>> pols;
  std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> byIss;
  std::unordered_map<std::string, std::string> emap;
  std::string emsg;
  ASSERT_TRUE(parseReloadableIniSections("test.cfg", cfg, pols, byIss, emap, emsg)) << emsg;

  ASSERT_EQ(pols.size(), 1u);
  EXPECT_EQ(pols[0]->issuer, "https://issuer-a.example");
  ASSERT_EQ(pols[0]->audiences.size(), 2u);
  EXPECT_EQ(pols[0]->audiences[0], "xrootd");
  EXPECT_EQ(pols[0]->audiences[1], "xrootd-admin");
  EXPECT_EQ(pols[0]->forcedIdentityClaim, "preferred_username");

  // Email keys are normalized (trimmed + lowercased).
  ASSERT_EQ(emap.count("alice@example.org"), 1u);
  EXPECT_EQ(emap["alice@example.org"], "alice");
}

TEST(XrdSecOIDCTest, ParseReloadableIniSectionsRejectsUnknownSection)
{
  const std::string cfg = "[bogus]\nfoo = bar\n";
  std::vector<std::shared_ptr<IssuerPolicy>> pols;
  std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> byIss;
  std::unordered_map<std::string, std::string> emap;
  std::string emsg;
  ASSERT_FALSE(parseReloadableIniSections("test.cfg", cfg, pols, byIss, emap, emsg));
  EXPECT_NE(emsg.find("unsupported section"), std::string::npos);
}

// XrdSecServer joins multiple sec.protparm lines with '\n'. Init must parse every
// record, not only the first line.
TEST(XrdSecOIDCTest, ParseInitParmsMultiLineProtparm)
{
  clearIssuerPolicies();
  expiry = 1;
  DebugToken = false;
  DebugTokenClaims = false;
  customIdentityClaims = false;
  IdentityClaims = {"sub"};

  const char *parms =
      " -issuer https://auth.cern.ch/auth/realms/cern\n"
      " -audience eos-service -expiry required -show-token-claims";

  std::shared_ptr<IssuerPolicy> curPolicy;
  ASSERT_TRUE(parseInitParms(parms, curPolicy, nullptr));

  ASSERT_EQ(IssuerPolicies.size(), 1u);
  EXPECT_EQ(IssuerPolicies[0]->issuer, "https://auth.cern.ch/auth/realms/cern");
  ASSERT_EQ(IssuerPolicies[0]->audiences.size(), 1u);
  EXPECT_EQ(IssuerPolicies[0]->audiences[0], "eos-service");
  EXPECT_EQ(expiry.load(), 1);
  EXPECT_TRUE(DebugTokenClaims.load());

  clearIssuerPolicies();
}

TEST(XrdSecOIDCTest, ParseInitParmsEmailMapFromProtparm)
{
  clearIssuerPolicies();
  EmailIdentityMap.clear();
  IdentityClaims = {"sub"};

  const char *parms =
      "-email-map foo.bar@gmail.com=foo\n"
      "-email-map Bob@Example.org=bobby";

  std::shared_ptr<IssuerPolicy> curPolicy;
  ASSERT_TRUE(parseInitParms(parms, curPolicy, nullptr));
  EXPECT_EQ(EmailIdentityMap["foo.bar@gmail.com"], "foo");
  EXPECT_EQ(EmailIdentityMap["bob@example.org"], "bobby");
}

TEST(XrdSecOIDCTest, ParseInitParmsEmailMapRejectsMalformed)
{
  clearIssuerPolicies();
  EmailIdentityMap.clear();
  IdentityClaims = {"sub"};

  std::shared_ptr<IssuerPolicy> curPolicy;
  ASSERT_FALSE(parseInitParms("-email-map not-an-email", curPolicy, nullptr));
}

TEST(XrdSecOIDCTest, PrescanInitParmsMultiLineProtparm)
{
  std::string selectedCfgPath = "/etc/xrootd/oidc.cfg";
  bool requestedCfgOverride = false;
  std::string inlineParms;
  const char *parms =
      " -config-file /custom/oidc.cfg\n"
      " -issuer https://issuer.example -audience xrootd";

  ASSERT_TRUE(prescanInitParms(parms, selectedCfgPath, requestedCfgOverride,
                               inlineParms, nullptr));
  EXPECT_TRUE(requestedCfgOverride);
  EXPECT_EQ(selectedCfgPath, "/custom/oidc.cfg");
  EXPECT_NE(inlineParms.find("-issuer"), std::string::npos);
  EXPECT_NE(inlineParms.find("-audience"), std::string::npos);
  EXPECT_EQ(inlineParms.find("-config-file"), std::string::npos);
}

TEST(XrdSecOIDCTest, LoadOIDCIniAsArgsBuildsOptsFromFile)
{
  ResetGlobalsForTest();
  EmailIdentityMap.clear();

  std::string tmpl = "/tmp/oidc_test_cfg_XXXXXX";
  int fd = mkstemp(tmpl.data());
  ASSERT_GE(fd, 0);
  ASSERT_EQ(fchmod(fd, 0600), 0); // owner-only, as the loader requires

  const std::string content =
      "[global]\n"
      "expiry = required\n"
      "identity-claim = preferred_username,sub\n"
      "[issuer \"https://issuer-a.example\"]\n"
      "audience = xrootd, adminaud\n"
      "forced-identity-claim = email\n"
      "base_path = /tree1/\n"
      "restricted_path = /restricted/\n"
      "restricted_path = /shared/\n"
      "[email-map]\n"
      "Bob@Example.org = bobby\n";
  ASSERT_EQ(write(fd, content.data(), content.size()),
            static_cast<ssize_t>(content.size()));
  close(fd);

  std::string opts, emsg;
  bool found = false;
  ASSERT_TRUE(loadOIDCIniAsArgs(tmpl.c_str(), opts, found, emsg)) << emsg;
  EXPECT_TRUE(found);
  EXPECT_NE(opts.find("-issuer https://issuer-a.example"), std::string::npos);
  EXPECT_NE(opts.find("-expiry required"), std::string::npos);
  EXPECT_NE(opts.find("-audience xrootd"), std::string::npos);
  EXPECT_NE(opts.find("-audience adminaud"), std::string::npos);
  EXPECT_NE(opts.find("-forced-identity-claim email"), std::string::npos);
  EXPECT_NE(opts.find("-base-path /tree1"), std::string::npos);
  EXPECT_NE(opts.find("-restricted-path /restricted"), std::string::npos);
  EXPECT_NE(opts.find("-restricted-path /shared"), std::string::npos);

  // The [email-map] section is applied directly to the global map (normalized).
  EXPECT_EQ(EmailIdentityMap.count("bob@example.org"), 1u);
  EXPECT_EQ(EmailIdentityMap["bob@example.org"], "bobby");

  unlink(tmpl.c_str());
}

// --- Server Authenticate() credential framing --------------------------------

TEST(XrdSecOIDCTest, AuthenticateRejectsMalformedFraming)
{
  ResetGlobalsForTest();
  XrdNetAddrInfo addr;
  XrdSecProtocoloidc prot("host.example", addr);

  // Too small (size <= 6) is rejected outright.
  {
    char *b = static_cast<char *>(malloc(4));
    memcpy(b, "oid", 4);
    XrdSecCredentials c(b, 4);
    EXPECT_EQ(prot.Authenticate(&c, nullptr, nullptr), -1);
  }

  // Correct size but wrong protocol prefix.
  {
    char *b = static_cast<char *>(malloc(11));
    memcpy(b, "xxxx", 4);
    b[4] = '\0';
    memcpy(b + 5, "token", 5);
    b[10] = '\0';
    XrdSecCredentials c(b, 11);
    EXPECT_EQ(prot.Authenticate(&c, nullptr, nullptr), -1);
  }

  // No NUL within the declared size (hostile, non-terminated buffer).
  {
    char *b = static_cast<char *>(malloc(8));
    memset(b, 'a', 8);
    XrdSecCredentials c(b, 8);
    EXPECT_EQ(prot.Authenticate(&c, nullptr, nullptr), -1);
  }

  // Valid "oidc" prefix but an empty token.
  {
    char *b = static_cast<char *>(malloc(7));
    memcpy(b, "oidc", 5); // "oidc\0"
    b[5] = '\0';
    b[6] = '\0';
    XrdSecCredentials c(b, 7);
    EXPECT_EQ(prot.Authenticate(&c, nullptr, nullptr), -1);
  }
}
