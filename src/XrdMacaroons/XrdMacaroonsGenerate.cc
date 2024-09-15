
#include "XrdMacaroonsGenerate.hh"
#include "XrdMacaroonsHandler.hh"
#include "XrdMacaroonsUtils.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucGatherConf.hh"

#include <macaroons.h>

using namespace Macaroons;

std::string
XrdMacaroonsGenerator::Generate(const std::string &id, const std::string &user, const std::string &path, const std::bitset<AOP_LastOp> &opers, time_t expiry, const std::vector<std::string> &other_caveats)
{
    if (m_secret.empty()) {
        m_log.Log(LogMask::Error, "Redirect", "Cannot overwrite redirect URL; no macaroon secret configured");
        return "";
    }

    std::string activity = "activity:READ_METADATA";
    if (opers[AOP_Read]) {
        activity += ",DOWNLOAD";
    }
    if (opers[AOP_Create]) {
        activity += ",UPLOAD";
    }
    if (opers[AOP_Delete]) {
        activity += ",DELETE";
    }
    if (opers[AOP_Chown]) {
        activity += ",MANAGE,UPDATE_METADATA";
    }
    if (opers[AOP_Readdir]) {
        activity += ",LIST";
    }

    char utc_time_buf[21];
    if (!strftime(utc_time_buf, 21, "%FT%TZ", gmtime(&expiry)))
    {
        m_log.Log(LogMask::Warning, "Redirect", "Failed to generate UTC time for macaroon");
        return "";
    }
    std::string time_caveat = "before:" + std::string(utc_time_buf);

    enum macaroon_returncode mac_err;
    struct macaroon *mac = macaroon_create(reinterpret_cast<const unsigned char*>(m_sitename.c_str()),
                                           m_sitename.size(),
                                           reinterpret_cast<const unsigned char*>(m_secret.c_str()),
                                           m_secret.size(),
                                           reinterpret_cast<const unsigned char*>(id.c_str()),
                                           id.size(), &mac_err);
    if (!mac) {
        m_log.Log(LogMask::Warning, "Redirect", "Failed to create a new macaroon");
        return "";
    }

    struct macaroon *mac_with_name;
    if (user.empty()) {
        mac_with_name = mac;
    } else {
        auto name_caveat = "name:" + user;
        mac_with_name = macaroon_add_first_party_caveat(mac,
                                                        reinterpret_cast<const unsigned char*>(name_caveat.c_str()),
                                                        name_caveat.size(),
                                                        &mac_err);
        macaroon_destroy(mac);
    }
    if (!mac_with_name)
    {
        m_log.Log(LogMask::Warning, "Redirect", "Failed to add username to macaroon");
        return "";
    }

    struct macaroon *mac_with_activities = macaroon_add_first_party_caveat(
            mac_with_name,
            reinterpret_cast<const unsigned char*>(activity.c_str()),
            activity.size(),
            &mac_err
    );

    macaroon_destroy(mac_with_name);
    if (!mac_with_activities)
    {
        m_log.Log(LogMask::Warning, "Redirect", "Failed to add activities to macaroon");
        return "";
    }

    std::string path_caveat = "path:" + Macaroons::NormalizeSlashes(path);
    struct macaroon *mac_with_path = macaroon_add_first_party_caveat(
                                                    mac_with_activities,
                                                    reinterpret_cast<const unsigned char*>(path_caveat.c_str()),
                                                    path_caveat.size(),
                                                    &mac_err);
    macaroon_destroy(mac_with_activities);
    if (!mac_with_path) {
        m_log.Log(LogMask::Warning, "Redirect", "Failed to add path restriction to macaroon");
        return "";
    }

    for (const auto &caveat : other_caveats)
    {
        struct macaroon *mac_tmp = mac_with_activities;
        mac_with_activities = macaroon_add_first_party_caveat(mac_tmp,
            reinterpret_cast<const unsigned char*>(caveat.c_str()),
            caveat.size(),
            &mac_err);
        macaroon_destroy(mac_tmp);
        if (!mac_with_activities)
        {
            return "";
        }
    }

    struct macaroon *mac_with_date = macaroon_add_first_party_caveat(
                                            mac_with_path,
                                            reinterpret_cast<const unsigned char*>(time_caveat.c_str()),
                                            time_caveat.size(),
                                            &mac_err);
    macaroon_destroy(mac_with_path);
    if (!mac_with_date) {
        m_log.Log(LogMask::Warning, "Redirect", "Failed to add expiration date to macaroon");
        return "";
    }

    size_t size_hint = macaroon_serialize_size_hint(mac_with_date);

    std::vector<char> macaroon_resp; macaroon_resp.resize(size_hint);
    if (macaroon_serialize(mac_with_date, &macaroon_resp[0], size_hint, &mac_err))
    {
        m_log.Log(LogMask::Warning, "Redirect", "Failed to serialize macaroon to base64");
        return "";
    }
    macaroon_destroy(mac_with_date);

    return std::string(&macaroon_resp[0]);
}

bool
XrdMacaroonsGenerator::Config()
{
    char *config_filename = nullptr;
    if (!XrdOucEnv::Import("XRDCONFIGFN", config_filename)) {
        return false;
    }
    XrdOucGatherConf scitokens_conf("scitokens.trace macaroons.secretkey all.sitename", &m_log);
    int result;
    if ((result = scitokens_conf.Gather(config_filename, XrdOucGatherConf::trim_lines)) < 0) {
        m_log.Emsg("Config", -result, "parsing config file", config_filename);
        return false;
    }

    char *val;
    std::string map_filename;
    while (scitokens_conf.GetLine()) {
        auto directive = scitokens_conf.GetToken();
        if (!strcmp(directive, "secretkey")) {
            if (!(val = scitokens_conf.GetToken())) {
                m_log.Emsg("Config", "macaroons.secretkey requires an argument.  Usage: macaroons.secretkey /path/to/file");
                return false;
            }
            if (!Macaroons::GetSecretKey(val, &m_log, m_secret)) {
                m_log.Emsg("Config", "Failed to configure the macaroons secret");
                return false;
            }
            continue;
        }
        if (!strcmp(directive, "sitename")) {
            if (!(val = scitokens_conf.GetToken())) {
                m_log.Emsg("Config", "all.sitename requires an argument.  Usage: all.sitename Site");
                return false;
            }
            m_sitename = val;
            continue;
        }
    }
    return true;
}