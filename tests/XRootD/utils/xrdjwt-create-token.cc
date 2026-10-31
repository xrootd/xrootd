// Mint an ES256 token, or a JWKS document, for the XrdSecjwt live test.
// OpenSSL only: the live test must not link scitokens-cpp.

#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

std::string b64url(const unsigned char *data, size_t len)
{
   if (!data || !len) return {};
   const int outLen = 4 * ((static_cast<int>(len) + 2) / 3);
   std::string out(static_cast<size_t>(outLen), '\0');
   const int n = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(&out[0]), data,
                                  static_cast<int>(len));
   if (n <= 0) return {};
   out.resize(static_cast<size_t>(n));
   for (char &c : out)
      {if (c == '+') c = '-';
       else if (c == '/') c = '_';
      }
   while (!out.empty() && out.back() == '=') out.pop_back();
   return out;
}

std::string b64url(std::string_view in)
{
   return b64url(reinterpret_cast<const unsigned char *>(in.data()), in.size());
}

bool jsonSafe(const std::string &s)
{
   if (s.empty()) return false;
   for (unsigned char c : s)
      if (c < 0x20 || c == '"' || c == '\\') return false;
   return true;
}

bool readPubXY(const std::string &pubPem, std::string &x, std::string &y)
{
   FILE *fp = fopen(pubPem.c_str(), "r");
   if (!fp)
      {std::cerr << "cannot read " << pubPem << ": " << strerror(errno) << "\n";
       return false;
      }
   EVP_PKEY *pkey = PEM_read_PUBKEY(fp, nullptr, nullptr, nullptr);
   fclose(fp);
   if (!pkey)
      {std::cerr << "not a PEM public key: " << pubPem << "\n";
       return false;
      }
   if (EVP_PKEY_base_id(pkey) != EVP_PKEY_EC)
      {std::cerr << "public key is not EC\n";
       EVP_PKEY_free(pkey);
       return false;
      }

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
   BIGNUM *bx = nullptr;
   BIGNUM *by = nullptr;
   int ok = EVP_PKEY_get_bn_param(pkey, "qx", &bx);
   ok = ok && EVP_PKEY_get_bn_param(pkey, "qy", &by);
   EVP_PKEY_free(pkey);
   if (!ok || !bx || !by)
      {std::cerr << "cannot read EC coordinates\n";
       BN_free(bx);
       BN_free(by);
       return false;
      }
#else
   const EC_KEY *ec = EVP_PKEY_get0_EC_KEY(pkey);
   const EC_GROUP *group = ec ? EC_KEY_get0_group(ec) : nullptr;
   const EC_POINT *point = ec ? EC_KEY_get0_public_key(ec) : nullptr;
   BIGNUM *bx = BN_new();
   BIGNUM *by = BN_new();
   int ok = ec && group && point && bx && by
         && EC_POINT_get_affine_coordinates_GFp(group, point, bx, by, nullptr);
   EVP_PKEY_free(pkey);
   if (!ok)
      {std::cerr << "cannot read EC coordinates\n";
       BN_free(bx);
       BN_free(by);
       return false;
      }
#endif

   std::vector<unsigned char> xb(32), yb(32);
   if (BN_bn2binpad(bx, xb.data(), 32) != 32 || BN_bn2binpad(by, yb.data(), 32) != 32)
      {std::cerr << "EC coordinates are not P-256\n";
       BN_free(bx);
       BN_free(by);
       return false;
      }
   BN_free(bx);
   BN_free(by);
   x = b64url(xb.data(), xb.size());
   y = b64url(yb.data(), yb.size());
   return !x.empty() && !y.empty();
}

int writeJwks(const char *pubPem, const char *outPath, const char *kid)
{
   if (!jsonSafe(kid))
      {std::cerr << "kid contains characters that cannot be embedded in JSON\n";
       return 2;
      }
   std::string x, y;
   if (!readPubXY(pubPem, x, y)) return 2;
   FILE *fp = fopen(outPath, "w");
   if (!fp)
      {std::cerr << "cannot write " << outPath << ": " << strerror(errno) << "\n";
       return 3;
      }
   const int n = fprintf(fp,
      "{\n"
      "  \"keys\": [{\n"
      "    \"alg\": \"ES256\",\n"
      "    \"crv\": \"P-256\",\n"
      "    \"kid\": \"%s\",\n"
      "    \"kty\": \"EC\",\n"
      "    \"use\": \"sig\",\n"
      "    \"x\": \"%s\",\n"
      "    \"y\": \"%s\"\n"
      "  }]\n"
      "}\n",
      kid, x.c_str(), y.c_str());
   fclose(fp);
   if (n < 0)
      {std::cerr << "failed to write JWKS\n";
       return 4;
      }
   return 0;
}

std::string signEs256(EVP_PKEY *pkey, const std::string &input)
{
   EVP_MD_CTX *ctx = EVP_MD_CTX_new();
   if (!ctx) return {};
   if (EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) != 1
   ||  EVP_DigestSignUpdate(ctx, input.data(), input.size()) != 1)
      {EVP_MD_CTX_free(ctx);
       return {};
      }
   size_t sigLen = 0;
   if (EVP_DigestSignFinal(ctx, nullptr, &sigLen) != 1)
      {EVP_MD_CTX_free(ctx);
       return {};
      }
   std::vector<unsigned char> der(sigLen);
   if (EVP_DigestSignFinal(ctx, der.data(), &sigLen) != 1)
      {EVP_MD_CTX_free(ctx);
       return {};
      }
   EVP_MD_CTX_free(ctx);

   const unsigned char *p = der.data();
   ECDSA_SIG *sig = d2i_ECDSA_SIG(nullptr, &p, static_cast<long>(sigLen));
   if (!sig) return {};
   const BIGNUM *r = nullptr;
   const BIGNUM *s = nullptr;
   ECDSA_SIG_get0(sig, &r, &s);
   std::vector<unsigned char> raw(64);
   int ok = r && s && BN_bn2binpad(r, raw.data(), 32) == 32
         && BN_bn2binpad(s, raw.data() + 32, 32) == 32;
   ECDSA_SIG_free(sig);
   if (!ok) return {};
   return b64url(raw.data(), raw.size());
}

int writeToken(const char *keyPem, const char *kid, const char *iss,
               const char *aud, const char *sub, long lifetime)
{
   if (!jsonSafe(kid) || !jsonSafe(iss) || !jsonSafe(aud) || !jsonSafe(sub))
      {std::cerr << "claim contains characters that cannot be embedded in JSON\n";
       return 2;
      }
   FILE *fp = fopen(keyPem, "r");
   if (!fp)
      {std::cerr << "cannot read " << keyPem << ": " << strerror(errno) << "\n";
       return 2;
      }
   EVP_PKEY *pkey = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
   fclose(fp);
   if (!pkey)
      {std::cerr << "not a PEM private key: " << keyPem << "\n";
       return 2;
      }

   const std::time_t now = std::time(nullptr);
   const std::string header = std::string("{\"alg\":\"ES256\",\"kid\":\"") + kid
                             + "\",\"typ\":\"JWT\"}";
   const std::string payload =
        std::string("{\"iss\":\"") + iss
      + "\",\"aud\":\"" + aud
      + "\",\"sub\":\"" + sub
      + "\",\"scope\":\"storage.read:/\""
      + ",\"iat\":" + std::to_string(static_cast<long long>(now - 30))
      + ",\"exp\":" + std::to_string(static_cast<long long>(now + lifetime))
      + "}";
   const std::string input = b64url(header) + "." + b64url(payload);
   const std::string sig = signEs256(pkey, input);
   EVP_PKEY_free(pkey);
   if (sig.empty())
      {std::cerr << "ES256 signature failed\n";
       return 4;
      }
   std::cout << input << "." << sig << "\n";
   return 0;
}

int main(int argc, char **argv)
{
   if (argc >= 2 && std::string(argv[1]) == "jwks")
      {if (argc != 5)
          {std::cerr << "Usage: " << argv[0]
                     << " jwks <pub.pem> <out.jwks> <kid>\n";
           return 1;
          }
       return writeJwks(argv[2], argv[3], argv[4]);
      }
   if (argc >= 2 && std::string(argv[1]) == "token")
      {if (argc != 7 && argc != 8)
          {std::cerr << "Usage: " << argv[0]
                     << " token <key.pem> <kid> <iss> <aud> <sub> [lifetime]\n";
           return 1;
          }
       long lifetime = 3600;
       if (argc == 8)
          {char *end = nullptr;
           lifetime = std::strtol(argv[7], &end, 10);
           if (!end || *end || argv[7][0] == '\0')
              {std::cerr << "lifetime must be an integer (seconds from now; negative expires)\n";
               return 1;
              }
          }
       return writeToken(argv[2], argv[3], argv[4], argv[5], argv[6], lifetime);
      }
   std::cerr << "Usage: " << argv[0] << " jwks|token ...\n";
   return 1;
}
