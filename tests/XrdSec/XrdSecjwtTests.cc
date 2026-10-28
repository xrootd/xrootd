#include <chrono>
#include <ctime>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

// Exercise internal JWT helpers directly in one TU.
#include "../../src/XrdSecjwt/XrdSecJWT.cc"

#include "XrdNet/XrdNetAddrInfo.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "../../src/XrdSecjwt/XrdSecProtocoljwt.cc"

using namespace XrdSecJWT::detail;

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
  DebugToken = false;
  DebugTokenClaims = false;
  JwksRefreshMin = kDefaultJwksRefreshMin;
  JWTLog.logger(&JWTLogger); // initSecProtocol() normally binds this
}

} // namespace

TEST(XrdSecJWTTest, ValidTokenPassesAndExtractsIdentity)
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

TEST(XrdSecJWTTest, AudienceMismatchFails)
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

TEST(XrdSecJWTTest, ExpiredTokenFails)
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

TEST(XrdSecJWTTest, MalformedTokenFails)
{
  ResetGlobalsForTest();
  std::string header;
  std::string payload;
  std::string identity;
  std::string emsg;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT("not.a.jwt", payload, header, identity, expOut, emsg));
}

TEST(XrdSecJWTTest, BadSignatureFails)
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

TEST(XrdSecJWTTest, UnknownIssuerFails)
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

TEST(XrdSecJWTTest, MultiIssuerAudiencePolicyMismatchFails)
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

TEST(XrdSecJWTTest, ForcedIdentityClaimPerIssuerPasses)
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

TEST(XrdSecJWTTest, ForcedIdentityClaimMissingFails)
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

TEST(XrdSecJWTTest, ForcedEmailIdentityClaimUsesMap)
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

TEST(XrdSecJWTTest, ForcedEmailIdentityClaimMissingMapFails)
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

TEST(XrdSecJWTTest, AlgNoneRejected)
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

TEST(XrdSecJWTTest, AlgHS256Rejected)
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

TEST(XrdSecJWTTest, MissingAlgRejected)
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

TEST(XrdSecJWTTest, NotYetValidTokenFails)
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

TEST(XrdSecJWTTest, ClockSkewAllowsRecentlyExpiredToken)
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

TEST(XrdSecJWTTest, ExpiryOptionalAllowsMissingExp)
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

TEST(XrdSecJWTTest, ExpiryRequiredRejectsMissingExp)
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

TEST(XrdSecJWTTest, ExpiryIgnoreAllowsExpiredToken)
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

TEST(XrdSecJWTTest, AudienceArrayMatchPasses)
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

TEST(XrdSecJWTTest, AudienceArrayMismatchFails)
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
TEST(XrdSecJWTTest, AudienceConfiguredButTokenMissingAudFails)
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
TEST(XrdSecJWTTest, NoConfiguredAudienceSkipsAudienceCheck)
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

TEST(XrdSecJWTTest, CritHeaderIsRejected)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT","crit":["b64"]})";
  const std::string payload = "{" + IssuerClaim() + R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) + ",\"sub\":\"alice\"}";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("crit"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecJWTTest, WlcgTokenWithoutAudIsRejectedEvenIfAudienceUnset)
{
  ResetGlobalsForTest();
  IssuerPolicies[0]->audiences.clear();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() + R"(,"exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"sub":"alice","wlcg.ver":"1.0","scope":"storage.read:/data"})";
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

TEST(XrdSecJWTTest, WlcgTokenWithAudPassesWhenAudienceUnset)
{
  ResetGlobalsForTest();
  IssuerPolicies[0]->audiences.clear();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":["https://wlcg.cern.ch/jwt/v1/any","storage.example"],"exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"sub":"alice","wlcg.ver":"1.0","scope":"storage.read:/data"})";
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

TEST(XrdSecJWTTest, MultiAudienceIdTokenRequiresAzp)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string exp = std::to_string(static_cast<long long>(NowSeconds() + 600));
  const std::string missing = "{" + IssuerClaim() +
      R"(,"aud":["xrootd","other-client"],"exp":)" + exp +
      R"(,"sub":"alice","scope":"openid profile email"})";
  const std::string present = "{" + IssuerClaim() +
      R"(,"aud":["xrootd","other-client"],"azp":"xrootd","exp":)" + exp +
      R"(,"sub":"alice","scope":"openid profile email"})";
  const std::string foreign = "{" + IssuerClaim() +
      R"(,"aud":["xrootd","other-client"],"azp":"not-an-aud","exp":)" + exp +
      R"(,"sub":"alice","scope":"openid profile email"})";

  std::string emsg;
  std::string outHdr;
  std::string outPayload;
  std::string identity;
  uint64_t expOut = 0;
  ASSERT_FALSE(parseAndValidateJWT(MakeJWT(km.pkey, hdr, missing).c_str(),
                                   outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("azp"), std::string::npos);

  emsg.clear();
  ASSERT_TRUE(parseAndValidateJWT(MakeJWT(km.pkey, hdr, present).c_str(),
                                  outPayload, outHdr, identity, expOut, emsg)) << emsg;

  emsg.clear();
  ASSERT_FALSE(parseAndValidateJWT(MakeJWT(km.pkey, hdr, foreign).c_str(),
                                   outPayload, outHdr, identity, expOut, emsg));
  EXPECT_NE(emsg.find("azp"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecJWTTest, DiscoveryIssuerMustMatch)
{
  std::string jwks;
  std::string emsg;
  ASSERT_TRUE(acceptDiscoveryDocument(
      R"({"issuer":"https://issuer.example/","jwks_uri":"https://issuer.example/jwks"})",
      "https://issuer.example", jwks, emsg)) << emsg;
  EXPECT_EQ(jwks, "https://issuer.example/jwks");

  EXPECT_FALSE(acceptDiscoveryDocument(
      R"({"issuer":"https://other.example","jwks_uri":"https://other.example/jwks"})",
      "https://issuer.example", jwks, emsg));
  EXPECT_NE(emsg.find("issuer"), std::string::npos);

  EXPECT_FALSE(acceptDiscoveryDocument(
      R"({"issuer":"https://issuer.example","jwks_uri":"http://issuer.example/jwks"})",
      "https://issuer.example", jwks, emsg));
  EXPECT_NE(emsg.find("https"), std::string::npos);
}

// --- Key id (kid) handling ---------------------------------------------------

TEST(XrdSecJWTTest, KidMismatchFails)
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

TEST(XrdSecJWTTest, KidlessTokenVerifiedAgainstAllKeys)
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

TEST(XrdSecJWTTest, PreferredUsernameWinsOverSub)
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

TEST(XrdSecJWTTest, EntityClaimsExportScopeJsonArray)
{
  ResetGlobalsForTest();
  std::string emsg;
  ASSERT_TRUE(storeEntityClaimEntry("scope", emsg)) << emsg;
  ASSERT_TRUE(storeEntityClaimEntry("wlcg.ver", emsg)) << emsg;

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
  EXPECT_EQ(entityAttrs["token.wlcg.ver"], "1.0");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecJWTTest, EntityClaimsPopulateMappedAttributes)
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

TEST(XrdSecJWTTest, AppendIssuerPathOptionAcceptsAbsolutePaths)
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

TEST(XrdSecJWTTest, ParseInitParmsIssuerPathOptions)
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

TEST(XrdSecJWTTest, ParseInitParmsIssuerBasePathReplacesPrevious)
{
  clearIssuerPolicies();
  std::shared_ptr<IssuerPolicy> curPolicy;
  ASSERT_TRUE(parseInitParms(
      "-issuer https://issuer.example -base-path /tree1/ -base-path /tree2/",
      curPolicy, nullptr));
  EXPECT_EQ(IssuerPolicies[0]->basePath, "/tree2");
}

TEST(XrdSecJWTTest, ParseInitParmsIssuerBasePathAllowsCommaInPath)
{
  clearIssuerPolicies();
  std::shared_ptr<IssuerPolicy> curPolicy;
  ASSERT_TRUE(parseInitParms(
      "-issuer https://issuer.example -base-path /tree,one/",
      curPolicy, nullptr));
  EXPECT_EQ(IssuerPolicies[0]->basePath, "/tree,one");
}

TEST(XrdSecJWTTest, ParseInitParmsIssuerPathOptionsRejectRelative)
{
  clearIssuerPolicies();
  std::shared_ptr<IssuerPolicy> curPolicy;
  EXPECT_FALSE(parseInitParms("-issuer https://issuer.example -base-path rel",
                              curPolicy, nullptr));
}

TEST(XrdSecJWTTest, IssuerPathOptionsPopulateEntityAttributes)
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

TEST(XrdSecJWTTest, EntityClaimsSkipMissingOrUnsafeValues)
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
      R"(,"preferred_username":"alice","sub":"bad\nvalue","azp":"client-app"})";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string identity;
  std::map<std::string, std::string> entityAttrs;
  ASSERT_TRUE(validateToken(token.c_str(), identity, emsg, nullptr, &entityAttrs)) << emsg;
  EXPECT_EQ(identity, "alice");
  EXPECT_EQ(entityAttrs["request.name"], "alice");
  EXPECT_EQ(entityAttrs.count("token.sub"), 0u);
  EXPECT_EQ(entityAttrs.count("token.subject"), 0u);
  EXPECT_EQ(entityAttrs["token.azp"], "client-app");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecJWTTest, DefaultUserIsIdentityWhenSubjectIsNotMapped)
{
  ResetGlobalsForTest();
  IssuerPolicies[0]->forcedIdentityClaim.clear();
  IssuerPolicies[0]->defaultUser = "cmsprod";

  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"sub":"alice","preferred_username":"alice","wlcg.groups":["/cms","/cms/prod"]})";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string identity, emsg;
  std::map<std::string, std::string> entityAttrs;
  ASSERT_TRUE(validateToken(token.c_str(), identity, emsg, nullptr, &entityAttrs)) << emsg;
  EXPECT_EQ(identity, "cmsprod");
  EXPECT_EQ(entityAttrs["request.name"], "cmsprod");
  EXPECT_EQ(entityAttrs["token.subject"], "alice");
  EXPECT_EQ(entityAttrs["jwt.grps"], "/cms /cms/prod");
  EXPECT_EQ(entityAttrs["jwt.vorg"], "https://issuer.example");
  EXPECT_EQ(entityAttrs.count("jwt.strategy"), 0u);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecJWTTest, IssuerStrategyIsExported)
{
  ResetGlobalsForTest();
  IssuerPolicies[0]->forcedIdentityClaim.clear();
  IssuerPolicies[0]->defaultUser = "cmsprod";
  IssuerPolicies[0]->authzStrategy = "group";
  IssuerPolicies[0]->onmissing = "deny";
  IssuerPolicies[0]->nameMapFile = "/etc/xrootd/map.json";

  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"sub":"alice"})";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string identity, emsg;
  std::map<std::string, std::string> entityAttrs;
  ASSERT_TRUE(validateToken(token.c_str(), identity, emsg, nullptr, &entityAttrs)) << emsg;
  EXPECT_EQ(entityAttrs["jwt.vorg"], "https://issuer.example");
  EXPECT_EQ(entityAttrs["jwt.strategy"], "group");
  EXPECT_EQ(entityAttrs["jwt.onmissing"], "deny");
  EXPECT_EQ(entityAttrs["jwt.mapfile"], "/etc/xrootd/map.json");
  EXPECT_EQ(entityAttrs["jwt.typ"], "JWT");

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecJWTTest, UnsafeAccountNameIsRejected)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km);

  const std::string hdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string payload = "{" + IssuerClaim() +
      R"(,"aud":"xrootd","exp":)" +
      std::to_string(static_cast<long long>(NowSeconds() + 600)) +
      R"(,"sub":"alice/prod"})";
  const std::string token = MakeJWT(km.pkey, hdr, payload);

  std::string identity, emsg;
  EXPECT_FALSE(validateToken(token.c_str(), identity, emsg, nullptr, nullptr));
  EXPECT_NE(emsg.find("safe account name"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecJWTTest, SciTokensDefaultUserMustBeASafeAccountName)
{
  std::vector<std::shared_ptr<IssuerPolicy>> pols;
  std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> byIss;
  std::string emsg;
  const std::string cfg =
      "[Issuer bad]\n"
      "issuer = https://scitokens.example/bad\n"
      "base_path = /data\n"
      "default_user = alice/prod\n";
  EXPECT_FALSE(parseSciTokensConfig("scitokens.cfg", cfg, pols, byIss, emsg));
  EXPECT_NE(emsg.find("default_user"), std::string::npos);
}

// --- Strip(): Bearer-prefix and whitespace handling --------------------------

TEST(XrdSecJWTTest, StripHandlesBearerAndWhitespace)
{
  std::string_view r = Strip("  Bearer token123  ");
  EXPECT_EQ(r, "token123");

  r = Strip("Bearer%20abc");
  EXPECT_EQ(r, "abc");

  r = Strip("plain");
  EXPECT_EQ(r, "plain");

  // Only the bytes in the view are considered.
  const char buf[] = "abcGARBAGE";
  r = Strip(std::string_view(buf, 3));
  EXPECT_EQ(r, "abc");

  // Whitespace-only input yields nothing.
  EXPECT_TRUE(Strip("     ").empty());
}

// --- base64url round trip ----------------------------------------------------

TEST(XrdSecJWTTest, Base64URLRoundTrip)
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

TEST(XrdSecJWTTest, LoadJWKSParsesRSAKey)
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

TEST(XrdSecJWTTest, LoadJWKSRejectsNonRSAAndMalformed)
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

TEST(XrdSecJWTTest, TokenCacheStoreLookupAndExpiry)
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

TEST(XrdSecJWTTest, TokenCacheEvictsSoonestToExpireWhenFull)
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

TEST(XrdSecJWTTest, ParseReloadableIniSectionsParsesIssuersAndEmailMap)
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

TEST(XrdSecJWTTest, ReloadKeepsIssuerDeclaredOutsideASection)
{
  const std::string cfg =
      "issuer = https://top.example\n"
      "audience = topaud\n"
      "[global]\n"
      "expiry = required\n"
      "issuer = https://global.example\n"
      "audience = globaud\n"
      "base_path = /data\n"
      "[issuer \"https://section.example\"]\n"
      "audience = secaud\n";

  std::vector<std::shared_ptr<IssuerPolicy>> pols;
  std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> byIss;
  std::unordered_map<std::string, std::string> emap;
  std::string emsg;
  ASSERT_TRUE(parseReloadableIniSections("test.cfg", cfg, pols, byIss, emap, emsg)) << emsg;

  ASSERT_EQ(pols.size(), 3u);
  ASSERT_EQ(byIss["https://top.example"]->audiences.size(), 1u);
  EXPECT_EQ(byIss["https://top.example"]->audiences[0], "topaud");
  EXPECT_TRUE(byIss["https://top.example"]->basePath.empty());
  ASSERT_EQ(byIss["https://global.example"]->audiences.size(), 1u);
  EXPECT_EQ(byIss["https://global.example"]->audiences[0], "globaud");
  EXPECT_EQ(byIss["https://global.example"]->basePath, "/data");
  ASSERT_EQ(byIss["https://section.example"]->audiences.size(), 1u);
  EXPECT_EQ(byIss["https://section.example"]->audiences[0], "secaud");

  ASSERT_FALSE(parseReloadableIniSections("test.cfg", "issuer =\n", pols, byIss, emap, emsg));
  EXPECT_NE(emsg.find("issuer value is empty"), std::string::npos);
}

TEST(XrdSecJWTTest, InlineIssuerOptionsSurviveAFileReload)
{
  const std::string cfg =
      "[issuer \"https://issuer-a.example\"]\n"
      "[email-map]\n"
      "Alice@Example.ORG = alice\n";
  std::vector<std::shared_ptr<IssuerPolicy>> pols;
  std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> byIss;
  std::unordered_map<std::string, std::string> emap;
  std::string emsg;
  ASSERT_TRUE(parseReloadableIniSections("test.cfg", cfg, pols, byIss, emap, emsg)) << emsg;
  const int expiryBefore = expiry.load();
  ASSERT_TRUE(overlayInlineIssuerParms(
      "-expiry required -issuer https://issuer-a.example -audience extra "
      "-issuer https://only.example -jwks-url https://only.example/jwks "
      "-email-map Bob@Example.org=bob",
      pols, byIss, emap, emsg)) << emsg;
  EXPECT_EQ(expiry.load(), expiryBefore);
  ASSERT_EQ(pols.size(), 2u);
  ASSERT_EQ(byIss.count("https://issuer-a.example"), 1u);
  ASSERT_EQ(byIss["https://issuer-a.example"]->audiences.size(), 1u);
  EXPECT_EQ(byIss["https://issuer-a.example"]->audiences[0], "extra");
  ASSERT_EQ(byIss.count("https://only.example"), 1u);
  EXPECT_EQ(byIss["https://only.example"]->jwksURL, "https://only.example/jwks");
  EXPECT_EQ(emap["alice@example.org"], "alice");
  EXPECT_EQ(emap["bob@example.org"], "bob");
}

TEST(XrdSecJWTTest, ParseReloadableIniSectionsRejectsUnknownSection)
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
TEST(XrdSecJWTTest, ParseInitParmsMultiLineProtparm)
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

TEST(XrdSecJWTTest, ParseInitParmsEmailMapFromProtparm)
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

TEST(XrdSecJWTTest, ParseInitParmsEmailMapRejectsMalformed)
{
  clearIssuerPolicies();
  EmailIdentityMap.clear();
  IdentityClaims = {"sub"};

  std::shared_ptr<IssuerPolicy> curPolicy;
  ASSERT_FALSE(parseInitParms("-email-map not-an-email", curPolicy, nullptr));
}

TEST(XrdSecJWTTest, PrescanInitParmsMultiLineProtparm)
{
  std::string selectedCfgPath = "/etc/xrootd/jwt.cfg";
  bool requestedCfgOverride = false;
  std::string inlineParms;
  const char *parms =
      " -config-file /custom/jwt.cfg\n"
      " -issuer https://issuer.example -audience xrootd";

  ASSERT_TRUE(prescanInitParms(parms, selectedCfgPath, requestedCfgOverride,
                               inlineParms, nullptr, nullptr, nullptr));
  EXPECT_TRUE(requestedCfgOverride);
  EXPECT_EQ(selectedCfgPath, "/custom/jwt.cfg");
  EXPECT_NE(inlineParms.find("-issuer"), std::string::npos);
  EXPECT_NE(inlineParms.find("-audience"), std::string::npos);
  EXPECT_EQ(inlineParms.find("-config-file"), std::string::npos);
}

TEST(XrdSecJWTTest, SciTokensConfigMapsIssuersForAccjwt)
{
  std::vector<std::shared_ptr<IssuerPolicy>> pols;
  std::unordered_map<std::string, std::shared_ptr<IssuerPolicy>> byIss;
  std::string emsg;
  const std::string cfg =
      "[Global]\n"
      "audience = https://localhost:7095, site\n"
      "audience_json = [\"quoted.aud\", \"second aud\"]\n"
      "onmissing = deny\n"
      "\n"
      "[Issuer OSG]\n"
      "issuer = https://scitokens.example/osg\n"
      "base_path = /stash\n"
      "map_subject = True\n"
      "default_user = osg\n"
      "groups_claim = memberOf\n"
      "authorization_strategy = capability group\n"
      "onmissing = Deny\n"
      "name_mapfile = /etc/xrootd/map.json\n"
      "\n"
      "[Issuer CMS]\n"
      "issuer = https://scitokens.example/cms\n"
      "base_path = /user/cms, /user/cms2\n"
      "restricted_path = /user/cms/public\n"
      "username_claim = uid\n"
      "map_subject = true\n"
      "default_user = cms\n"
      "\n"
      "[Issuer CMS-again]\n"
      "issuer = https://scitokens.example/cms\n"
      "base_path = /cms\n"
      "map_subject = false\n"
      "\n"
      "[Issuer Multi]\n"
      "issuer = https://scitokens.example/multi\n"
      "base_path = /data, /data2\n";

  ASSERT_TRUE(parseSciTokensConfig("scitokens.cfg", cfg, pols, byIss, emsg)) << emsg;
  ASSERT_EQ(pols.size(), 3u);

  EXPECT_EQ(pols[0]->issuer, "https://scitokens.example/osg");
  EXPECT_EQ(pols[0]->basePath, "/stash");
  EXPECT_EQ(pols[0]->forcedIdentityClaim, "sub");
  EXPECT_EQ(pols[0]->defaultUser, "osg");
  EXPECT_EQ(pols[0]->groupsClaim, "memberOf");
  EXPECT_EQ(pols[0]->authzStrategy, "capability+group");
  EXPECT_EQ(pols[0]->onmissing, "deny");
  EXPECT_EQ(pols[0]->nameMapFile, "/etc/xrootd/map.json");
  ASSERT_EQ(pols[0]->audiences.size(), 4u);
  EXPECT_EQ(pols[0]->audiences[0], "https://localhost:7095");
  EXPECT_EQ(pols[0]->audiences[1], "site");
  EXPECT_EQ(pols[0]->audiences[2], "quoted.aud");
  EXPECT_EQ(pols[0]->audiences[3], "second aud");

  // Later section with the same issuer URL replaces the earlier one.
  // username_claim is absent there, and map_subject is false, so identity
  // falls back to the plugin default instead of "uid".
  EXPECT_EQ(byIss["https://scitokens.example/cms"]->basePath, "/cms");
  EXPECT_TRUE(byIss["https://scitokens.example/cms"]->forcedIdentityClaim.empty());
  EXPECT_TRUE(byIss["https://scitokens.example/cms"]->defaultUser.empty());
  EXPECT_EQ(byIss["https://scitokens.example/cms"]->groupsClaim, "wlcg.groups");
  EXPECT_TRUE(byIss["https://scitokens.example/cms"]->authzStrategy.empty());
  EXPECT_TRUE(byIss["https://scitokens.example/cms"]->onmissing.empty());
  EXPECT_TRUE(byIss["https://scitokens.example/cms"]->nameMapFile.empty());
  EXPECT_TRUE(byIss["https://scitokens.example/cms"]->restrictedPaths.empty());
  EXPECT_EQ(byIss["https://scitokens.example/cms"]->audiences, pols[0]->audiences);
  EXPECT_EQ(byIss["https://scitokens.example/multi"]->basePath, "[\"/data\",\"/data2\"]");

  const std::string bad =
      "[Issuer broken]\n"
      "issuer = https://scitokens.example/broken\n";
  EXPECT_FALSE(parseSciTokensConfig("scitokens.cfg", bad, pols, byIss, emsg));
  EXPECT_NE(emsg.find("base_path"), std::string::npos);

  const std::string badStrategy =
      "[Issuer broken]\n"
      "issuer = https://scitokens.example/broken\n"
      "base_path = /data\n"
      "authorization_strategy = capability nope\n";
  EXPECT_FALSE(parseSciTokensConfig("scitokens.cfg", badStrategy, pols, byIss, emsg));
  EXPECT_NE(emsg.find("nope"), std::string::npos);

  const std::string badOnMissing =
      "[Issuer broken]\n"
      "issuer = https://scitokens.example/broken\n"
      "base_path = /data\n"
      "onmissing = sometimes\n";
  EXPECT_FALSE(parseSciTokensConfig("scitokens.cfg", badOnMissing, pols, byIss, emsg));
  EXPECT_NE(emsg.find("onmissing"), std::string::npos);

  const std::string relativeMap =
      "[Issuer broken]\n"
      "issuer = https://scitokens.example/broken\n"
      "base_path = /data\n"
      "name_mapfile = relative.json\n";
  EXPECT_FALSE(parseSciTokensConfig("scitokens.cfg", relativeMap, pols, byIss, emsg));
  EXPECT_NE(emsg.find("name_mapfile"), std::string::npos);

  std::string sciPath;
  bool sci = false;
  bool jwtFile = false;
  std::string inlineParms;
  std::string selected = "/etc/xrootd/jwt.cfg";
  ASSERT_TRUE(prescanInitParms("-scitokens-config /etc/xrootd/scitokens.cfg -expiry required",
                               selected, jwtFile, inlineParms, nullptr, &sciPath, &sci));
  EXPECT_TRUE(sci);
  EXPECT_FALSE(jwtFile);
  EXPECT_EQ(sciPath, "/etc/xrootd/scitokens.cfg");
  EXPECT_NE(inlineParms.find("-expiry"), std::string::npos);
  EXPECT_EQ(inlineParms.find("scitokens"), std::string::npos);
  sci = false;
  jwtFile = false;
  EXPECT_FALSE(prescanInitParms(
      "-config-file /etc/xrootd/jwt.cfg -scitokens-config /etc/xrootd/scitokens.cfg",
      selected, jwtFile, inlineParms, nullptr, &sciPath, &sci));
}

TEST(XrdSecJWTTest, LoadJWTIniAsArgsBuildsOptsFromFile)
{
  ResetGlobalsForTest();
  EmailIdentityMap.clear();

  std::string tmpl = "/tmp/jwt_test_cfg_XXXXXX";
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
  ASSERT_TRUE(loadJWTIniAsArgs(tmpl.c_str(), opts, found, emsg)) << emsg;
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

TEST(XrdSecJWTTest, AuthenticateRejectsMalformedFraming)
{
  ResetGlobalsForTest();
  XrdNetAddrInfo addr;
  XrdSecProtocoljwt prot("host.example", addr);

  // Too small (size <= 5) is rejected outright.
  {
    char *b = static_cast<char *>(malloc(5));
    memcpy(b, "jwt", 4);
    b[4] = 'x';
    XrdSecCredentials c(b, 5);
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

  // Valid "jwt" prefix but an empty token.
  {
    char *b = static_cast<char *>(malloc(6));
    memcpy(b, "jwt", 4); // "jwt\0"
    b[4] = '\0';
    b[5] = '\0';
    XrdSecCredentials c(b, 6);
    EXPECT_EQ(prot.Authenticate(&c, nullptr, nullptr), -1);
  }
}

// =============================================================================
// Hardening review follow-ups
// =============================================================================

namespace {

// Left-pad a BIGNUM to a fixed width and base64url-encode it (JWK EC coords
// and JWS ECDSA signature halves are fixed width).
std::string BigNumToBase64URLPadded(const BIGNUM *bn, int width)
{
  std::vector<unsigned char> buf(width);
  if (BN_bn2binpad(bn, buf.data(), width) != width) {
    ADD_FAILURE() << "BN_bn2binpad failed";
    return std::string();
  }
  return Base64URLEncode(buf.data(), buf.size());
}

struct EcKeyMaterial {
  EVP_PKEY *pkey{nullptr};
  std::string jwks;
};

// Generate a P-256 key pair and the matching JWKS document (kid "e1").
EcKeyMaterial MakeEcKeyAndJWKS(const char *kid = "e1")
{
  EcKeyMaterial km;
  EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr));
  if (!ctx || EVP_PKEY_keygen_init(ctx.get()) != 1 ||
      EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(), NID_X9_62_prime256v1) != 1) {
    ADD_FAILURE() << "EC keygen setup failed";
    return km;
  }
  if (EVP_PKEY_keygen(ctx.get(), &km.pkey) != 1 || !km.pkey) {
    ADD_FAILURE() << "EC keygen failed";
    km.pkey = nullptr;
    return km;
  }

  BIGNUM *x = nullptr;
  BIGNUM *y = nullptr;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  if (EVP_PKEY_get_bn_param(km.pkey, OSSL_PKEY_PARAM_EC_PUB_X, &x) != 1 ||
      EVP_PKEY_get_bn_param(km.pkey, OSSL_PKEY_PARAM_EC_PUB_Y, &y) != 1) {
    ADD_FAILURE() << "failed to extract EC x/y";
    if (x) BN_free(x);
    if (y) BN_free(y);
    EVP_PKEY_free(km.pkey);
    km.pkey = nullptr;
    return km;
  }
#else
  const EC_KEY *ec = EVP_PKEY_get0_EC_KEY(km.pkey);
  x = BN_new();
  y = BN_new();
  if (!ec || !x || !y ||
      EC_POINT_get_affine_coordinates(EC_KEY_get0_group(ec),
                                      EC_KEY_get0_public_key(ec), x, y, nullptr) != 1) {
    ADD_FAILURE() << "failed to extract EC x/y";
    if (x) BN_free(x);
    if (y) BN_free(y);
    EVP_PKEY_free(km.pkey);
    km.pkey = nullptr;
    return km;
  }
#endif
  std::string kidJson = kid ? std::string("\"kid\":\"") + kid + "\"," : std::string();
  km.jwks = "{\"keys\":[{\"kty\":\"EC\"," + kidJson + "\"crv\":\"P-256\",\"x\":\"" +
            BigNumToBase64URLPadded(x, 32) + "\",\"y\":\"" +
            BigNumToBase64URLPadded(y, 32) + "\"}]}";
  BN_free(x);
  BN_free(y);
  return km;
}

// ES256: EVP_DigestSign yields a DER ECDSA-Sig-Value; JWS wants raw r||s.
std::string SignES256(EVP_PKEY *pkey, const std::string &data)
{
  EvpMdCtxPtr mctx(EVP_MD_CTX_new());
  if (!mctx ||
      EVP_DigestSignInit(mctx.get(), nullptr, EVP_sha256(), nullptr, pkey) != 1 ||
      EVP_DigestSignUpdate(mctx.get(), data.data(), data.size()) != 1) {
    ADD_FAILURE() << "ES256 sign init failed";
    return std::string();
  }
  size_t siglen = 0;
  if (EVP_DigestSignFinal(mctx.get(), nullptr, &siglen) != 1) return std::string();
  std::vector<unsigned char> der(siglen);
  if (EVP_DigestSignFinal(mctx.get(), der.data(), &siglen) != 1) return std::string();
  const unsigned char *p = der.data();
  EcdsaSigPtr sig(d2i_ECDSA_SIG(nullptr, &p, static_cast<long>(siglen)));
  if (!sig) {
    ADD_FAILURE() << "d2i_ECDSA_SIG failed";
    return std::string();
  }
  const BIGNUM *r = nullptr;
  const BIGNUM *s = nullptr;
  ECDSA_SIG_get0(sig.get(), &r, &s);
  std::vector<unsigned char> raw(64);
  BN_bn2binpad(r, raw.data(), 32);
  BN_bn2binpad(s, raw.data() + 32, 32);
  return Base64URLEncode(raw.data(), raw.size());
}

// PS256: RSASSA-PSS with SHA-256 and salt length == digest length.
std::string SignPS256(EVP_PKEY *pkey, const std::string &data)
{
  EvpMdCtxPtr mctx(EVP_MD_CTX_new());
  EVP_PKEY_CTX *pctx = nullptr;
  if (!mctx ||
      EVP_DigestSignInit(mctx.get(), &pctx, EVP_sha256(), nullptr, pkey) != 1 ||
      EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING) <= 0 ||
      EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, RSA_PSS_SALTLEN_DIGEST) <= 0 ||
      EVP_DigestSignUpdate(mctx.get(), data.data(), data.size()) != 1) {
    ADD_FAILURE() << "PS256 sign init failed";
    return std::string();
  }
  size_t siglen = 0;
  if (EVP_DigestSignFinal(mctx.get(), nullptr, &siglen) != 1) return std::string();
  std::vector<unsigned char> sig(siglen);
  if (EVP_DigestSignFinal(mctx.get(), sig.data(), &siglen) != 1) return std::string();
  return Base64URLEncode(sig.data(), siglen);
}

std::string StandardPayload(const char *sub = "alice")
{
  return "{" + IssuerClaim() + R"(,"aud":"xrootd","exp":)" +
         std::to_string(static_cast<long long>(NowSeconds() + 600)) +
         ",\"sub\":\"" + sub + "\"}";
}

bool Validate(const std::string &token, std::string &identity, std::string &emsg)
{
  std::string outHdr, outPayload;
  uint64_t expOut = 0;
  return parseAndValidateJWT(token.c_str(), outPayload, outHdr, identity, expOut, emsg);
}

} // namespace

// --- Additional JWS algorithms ------------------------------------------------

TEST(XrdSecJWTTest, Es256TokenVerifiesAgainstEcJwk)
{
  ResetGlobalsForTest();
  EcKeyMaterial km = MakeEcKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);

  std::string emsg;
  ASSERT_TRUE(loadJWKS(km.jwks, IssuerPolicies[0]->jwksKeys, emsg)) << emsg;
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();
  ASSERT_NE(IssuerPolicies[0]->jwksKeys.find("e1"), IssuerPolicies[0]->jwksKeys.end());

  const std::string hdr = R"({"alg":"ES256","kid":"e1","typ":"JWT"})";
  const std::string signedData = Base64URLEncode(hdr) + "." + Base64URLEncode(StandardPayload());
  const std::string token = signedData + "." + SignES256(km.pkey, signedData);

  std::string identity;
  ASSERT_TRUE(Validate(token, identity, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");

  // Tampering with the payload must break the signature.
  const std::string bad = Base64URLEncode(hdr) + "." + Base64URLEncode(StandardPayload("mallory")) +
                          "." + token.substr(token.rfind('.') + 1);
  EXPECT_FALSE(Validate(bad, identity, emsg));
  EXPECT_NE(emsg.find("signature"), std::string::npos);

  // A raw ECDSA signature of the wrong width is rejected before verification.
  const std::string shortSig = signedData + "." + Base64URLEncode(std::string(63, 'A'));
  EXPECT_FALSE(Validate(shortSig, identity, emsg));

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecJWTTest, Es256RejectsRsaKeyAndRs256RejectsEcKey)
{
  ResetGlobalsForTest();
  KeyMaterial rsa = MakeKeyAndJWKS();
  EcKeyMaterial ec = MakeEcKeyAndJWKS();
  ASSERT_NE(rsa.pkey, nullptr);
  ASSERT_NE(ec.pkey, nullptr);

  // Only the RSA key is installed under kid "k1".
  InstallTestKey(rsa, "k1");

  // Header claims ES256 with kid k1 (an RSA key): key-type confusion must fail.
  {
    const std::string hdr = R"({"alg":"ES256","kid":"k1","typ":"JWT"})";
    const std::string signedData = Base64URLEncode(hdr) + "." + Base64URLEncode(StandardPayload());
    const std::string token = signedData + "." + SignES256(ec.pkey, signedData);
    std::string identity, emsg;
    EXPECT_FALSE(Validate(token, identity, emsg));
  }
  // Header claims RS256 but only an EC key is present under that kid.
  {
    freeKeys(IssuerPolicies[0]->jwksKeys);
    std::string emsg;
    ASSERT_TRUE(loadJWKS(ec.jwks, IssuerPolicies[0]->jwksKeys, emsg)) << emsg;
    IssuerPolicies[0]->lastJwksLoad = NowSeconds();
    const std::string hdr = R"({"alg":"RS256","kid":"e1","typ":"JWT"})";
    const std::string token = MakeJWT(rsa.pkey, hdr, StandardPayload());
    std::string identity;
    EXPECT_FALSE(Validate(token, identity, emsg));
  }

  EVP_PKEY_free(rsa.pkey);
  EVP_PKEY_free(ec.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecJWTTest, Ps256TokenVerifiesAndIsNotInterchangeableWithRs256)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km, "k1");

  const std::string payload = StandardPayload();
  const std::string psHdr = R"({"alg":"PS256","kid":"k1","typ":"JWT"})";
  const std::string psSigned = Base64URLEncode(psHdr) + "." + Base64URLEncode(payload);
  const std::string psToken = psSigned + "." + SignPS256(km.pkey, psSigned);

  std::string identity, emsg;
  ASSERT_TRUE(Validate(psToken, identity, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");

  // A PKCS#1 v1.5 signature presented as PS256 must not verify, and vice versa.
  const std::string psHdrPkcs1 = psSigned + "." + SignRS256(km.pkey, psSigned);
  EXPECT_FALSE(Validate(psHdrPkcs1, identity, emsg));

  const std::string rsHdr = R"({"alg":"RS256","kid":"k1","typ":"JWT"})";
  const std::string rsSigned = Base64URLEncode(rsHdr) + "." + Base64URLEncode(payload);
  const std::string rsHdrPss = rsSigned + "." + SignPS256(km.pkey, rsSigned);
  EXPECT_FALSE(Validate(rsHdrPss, identity, emsg));

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

TEST(XrdSecJWTTest, UnsupportedAlgsStillRejected)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km, "k1");

  for (const char *alg : {"none", "HS256", "HS512", "EdDSA", "RS255", "rs256", ""}) {
    const std::string hdr = std::string(R"({"alg":")") + alg + R"(","kid":"k1"})";
    const std::string token = MakeJWT(km.pkey, hdr, StandardPayload());
    std::string identity, emsg;
    EXPECT_FALSE(Validate(token, identity, emsg)) << "alg=" << alg;
    EXPECT_TRUE(emsg.find("alg") != std::string::npos) << "alg=" << alg << " emsg=" << emsg;
  }

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

// --- kid-less JWKS entries ----------------------------------------------------

TEST(XrdSecJWTTest, LoadJWKSAcceptsKeysWithoutKid)
{
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  // Strip the kid from the generated JWKS.
  std::string jwks = km.jwks;
  const size_t p = jwks.find("\"kid\":\"k1\",");
  ASSERT_NE(p, std::string::npos);
  jwks.erase(p, strlen("\"kid\":\"k1\","));

  std::map<std::string, EvpPkeyPtr> keys;
  std::string emsg;
  ASSERT_TRUE(loadJWKS(jwks, keys, emsg)) << emsg;
  ASSERT_EQ(keys.size(), 1u);
  EXPECT_TRUE(isAnonymousKid(keys.begin()->first));

  freeKeys(keys);
  EVP_PKEY_free(km.pkey);
}

TEST(XrdSecJWTTest, TokenWithKidVerifiesAgainstKidlessJwkOnly)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);

  std::string jwks = km.jwks;
  jwks.erase(jwks.find("\"kid\":\"k1\","), strlen("\"kid\":\"k1\","));
  std::string emsg;
  ASSERT_TRUE(loadJWKS(jwks, IssuerPolicies[0]->jwksKeys, emsg)) << emsg;
  IssuerPolicies[0]->lastJwksLoad = NowSeconds();

  // Token names a kid the JWKS does not have; the only key is anonymous → OK.
  const std::string hdr = R"({"alg":"RS256","kid":"whatever","typ":"JWT"})";
  std::string identity;
  ASSERT_TRUE(Validate(MakeJWT(km.pkey, hdr, StandardPayload()), identity, emsg)) << emsg;
  EXPECT_EQ(identity, "alice");

  // With a *named* key that doesn't match, an unknown kid still fails
  // (KidMismatchFails covers the named-only case). Here: named + anonymous,
  // token signed by a key we do not hold at all → fail.
  EcKeyMaterial ec = MakeEcKeyAndJWKS("named");
  ASSERT_NE(ec.pkey, nullptr);
  std::map<std::string, EvpPkeyPtr> ecKeys;
  ASSERT_TRUE(loadJWKS(ec.jwks, ecKeys, emsg)) << emsg;
  IssuerPolicies[0]->jwksKeys["named"] = std::move(ecKeys["named"]);
  const std::string hdrEs = R"({"alg":"ES256","kid":"unknown","typ":"JWT"})";
  const std::string signedData = Base64URLEncode(hdrEs) + "." + Base64URLEncode(StandardPayload());
  // Signed with a fresh EC key we never published.
  EcKeyMaterial rogue = MakeEcKeyAndJWKS("rogue");
  ASSERT_NE(rogue.pkey, nullptr);
  const std::string rogueTok = signedData + "." + SignES256(rogue.pkey, signedData);
  EXPECT_FALSE(Validate(rogueTok, identity, emsg));

  EVP_PKEY_free(km.pkey);
  EVP_PKEY_free(ec.pkey);
  EVP_PKEY_free(rogue.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
}

// --- Forced JWKS refresh rate limiting ---------------------------------------

TEST(XrdSecJWTTest, ForcedRefreshIsRateLimitedPerIssuer)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km, "k1");
  auto policy = IssuerPolicies[0];
  policy->jwksURL = "https://jwks.invalid/keys"; // would be fetched if not limited
  JwksRefreshMin = 30;

  // A forced refresh attempted a moment ago blocks a new one without any I/O.
  policy->lastForcedAttempt = NowSeconds();
  std::string emsg;
  EXPECT_FALSE(refreshJWKSForPolicy(policy, true, emsg));
  EXPECT_NE(emsg.find("rate-limited"), std::string::npos);

  // The periodic (non-forced) path is unaffected while keys are fresh.
  emsg.clear();
  EXPECT_TRUE(refreshJWKSForPolicy(policy, false, emsg)) << emsg;

  // A token with an unknown kid therefore fails fast with a signature error
  // instead of triggering a network fetch.
  const std::string hdr = R"({"alg":"RS256","kid":"rotated","typ":"JWT"})";
  std::string identity;
  EXPECT_FALSE(Validate(MakeJWT(km.pkey, hdr, StandardPayload()), identity, emsg));
  EXPECT_NE(emsg.find("signature validation failed"), std::string::npos);
  EXPECT_NE(emsg.find("rate-limited"), std::string::npos);

  // The cooldown never applies while no keys are held at all (bootstrap).
  freeKeys(policy->jwksKeys);
  policy->lastForcedAttempt = NowSeconds();
  policy->jwksURL.clear();
  policy->jwtConfigURL.clear();
  emsg.clear();
  EXPECT_FALSE(refreshJWKSForPolicy(policy, true, emsg));
  EXPECT_EQ(emsg.find("rate-limited"), std::string::npos) << emsg; // failed for lack of URL, not cooldown

  EVP_PKEY_free(km.pkey);
}

// A failed periodic refresh is remembered. The next periodic attempt inside
// -jwks-refresh-min does not fetch again and keeps the keys already in memory.
// A forced refresh is not suppressed by that backoff.
TEST(XrdSecJWTTest, FailedPeriodicRefreshBacksOffAndKeepsKeys)
{
  ResetGlobalsForTest();
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km, "k1");
  auto policy = IssuerPolicies[0];
  JwksCacheFile.clear();
  JwksRefresh = 300;
  JwksRefreshMin = 30;
  // Stale keys and no endpoint: the fetch fails locally, with no network wait.
  policy->lastJwksLoad = NowSeconds() - 400;
  policy->jwksURL.clear();
  policy->jwtConfigURL.clear();
  const time_t loadedAt = policy->lastJwksLoad;

  std::string emsg;
  EXPECT_FALSE(refreshJWKSForPolicy(policy, false, emsg));
  EXPECT_NE(emsg.find("JWT config URL not set"), std::string::npos);
  EXPECT_FALSE(policy->jwksKeys.empty());
  EXPECT_NE(policy->lastFetchFailure, 0);
  EXPECT_EQ(policy->lastJwksLoad, loadedAt);

  emsg.clear();
  EXPECT_TRUE(refreshJWKSForPolicy(policy, false, emsg)) << emsg;
  EXPECT_FALSE(policy->jwksKeys.empty());
  EXPECT_EQ(policy->lastJwksLoad, loadedAt);

  // Forced refresh still attempts a fetch; the periodic backoff does not apply.
  policy->lastForcedAttempt = 0;
  emsg.clear();
  EXPECT_FALSE(refreshJWKSForPolicy(policy, true, emsg));
  EXPECT_EQ(emsg.find("rate-limited"), std::string::npos) << emsg;
  EXPECT_NE(emsg.find("JWT config URL not set"), std::string::npos);

  emsg.clear();
  EXPECT_FALSE(refreshJWKSForPolicy(policy, true, emsg));
  EXPECT_NE(emsg.find("rate-limited"), std::string::npos);

  EVP_PKEY_free(km.pkey);
  freeKeys(policy->jwksKeys);
}

// --- Bearer prefix, NumericDate floats, fingerprint --------------------------

TEST(XrdSecJWTTest, StripBearerPrefixIsCaseInsensitive)
{
  std::string_view t = Strip("bearer abc.def.ghi");
  EXPECT_EQ(t, "abc.def.ghi");

  t = Strip("BEARER%20abc.def.ghi");
  EXPECT_EQ(t, "abc.def.ghi");

  // Something that merely starts with "bearer" is not a scheme prefix.
  t = Strip("bearerabc");
  EXPECT_EQ(t, "bearerabc");
}

TEST(XrdSecJWTTest, NumericDateClaimsAcceptFloats)
{
  nlohmann::json obj;
  ASSERT_TRUE(parseJsonObject(R"({"exp":1.7e9,"nbf":1700000000.0,"neg":-1.0,"nan":"x"})", obj));
  uint64_t v = 0;
  ASSERT_TRUE(getUintClaim(obj, "exp", v));
  EXPECT_EQ(v, 1700000000u);
  ASSERT_TRUE(getUintClaim(obj, "nbf", v));
  EXPECT_EQ(v, 1700000000u);
  EXPECT_FALSE(getUintClaim(obj, "neg", v));
  EXPECT_FALSE(getUintClaim(obj, "nan", v));
}

TEST(XrdSecJWTTest, Sha256HexIsFixedWidthHex)
{
  const std::string h = sha256hex("token");
  ASSERT_EQ(h.size(), 64u);
  for (char c : h) EXPECT_TRUE(isxdigit(static_cast<unsigned char>(c)));
  EXPECT_EQ(h, "3c469e9d6c5875d37a43f353d4f88e61fcf812c66eee3457465a40b0da4153e0");
}

// --- Token cache lifetime semantics -----------------------------------------

TEST(XrdSecJWTTest, ValidateTokenCachesExpiredTokenWhenExpiryIgnored)
{
  ResetGlobalsForTest();
  expiry = 0; // ignore
  TokenCacheMax = 10;
  TokenCacheNoExpTTL = 60;
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km, "k1");

  const std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd",
                                      NowSeconds() - 600, "alice");
  std::string identity, emsg;
  ASSERT_TRUE(validateToken(token.c_str(), identity, emsg, nullptr, nullptr)) << emsg;
  EXPECT_EQ(identity, "alice");
  EXPECT_EQ(TokenCacheMisses.load(), 1u);

  // The entry must be alive (previously it was inserted already expired).
  ASSERT_TRUE(validateToken(token.c_str(), identity, emsg, nullptr, nullptr)) << emsg;
  EXPECT_EQ(TokenCacheHits.load(), 1u);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
  TokenCache.clear();
}

TEST(XrdSecJWTTest, ValidateTokenCacheLifetimeIncludesClockSkew)
{
  ResetGlobalsForTest();
  expiry = 1;
  ClockSkew = 120;
  TokenCacheMax = 10;
  KeyMaterial km = MakeKeyAndJWKS();
  ASSERT_NE(km.pkey, nullptr);
  InstallTestKey(km, "k1");

  const time_t exp = NowSeconds() + 300;
  const std::string token = MakeToken(km.pkey, "https://issuer.example", "xrootd", exp, "alice");
  std::string identity, emsg;
  uint64_t expTime = 0;
  ASSERT_TRUE(validateToken(token.c_str(), identity, emsg, &expTime, nullptr)) << emsg;
  EXPECT_EQ(expTime, static_cast<uint64_t>(exp) + 120u);

  EVP_PKEY_free(km.pkey);
  freeKeys(IssuerPolicies[0]->jwksKeys);
  TokenCache.clear();
}

// --- Idempotent (re)initialization -------------------------------------------

TEST(XrdSecJWTTest, ResetGlobalsToDefaultsRestoresEveryTunable)
{
  ResetGlobalsForTest();
  expiry = -1;
  MaxTokSize = 123;
  ClockSkew = 7;
  JwksRefresh = 9;
  JwksRefreshMin = 1;
  TokenCacheMax = 3;
  TokenCacheNoExpTTL = 4;
  DebugToken = true;
  DebugTokenClaims = true;
  customIdentityClaims = true;
  IdentityClaims = {"sub", "sub"};
  JwksCacheFile = "/tmp/x";
  JwksCacheTTL = 5;
  EntityClaimMappings.push_back({"scope", "token.scope"});
  EmailIdentityMap["a@b"] = "a";
  CachedTokenEntry e; e.expiresAt = static_cast<uint64_t>(NowSeconds()) + 100;
  tokenCacheStore("k", e, static_cast<uint64_t>(NowSeconds()));

  resetGlobalsToDefaults();

  EXPECT_EQ(expiry.load(), kDefaultExpiry);
  EXPECT_EQ(MaxTokSize.load(), kDefaultMaxTokSize);
  EXPECT_EQ(ClockSkew.load(), kDefaultClockSkew);
  EXPECT_EQ(JwksRefresh.load(), kDefaultJwksRefresh);
  EXPECT_EQ(JwksRefreshMin.load(), kDefaultJwksRefreshMin);
  EXPECT_EQ(TokenCacheMax.load(), kDefaultTokenCacheMax);
  EXPECT_EQ(TokenCacheNoExpTTL.load(), kDefaultTokenCacheNoExpTTL);
  EXPECT_FALSE(DebugToken.load());
  EXPECT_FALSE(DebugTokenClaims.load());
  EXPECT_FALSE(customIdentityClaims);
  EXPECT_EQ(IdentityClaims, kDefaultIdentityClaims);
  EXPECT_TRUE(JwksCacheFile.empty());
  EXPECT_EQ(JwksCacheTTL, 0);
  EXPECT_TRUE(EntityClaimMappings.empty());
  EXPECT_TRUE(EmailIdentityMap.empty());
  EXPECT_TRUE(IssuerPolicies.empty());
  EXPECT_EQ(tokenCacheSize(), 0u);

  // Re-applying the same -identity-claim list twice must not duplicate entries.
  std::shared_ptr<IssuerPolicy> cur;
  ASSERT_TRUE(parseInitParms("-identity-claim sub -identity-claim email", cur, nullptr));
  resetGlobalsToDefaults();
  ASSERT_TRUE(parseInitParms("-identity-claim sub -identity-claim email", cur, nullptr));
  EXPECT_EQ(IdentityClaims, (std::vector<std::string>{"sub", "email"}));
}

TEST(XrdSecJWTTest, ParseInitParmsAcceptsJwksRefreshMin)
{
  ResetGlobalsForTest();
  std::shared_ptr<IssuerPolicy> cur;
  ASSERT_TRUE(parseInitParms("-jwks-refresh-min 45", cur, nullptr));
  EXPECT_EQ(JwksRefreshMin.load(), 45);
  ASSERT_TRUE(parseInitParms("-jwks-refresh-min 0", cur, nullptr)); // disables cooldown
  EXPECT_EQ(JwksRefreshMin.load(), 0);
  EXPECT_FALSE(parseInitParms("-jwks-refresh-min -1", cur, nullptr));
  EXPECT_FALSE(parseInitParms("-jwks-refresh-min abc", cur, nullptr));

  std::string opts, emsg;
  EXPECT_TRUE(addIniKV("jwks-refresh-min", "12", false, opts, emsg)) << emsg;
  EXPECT_NE(opts.find("-jwks-refresh-min 12"), std::string::npos);
}

// --- Config change signature --------------------------------------------------

TEST(XrdSecJWTTest, ConfigChangeSignatureIncludesSizeAndCtime)
{
  std::scoped_lock lock(ConfigMtx);
  SafeFileResult a{};
  a.ino = 10; a.mtime = 100; a.ctime = 100; a.size = 42; a.found = true;
  recordConfigMetaLocked(a);
  EXPECT_TRUE(configMetaUnchangedLocked(a));

  SafeFileResult b = a; b.size = 43;
  EXPECT_FALSE(configMetaUnchangedLocked(b));
  SafeFileResult c = a; c.ctime = 101;
  EXPECT_FALSE(configMetaUnchangedLocked(c));
  SafeFileResult d = a; d.ino = 11;
  EXPECT_FALSE(configMetaUnchangedLocked(d));
  SafeFileResult e = a; e.mtime = 101;
  EXPECT_FALSE(configMetaUnchangedLocked(e));

  JWTConfigStatValid = false;

  SafeFileResult bad{};
  bad.ino = 7; bad.mtime = 1; bad.ctime = 2; bad.size = 9; bad.found = true;
  recordRejectedConfigMetaLocked(bad);
  EXPECT_TRUE(rejectedConfigMetaLocked(bad));
  SafeFileResult edited = bad; edited.size = 10;
  EXPECT_FALSE(rejectedConfigMetaLocked(edited));
  clearRejectedConfigMetaLocked();
  EXPECT_FALSE(rejectedConfigMetaLocked(bad));
}

// --- Server-side -maxsz is honoured ------------------------------------------

TEST(XrdSecJWTTest, AuthenticateHonoursConfiguredMaxTokenSize)
{
  ResetGlobalsForTest();
  MaxTokSize = 65536;
  XrdNetAddrInfo addr;
  XrdSecProtocoljwt prot("host.example", addr);

  // A 20000-byte token exceeds the old hard-coded 8192 limit but is within
  // -maxsz; it must reach the validator (and fail there for not being a JWT),
  // not be rejected up front as "Credential too large".
  const int tlen = 20000;
  const int bsz = 4 + tlen + 1;
  char *b = static_cast<char *>(malloc(bsz));
  memcpy(b, "jwt", 4);
  memset(b + 4, 'x', tlen);
  b[4 + tlen] = '\0';
  XrdSecCredentials c(b, bsz);
  XrdOucErrInfo erp;
  EXPECT_EQ(prot.Authenticate(&c, nullptr, &erp), -1);
  int ec = 0;
  const std::string et = erp.getErrText(ec);
  EXPECT_EQ(et.find("too large"), std::string::npos) << et;
  EXPECT_NE(et.find("not JWT"), std::string::npos) << et;

  // And the limit still applies once -maxsz is lowered below the token size.
  MaxTokSize = 8192;
  XrdSecProtocoljwt prot2("host.example", addr);
  XrdOucErrInfo erp2;
  EXPECT_EQ(prot2.Authenticate(&c, nullptr, &erp2), -1);
  const std::string et2 = erp2.getErrText(ec);
  EXPECT_NE(et2.find("too large"), std::string::npos) << et2;
}
