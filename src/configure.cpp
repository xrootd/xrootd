
#include <fcntl.h>

#include <openssl/bio.h>
#include <openssl/evp.h>

#include <XrdOuc/XrdOucStream.hh>

#include "handler.hh"


using namespace Macaroons;

#define TS_Xeq(x, m) (!strcmp(x, var)) success = m(config_obj)
bool Handler::Config(const char *config, XrdOucEnv *env)
{
  XrdOucStream config_obj(m_log, getenv("XRDINSTANCE"), env, "=====> ");

  // Open and attach the config file
  //
  int cfg_fd;
  if ((cfg_fd = open(config, O_RDONLY, 0)) < 0) {
    return m_log->Emsg("Config", errno, "open config file", config);
  }
  config_obj.Attach(cfg_fd);

  // Process items
  //
  char *orig_var, *var;
  bool success = true, ismine;
  while ((orig_var = config_obj.GetMyFirstWord())) {
    var = orig_var;
    if ((ismine = !strncmp("all.sitename", var, 12))) var += 4;
    else if ((ismine = !strncmp("macaroons.", var, 10)) && var[10]) var += 10;

    

    if (!ismine) {continue;}

    if TS_Xeq("secretkey", xsecretkey);
    else if TS_Xeq("sitename", xsitename);
    else {
        m_log->Say("Config warning: ignoring unknown directive '", orig_var, "'.");
        config_obj.Echo();
        continue;
    }
    if (!success) {
        config_obj.Echo();
        break;
    }
  }

  if (success && !m_location.size())
  {
    m_log->Emsg("Config", "all.sitename must be specified to use macaroons.");
    return false;
  }

  return success;
}


bool Handler::xsitename(XrdOucStream &config_obj)
{
  char *val = config_obj.GetWord();
  if (!val || !val[0])
  {
    m_log->Emsg("Config", "all.sitename requires a name");
    return false;
  }

  m_location = val;
  return true;
}

bool Handler::xsecretkey(XrdOucStream &config_obj)
{
  char *val = config_obj.GetWord();
  if (!val || !val[0])
  {
    m_log->Emsg("Config", "Shared secret key not specified");
    return false;
  }

  FILE *fp = fopen(val, "r+");

  if (fp == NULL) {
    m_log->Emsg("Config", "Cannot open shared secret key file '", val, "'");
    m_log->Emsg("Config", "Cannot open shared secret key file. err: ", strerror(errno));
    return false;
  }

  BIO *bio, *b64, *bio_out;
  char inbuf[512];
  int inlen;

  b64 = BIO_new(BIO_f_base64());
  if (!b64)
  {
    m_log->Emsg("Config", "Failed to allocate base64 filter");
    return false;
  }
  bio = BIO_new_fp(fp, 0); // fp will be closed when BIO is freed.
  if (!bio)
  {
    BIO_free_all(b64);
    m_log->Emsg("Config", "Failed to allocate BIO filter");
    return false;
  }
  bio_out = BIO_new(BIO_s_mem());
  if (!bio_out)
  {
    BIO_free_all(b64);
    BIO_free_all(bio);
    m_log->Emsg("Config", "Failed to allocate BIO output");
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
    m_log->Emsg("Config", "Failure when reading secret key", strerror(errno));
    return false;
  }
  if (!BIO_flush(bio_out)) {
    BIO_free_all(b64);
    BIO_free_all(bio_out);
    m_log->Emsg("Config", "Failure when flushing secret key", strerror(errno));
    return false;
  }

  char *decoded;
  long data_len = BIO_get_mem_data(bio_out, &decoded);
  BIO_free_all(b64);

  m_secret = std::string(decoded, data_len);

  BIO_free_all(bio_out);

  if (m_secret.size() < 32) {
    m_log->Emsg("Config", "Secret key is too short; must be 32 bytes long.  Try running 'openssl rand -base64 -out", val, "64' to generate a new key");
    return false;
  }

  return true;
}
