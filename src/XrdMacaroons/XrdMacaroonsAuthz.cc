
#include <stdexcept>
#include <sstream>

#include <ctime>

#include "macaroons.h"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"

#include "XrdMacaroonsHandler.hh"
#include "XrdMacaroonsAuthz.hh"

using namespace Macaroons;


namespace {

class AuthzCheck
{
public:
    AuthzCheck(const char *req_path, const Access_Operation req_oper, ssize_t m_max_duration, XrdSysError &log);

    const std::string &GetSecName() const {return m_sec_name;}
    const std::string &GetErrorMessage() const {return m_emsg;}

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
    std::string m_emsg;
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
        case AOP_Excl_Create: // fallthrough
        case AOP_Create:
            new_privs |= static_cast<int>(XrdAccPriv_Create);
            break;
        case AOP_Delete:
            new_privs |= static_cast<int>(XrdAccPriv_Delete);
            break;
        case AOP_Excl_Insert: // fallthrough
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


// Accept any value of the path, name, or activity caveats
int validate_verify_empty(void *emsg_ptr,
                          const unsigned char *pred,
                          size_t pred_sz)
{
    if ((pred_sz >= 5) && (!memcmp(reinterpret_cast<const char *>(pred), "path:", 5) ||
                           !memcmp(reinterpret_cast<const char *>(pred), "name:", 5)))
    {
        return 0;
    }
    if ((pred_sz >= 9) && (!memcmp(reinterpret_cast<const char *>(pred), "activity:", 9)))
    {
        return 0;
    }
    return 1;
}

}


Authz::Authz(XrdSysLogger *log, char const *config, XrdAccAuthorize *chain)
    : m_max_duration(86400),
    m_chain(chain),
    m_log(log, "macarons_"),
    m_authz_behavior(static_cast<int>(Handler::AuthzBehavior::PASSTHROUGH))
{
    Handler::AuthzBehavior behavior(Handler::AuthzBehavior::PASSTHROUGH);
    XrdOucEnv env;
    if (!Handler::Config(config, &env, &m_log, m_location, m_secret, m_max_duration, behavior))
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
    // We don't allow any testing to occur in this authz module, preventing
    // a macaroon to be used to receive further macaroons.
    if (oper == AOP_Any)
    {
        return m_chain ? m_chain->Access(Entity, path, oper, env) : XrdAccPriv_None;
    }

    const char *authz = env ? env->Get("authz") : nullptr;
    if (authz && !strncmp(authz, "Bearer%20", 9))
    {
        authz += 9;
    }

        // If there's no request-specific token, check for a ZTN session token
    if (!authz && Entity && !strcmp("ztn", Entity->prot) && Entity->creds &&
        Entity->credslen && Entity->creds[Entity->credslen] == '\0')
    {
        authz = Entity->creds;
    }

    if (!authz) {
        return OnMissing(Entity, path, oper, env);
    }

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
        std::string location_str(reinterpret_cast<const char *>(macaroon_loc), location_sz);
        m_log.Emsg("Access", "Macaroon is for incorrect location", location_str.c_str());
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
        const std::string &username = check_helper.GetSecName();
        m_log.Log(LogMask::Debug, "Access", "Setting the request name to", username.c_str());
        Entity->eaAPI->Add("request.name", username,true);
    }

    // We passed verification - give the correct privilege.
    return AddPriv(oper, XrdAccPriv_None);
}

bool Authz::Validate(const char   *token,
                     std::string  &emsg,
                     long long    *expT,
                     XrdSecEntity *entP)
{
    macaroon_returncode mac_err = MACAROON_SUCCESS;
    std::unique_ptr<struct macaroon, decltype(&macaroon_destroy)> macaroon(
        macaroon_deserialize(token, &mac_err),
        &macaroon_destroy);

    if (!macaroon)
    {
        emsg = "Failed to deserialize the token as a macaroon";
        // Purposely log at debug level in case if this validation is ever
        // chained so we don't have overly-chatty logs.
        m_log.Log(LogMask::Debug, "Validate", emsg.c_str());
        return false;
    }

    std::unique_ptr<struct macaroon_verifier, decltype(&macaroon_verifier_destroy)> verifier(
        macaroon_verifier_create(), &macaroon_verifier_destroy);
    if (!verifier)
    {
        emsg = "Internal error: failed to create a verifier.";
        m_log.Log(LogMask::Error, "Validate", emsg.c_str());
        return false;
    }

    // Note the path and operation here are ignored as we won't use those validators
    AuthzCheck check_helper("/", AOP_Read, m_max_duration, m_log);

    if (macaroon_verifier_satisfy_general(verifier.get(), AuthzCheck::verify_before_s, &check_helper, &mac_err) ||
        macaroon_verifier_satisfy_general(verifier.get(), validate_verify_empty, nullptr, &mac_err))
    {
        emsg = "Failed to configure the verifier";
        m_log.Log(LogMask::Error, "Validate", emsg.c_str());
        return false;
    }

    const unsigned char *macaroon_loc;
    size_t location_sz;
    macaroon_location(macaroon.get(), &macaroon_loc, &location_sz);
    if (strncmp(reinterpret_cast<const char *>(macaroon_loc), m_location.c_str(), location_sz))
    {
        emsg = "Macaroon contains incorrect location: " +
            std::string(reinterpret_cast<const char *>(macaroon_loc), location_sz);
        m_log.Log(LogMask::Warning, "Validate", emsg.c_str(), ("all.sitename is " + m_location).c_str());
        return false;
    }

    if (macaroon_verify(verifier.get(), macaroon.get(),
                        reinterpret_cast<const unsigned char *>(m_secret.c_str()),
                        m_secret.size(),
                        nullptr, 0,
                        &mac_err))
    {
        emsg = "Macaroon verification error" + (check_helper.GetErrorMessage().size() ?
            (", " + check_helper.GetErrorMessage()) : "");
        m_log.Log(LogMask::Warning, "Validate", emsg.c_str());
        return false;
    }

    const unsigned char *macaroon_id;
    size_t id_sz;
    macaroon_identifier(macaroon.get(), &macaroon_id, &id_sz);
    m_log.Log(LogMask::Info, "Validate", ("Macaroon verification successful; ID " +
        std::string(reinterpret_cast<const char *>(macaroon_id), id_sz)).c_str());

    return true;
}


AuthzCheck::AuthzCheck(const char *req_path, const Access_Operation req_oper, ssize_t max_duration, XrdSysError &log)
      : m_max_duration(max_duration),
        m_log(log),
        m_path(NormalizeSlashes(req_path)),
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
    case AOP_Update:
    case AOP_Create:
        m_desired_activity = "MANAGE";
        break;
    case AOP_Rename:
    case AOP_Excl_Create:
    case AOP_Excl_Insert:
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
    m_log.Log(LogMask::Debug, "AuthzCheck", "Checking macaroon for expiration; caveat:", pred_str.c_str());

    struct tm caveat_tm;
    if (strptime(&pred_str[7], "%Y-%m-%dT%H:%M:%SZ", &caveat_tm) == nullptr)
    {
        m_emsg = "Failed to parse time string: " + pred_str.substr(7);
        m_log.Log(LogMask::Warning, "AuthzCheck", m_emsg.c_str());
        return 1;
    }
    caveat_tm.tm_isdst = -1;

    time_t caveat_time = timegm(&caveat_tm);
    if (-1 == caveat_time)
    {
        m_emsg = "Failed to generate unix time: " + pred_str.substr(7);
        m_log.Log(LogMask::Warning, "AuthzCheck", m_emsg.c_str());
        return 1;
    }
    if ((m_max_duration > 0) && (caveat_time > m_now + m_max_duration))
    {
        m_emsg = "Max token age is greater than configured max duration; rejecting";
        m_log.Log(LogMask::Warning, "AuthzCheck", m_emsg.c_str());
        return 1;
    }

    int result = (m_now >= caveat_time);
    if (!result)
    {
        m_log.Log(LogMask::Debug, "AuthzCheck", "Macaroon has not expired.");
    }
    else
    {
        m_emsg = "Macaroon expired at " + pred_str.substr(7);
        m_log.Log(LogMask::Debug, "AuthzCheck", m_emsg.c_str());
    }
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
        if ((activity == m_desired_activity) || ((m_desired_activity == "UPLOAD") && (activity == "MANAGE")))
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
    std::string pred_str_raw(reinterpret_cast<const char *>(pred), pred_sz);
    if (strncmp("path:", pred_str_raw.c_str(), 5)) {return 1;}
    std::string pred_str = NormalizeSlashes(pred_str_raw.substr(5));
    m_log.Log(LogMask::Debug, "AuthzCheck", "running verify path", pred_str.c_str());

    if ((m_path.find("/./") != std::string::npos) ||
        (m_path.find("/../") != std::string::npos))
    {
        m_log.Log(LogMask::Info, "AuthzCheck", "invalid requested path", m_path.c_str());
        return 1;
    }

    int result = strncmp(pred_str.c_str(), m_path.c_str(), pred_str.size());
    if (!result)
    {
        m_log.Log(LogMask::Debug, "AuthzCheck", "path request verified for", m_path.c_str());
    }
    // READ_METADATA permission for /foo/bar automatically implies permission
    // to READ_METADATA for /foo.
    else if (m_oper == AOP_Stat)
    {
        result = strncmp(m_path.c_str(), pred_str.c_str(), m_path.size());
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
