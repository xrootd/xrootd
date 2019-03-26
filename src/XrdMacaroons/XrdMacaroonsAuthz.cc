
#include <stdexcept>
#include <sstream>

#include <time.h>

#include "macaroons.h"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecEntity.hh"

#include "XrdMacaroonsHandler.hh"
#include "XrdMacaroonsAuthz.hh"

using namespace Macaroons;


namespace {

class AuthzCheck
{
public:
    AuthzCheck(const char *req_path, const Access_Operation req_oper, ssize_t m_max_duration, XrdSysError &log);

    const std::string &GetSecName() const {return m_sec_name;}

    static int verify_before_s(void *authz_ptr,
                               const unsigned char *pred,
                               size_t pred_sz);

    static int verify_activity_s(void *authz_ptr,
                                 const unsigned char *pred,
                                 size_t pred_sz);

    static int verify_path_s(void *authz_ptr,
                             const unsigned char *pred,
                             size_t pred_sz);

    static int verify_name_s(void *authz_ptr,
                             const unsigned char *pred,
                             size_t pred_sz);

private:
    int verify_before(const unsigned char *pred, size_t pred_sz);
    int verify_activity(const unsigned char *pred, size_t pred_sz);
    int verify_path(const unsigned char *pred, size_t pred_sz);
    int verify_name(const unsigned char *pred, size_t pred_sz);

    ssize_t m_max_duration;
    XrdSysError &m_log;
    const std::string m_path;
    std::string m_desired_activity;
    std::string m_sec_name;
    Access_Operation m_oper;
    time_t m_now;
};


static XrdAccPrivs AddPriv(Access_Operation op, XrdAccPrivs privs)
{
    int new_privs = privs;
    switch (op) {
        case AOP_Any:
            break;
        case AOP_Chmod:
            new_privs |= static_cast<int>(XrdAccPriv_Chmod);
            break;
        case AOP_Chown:
            new_privs |= static_cast<int>(XrdAccPriv_Chown);
            break;
        case AOP_Create:
            new_privs |= static_cast<int>(XrdAccPriv_Create);
            break;
        case AOP_Delete:
            new_privs |= static_cast<int>(XrdAccPriv_Delete);
            break;
        case AOP_Insert:
            new_privs |= static_cast<int>(XrdAccPriv_Insert);
            break;
        case AOP_Lock:
            new_privs |= static_cast<int>(XrdAccPriv_Lock);
            break;
        case AOP_Mkdir:
            new_privs |= static_cast<int>(XrdAccPriv_Mkdir);
            break;
        case AOP_Read:
            new_privs |= static_cast<int>(XrdAccPriv_Read);
            break;
        case AOP_Readdir:
            new_privs |= static_cast<int>(XrdAccPriv_Readdir);
            break;
        case AOP_Rename:
            new_privs |= static_cast<int>(XrdAccPriv_Rename);
            break;
        case AOP_Stat:
            new_privs |= static_cast<int>(XrdAccPriv_Lookup);
            break;
        case AOP_Update:
            new_privs |= static_cast<int>(XrdAccPriv_Update);
            break;
    };
    return static_cast<XrdAccPrivs>(new_privs);
}

}


Authz::Authz(XrdSysLogger *log, char const *config, XrdAccAuthorize *chain)
    : m_max_duration(86400),
    m_chain(chain),
    m_log(log, "macarons_"),
    m_authz_behavior(static_cast<int>(Handler::AuthzBehavior::PASSTHROUGH))
{
    Handler::AuthzBehavior behavior(Handler::AuthzBehavior::PASSTHROUGH);
    if (!Handler::Config(config, nullptr, &m_log, m_location, m_secret, m_max_duration, behavior))
    {
        throw std::runtime_error("Macaroon authorization config failed.");
    }
    m_authz_behavior = static_cast<int>(behavior);
}


XrdAccPrivs
Authz::OnMissing(const XrdSecEntity *Entity, const char *path,
                 const Access_Operation oper, XrdOucEnv *env)
{
    switch (m_authz_behavior) {
        case Handler::AuthzBehavior::PASSTHROUGH:
            return m_chain ? m_chain->Access(Entity, path, oper, env) : XrdAccPriv_None;
        case Handler::AuthzBehavior::ALLOW:
            return AddPriv(oper, XrdAccPriv_None);;
        case Handler::AuthzBehavior::DENY:
            return XrdAccPriv_None;
    }
    // Code should be unreachable.
    return XrdAccPriv_None;
}

XrdAccPrivs
Authz::Access(const XrdSecEntity *Entity, const char *path,
              const Access_Operation oper, XrdOucEnv *env)
{
    const char *authz = env ? env->Get("authz") : nullptr;
    // We don't allow any testing to occur in this authz module, preventing
    // a macaroon to be used to receive further macaroons.
    if (oper == AOP_Any)
    {
        return m_chain ? m_chain->Access(Entity, path, oper, env) : XrdAccPriv_None;
    }
    if (!authz || strncmp(authz, "Bearer%20", 9))
    {
        //m_log.Emsg("Access", "No bearer token present");
        return OnMissing(Entity, path, oper, env);
    }
    authz += 9;

    macaroon_returncode mac_err = MACAROON_SUCCESS;
    struct macaroon* macaroon = macaroon_deserialize(
        authz,
        &mac_err);
    if (!macaroon)
    {
        // Do not log - might be other token type!
        //m_log.Emsg("Access", "Failed to parse the macaroon");
        return OnMissing(Entity, path, oper, env);
    }

    struct macaroon_verifier *verifier = macaroon_verifier_create();
    if (!verifier)
    {
        m_log.Emsg("Access", "Failed to create a new macaroon verifier");
        return XrdAccPriv_None;
    }
    if (!path)
    {
        m_log.Emsg("Access", "Request with no provided path.");
        macaroon_verifier_destroy(verifier);
        return XrdAccPriv_None;
    }

    AuthzCheck check_helper(path, oper, m_max_duration, m_log);

    if (macaroon_verifier_satisfy_general(verifier, AuthzCheck::verify_before_s, &check_helper, &mac_err) ||
        macaroon_verifier_satisfy_general(verifier, AuthzCheck::verify_activity_s, &check_helper, &mac_err) ||
        macaroon_verifier_satisfy_general(verifier, AuthzCheck::verify_name_s, &check_helper, &mac_err) ||
        macaroon_verifier_satisfy_general(verifier, AuthzCheck::verify_path_s, &check_helper, &mac_err))
    {
        m_log.Emsg("Access", "Failed to configure caveat verifier:");
        macaroon_verifier_destroy(verifier);
        return XrdAccPriv_None;
    }

    const unsigned char *macaroon_loc;
    size_t location_sz;
    macaroon_location(macaroon, &macaroon_loc, &location_sz);
    if (strncmp(reinterpret_cast<const char *>(macaroon_loc), m_location.c_str(), location_sz))
    {
        m_log.Emsg("Access", "Macaroon is for incorrect location", reinterpret_cast<const char *>(macaroon_loc));
        macaroon_verifier_destroy(verifier);
        macaroon_destroy(macaroon);
        return m_chain ? m_chain->Access(Entity, path, oper, env) : XrdAccPriv_None;
    }

    if (macaroon_verify(verifier, macaroon,
                         reinterpret_cast<const unsigned char *>(m_secret.c_str()),
                         m_secret.size(),
                         NULL, 0, // discharge macaroons
                         &mac_err))
    {
        m_log.Log(LogMask::Debug, "Access", "Macaroon verification failed");
        macaroon_verifier_destroy(verifier);
        macaroon_destroy(macaroon);
        return m_chain ? m_chain->Access(Entity, path, oper, env) : XrdAccPriv_None;
    }
    macaroon_verifier_destroy(verifier);

    const unsigned char *macaroon_id;
    size_t id_sz;
    macaroon_identifier(macaroon, &macaroon_id, &id_sz);

    std::string macaroon_id_str(reinterpret_cast<const char *>(macaroon_id), id_sz);
    m_log.Log(LogMask::Info, "Access", "Macaroon verification successful; ID", macaroon_id_str.c_str());
    macaroon_destroy(macaroon);

    // Copy the name, if present into the macaroon, into the credential object.
    if (Entity && check_helper.GetSecName().size()) {
        m_log.Log(LogMask::Debug, "Access", "Setting the security name to", check_helper.GetSecName().c_str());
        XrdSecEntity &myEntity = *const_cast<XrdSecEntity *>(Entity);
        if (myEntity.name) {free(myEntity.name);}
        myEntity.name = strdup(check_helper.GetSecName().c_str());
    }

    // We passed verification - give the correct privilege.
    return AddPriv(oper, XrdAccPriv_None);
}


AuthzCheck::AuthzCheck(const char *req_path, const Access_Operation req_oper, ssize_t max_duration, XrdSysError &log)
      : m_max_duration(max_duration),
        m_log(log),
        m_path(req_path),
        m_oper(req_oper),
        m_now(time(NULL))
{
    switch (m_oper)
    {
    case AOP_Any:
        break;
    case AOP_Chmod:
    case AOP_Chown:
        m_desired_activity = "UPDATE_METADATA";
        break;
    case AOP_Insert:
    case AOP_Lock:
    case AOP_Mkdir:
    case AOP_Rename:
    case AOP_Update:
        m_desired_activity = "MANAGE";
        break;
    case AOP_Create:
        m_desired_activity = "UPLOAD";
        break;
    case AOP_Delete:
        m_desired_activity = "DELETE";
        break;
    case AOP_Read:
        m_desired_activity = "DOWNLOAD";
        break;
    case AOP_Readdir:
        m_desired_activity = "LIST";
        break;
    case AOP_Stat:
        m_desired_activity = "READ_METADATA";
    };
}


int
AuthzCheck::verify_before_s(void *authz_ptr,
                            const unsigned char *pred,
                            size_t pred_sz)
{
    return static_cast<AuthzCheck*>(authz_ptr)->verify_before(pred, pred_sz);
}


int
AuthzCheck::verify_activity_s(void *authz_ptr,
                              const unsigned char *pred,
                              size_t pred_sz)
{
    return static_cast<AuthzCheck*>(authz_ptr)->verify_activity(pred, pred_sz);
}


int
AuthzCheck::verify_path_s(void *authz_ptr,
                          const unsigned char *pred,
                          size_t pred_sz)
{
    return static_cast<AuthzCheck*>(authz_ptr)->verify_path(pred, pred_sz);
}


int
AuthzCheck::verify_name_s(void *authz_ptr,
                          const unsigned char *pred,
                          size_t pred_sz)
{
    return static_cast<AuthzCheck*>(authz_ptr)->verify_name(pred, pred_sz);
}


int
AuthzCheck::verify_before(const unsigned char * pred, size_t pred_sz)
{
    std::string pred_str(reinterpret_cast<const char *>(pred), pred_sz);
    if (strncmp("before:", pred_str.c_str(), 7))
    {
        return 1;
    }
    m_log.Log(LogMask::Debug, "AuthzCheck", "running verify before", pred_str.c_str());

    struct tm caveat_tm;
    if (strptime(&pred_str[7], "%Y-%m-%dT%H:%M:%SZ", &caveat_tm) == nullptr)
    {
        m_log.Log(LogMask::Debug, "AuthzCheck", "failed to parse time string", &pred_str[7]);
        return 1;
    }
    caveat_tm.tm_isdst = -1;

    time_t caveat_time = timegm(&caveat_tm);
    if (-1 == caveat_time)
    {
        m_log.Log(LogMask::Debug, "AuthzCheck", "failed to generate unix time", &pred_str[7]);
        return 1;
    }
    if ((m_max_duration > 0) && (caveat_time > m_now + m_max_duration))
    {
        m_log.Log(LogMask::Warning, "AuthzCheck", "Max token age is greater than configured max duration; rejecting");
        return 1;
    }

    int result = (m_now >= caveat_time);
    if (!result) m_log.Log(LogMask::Debug, "AuthzCheck", "verify before successful");
    else m_log.Log(LogMask::Debug, "AuthzCheck", "verify before failed");
    return result;
}


int
AuthzCheck::verify_activity(const unsigned char * pred, size_t pred_sz)
{
    if (!m_desired_activity.size()) {return 1;}
    std::string pred_str(reinterpret_cast<const char *>(pred), pred_sz);
    if (strncmp("activity:", pred_str.c_str(), 9)) {return 1;}
    m_log.Log(LogMask::Debug, "AuthzCheck", "running verify activity", pred_str.c_str());

    std::stringstream ss(pred_str.substr(9));
    for (std::string activity; std::getline(ss, activity, ','); )
    {
        // Any allowed activity also implies "READ_METADATA"
        if (m_desired_activity == "READ_METADATA") {return 0;}
        if (activity == m_desired_activity)
        {
            m_log.Log(LogMask::Debug, "AuthzCheck", "macaroon has desired activity", activity.c_str());
            return 0;
        }
    }
    m_log.Log(LogMask::Info, "AuthzCheck", "macaroon does NOT have desired activity", m_desired_activity.c_str());
    return 1;
}


int
AuthzCheck::verify_path(const unsigned char * pred, size_t pred_sz)
{
    std::string pred_str(reinterpret_cast<const char *>(pred), pred_sz);
    if (strncmp("path:", pred_str.c_str(), 5)) {return 1;}
    m_log.Log(LogMask::Debug, "AuthzCheck", "running verify path", pred_str.c_str());

    if ((m_path.find("/./") != std::string::npos) ||
        (m_path.find("/../") != std::string::npos))
    {
        m_log.Log(LogMask::Info, "AuthzCheck", "invalid requested path", m_path.c_str());
        return 1;
    }
    size_t compare_chars = pred_str.size() - 5;
    if (pred_str[compare_chars + 5 - 1] == '/') {compare_chars--;}

    int result = strncmp(pred_str.c_str() + 5, m_path.c_str(), compare_chars);
    if (!result)
    {
        m_log.Log(LogMask::Debug, "AuthzCheck", "path request verified for", m_path.c_str());
    }
    // READ_METADATA permission for /foo/bar automatically implies permission
    // to READ_METADATA for /foo.
    else if (m_oper == AOP_Stat)
    {
        result = strncmp(m_path.c_str(), pred_str.c_str() + 5, m_path.size());
        if (!result) {m_log.Log(LogMask::Debug, "AuthzCheck", "READ_METADATA path request verified for", m_path.c_str());}
        else {m_log.Log(LogMask::Debug, "AuthzCheck", "READ_METADATA path request NOT allowed", m_path.c_str());}
    }
    else
    {
        m_log.Log(LogMask::Debug, "AuthzCheck", "path request NOT allowed", m_path.c_str());
    }

    return result;
}


int
AuthzCheck::verify_name(const unsigned char * pred, size_t pred_sz)
{
    std::string pred_str(reinterpret_cast<const char *>(pred), pred_sz);
    if (strncmp("name:", pred_str.c_str(), 5)) {return 1;}
    if (pred_str.size() < 6) {return 1;}
    m_log.Log(LogMask::Debug, "AuthzCheck", "Verifying macaroon with", pred_str.c_str());

    // Make a copy of the name for the XrdSecEntity; this will be used later.
    m_sec_name = pred_str.substr(5);

    return 0;
}
