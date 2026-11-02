#include "XrdOssMirageWrapper.hh"
#include "XrdOssMirageWrapperDir.hh"
#include "XrdOssMirageWrapperFile.hh"
#include "XrdOssMirageXAttrWrapper.hh"

#include "XrdVersion.hh"

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysFAttr.hh"

#include <cerrno>
#include <cstring>
#include <mutex>

XrdVERSIONINFO(XrdOssAddStorageSystem2, XrdOssMirageWrapper);

static std::string parse_prefix(const char *parms)
{
    const std::string_view text = parms ? parms : "";

    const auto begin = text.find_first_not_of(" \t");
    if (begin == std::string_view::npos)
        return "";

    const auto end = text.find_first_of(" \t", begin);
    std::string prefix(text.substr(begin, end - begin));
    if (prefix.back() != '/')
        prefix.push_back('/');
    return prefix;
}

extern "C"
{
    XrdOss *XrdOssAddStorageSystem2(XrdOss *curr_oss, XrdSysLogger *logger,
                                    const char *config_fn, const char *parms,
                                    XrdOucEnv *envP)
    {
        XrdSysError log(logger, "XrdOssMirage");

        const auto prefix = parse_prefix(parms);
        if (prefix.empty())
        {
            log.Emsg("Config", "initialization failed: no mirage path given in the plugin parameters");
            return nullptr;
        }

        if (prefix == "/")
        {
            log.Emsg("Config", "initialization failed: mirage path '/' is not allowed because it covers the whole namespace");
            return nullptr;
        }

        return new XrdOssMirageWrapper(curr_oss, logger, parms, envP);
    }
}

XrdOssMirageWrapper::XrdOssMirageWrapper(XrdOss *curr_oss, XrdSysLogger *logger, const char *parms, XrdOucEnv *envP) :
    XrdOssWrapper(*curr_oss),
    XrdOssMirageWrapperBase(mirage),
    mirage(parse_prefix(parms))
{

}

XrdOss &XrdOssMirageWrapper::route(const char *path)
{
    return is_mirage_path(path) ? static_cast<XrdOss &>(mirage) : wrapPI;
}

XrdOssDF *XrdOssMirageWrapper::newDir(const char *tident)
{
    return new XrdOssMirageWrapperDir(mirage, std::unique_ptr<XrdOssDF>(wrapPI.newDir(tident)), tident);
}

XrdOssDF *XrdOssMirageWrapper::newFile(const char *tident)
{
    return new XrdOssMirageWrapperFile(mirage, std::unique_ptr<XrdOssDF>(wrapPI.newFile(tident)), tident);
}

int XrdOssMirageWrapper::Lfn2Pfn(const char *Path, char *buff, int blen)
{
    // A mirage path lives only in memory, so its physical name is its logical
    // name. Keeping it unchanged (instead of forwarding to the stacked plugin,
    // which would prepend oss.localroot) is what lets the extended-attribute
    // wrapper route on and key by the same path the file was created under.
    if (is_mirage_path(Path))
    {
        if (static_cast<int>(std::strlen(Path)) >= blen)
            return -ENAMETOOLONG;
        std::strcpy(buff, Path);
        return 0;
    }

    return wrapPI.Lfn2Pfn(Path, buff, blen);
}

const char *XrdOssMirageWrapper::Lfn2Pfn(const char *Path, char *buff, int blen, int &rc)
{
    if (is_mirage_path(Path))
    {
        rc = 0;
        return Path;
    }

    return wrapPI.Lfn2Pfn(Path, buff, blen, rc);
}

int XrdOssMirageWrapper::Chmod(const char *path, mode_t mode, XrdOucEnv *envP)
{
    return route(path).Chmod(path, mode, envP);
}

int XrdOssMirageWrapper::Create(const char *tid, const char *path, mode_t mode, XrdOucEnv &env, int opts)
{
    return route(path).Create(tid, path, mode, env, opts);
}

int XrdOssMirageWrapper::Mkdir(const char *path, mode_t mode, int mkpath, XrdOucEnv *envP)
{
    return route(path).Mkdir(path, mode, mkpath, envP);
}

int XrdOssMirageWrapper::Reloc(const char *tident, const char *path, const char *cgName, const char *anchor)
{
    return route(path).Reloc(tident, path, cgName, anchor);
}

int XrdOssMirageWrapper::Remdir(const char *path, int Opts, XrdOucEnv *envP)
{
    return route(path).Remdir(path, Opts, envP);
}

int XrdOssMirageWrapper::Rename(const char *oPath, const char *nPath, XrdOucEnv *oEnvP, XrdOucEnv *nEnvP)
{
    if (is_mirage_path(oPath) && is_mirage_path(nPath))
        return mirage.Rename(oPath, nPath, oEnvP, nEnvP);
    else if (!is_mirage_path(oPath) && !is_mirage_path(nPath))
        return wrapPI.Rename(oPath, nPath, oEnvP, nEnvP);
    else
        return -EXDEV;
}

int XrdOssMirageWrapper::Stat(const char *path, struct stat *buff, int opts, XrdOucEnv *envP)
{
    return route(path).Stat(path, buff, opts, envP);
}

int XrdOssMirageWrapper::StatFS(const char *path, char *buff, int &blen, XrdOucEnv *envP)
{
    return route(path).StatFS(path, buff, blen, envP);
}

int XrdOssMirageWrapper::StatPF(const char *path, struct stat *buff, int opts)
{
    return route(path).StatPF(path, buff, opts);
}

int XrdOssMirageWrapper::StatPF(const char *path, struct stat *buff)
{
    return route(path).StatPF(path, buff);
}

int XrdOssMirageWrapper::StatXA(const char *path, char *buff, int &blen, XrdOucEnv *envP)
{
    return route(path).StatXA(path, buff, blen, envP);
}

int XrdOssMirageWrapper::StatXP(const char *path, unsigned long long &attr, XrdOucEnv *envP)
{
    // Export attributes come from the server configuration, not from the
    // mirage content, so the stacked plugin always answers.
    return wrapPI.StatXP(path, attr, envP);
}

int XrdOssMirageWrapper::Truncate(const char *path, unsigned long long fsize, XrdOucEnv *envP)
{
    return route(path).Truncate(path, fsize, envP);
}

int XrdOssMirageWrapper::Unlink(const char *path, int Opts, XrdOucEnv *envP)
{
    return route(path).Unlink(path, Opts, envP);
}

void XrdOssMirageWrapper::Connect(XrdOucEnv &env)
{
    wrapPI.Connect(env);
    mirage.Connect(env);
}

void XrdOssMirageWrapper::Disc(XrdOucEnv &env)
{
    wrapPI.Disc(env);
    mirage.Disc(env);
}

void XrdOssMirageWrapper::EnvInfo(XrdOucEnv *envP)
{
    wrapPI.EnvInfo(envP);
    mirage.EnvInfo(envP);
}

uint64_t XrdOssMirageWrapper::Features()
{
    return wrapPI.Features() | mirage.Features();
}

bool XrdOssMirageWrapper::getErrMsg(std::string &eText)
{
    return wrapPI.getErrMsg(eText) || mirage.getErrMsg(eText);
}

int XrdOssMirageWrapper::StatLS(XrdOucEnv &env, const char *path, char *buff, int &blen)
{
    return route(path).StatLS(env, path, buff, blen);
}
