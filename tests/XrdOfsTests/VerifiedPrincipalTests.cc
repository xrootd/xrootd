// Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSys/XrdSysLogger.hh"
#include <gtest/gtest.h>
#include <scitokens/scitokens.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unistd.h>

TEST(VerifiedPrincipal, SciTokensPublishesVerifiedIssuerAndSubject) {
  char directory[] = "/tmp/xrd-principal-XXXXXX";
  ASSERT_NE(mkdtemp(directory), nullptr);
  const std::string root(directory);
  struct Cleanup { std::string root; ~Cleanup() { std::filesystem::remove_all(root); } } cleanup{root};
  ASSERT_EQ(setenv("XDG_CACHE_HOME", directory, 1), 0);
  const auto server = root + "/xrootd.cfg", config = root + "/scitokens.cfg";
  std::ofstream(server) << "scitokens.trace error\n";
  ASSERT_EQ(setenv("XRDCONFIGFN", server.c_str(), 1), 0);
  std::ofstream cfg(config);
  cfg << "[Global]\naudience = https://wlcg.cern.ch/jwt/v1/any\n";
  for (auto issuer : {"https://issuer-one.invalid", "https://issuer-two.invalid"})
    cfg << "[Issuer " << issuer << "]\nissuer = " << issuer
        << "\nbase_path = /\ndefault_user = shared\n";
  cfg.close();

  auto *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
  ASSERT_NE(ctx, nullptr);
  std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> context(ctx, EVP_PKEY_CTX_free);
  ASSERT_EQ(EVP_PKEY_keygen_init(ctx), 1);
  ASSERT_EQ(EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_X9_62_prime256v1), 1);
  EVP_PKEY *rawKey = nullptr;
  ASSERT_EQ(EVP_PKEY_keygen(ctx, &rawKey), 1);
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(rawKey, EVP_PKEY_free);
  std::unique_ptr<BIO, decltype(&BIO_free)> pub(BIO_new(BIO_s_mem()), BIO_free), priv(BIO_new(BIO_s_mem()), BIO_free);
  ASSERT_EQ(PEM_write_bio_PUBKEY(pub.get(), key.get()), 1);
  ASSERT_EQ(PEM_write_bio_PrivateKey(priv.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr), 1);
  char *p = nullptr;
  auto length = BIO_get_mem_data(pub.get(), &p); const std::string publicKey(p, length);
  length = BIO_get_mem_data(priv.get(), &p); const std::string privateKey(p, length);
  char *error = nullptr;
  for (auto issuer : {"https://issuer-one.invalid", "https://issuer-two.invalid"})
    ASSERT_EQ(scitoken_store_public_ec_key(issuer, "test", publicKey.c_str(), &error), 0) << (error ? error : "");
  auto signing = scitoken_key_create("test", "ES256", publicKey.c_str(), privateKey.c_str(), &error);
  ASSERT_NE(signing, nullptr);
  std::unique_ptr<void, decltype(&scitoken_key_destroy)> signingKey(signing, scitoken_key_destroy);

  // Load the real verifier plugin, including signature and scope enforcement.
  auto *library = dlopen(SCITOKENS_MODULE, RTLD_NOW | RTLD_LOCAL);
  ASSERT_NE(library, nullptr) << dlerror();
  using Factory = XrdAccAuthorize *(*)(XrdSysLogger *, const char *, const char *);
  auto factory = reinterpret_cast<Factory>(dlsym(library, "XrdAccAuthorizeObject"));
  ASSERT_NE(factory, nullptr);
  XrdSysLogger logger;
  auto *auth = factory(&logger, server.c_str(), ("config=" + config).c_str());
  ASSERT_NE(auth, nullptr);
  XrdSecEntity client("unix"); client.name = const_cast<char *>("shared");
  for (auto issuer : {"https://issuer-one.invalid", "https://issuer-two.invalid"}) {
    auto raw = scitoken_create(signing);
    std::unique_ptr<void, decltype(&scitoken_destroy)> token(raw, scitoken_destroy);
    ASSERT_NE(raw, nullptr);
    ASSERT_EQ(scitoken_set_claim_string(raw, "iss", issuer, &error), 0);
    ASSERT_EQ(scitoken_set_claim_string(raw, "sub", "alice", &error), 0);
    ASSERT_EQ(scitoken_set_claim_string(raw, "scope", "storage.read:/allowed", &error), 0);
    scitoken_set_serialize_profile(raw, SciTokenProfile::WLCG_1_0);
    scitoken_set_lifetime(raw, 120);
    char *serialized = nullptr;
    ASSERT_EQ(scitoken_serialize(raw, &serialized, &error), 0);
    std::string encoded(serialized); free(serialized);
    XrdOucEnv env(("authz=" + encoded).c_str());
    ASSERT_NE(auth->Access(&client, "/allowed/file", AOP_Read, &env), XrdAccPriv_None);
    std::string value;
    ASSERT_TRUE(client.eaAPI->Get("token.issuer", value)); EXPECT_EQ(value, issuer);
    ASSERT_TRUE(client.eaAPI->Get("token.subject", value)); EXPECT_EQ(value, "alice");
    ASSERT_TRUE(client.eaAPI->Get("request.name", value)); EXPECT_EQ(value, "shared");
    EXPECT_EQ(auth->Access(&client, "/denied", AOP_Read, &env), XrdAccPriv_None);
    client.eaAPI->Add("token.issuer", "", true);
    client.eaAPI->Add("token.subject", "", true);
    auto signature = encoded.rfind('.') + 1;
    encoded[signature] = encoded[signature] == 'A' ? 'B' : 'A';
    XrdOucEnv invalid(("authz=" + encoded).c_str());
    EXPECT_EQ(auth->Access(&client, "/allowed/file", AOP_Read, &invalid), XrdAccPriv_None);
    client.eaAPI->Get("token.issuer", value); EXPECT_TRUE(value.empty());
    client.eaAPI->Get("token.subject", value); EXPECT_TRUE(value.empty());
  }
  // Factory objects and module stay alive until process exit, as in the server.
}
