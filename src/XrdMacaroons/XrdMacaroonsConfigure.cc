
#include <fcntl.h>
#include <cerrno>

#include <openssl/bio.h>
#include <openssl/evp.h>

#include <XrdOuc/XrdOucEnv.hh>
#include <XrdOuc/XrdOucGatherConf.hh>
#include <XrdOuc/XrdOucStream.hh>
#include <XrdSys/XrdSysE2T.hh>

#include "XrdMacaroonsHandler.hh"


using namespace Macaroons;

bool XrdMacaroonsConfigFactory::m_configured = false;
XrdMacaroonsConfig XrdMacaroonsConfigFactory::m_config;


static bool xonmissing(XrdOucGatherConf &config_obj, XrdSysError &log, AuthzBehavior &behavior)
{
  char *val = config_obj.GetToken();
  if (!val || !val[0])
  {
    log.Emsg("Config", "macaroons.onmissing requires a value (valid values: passthrough [default], allow, deny)");
    return false;
  }
  if (!strcasecmp(val, "passthrough")) {
    behavior = AuthzBehavior::PASSTHROUGH;
  } else if (!strcasecmp(val, "allow")) {
    behavior = AuthzBehavior::ALLOW;
  } else if (!strcasecmp(val, "deny")) {
    behavior = AuthzBehavior::DENY;
  } else
  {
    log.Emsg("Config", "macaroons.onmissing is invalid (valid values: passthrough [default], allow, deny)! Provided value:", val);
    return false;
  }
  return true;
}

const XrdMacaroonsConfig &
XrdMacaroonsConfigFactory::Get(XrdSysError &log) {
  if (m_configured) {
    return m_config;
  }
  if (!Config(log)) {
    throw std::runtime_error("failed to generate the macaroons configuration");
  }
  log.setMsgMask(m_config.mask);
  m_configured = true;
  return m_config;
}


bool XrdMacaroonsConfigFactory::Config(XrdSysError &log)
{
  char *config_filename = nullptr;
  if (!XrdOucEnv::Import("XRDCONFIGFN", config_filename)) {
    return false;
  }
  XrdOucGatherConf macaroons_conf("macaroons.trace macaroons.secretkey all.sitename macaroons.maxduration macaroons.onmissing", &log);
  int result;
  if ((result = macaroons_conf.Gather(config_filename, XrdOucGatherConf::trim_lines)) < 0) {
    log.Emsg("Config", -result, "parsing config file", config_filename);
    return false;
  }

  // Set default mask for logging.
  m_config.mask = LogMask::Error | LogMask::Warning;

  // Set default maximum duration (24 hours).
  m_config.maxDuration = 24*3600;

  // Process items
  //
  bool success = true;
  while (macaroons_conf.GetLine()) {
    auto directive = macaroons_conf.GetToken();
    if (!strcmp(directive, "secretkey")) {success = xsecretkey(macaroons_conf, log, m_config.secret);}
    else if (!strcmp("sitename", directive)) {success = xsitename(macaroons_conf, log, m_config.site);}
    else if (!strcmp("trace", directive)) {success = xtrace(macaroons_conf, log);}
    else if (!strcmp("maxduration", directive)) {success = xmaxduration(macaroons_conf, log, m_config.maxDuration);}
    else if (!strcmp("onmissing", directive)) {success = xonmissing(macaroons_conf, log, m_config.behavior);}
    else {
        log.Say("Config warning: ignoring unknown directive '", directive, "'.");
        macaroons_conf.EchoLine();
        continue;
    }
    if (!success) {
        log.Emsg("MacaroonsConfig", "failed to process configuration directive");
        macaroons_conf.EchoLine();
        break;
    }
  }

  if (success) {
    if (m_config.site.empty())
    {
      log.Emsg("Config", "all.sitename must be specified to use macaroons.");
      return false;
    }
    if (m_config.secret.empty())
    {
      log.Emsg("Config", "macaroons.secretkey must be specified and the file non-empty to use macaroons.");
      return false;
    }
  }
  m_config.mask = log.getMsgMask();

  return success;
}


bool XrdMacaroonsConfigFactory::xtrace(XrdOucGatherConf &Config, XrdSysError &log)
{
  static struct traceopts { const char *opname; enum LogMask opval; } tropts[] = {
    { "all",     LogMask::All     },
    { "error",   LogMask::Error   },
    { "warning", LogMask::Warning },
    { "info",    LogMask::Info    },
    { "debug",   LogMask::Debug   }
  };

  int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);

  char *val = Config.GetToken();

  if (!val || !*val) {
    log.Emsg("Config", "macaroons.trace requires at least one directive"
                        " [ all | error | warning | info | debug | none | off ]");
    return false;
  }

  while (val && *val) {
    if (strcmp(val, "off") == 0 || strcmp(val, "none") == 0) {
      trval = 0;
    } else {
      if ((neg = (val[0] == '-' && val[1])))
        val++;
      for (i = 0; i < numopts; i++) {
        if (!strcmp(val, tropts[i].opname)) {
          if (neg)
            trval &= ~tropts[i].opval;
          else
            trval |= tropts[i].opval;
          break;
        }
      }
      if (neg) --val;
      if (i >= numopts)
        log.Emsg("Config", "macaroons.trace: ignoring invalid trace option:", val);
    }
    val = Config.GetToken();
  }

  log.setMsgMask(trval);

  return true;
}

bool XrdMacaroonsConfigFactory::xmaxduration(XrdOucGatherConf &config_obj, XrdSysError &log, ssize_t &max_duration)
{
  char *val = config_obj.GetToken();
  if (!val || !val[0])
  {
    log.Emsg("Config", "macaroons.maxduration requires a value");
    return false;
  }
  char *endptr = NULL;
  long int max_duration_parsed = strtoll(val, &endptr, 10);
  if (endptr == val)
  {
    log.Emsg("Config", "Unable to parse macaroons.maxduration as an integer", val);
    return false;
  }
  if (errno != 0)
  {
    log.Emsg("Config", errno, "parse macaroons.maxduration as an integer.");
  }
  max_duration = max_duration_parsed;

  return true;
}

bool XrdMacaroonsConfigFactory::xsitename(XrdOucGatherConf &config_obj, XrdSysError &log, std::string &location)
{
  char *val = config_obj.GetToken();
  if (!val || !val[0])
  {
    log.Emsg("Config", "all.sitename requires a name");
    return false;
  }

  location = val;
  return true;
}

bool XrdMacaroonsConfigFactory::xsecretkey(XrdOucGatherConf &config_obj, XrdSysError &log, std::string &secret)
{
  char *val = config_obj.GetToken();
  if (!val || !val[0])
  {
    log.Emsg("Config", "Shared secret key not specified");
    return false;
  }

  FILE *fp = fopen(val, "rb");

  if (fp == NULL) {
    log.Emsg("Config", errno, "open shared secret key file", val);
    return false;
  }

  BIO *bio, *b64, *bio_out;
  char inbuf[512];
  int inlen;

  b64 = BIO_new(BIO_f_base64());
  if (!b64)
  {
    log.Emsg("Config", "Failed to allocate base64 filter");
    return false;
  }
  bio = BIO_new_fp(fp, 0); // fp will be closed when BIO is freed.
  if (!bio)
  {
    BIO_free_all(b64);
    log.Emsg("Config", "Failed to allocate BIO filter");
    return false;
  }
  bio_out = BIO_new(BIO_s_mem());
  if (!bio_out)
  {
    BIO_free_all(b64);
    BIO_free_all(bio);
    log.Emsg("Config", "Failed to allocate BIO output");
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
    log.Emsg("Config", errno, "read secret key.");
    return false;
  }
  if (!BIO_flush(bio_out)) {
    BIO_free_all(b64);
    BIO_free_all(bio_out);
    log.Emsg("Config", errno, "flush secret key.");
    return false;
  }

  char *decoded;
  long data_len = BIO_get_mem_data(bio_out, &decoded);
  BIO_free_all(b64);

  secret = std::string(decoded, data_len);

  BIO_free_all(bio_out);

  if (secret.size() < 32) {
    log.Emsg("Config", "Secret key is too short; must be 32 bytes long.  Try running 'openssl rand -base64 -out", val, "64' to generate a new key");
    return false;
  }

  return true;
}
