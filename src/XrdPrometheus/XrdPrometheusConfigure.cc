
#include "XrdPrometheus.hh"

#include <XrdOuc/XrdOucEnv.hh>
#include <XrdOuc/XrdOucGatherConf.hh>
#include <XrdSys/XrdSysError.hh>

using namespace XrdPrometheus;

bool Handler::Config(const char *config_filename)
{
    XrdOucGatherConf prometheus_conf("prometheus", &m_log);
    int result;
    if ( (result = prometheus_conf.Gather(config_filename, XrdOucGatherConf::trim_lines)) < 0) {
        m_log.Emsg("Config", -result, "parsing config file", config_filename);
        return false;
    }

    while (prometheus_conf.GetLine()) {
        auto val = prometheus_conf.GetToken();
        if (!strcmp(val, "prometheus.trace") && !ConfigureTrace(prometheus_conf)) {
            return false;
        }
    }
    return true;
}

bool Handler::ConfigureTrace(XrdOucGatherConf &config_obj)
{
    char *val = config_obj.GetToken();
    if (!val || !val[0])
    {
        m_log.Emsg("Config", "prometheus.trace requires at least one directive [all | error | warning | info | debug | none]");
        return false;
    }
    // If the config option is given, reset the log mask.
    m_log.setMsgMask(0);

    do {
        if (!strcmp(val, "all"))
        {
            m_log.setMsgMask(m_log.getMsgMask() | LogMask::All);
        }
        else if (!strcmp(val, "error"))
        {
            m_log.setMsgMask(m_log.getMsgMask() | LogMask::Error);
        }
        else if (!strcmp(val, "warning"))
        {
            m_log.setMsgMask(m_log.getMsgMask() | LogMask::Warning);
        }
        else if (!strcmp(val, "info"))
        {
            m_log.setMsgMask(m_log.getMsgMask() | LogMask::Info);
        }
        else if (!strcmp(val, "debug"))
        {
            m_log.setMsgMask(m_log.getMsgMask() | LogMask::Debug);
        }
        else if (!strcmp(val, "none"))
        {
            m_log.setMsgMask(0);
        }
        else
        {
            m_log.Emsg("Config", "prometheus.trace encountered an unknown directive:", val);
            return false;
        }
        val = config_obj.GetToken();
    } while (val);

    return true;
}

