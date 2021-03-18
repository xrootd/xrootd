
#include "XrdTpcTPC.hh"

#include <dlfcn.h>
#include <fcntl.h>

#include "XrdOuc/XrdOuca2x.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucPinPath.hh"
#include "XrdSfs/XrdSfsInterface.hh"


using namespace TPC;


bool TPCHandler::Configure(const char *configfn, XrdOucEnv *myEnv)
{
    XrdOucEnv cfgEnv;
    XrdOucStream Config(&m_log, getenv("XRDINSTANCE"), &cfgEnv, "=====> ");

    m_log.setMsgMask(LogMask::Info | LogMask::Warning | LogMask::Error);

    std::string authLib;
    std::string authLibParms;
    int cfgFD = open(configfn, O_RDONLY, 0);
    if (cfgFD < 0) {
        m_log.Emsg("Config", errno, "open config file", configfn);
        return false;
    }
    Config.Attach(cfgFD);
    static const char *cvec[] = { "*** http tpc plugin config:", 0 };
    Config.Capture(cvec);
    const char *val;
    while ((val = Config.GetMyFirstWord())) {
        if (!strcmp("http.desthttps", val)) {
            if (!(val = Config.GetWord())) {
                Config.Close();
                m_log.Emsg("Config", "http.desthttps value not specified");
                return false;
            }
            if (!strcmp("1", val) || !strcasecmp("yes", val) || !strcasecmp("true", val)) {
                m_desthttps = true;
            } else if (!strcmp("0", val) || !strcasecmp("no", val) || !strcasecmp("false", val)) {
                m_desthttps = false;
            } else {
                Config.Close();
                m_log.Emsg("Config", "https.desthttps value is invalid", val);
                return false;
            }
        } else if (!strcmp("tpc.trace", val)) {
            if (!ConfigureLogger(Config)) {
                Config.Close();
                return false;
            }
        } else if (!strcmp("tpc.timeout", val)) {
            if (!(val = Config.GetWord())) {
                m_log.Emsg("Config","tpc.timeout value not specified.");  return false;
            }
            if (XrdOuca2x::a2tm(m_log, "timeout value", val, &m_timeout, 0)) return false;
                // First byte timeout can be set separately from the continuous timeout.
            if ((val = Config.GetWord())) {
                if (XrdOuca2x::a2tm(m_log, "first byte timeout value", val, &m_first_timeout, 0)) return false;
            } else {
                m_first_timeout = 2*m_timeout;
            }
        }
    }
    Config.Close();

    const char *cadir = nullptr, *cafile = nullptr;
    if ((cadir = myEnv->Get("http.cadir"))) {
        m_cadir = cadir;
        if (XrdTpcNSSSupport::NeedsNSSHack()) {
            m_nss_hack.reset(new XrdTpcNSSSupport(&m_log, m_cadir));
        }
    }
    if ((cafile = myEnv->Get("http.cafile"))) {
        m_cafile = cafile;
    }
    if (!cadir && !cafile) {
        m_log.Emsg("Config", "neither xrd.tls cadir nor certfile value specified; is TLS enabled?");
        return false;
    }

    void *sfs_raw_ptr;
    if ((sfs_raw_ptr = myEnv->GetPtr("XrdSfsFileSystem*"))) {
        m_sfs = static_cast<XrdSfsFileSystem*>(sfs_raw_ptr);
        m_log.Emsg("Config", "Using filesystem object from the framework.");
        return true;
    } else {
        m_log.Emsg("Config", "No filesystem object available to HTTP-TPC subsystem.  Internal error.");
        return false;
    }
    return true;
}

bool TPCHandler::ConfigureLogger(XrdOucStream &config_obj)
{
    char *val = config_obj.GetWord();
    if (!val || !val[0])
    {   
        m_log.Emsg("Config", "tpc.trace requires at least one directive [all | error | warning | info | debug | none]");
        return false;
    }
    // If the config option is given, reset the log mask.
    m_log.setMsgMask(0);
    
    do {
        if (!strcasecmp(val, "all"))
        {   
            m_log.setMsgMask(m_log.getMsgMask() | LogMask::All);
        }
        else if (!strcasecmp(val, "error"))
        {   
            m_log.setMsgMask(m_log.getMsgMask() | LogMask::Error);
        }
        else if (!strcasecmp(val, "warning"))
        {   
            m_log.setMsgMask(m_log.getMsgMask() | LogMask::Warning);
        }
        else if (!strcasecmp(val, "info"))
        {   
            m_log.setMsgMask(m_log.getMsgMask() | LogMask::Info);
        }
        else if (!strcasecmp(val, "debug"))
        {   
            m_log.setMsgMask(m_log.getMsgMask() | LogMask::Debug);
        }
        else if (!strcasecmp(val, "none"))
        {   
            m_log.setMsgMask(0);
        }
        else
        {   
            m_log.Emsg("Config", "tpc.trace encountered an unknown directive (valid values: [all | error | warning | info | debug | none]):", val);
            return false;
        }
        val = config_obj.GetWord();
    } while (val);
    
    return true;
}
