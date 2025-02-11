
#include "XrdAcc/XrdAccAuthorize.hh"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <string.h>

class XrdSysError;

/**
 * Class and function definitions for the SciTokens plugin.
 */

typedef std::vector<std::pair<Access_Operation, std::string>> AccessRulesRaw;

// Class representing a rule in the administrator-provided mapfile.
// All predicates must match for the rule to apply.
struct MapRule
{
    MapRule(const std::string &sub,
            const std::string &username,
            const std::string &path_prefix,
            const std::string &group,
            const std::string &result)
        : m_sub(sub),
          m_username(username),
          m_path_prefix(path_prefix),
          m_group(group),
          m_result(result)
    {
        //std::cerr << "Making a rule {sub=" << sub << ", username=" << username << ", path=" << path_prefix << ", group=" << group << ", result=" << name << "}" << std::endl;
    }

    const std::string match(const std::string &sub,
                            const std::string &username,
                            const std::string_view &req_path,
                            const std::vector<std::string> &groups) const
    {
        if (!m_sub.empty() && sub != m_sub) {return "";}

        if (!m_username.empty() && username != m_username) {return "";}

        if (!m_path_prefix.empty() &&
            strncmp(req_path.data(), m_path_prefix.c_str(), m_path_prefix.size()))
        {
            return "";
        }

        if (!m_group.empty()) {
            for (const auto &group : groups) {
                if (group == m_group)
                    return m_result;
            }
            return "";
        }
        return m_result;
    }

    std::string m_sub;
    std::string m_username;
    std::string m_path_prefix;
    std::string m_group;
    std::string m_result;
};

// Control whether a given issuer is required for the paths it authorizes
enum class AuthzSetting {
    None, // Issuer's authorization is not necessary
    Read, // Authorization from this issuer is necessary for reads.
    Write, // Authorization from this issuer is necessary for writes.
    All, // Authorization from this issuer is necessary for all operations.
};

// Controls what part of the token is used to determine a positive authorization.
//
// E.g., if IssuerAuthz::Group is set, then the positive authorization may be based
// on the groups embedded in the token.
enum IssuerAuthz {
    Capability = 0x01,
    Group = 0x02,
    Mapping = 0x04,
    Default = 0x07
};

// Given a list of access rules, this class determines whether a requested operation / path
// is permitted by the access rules.
class SubpathMatch final {
public:
    SubpathMatch() = default;
    SubpathMatch(const AccessRulesRaw &rules)
    : m_rules(rules)
    {}

    // Determine whether the known access rules permit the requested `oper` on `path`.
    bool apply(Access_Operation oper, const std::string_view path) const {
        auto is_subdirectory = [](const std::string_view& dir, const std::string_view& subdir) {
            if (subdir.size() < dir.size())
                return false;

            if (subdir.compare(0, dir.size(), dir, 0, dir.size()) != 0)
                return false;

            return dir.size() == subdir.size() || subdir[dir.size()] == '/' || dir == "/";
        };

        for (const auto & rule : m_rules) {
            // Skip rules that don't match the current operation
            if (rule.first != oper)
                continue;

            // If the rule allows any path, allow the operation
            if (rule.second == "/")
                return true;

            // Allow operation if path is a subdirectory of the rule's path
            if (is_subdirectory(rule.second, path)) {
                return true;
            } else {
                // Allow stat and mkdir of parent directories to comply with WLCG token specs
                if (oper == AOP_Stat || oper == AOP_Mkdir)
                if (is_subdirectory(path, rule.second))
                    return true;
            }
        }
        return false;
    }

    bool empty() const {return m_rules.empty();} // Returns true if there are no rules to match

    std::string str() const; // Returns a human-friendly representation of the access rules

    size_t size() const {return m_rules.size();} // Returns the count of rules
private:

    AccessRulesRaw m_rules;
};

/**
 * A class that encapsulates the access rules generated from a token.
 * 
 * The access rules are generated from the token's claims; the object
 * is intended to be kept in a cache and periodically checked for expiration.
 */
class XrdAccRules
{
public:
    XrdAccRules(uint64_t expiry_time, const std::string &username, const std::string &token_subject,
        const std::string &issuer, const std::vector<MapRule> &rules, const std::vector<std::string> &groups,
        uint32_t authz_strategy, AuthzSetting acceptable_authz) :
        m_authz_strategy(authz_strategy),
        m_acceptable_authz(acceptable_authz),
        m_expiry_time(expiry_time),
        m_username(username),
        m_token_subject(token_subject),
        m_issuer(issuer),
        m_map_rules(rules),
        m_groups(groups)
    {}

    ~XrdAccRules() {}

    bool apply(Access_Operation oper, const std::string_view path) {
        return m_matcher.apply(oper, path);
    }

    // Check to see if the access rules generated for this token have expired
    bool expired() const;

    void parse(const AccessRulesRaw &rules) {
        m_matcher = SubpathMatch(rules);
    }

    std::string get_username(const std::string_view &req_path) const
    {
        for (const auto &rule : m_map_rules) {
            std::string name = rule.match(m_token_subject, m_username, req_path, m_groups);
            if (!name.empty()) {
                return name;
            }
        }
        return "";
    }

    const std::string str() const;

        // Return the token's subject, an opaque unique string within the issuer's
        // namespace.  It may or may not be related to the username one should
        // use within the authorization framework.
    const std::string & get_token_subject() const {return m_token_subject;}
    const std::string & get_default_username() const {return m_username;}
    const std::string & get_issuer() const {return m_issuer;}

    uint32_t get_authz_strategy() const {return m_authz_strategy;}
    bool acceptable_authz(Access_Operation oper) const {
        if (m_acceptable_authz == AuthzSetting::All) return true;
        if (m_acceptable_authz == AuthzSetting::None) return false;

        bool is_read = oper == AOP_Read || oper == AOP_Readdir || oper == AOP_Stat;
        if (is_read) return m_acceptable_authz == AuthzSetting::Read;
        else return m_acceptable_authz == AuthzSetting::Write;
    }

    size_t size() const {return m_matcher.size();}
    const std::vector<std::string> &groups() const {return m_groups;}

private:
    const uint32_t m_authz_strategy;
    const AuthzSetting m_acceptable_authz;
    SubpathMatch m_matcher;
    const uint64_t m_expiry_time{0};
    const std::string m_username;
    const std::string m_token_subject;
    const std::string m_issuer;
    const std::vector<MapRule> m_map_rules;
    const std::vector<std::string> m_groups;
};

bool AuthorizesRequiredIssuers(Access_Operation client_oper, const std::string_view &path,
    const std::vector<std::pair<std::unique_ptr<SubpathMatch>, std::string>> &required_issuers,
    const std::vector<std::shared_ptr<XrdAccRules>> &access_rules_list);

bool
appendWLCGAudiences(const std::vector<std::string> &hostnames, XrdOucEnv *env,
    XrdSysError &eDest, std::vector<std::string> &audiences);

void splitEntries(const std::string_view entry_string, std::vector<std::string> &entries);
