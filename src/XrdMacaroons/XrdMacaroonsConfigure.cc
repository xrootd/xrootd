
#include <fcntl.h>

#include <openssl/bio.h>
#include <openssl/evp.h>

#include <XrdOuc/XrdOucStream.hh>

#include "XrdMacaroonsHandler.hh"


using namespace Macaroons;


static bool xonmissing(XrdOucStream &config_obj, XrdSysError *log, Handler::AuthzBehavior &behavior)
{
  char *val = config_obj.GetWord();
  if (!val || !val[0])
  {
    log->Emsg("Config", "macaroons.onmissing requires a value (valid values: passthrough [default], allow, deny)");
    return false;
  }
  if (!strcasecmp(val, "passthrough")) {
    behavior = Handler::AuthzBehavior::PASSTHROUGH;
  } else if (!strcasecmp(val, "allow")) {
    behavior = Handler::AuthzBehavior::ALLOW;
  } else if (!strcasecmp(val, "deny")) {
    behavior = Handler::AuthzBehavior::DENY;
  } else
  {
    log->Emsg("Config", "macaroons.onmissing is invalid (valid values: passthrough [default], allow, deny)! Provided value:", val);
    return false;
  }
  return true;
}

bool Handler::Config(const char *config, XrdOucEnv *env, XrdSysError *log,
    std::string &location, std::string &secret, ssize_t &max_duration,
    AuthzBehavior &behavior)
{
  XrdOucStream config_obj(log, getenv("XRDINSTANCE"), env, "=====> ");

  // Open and attach the config file
  //
  int cfg_fd;
  if ((cfg_fd = open(config, O_RDONLY, 0)) < 0) {
    return log->Emsg("Config", errno, "open config file", config);
  }
  config_obj.Attach(cfg_fd);

  // Set default mask for logging.
  log->setMsgMask(LogMask::Error | LogMask::Warning);

  // Set default maximum duration (24 hours).
  max_duration = 24*3600;

  // Process items
  //
  char *orig_var, *var;
  bool success = true, ismine;
  while ((orig_var = config_obj.GetMyFirstWord())) {
    var = orig_var;
    if ((ismine = !strncmp("all.sitename", var, 12))) var += 4;
    else if ((ismine = !strncmp("macaroons.", var, 10)) && var[10]) var += 10;

    

    if (!ismine) {continue;}

    if (!strcmp("secretkey", var)) {success = xsecretkey(config_obj, log, secret);}
    else if (!strcmp("sitename", var)) {success = xsitename(config_obj, log, location);}
    else if (!strcmp("trace", var)) {success = xtrace(config_obj, log);}
    else if (!strcmp("maxduration", var)) {success = xmaxduration(config_obj, log, max_duration);}
    else if (!strcmp("onmissing", var)) {success = xonmissing(config_obj, log, behavior);}
    else {
        log->Say("Config warning: ignoring unknown directive '", orig_var, "'.");
        config_obj.Echo();
        continue;
    }
    if (!success) {
        config_obj.Echo();
        break;
    }
  }

  if (success && !location.size())
  {
    log->Emsg("Config", "all.sitename must be specified to use macaroons.");
    return false;
  }

  return success;
}


bool Handler::xtrace(XrdOucStream &config_obj, XrdSysError *log)
{
    char *val = config_obj.GetWord();
    if (!val || !val[0])
    {
        log->Emsg("Config", "macaroons.trace requires at least one directive [all | error | warning | info | debug | none]");
        return false;
    }
    // If the config option is given, reset the log mask.
    log->setMsgMask(0);

    do {
        if (!strcmp(val, "all"))
        {
            log->setMsgMask(log->getMsgMask() | LogMask::All);
        }
        else if (!strcmp(val, "error"))
        {
            log->setMsgMask(log->getMsgMask() | LogMask::Error);
        }
        else if (!strcmp(val, "warning"))
        {
            log->setMsgMask(log->getMsgMask() | LogMask::Warning);
        }
        else if (!strcmp(val, "info"))
        {
            log->setMsgMask(log->getMsgMask() | LogMask::Info);
        }
        else if (!strcmp(val, "debug"))
        {
            log->setMsgMask(log->getMsgMask() | LogMask::Debug);
        }
        else if (!strcmp(val, "none"))
        {
            log->setMsgMask(0);
        }
        else
        {
            log->Emsg("Config", "macaroons.trace encountered an unknown directive:", val);
            return false;
        }
        val = config_obj.GetWord();
    } while (val);

    return true;
}


bool Handler::xmaxduration(XrdOucStream &config_obj, XrdSysError *log, ssize_t &max_duration)
{
  char *val = config_obj.GetWord();
  if (!val || !val[0])
  {
    log->Emsg("Config", "macaroons.maxduration requires a value");
    return false;
  }
  char *endptr = NULL;
  long int max_duration_parsed = strtoll(val, &endptr, 10);
  if (endptr == val)
  {
    log->Emsg("Config", "Unable to parse macaroons.maxduration as an integer", val);
    return false;
  }
  if (errno != 0)
  {
    log->Emsg("Config", "Failure when parsing macaroons.maxduration as an integer", strerror(errno));
  }
  max_duration = max_duration_parsed;

  return true;
}

bool Handler::xsitename(XrdOucStream &config_obj, XrdSysError *log, std::string &location)
{
  char *val = config_obj.GetWord();
  if (!val || !val[0])
  {
    log->Emsg("Config", "all.sitename requires a name");
    return false;
  }

  location = val;
  return true;
}

bool Handler::xsecretkey(XrdOucStream &config_obj, XrdSysError *log, std::string &secret)
{
  char *val = config_obj.GetWord();
  if (!val || !val[0])
  {
    log->Emsg("Config", "Shared secret key not specified");
    return false;
  }

  FILE *fp = fopen(val, "rb");

  if (fp == NULL) {
    log->Emsg("Config", "Cannot open shared secret key file '", val, "'");
    log->Emsg("Config", "Cannot open shared secret key file. err: ", strerror(errno));
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
    log->Emsg("Config", "Failure when reading secret key", strerror(errno));
    return false;
  }
  if (!BIO_flush(bio_out)) {
    BIO_free_all(b64);
    BIO_free_all(bio_out);
    log->Emsg("Config", "Failure when flushing secret key", strerror(errno));
    return false;
  }

  char *decoded;
  long data_len = BIO_get_mem_data(bio_out, &decoded);
  BIO_free_all(b64);

  secret = std::string(decoded, data_len);

  BIO_free_all(bio_out);

  if (secret.size() < 32) {
    log->Emsg("Config", "Secret key is too short; must be 32 bytes long.  Try running 'openssl rand -base64 -out", val, "64' to generate a new key");
    return false;
  }

  return true;
}
