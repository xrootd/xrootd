
#include "XrdMacaroonsUtils.hh"

#include "XrdSys/XrdSysError.hh"

#include <openssl/bio.h>
#include <openssl/evp.h>

#include <string>

bool
Macaroons::GetSecretKey(const std::string &filename, XrdSysError *log, std::string &secret)
{
    FILE *fp = fopen(filename.c_str(), "rb");

    if (fp == NULL) {
        log->Emsg("Config", errno, "open shared secret key file", filename.c_str());
        return false;
    }

    BIO *bio, *b64, *bio_out;
    char inbuf[512];
    int inlen;

    b64 = BIO_new(BIO_f_base64());
    if (!b64)
    {
        log->Emsg("Config", "Failed to allocate base64 filter");
        return false;
    }
    bio = BIO_new_fp(fp, 0); // fp will be closed when BIO is freed.
    if (!bio)
    {
        BIO_free_all(b64);
        log->Emsg("Config", "Failed to allocate BIO filter");
        return false;
    }
    bio_out = BIO_new(BIO_s_mem());
    if (!bio_out)
    {
        BIO_free_all(b64);
        BIO_free_all(bio);
        log->Emsg("Config", "Failed to allocate BIO output");
        return false;
    }

    BIO_push(b64, bio);
    while ((inlen = BIO_read(b64, inbuf, 512)) > 0)
    {
        if (inlen < 0) {
            if (errno == EINTR) continue;
            break;
        } else {
            BIO_write(bio_out, inbuf, inlen);
        }
    }
    if (inlen < 0) {
        BIO_free_all(b64);
        BIO_free_all(bio_out);
        log->Emsg("Config", errno, "read secret key.");
        return false;
    }
    if (!BIO_flush(bio_out)) {
        BIO_free_all(b64);
        BIO_free_all(bio_out);
        log->Emsg("Config", errno, "flush secret key.");
        return false;
    }

    char *decoded;
    long data_len = BIO_get_mem_data(bio_out, &decoded);
    BIO_free_all(b64);

    secret = std::string(decoded, data_len);

    BIO_free_all(bio_out);

    if (secret.size() < 32) {
        log->Emsg("Config", "Secret key is too short; must be 32 bytes long.  Try running 'openssl rand -base64 -out", filename.c_str(), "64' to generate a new key");
        return false;
    }

    return true;
}

std::string Macaroons::NormalizeSlashes(const std::string &input)
{
    std::string output;
    // In most cases, the output should be "about as large"
    // as the input
    output.reserve(input.size());
    char prior_chr = '\0';
    size_t output_idx = 0;
    for (size_t idx = 0; idx < input.size(); idx++) {
        char chr = input[idx];
        if (prior_chr == '/' && chr == '/') {
            output_idx++;
            continue;
        }
        output += input[output_idx];
        prior_chr = chr;
        output_idx++;
    }
    return output;
}
