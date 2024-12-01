

///
// Given a PEM-formatted EC public key, create an equivalent JWKS file
// with the specified KID

/*

Example JWKS:

```
{
  "keys": [
    {
      "alg": "ES256",
      "kid": "key",
      "kty": "EC",
      "use": "sig",
      "x": "ncSCrGTBTXXOhNiAOTwNdPjwRz1hVY4saDNiHQK9Bh4=",
      "y": "sCsFXvx7FAAklwq3CzRCBcghqZOFPB2dKUayS6LY_Lo="
    }
  ]
}
```

*/

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/param_build.h>
#endif
#include <openssl/pem.h>

#define EC_NAME NID_X9_62_prime256v1


#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

std::string b64url_encode_nopadding(std::string_view input) {
    std::unique_ptr<BIO, decltype(&BIO_free_all)> mem_bio(
        BIO_new(BIO_s_mem()), BIO_free_all);
    std::unique_ptr<BIO, decltype(&BIO_free_all)> b64_bio(
        BIO_new(BIO_f_base64()), BIO_free_all);
    BIO_push(b64_bio.get(), mem_bio.get());
    auto mem_ptr = mem_bio.release();
    BIO_set_flags(b64_bio.get(), BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64_bio.get(), input.data(), input.length());
    BIO_flush(b64_bio.get());

    char *b64_data;
    auto len = BIO_get_mem_data(mem_ptr, &b64_data);
    if (len < 0) {
        return "";
    }

    std::string result(b64_data, len);
    std::replace(result.begin(), result.end(), '+', '-');
    std::replace(result.begin(), result.end(), '/', '_');
    return result.substr(0, result.find('='));
}

bool readPubkey(const std::string &fname, std::string &x, std::string &y) {
    std::unique_ptr<BIO, decltype(&BIO_free_all)> pubkey_bio(
        BIO_new_file(fname.c_str(), "r"), BIO_free_all);

    if (!pubkey_bio) {
      std::cerr << "Failed to load public key from " << fname << ": " << strerror(errno) << std::endl;
      return false;
    }

    std::unique_ptr<BIGNUM, decltype(&BN_free)> x_bignum(BN_new(), BN_free);
    std::unique_ptr<BIGNUM, decltype(&BN_free)> y_bignum(BN_new(), BN_free);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey(
        PEM_read_bio_PUBKEY(pubkey_bio.get(), nullptr, nullptr, nullptr),
        EVP_PKEY_free);
    if (!pkey.get()) {
        return false;
    }

    std::unique_ptr<EC_GROUP, decltype(&EC_GROUP_free)> ec_group(
        EC_GROUP_new_by_curve_name(EC_NAME), EC_GROUP_free);
    if (!ec_group.get()) {
        std::cerr << "Unable to get OpenSSL EC group" << std::endl;
        return false;
    }

    std::unique_ptr<EC_POINT, decltype(&EC_POINT_free)> q_point(
        EC_POINT_new(ec_group.get()), EC_POINT_free);
    if (!q_point.get()) {
        std::cerr << "Unable to get OpenSSL EC point" << std::endl;
        return false;
    }

    OSSL_PARAM *params;
    if (!EVP_PKEY_todata(pkey.get(), EVP_PKEY_PUBLIC_KEY, &params)) {
        std::cerr << "Unable to get OpenSSL public key parameters" << std::endl;
        return false;
    }

    void *buf = NULL;
    size_t buf_len, max_len = 256;
    OSSL_PARAM *p = OSSL_PARAM_locate(params, "pub");
    if (!p || !OSSL_PARAM_get_octet_string(p, &buf, max_len, &buf_len) ||
        !EC_POINT_oct2point(ec_group.get(), q_point.get(),
                            static_cast<unsigned char *>(buf), buf_len,
                            nullptr)) {
        std::cerr << "Failed to to set OpenSSL EC point with public key information" << std::endl;
        return false;
    }

    if (!EC_POINT_get_affine_coordinates(ec_group.get(), q_point.get(),
                                         x_bignum.get(), y_bignum.get(),
                                         NULL)) {
        std::cerr << "Unable to get OpenSSL affine coordinates" << std::endl;
        return false;
    }

    OSSL_PARAM_free(params);
#else
    std::unique_ptr<EC_KEY, decltype(&EC_KEY_free)> pkey(
        PEM_read_bio_EC_PUBKEY(pubkey_bio.get(), nullptr, nullptr, nullptr),
        EC_KEY_free);
    if (!pkey) {
        return false;
    }

    EC_GROUP *params = (EC_GROUP *)EC_KEY_get0_group(pkey.get());
    if (!params) {
        std::cerr << "Unable to get OpenSSL EC group" << std::endl;
        return false;
    }

    const EC_POINT *point = EC_KEY_get0_public_key(pkey.get());
    if (!point) {
        std::cerr << "Unable to get OpenSSL EC point" << std::endl;
        return false;
    }

    if (!EC_POINT_get_affine_coordinates_GFp(params, point, x_bignum.get(),
                                             y_bignum.get(), nullptr)) {
        std::cerr << "Unable to get OpenSSL affine coordinates" << std::endl;
        return false;
    }
#endif

    auto x_num = BN_num_bytes(x_bignum.get());
    auto y_num = BN_num_bytes(y_bignum.get());
    std::vector<unsigned char> x_bin;
    x_bin.resize(x_num);
    std::vector<unsigned char> y_bin;
    y_bin.resize(y_num);
    BN_bn2bin(x_bignum.get(), &x_bin[0]);
    BN_bn2bin(y_bignum.get(), &y_bin[0]);
    x = b64url_encode_nopadding(std::string(reinterpret_cast<char *>(&x_bin[0]), x_num));
    y = b64url_encode_nopadding(std::string(reinterpret_cast<char *>(&y_bin[0]), y_num));

    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " issuer.pem issuer.jwks kid" << std::endl;
        return 1;
    }

    std::string x, y;
    if (!readPubkey(argv[1], x, y)) {
        std::cerr << "Failed to read EC pubkey " << argv[1] << std::endl;
        return 2;
    }

    auto fd = fopen(argv[2], "w");
    if (!fd) {
        std::cerr << "Failed to open output JWKS " << argv[2] << ": " << strerror(errno) << std::endl;
        return 3;
    }

    auto rc = fprintf(fd, R"(
{
  "keys": [
    {
      "alg": "ES256",
      "crv": "P-256",
      "kid": "%s",
      "kty": "EC",
      "use": "sig",
      "x": "%s",
      "y": "%s"
    }
  ]
}
)",
    argv[3],
    x.c_str(),
    y.c_str());

    if (rc < 0) {
        std::cerr << "Error writing to output file: " << strerror(errno) << std::endl;
        return 4;
    }
    return 0;
}
