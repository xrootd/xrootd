//------------------------------------------------------------------------------
// Copyright (c) 2025 by European Organization for Nuclear Research (CERN)
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#include "XrdAccjwt/XrdAccjwtScope.hh"

#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucGatherConf.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecEntityAttr.hh"
#include "XrdSec/XrdSecMonitor.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdVersion.hh"

#ifdef XRDACCJWT_JWT
#include "XrdSecjwt/XrdSecJWT.hh"
#endif

#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

XrdVERSIONINFO(XrdAccAuthorizeObject, XrdAccjwt);
XrdVERSIONINFO(XrdAccAuthorizeObjAdd, XrdAccjwt);

using namespace XrdAccjwtScope;

namespace {

enum LogMask {
    Debug   = 0x01,
    Info    = 0x02,
    Warning = 0x04,
    Error   = 0x08,
    All     = 0xff
};

enum class AuthzBehavior {
    PASSTHROUGH,
    ALLOW,
    DENY
};

const char *OpToName(Access_Operation op)
{
    switch (op) {
        case AOP_Any: return "any";
        case AOP_Chmod: return "chmod";
        case AOP_Chown: return "chown";
        case AOP_Create: return "create";
        case AOP_Excl_Create: return "excl_create";
        case AOP_Delete: return "del";
        case AOP_Excl_Insert: return "excl_insert";
        case AOP_Insert: return "insert";
        case AOP_Lock: return "lock";
        case AOP_Mkdir: return "mkdir";
        case AOP_Read: return "read";
        case AOP_Readdir: return "dir";
        case AOP_Rename: return "mv";
        case AOP_Stat: return "stat";
        case AOP_Update: return "update";
        case AOP_Stage: return "stage";
        case AOP_Poll: return "poll";
    };
    return "unknown";
}

bool IsTransferOp(Access_Operation oper)
{
    return oper == AOP_Read || oper == AOP_Update
        || oper == AOP_Create || oper == AOP_Excl_Create;
}

// Same record XrdSciTokens emits when a transfer is authorized. Identity
// fields come from the entity sec.protocol jwt already filled in.
void ReportTokenInfo(const XrdSecEntity *Entity, Access_Operation oper,
                     const std::string *subjectOverride = nullptr)
{
    if (!Entity || !Entity->secMon || !IsTransferOp(oper)) return;

    std::string subject;
    std::string username;
    if (Entity->eaAPI) {
        if (!subjectOverride) Entity->eaAPI->Get("token.subject", subject);
        Entity->eaAPI->Get("request.name", username);
    }
    if (subjectOverride) subject = *subjectOverride;
    if (username.empty() && Entity->name) username = Entity->name;

    char buff[2048];
    std::snprintf(buff, sizeof(buff), "s=%s&n=%s&o=%s&r=%s&g=%.1024s",
                  subject.c_str(),
                  username.c_str(),
                  Entity->vorg ? Entity->vorg : "",
                  Entity->role ? Entity->role : "",
                  Entity->grps ? Entity->grps : "");
    Entity->secMon->Report(XrdSecMonitor::TokenInfo, buff);
}

bool ParseCanonicalPaths(const std::string &path, std::vector<std::string> &results)
{
    size_t pos = 0;
    do {
        while (path.size() > pos && (path[pos] == ',' || path[pos] == ' ')) pos++;
        auto next_pos = path.find_first_of(", ", pos);
        auto next_path = path.substr(pos, next_pos - pos);
        pos = next_pos;
        if (!next_path.empty()) {
            std::string canonical_path;
            if (MakeCanonical(next_path, canonical_path)) {
                results.emplace_back(std::move(canonical_path));
            }
        }
    } while (pos != std::string::npos);
    return !results.empty();
}

std::string TrimCopy(const std::string &s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

bool ParseAuthzBehavior(const std::string &value, AuthzBehavior &behavior)
{
    if (value.empty()) return false;
    if (!strcasecmp(value.c_str(), "passthrough")) {
        behavior = AuthzBehavior::PASSTHROUGH;
        return true;
    }
    if (!strcasecmp(value.c_str(), "allow")) {
        behavior = AuthzBehavior::ALLOW;
        return true;
    }
    if (!strcasecmp(value.c_str(), "deny")) {
        behavior = AuthzBehavior::DENY;
        return true;
    }
    return false;
}

bool ParsePluginParm(const std::string &parm, const std::string &key, std::string &value)
{
    size_t pos = 0;
    do {
        while (parm.size() > pos && (parm[pos] == ' ' || parm[pos] == ',')) pos++;
        auto next_pos = parm.find_first_of(", ", pos);
        auto arg = parm.substr(pos, next_pos - pos);
        pos = next_pos;
        if (arg.empty()) continue;

        auto eq = arg.find('=');
        if (eq == std::string::npos) continue;
        auto argKey = TrimCopy(arg.substr(0, eq));
        if (argKey != key) continue;
        value = TrimCopy(arg.substr(eq + 1));
        return true;
    } while (pos != std::string::npos);
    return false;
}

std::string LogMaskToString(int mask)
{
    if (mask == LogMask::All) return "all";

    bool has_entry = false;
    std::stringstream ss;
    if (mask & LogMask::Debug) {
        ss << "debug";
        has_entry = true;
    }
    if (mask & LogMask::Info) {
        ss << (has_entry ? ", " : "") << "info";
        has_entry = true;
    }
    if (mask & LogMask::Warning) {
        ss << (has_entry ? ", " : "") << "warning";
        has_entry = true;
    }
    if (mask & LogMask::Error) {
        ss << (has_entry ? ", " : "") << "error";
    }
    return ss.str();
}

} // namespace

class XrdAccjwt : public XrdAccAuthorize
{
public:
    XrdAccjwt(XrdSysLogger *lp, const char *parms, XrdAccAuthorize *chain)
        : m_chain(chain),
          m_parms(parms ? parms : ""),
          m_log(lp, "accjwt_")
    {
        m_log.Say("++++++ XrdAccjwt: Initialized token scope authorization.");
        if (!Config()) {
            throw std::runtime_error("Failed to configure XrdAccjwt authorization.");
        }
    }

    ~XrdAccjwt() override = default;

    XrdAccPrivs Access(const XrdSecEntity *Entity,
                       const char *path,
                       const Access_Operation oper,
                       XrdOucEnv *env) override
    {
        std::string unused;
        return Access(Entity, path, oper, unused, env);
    }

    XrdAccPrivs Access(const XrdSecEntity *Entity,
                       const char *path,
                       const Access_Operation oper,
                       std::string &eInfo,
                       XrdOucEnv *env) override
    {
        if (!Entity || !Entity->eaAPI) return OnMissing(Entity, path, oper, env, m_authzBehavior);

        // request.name is replaced in place and the entity lives for the
        // connection. Remember the handshake account once, and publish a
        // mapped name only when this operation is granted.
        const std::string handshakeName = RememberHandshakeName(Entity);
        auto restoreHandshake = [&]() {
            if (!handshakeName.empty())
                Entity->eaAPI->Add("request.name", handshakeName, true);
        };
        auto decline = [&](AuthzBehavior behavior) {
            restoreHandshake();
            return OnMissing(Entity, path, oper, env, behavior);
        };
        auto denyHere = [&]() {
            restoreHandshake();
            return XrdAccPriv_None;
        };

        const char *authz = OpenAuthzToken(env ? env->Get("authz") : nullptr);
        // eos+jwt is authorized by the next plugin. Do not validate it or
        // apply storage scopes, and do not follow onmissing deny.
        if (authz && IsEosJwtType(TokenHeaderType(authz))) {
            eInfo = "eos+jwt token is not evaluated";
            return decline(AuthzBehavior::PASSTHROUGH);
        }
        const std::map<std::string, std::string> *reqAttrs = nullptr;
        std::map<std::string, std::string> reqStore;
        if (authz) {
#ifdef XRDACCJWT_JWT
            if (!XrdSecJWT::IsConfigured()) {
                eInfo = "authz token cannot be validated";
                m_log.Emsg("Access", eInfo.c_str());
                return decline(m_authzBehavior);
            }
            std::string identity, why;
            if (!XrdSecJWT::ValidateToken(authz, identity, why, nullptr, &reqStore)) {
                eInfo = why.empty() ? "authz token rejected" : why;
                m_log.Emsg("Access", "authz token rejected:", eInfo.c_str());
                return decline(m_authzBehavior);
            }
            reqAttrs = &reqStore;
#else
            eInfo = "authz token cannot be validated";
            m_log.Emsg("Access", eInfo.c_str());
            return decline(m_authzBehavior);
#endif
        }

        if (!authz) {
            std::string loginTyp;
            if (Entity->eaAPI->Get("jwt.typ", loginTyp) && IsEosJwtType(loginTyp)) {
                eInfo = "eos+jwt token is not evaluated";
                return decline(AuthzBehavior::PASSTHROUGH);
            }
        }

        auto getAttr = [&](const char *key, std::string &out) -> bool {
            out.clear();
            if (reqAttrs) {
                const auto it = reqAttrs->find(key);
                if (it == reqAttrs->end() || it->second.empty()) return false;
                out = it->second;
                return true;
            }
            return Entity->eaAPI->Get(key, out);
        };

        AuthzBehavior onMissing = m_authzBehavior;
        std::string issuerOnMissing;
        if (getAttr("jwt.onmissing", issuerOnMissing)) {
            if (!ParseAuthzBehavior(issuerOnMissing, onMissing)) {
                eInfo = "Invalid jwt.onmissing entity attribute";
                return denyHere();
            }
        }

        std::string scopeClaim;
        const bool haveScope = getAttr(m_scopeAttr.c_str(), scopeClaim);

        std::string wlcgVer;
        getAttr(m_wlcgVerAttr.c_str(), wlcgVer);

        std::string entityBasePath;
        getAttr("base_path", entityBasePath);
        std::vector<std::string> basePaths;
        if (!ResolveBasePaths(m_basePath, entityBasePath, basePaths, eInfo)) {
            if (eInfo.empty()) eInfo = "Invalid base_path entity attribute";
            return denyHere();
        }

        std::vector<std::string> restrictedPaths;
        std::string restrictedJson;
        if (getAttr("restricted_path", restrictedJson)) {
            if (!ParseRestrictedPathsJson(restrictedJson, restrictedPaths, eInfo)) {
                return denyHere();
            }
        }

        std::string reqPath;
        if (!MakeCanonical(path, reqPath)) {
            eInfo = "Invalid request path";
            return denyHere();
        }

        unsigned strategy = m_strategy;
        if (!m_strategySet) {
            std::string fromEntity;
            if (getAttr("jwt.strategy", fromEntity)) {
                std::string why;
                if (!ParseAuthzStrategy(fromEntity, strategy, why)) {
                    eInfo = why.empty() ? "Invalid jwt.strategy entity attribute" : why;
                    return denyHere();
                }
            }
        }

        std::string handshakeSubject;
        Entity->eaAPI->Get("token.subject", handshakeSubject);
        const MapIdentity who = IdentityForMap(reqAttrs, handshakeName, Entity->grps,
                                               handshakeSubject);
        std::string mapFile;
        getAttr("jwt.mapfile", mapFile);
        std::string mapped;
        if (!LookupMappedName(strategy, reqPath, basePaths, eInfo, who, mapFile, mapped)) {
            return denyHere();
        }

        if (!restrictedJson.empty()
        &&  !RequestMatchesRestrictedPaths(reqPath, restrictedPaths)) {
            eInfo = "Request path is outside restricted paths";
            return decline(onMissing);
        }

        if (!haveScope) return decline(onMissing);

        std::vector<ScopeRule> rules;
        if (!ParseScopeClaim(scopeClaim, wlcgVer, rules)) {
            const auto profile = DetectProfile(wlcgVer);
            m_log.Log(LogMask::Warning, "Access", profile == ScopeProfile::WLCG
                      ? "No valid WLCG storage scopes in"
                      : "No valid SciTokens storage scopes in",
                      m_scopeAttr.c_str());
            eInfo = (profile == ScopeProfile::WLCG)
                ? "No valid WLCG storage scopes in token"
                : "No valid SciTokens storage scopes in token";
            return decline(onMissing);
        }

        auto publishName = [&]() {
            const std::string &shown = mapped.empty() ? handshakeName : mapped;
            if (!shown.empty())
                Entity->eaAPI->Add("request.name", shown, true);
        };

        if (oper == AOP_Any) {
            const XrdAccPrivs privs = PrivilegesForPath(rules, basePaths, reqPath);
            if (CapabilityGrants(strategy, privs != XrdAccPriv_None)) {
                publishName();
                return privs;
            }
            eInfo = "Token scope does not authorize this operation";
            return decline(onMissing);
        }

        if (CapabilityGrants(strategy, AuthorizeOperation(rules, basePaths, reqPath, oper))) {
            publishName();
            if (m_log.getMsgMask() & LogMask::Debug) {
                std::stringstream ss;
                ss << "Grant " << (IsWlcgProfile(wlcgVer) ? "WLCG" : "SciTokens")
                   << " authorization for operation=" << OpToName(oper)
                   << ", path=" << reqPath;
                m_log.Log(LogMask::Debug, "Access", ss.str().c_str());
            }
            ReportTokenInfo(Entity, oper, reqAttrs ? &who.sub : nullptr);
            return AddPriv(oper, XrdAccPriv_None);
        }

        eInfo = "Token scope does not authorize this operation";
        return decline(onMissing);
    }

    int Audit(const int accok,
              const XrdSecEntity *Entity,
              const char *path,
              const Access_Operation oper,
              XrdOucEnv *Env) override
    {
        (void)accok;
        (void)Entity;
        (void)path;
        (void)oper;
        (void)Env;
        return 0;
    }

    int Test(const XrdAccPrivs priv, const Access_Operation oper) override
    {
        return m_chain ? m_chain->Test(priv, oper) : 0;
    }

private:
    XrdAccPrivs OnMissing(const XrdSecEntity *Entity, const char *path,
                          const Access_Operation oper, XrdOucEnv *env,
                          AuthzBehavior behavior)
    {
        switch (behavior) {
            case AuthzBehavior::PASSTHROUGH:
                return m_chain ? m_chain->Access(Entity, path, oper, env)
                               : XrdAccPriv_None;
            case AuthzBehavior::ALLOW:
                return AddPriv(oper, XrdAccPriv_None);
            case AuthzBehavior::DENY:
                return XrdAccPriv_None;
        }
        return XrdAccPriv_None;
    }

    // The first request.name seen on this connection is the handshake account.
    // Later grants may replace request.name; this copy is what mapping matches.
    std::string RememberHandshakeName(const XrdSecEntity *Entity)
    {
        std::string stored;
        if (Entity->eaAPI->Get("jwt.handshake.name", stored) && !stored.empty())
            return stored;
        std::string name;
        Entity->eaAPI->Get("request.name", name);
        if (name.empty() && Entity->name) name = Entity->name;
        if (!name.empty())
            Entity->eaAPI->Add("jwt.handshake.name", name, true);
        return name;
    }

    bool LookupMappedName(unsigned strategy,
                          const std::string &reqPath,
                          const std::vector<std::string> &basePaths,
                          std::string &eInfo,
                          const MapIdentity &who,
                          const std::string &mapFile,
                          std::string &mapped)
    {
        mapped.clear();
        if ((strategy & AuthzStrategyMapping) == 0 || mapFile.empty()) return true;

        std::shared_ptr<const std::vector<NameMapRule>> rules;
        if (!CachedNameMap(mapFile, rules, eInfo)) {
            m_log.Emsg("Access", "name_mapfile rejected:", eInfo.c_str());
            if (eInfo.empty()) eInfo = "name_mapfile cannot be read";
            return false;
        }

        mapped = MatchNameMap(*rules, who.sub, who.username, who.groups,
                              reqPath, basePaths);
        return true;
    }

    bool CachedNameMap(const std::string &path,
                       std::shared_ptr<const std::vector<NameMapRule>> &rules,
                       std::string &emsg)
    {
        struct stat st;
        if (stat(path.c_str(), &st) != 0) {
            emsg = "name_mapfile cannot be read";
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            auto it = m_maps.find(path);
            if (it != m_maps.end()
            &&  it->second.mtime == st.st_mtime
            &&  it->second.size == st.st_size) {
                rules = it->second.rules;
                return true;
            }
        }

        std::vector<NameMapRule> loaded;
        if (!LoadNameMapFile(path, loaded, emsg)) return false;
        auto stored = std::make_shared<const std::vector<NameMapRule>>(std::move(loaded));

        std::lock_guard<std::mutex> lock(m_mapMutex);
        auto it = m_maps.find(path);
        if (it != m_maps.end()
        &&  it->second.mtime == st.st_mtime
        &&  it->second.size == st.st_size) {
            rules = it->second.rules;
            return true;
        }
        m_maps[path] = CachedMap{st.st_mtime, st.st_size, stored};
        rules = std::move(stored);
        return true;
    }

    bool Config()
    {
        m_log.setMsgMask(LogMask::Error | LogMask::Warning);

        ParsePluginParm(m_parms, "base_path", m_basePath);
        ParsePluginParm(m_parms, "scope_attr", m_scopeAttr);
        ParsePluginParm(m_parms, "wlcg_ver_attr", m_wlcgVerAttr);
        ParsePluginParm(m_parms, "onmissing", m_onmissingParm);
        ParsePluginParm(m_parms, "strategy", m_strategyParm);

        if (m_scopeAttr.empty()) m_scopeAttr = "token.scope";
        if (m_wlcgVerAttr.empty()) m_wlcgVerAttr = "token.wlcg.ver";

        if (!m_onmissingParm.empty()) {
            if (!ParseAuthzBehavior(m_onmissingParm, m_authzBehavior)) {
                m_log.Emsg("Config", "Invalid onmissing value in authlib parms:",
                           m_onmissingParm.c_str());
                return false;
            }
        }
        if (!m_strategyParm.empty()) {
            std::string why;
            if (!ParseAuthzStrategy(m_strategyParm, m_strategy, why)) {
                m_log.Emsg("Config", "Invalid strategy value in authlib parms:",
                           why.c_str());
                return false;
            }
            m_strategySet = true;
        }

        char *config_filename = nullptr;
        if (!XrdOucEnv::Import("XRDCONFIGFN", config_filename)) return false;

        XrdOucGatherConf accjwt_conf("accjwt.", &m_log);
        int result = accjwt_conf.Gather(config_filename, XrdOucGatherConf::trim_lines);
        if (result < 0) {
            m_log.Emsg("Config", -result, "parsing config file", config_filename);
            return false;
        }

        char *val;
        while (accjwt_conf.GetLine()) {
            accjwt_conf.GetToken();
            if (!(val = accjwt_conf.GetToken())) continue;

            if (!strcmp(val, "trace")) {
                if (!ParseTrace(accjwt_conf)) return false;
            } else if (!strcmp(val, "basepath")) {
                if (!(val = accjwt_conf.GetToken()) || !*val) {
                    m_log.Emsg("Config", "accjwt.basepath requires a value.");
                    return false;
                }
                std::vector<std::string> paths;
                if (!ParseCanonicalPaths(val, paths)) {
                    m_log.Emsg("Config", "accjwt.basepath contains no valid paths:", val);
                    return false;
                }
                m_basePath = paths.front();
            } else if (!strcmp(val, "scopeattr")) {
                if (!(val = accjwt_conf.GetToken()) || !*val) {
                    m_log.Emsg("Config", "accjwt.scopeattr requires a value.");
                    return false;
                }
                m_scopeAttr = val;
            } else if (!strcmp(val, "wlcgverattr")) {
                if (!(val = accjwt_conf.GetToken()) || !*val) {
                    m_log.Emsg("Config", "accjwt.wlcgverattr requires a value.");
                    return false;
                }
                m_wlcgVerAttr = val;
            } else if (!strcmp(val, "onmissing")) {
                if (!(val = accjwt_conf.GetToken()) || !*val) {
                    m_log.Emsg("Config", "accjwt.onmissing requires a value.");
                    return false;
                }
                if (!ParseAuthzBehavior(val, m_authzBehavior)) {
                    m_log.Emsg("Config", "accjwt.onmissing is invalid:", val);
                    return false;
                }
            } else if (!strcmp(val, "strategy")) {
                if (!(val = accjwt_conf.GetToken()) || !*val) {
                    m_log.Emsg("Config", "accjwt.strategy requires a value.");
                    return false;
                }
                std::string text = val;
                while ((val = accjwt_conf.GetToken())) {
                    text.push_back(' ');
                    text += val;
                }
                std::string why;
                if (!ParseAuthzStrategy(text, m_strategy, why)) {
                    m_log.Emsg("Config", "accjwt.strategy is invalid:", why.c_str());
                    return false;
                }
                m_strategySet = true;
            } else {
                m_log.Emsg("Config", "accjwt encountered unknown directive:", val);
                return false;
            }
        }

        if (!m_basePath.empty()) {
            std::string canonical;
            if (!MakeCanonical(m_basePath, canonical)) {
                m_log.Emsg("Config", "accjwt.basepath is not a valid path:",
                           m_basePath.c_str());
                return false;
            }
            m_basePath = canonical;
        }

        m_log.Emsg("Config", "Logging levels enabled -", LogMaskToString(m_log.getMsgMask()).c_str());
        if (!m_basePath.empty()) {
            m_log.Say("Config using base path ", m_basePath.c_str());
        }
        m_log.Say("Config reading scopes from entity attribute ", m_scopeAttr.c_str());
        m_log.Say("Config reading WLCG profile marker from ", m_wlcgVerAttr.c_str());
        if (m_strategySet) {
            m_log.Say("Config authorization strategy ",
                      AuthzStrategyText(m_strategy).c_str());
        } else {
            m_log.Say("Config authorization strategy from jwt.strategy, default ",
                      AuthzStrategyText(AuthzStrategyDefault).c_str());
        }
        m_log.Say("Config onmissing ",
                  m_authzBehavior == AuthzBehavior::ALLOW ? "allow" :
                  m_authzBehavior == AuthzBehavior::DENY ? "deny" : "passthrough",
                  ", unless the issuer sets jwt.onmissing");
        return true;
    }

    bool ParseTrace(XrdOucGatherConf &conf)
    {
        char *val = conf.GetToken();
        if (!val || !*val) {
            m_log.Emsg("Config", "accjwt.trace requires an argument.");
            return false;
        }
        do {
            if (!strcmp(val, "all")) m_log.setMsgMask(m_log.getMsgMask() | LogMask::All);
            else if (!strcmp(val, "error")) m_log.setMsgMask(m_log.getMsgMask() | LogMask::Error);
            else if (!strcmp(val, "warning")) m_log.setMsgMask(m_log.getMsgMask() | LogMask::Warning);
            else if (!strcmp(val, "info")) m_log.setMsgMask(m_log.getMsgMask() | LogMask::Info);
            else if (!strcmp(val, "debug")) m_log.setMsgMask(m_log.getMsgMask() | LogMask::Debug);
            else if (!strcmp(val, "none")) m_log.setMsgMask(0);
            else {
                m_log.Emsg("Config", "accjwt.trace encountered unknown directive:", val);
                return false;
            }
        } while ((val = conf.GetToken()));
        return true;
    }

    XrdAccAuthorize *m_chain;
    std::string m_parms;
    std::string m_basePath;
    std::string m_scopeAttr;
    std::string m_wlcgVerAttr;
    std::string m_onmissingParm;
    std::string m_strategyParm;
    AuthzBehavior m_authzBehavior{AuthzBehavior::PASSTHROUGH};
    unsigned m_strategy{AuthzStrategyDefault};
    bool m_strategySet{false};
    struct CachedMap {
        time_t mtime = 0;
        off_t size = 0;
        std::shared_ptr<const std::vector<NameMapRule>> rules;
    };
    std::mutex m_mapMutex;
    std::map<std::string, CachedMap> m_maps;
    XrdSysError m_log;
};

XrdAccjwt *accToken = nullptr;

void InitAccjwt(XrdSysLogger *lp, const char *cfn, const char *parm,
                  XrdAccAuthorize *accP)
{
    (void)cfn;
    try {
        accToken = new XrdAccjwt(lp, parm, accP);
    } catch (std::exception &) {
        accToken = nullptr;
    }
}

extern "C" {

XrdAccAuthorize *XrdAccAuthorizeObjAdd(XrdSysLogger *lp,
                                       const char *cfn,
                                       const char *parm,
                                       XrdOucEnv *envP,
                                       XrdAccAuthorize *accP)
{
    (void)envP;
    if (!accToken) InitAccjwt(lp, cfn, parm, accP);
    return accToken;
}

XrdAccAuthorize *XrdAccAuthorizeObject(XrdSysLogger *lp,
                                       const char *cfn,
                                       const char *parm)
{
    if (!accToken) InitAccjwt(lp, cfn, parm, nullptr);
    return accToken;
}

XrdAccAuthorize *XrdAccAuthorizeObject2(XrdSysLogger *lp,
                                        const char *cfn,
                                        const char *parm,
                                        XrdOucEnv *envP)
{
    (void)envP;
    if (!accToken) InitAccjwt(lp, cfn, parm, nullptr);
    return accToken;
}

}
