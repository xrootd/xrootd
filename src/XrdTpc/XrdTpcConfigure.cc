
#include "XrdTpcTPC.hh"

#include <dlfcn.h>
#include <fcntl.h>

#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucPinPath.hh"
#include "XrdSfs/XrdSfsInterface.hh"

extern XrdSfsFileSystem *XrdSfsGetDefaultFileSystem(XrdSfsFileSystem *native_fs,
                                                    XrdSysLogger     *lp,
                                                    const char       *configfn,
                                                    XrdOucEnv        *EnvInfo);


using namespace TPC;


static XrdSfsFileSystem *load_sfs(void *handle, bool alt, XrdSysError &log, const std::string &libpath, const char *configfn, XrdOucEnv &myEnv, XrdSfsFileSystem *prior_sfs) {
    XrdSfsFileSystem *sfs = NULL;
    if (alt) {
        XrdSfsFileSystem2_t ep = (XrdSfsFileSystem *(*)(XrdSfsFileSystem *, XrdSysLogger *, const char *, XrdOucEnv *))
                      (dlsym(handle, "XrdSfsGetFileSystem2"));
        if (ep == NULL) {
            log.Emsg("Config", "Failed to load XrdSfsGetFileSystem2 from library ", libpath.c_str(), dlerror());
            return NULL;
        }
        sfs = ep(prior_sfs, log.logger(), configfn, &myEnv);
    } else {
        XrdSfsFileSystem_t ep = (XrdSfsFileSystem *(*)(XrdSfsFileSystem *, XrdSysLogger *, const char *))
                              (dlsym(handle, "XrdSfsGetFileSystem"));
        if (ep == NULL) {
            log.Emsg("Config", "Failed to load XrdSfsGetFileSystem from library ", libpath.c_str(), dlerror());
            return NULL;
        }
        sfs = ep(prior_sfs, log.logger(), configfn);
    }
    if (!sfs) {
        log.Emsg("Config", "Failed to initialize filesystem library for TPC handler from ", libpath.c_str());
        return NULL;
    }
    return sfs;
}


bool TPCHandler::ConfigureFSLib(XrdOucStream &Config, std::string &path1, bool &path1_alt, std::string &path2, bool &path2_alt) {
    char *val;
    if (!(val = Config.GetWord())) {
        m_log.Emsg("Config", "fslib not specified");
        return false;
    }
    if (!strcmp("throttle", val)) {
        path2 = "libXrdThrottle.so";
        if (!(val = Config.GetWord())) {
            m_log.Emsg("Config", "fslib throttle target library not specified");
            return false;
        }
    }
    else if (!strcmp("-2", val)) {
        path2_alt = true;
        if (!(val = Config.GetWord())) {
            m_log.Emsg("Config", "fslib library not specified");
            return false;
        }
        path2 = val;
    }
    else {
        path2 = val;
    }
    if (!(val = Config.GetWord()) || !strcmp("default", val)) {
        // There is not a second path specified or we requested the default path.
        // Configuration of the form "xrootd.fslib /some/path.so"
        // or                        "xrootd.fslib /some/path.so default"
        if ((path2 == "libXrdThrottle.so") || val) {
            // Default path specified as base or no default path specified, but chaining
            // with the throttle plugin.
            // Configuration of the form "xrootd.fslib throttle"
            //                        or "xrootd.fslib throttle default"
            path1 = "default";
        } else {
            // Only one path was specified - only load base.
            // Configuration of the form "xrootd.fslib /some/base_path.so"
            path1 = path2;
            path2 = "";
            path1_alt = path2_alt;
        }
    } else if (!strcmp("-2", val)) {
        // Configuration of the form  "xrootd.fslib /some/path.so -2 /some/base_path.so"
        path1_alt = true;
        if (!(val = Config.GetWord())) {
            m_log.Emsg("Config", "fslib base library not specified");
            return false;
        }
        path1 = val;
    } else {
        // Configuration of the form "xrootd.fslib /some/path.so /some/base_path.so"
        path1 = val;
    }
    return true;
}

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
    const char *val;
    std::string path2, path1 = "default";
    bool path1_alt = false, path2_alt = false;
    while ((val = Config.GetMyFirstWord())) {
        if (!strcmp("xrootd.fslib", val)) {
            if (!ConfigureFSLib(Config, path1, path1_alt, path2, path2_alt)) {
                Config.Close();
                m_log.Emsg("Config", "Failed to parse the xrootd.fslib directive");
                return false;
            }
            m_log.Emsg("Config", "xrootd.fslib line successfully processed by TPC handler.  Base library:", path1.c_str());
            if (path2.size()) {
                m_log.Emsg("Config", "Chained library:", path2.c_str());
            }
        } else if (!strcmp("http.desthttps", val)) {
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

    XrdSfsFileSystem *base_sfs = NULL;
    if (path1 == "default") {
        m_log.Emsg("Config", "Loading the default filesystem");
        base_sfs = XrdSfsGetDefaultFileSystem(NULL, m_log.logger(), configfn, myEnv);
        m_log.Emsg("Config", "Finished loading the default filesystem");
    } else {
        char resolvePath[2048];
        bool usedAltPath{true};
        if (!XrdOucPinPath(path1.c_str(), usedAltPath, resolvePath, 2048)) {
            m_log.Emsg("Config", "Failed to locate appropriately versioned base filesystem library for ", path1.c_str());
            return false;
        }
        m_handle_base = dlopen(resolvePath, RTLD_LOCAL|RTLD_NOW);
        if (m_handle_base == NULL) {
            m_log.Emsg("Config", "Failed to base plugin ", resolvePath, dlerror());
            return false;
        }
        base_sfs = load_sfs(m_handle_base, path1_alt, m_log, path1, configfn, *myEnv, NULL);
    }
    if (!base_sfs) {
        m_log.Emsg("Config", "Failed to initialize filesystem library for TPC handler from ", path1.c_str());
        return false;
    }
    XrdSfsFileSystem *chained_sfs = NULL;
    if (!path2.empty()) {
        char resolvePath[2048];
        bool usedAltPath{true};
        if (!XrdOucPinPath(path2.c_str(), usedAltPath, resolvePath, 2048)) {
            m_log.Emsg("Config", "Failed to locate appropriately versioned chained filesystem library for ", path2.c_str());
            return false;
        }
        m_handle_chained = dlopen(resolvePath, RTLD_LOCAL|RTLD_NOW);
        if (m_handle_chained == NULL) {
            m_log.Emsg("Config", "Failed to chained plugin ", resolvePath, dlerror());
            return false;
        }
        chained_sfs = load_sfs(m_handle_chained, path2_alt, m_log, path2, configfn, *myEnv, base_sfs);
    }
    m_sfs.reset(chained_sfs ? chained_sfs : base_sfs);
    m_log.Emsg("Config", "Successfully configured the filesystem object for TPC handler");
    return true;
}
