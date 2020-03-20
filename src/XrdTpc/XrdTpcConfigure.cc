
#include "XrdTpcTPC.hh"

#include <dlfcn.h>
#include <fcntl.h>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucPinPath.hh"
#include "XrdSfs/XrdSfsInterface.hh"


using namespace TPC;


bool TPCHandler::Configure(const char *configfn, XrdOucEnv *myEnv)
{
    XrdOucStream Config(&m_log, getenv("XRDINSTANCE"), myEnv, "=====> ");

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
        } else if (!strcmp("http.cadir", val)) {
            if (!(val = Config.GetWord())) {
                Config.Close();
                m_log.Emsg("Config", "http.cadir value not specified");
                return false;
            }
            m_cadir = val;
        }
    }
    Config.Close();

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
