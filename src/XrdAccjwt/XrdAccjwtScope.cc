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

#include "XrdOuc/XrdOucJson.hh"
#include "XrdOuc/XrdOucPrivateUtils.hh"

#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>

namespace XrdAccjwtScope {

namespace {

std::string TrimCopy(const std::string &s)
{
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

bool ParseWlcgCapability(std::string_view cap, StorageCapability &out)
{
    if (cap == "read") out = StorageCapability::Read;
    else if (cap == "create") out = StorageCapability::Create;
    else if (cap == "modify") out = StorageCapability::Modify;
    else if (cap == "stage") out = StorageCapability::Stage;
    else if (cap == "poll") out = StorageCapability::Poll;
    else return false;
    return true;
}

bool ParseWlcgScopeToken(const std::string &token, ScopeRule &rule)
{
    const auto colon = token.find(':');
    if (colon == std::string::npos) return false;

    const std::string prefix = TrimCopy(std::string(token.substr(0, colon)));
    const std::string path = TrimCopy(std::string(token.substr(colon + 1)));
    if (prefix.empty() || path.empty()) return false;

    const std::string_view storage_prefix = "storage.";
    if (prefix.compare(0, storage_prefix.size(), storage_prefix) != 0) return false;

    if (!ParseWlcgCapability(std::string_view(prefix).substr(storage_prefix.size()),
                             rule.capability))
        return false;

    if (!MakeCanonical(path, rule.path)) return false;
    return true;
}

bool ParseSciTokensScopeToken(const std::string &token, ScopeRule &rule)
{
    const auto colon = token.find(':');
    if (colon == std::string::npos) return false;

    const std::string prefix = TrimCopy(std::string(token.substr(0, colon)));
    const std::string path = TrimCopy(std::string(token.substr(colon + 1)));
    if (prefix.empty() || path.empty()) return false;

    if (prefix == "read") rule.capability = StorageCapability::Read;
    else if (prefix == "write") rule.capability = StorageCapability::Modify;
    else return false;

    if (!MakeCanonical(path, rule.path)) return false;
    return true;
}

bool ParseScopeClaimForProfile(const std::string &scopeClaim,
                               ScopeProfile profile,
                               std::vector<ScopeRule> &rules)
{
    rules.clear();
    std::istringstream iss(scopeClaim);
    std::string token;
    while (iss >> token) {
        ScopeRule rule;
        const bool parsed = (profile == ScopeProfile::WLCG)
            ? ParseWlcgScopeToken(token, rule)
            : ParseSciTokensScopeToken(token, rule);
        if (parsed) rules.push_back(std::move(rule));
    }
    return !rules.empty();
}

} // namespace

bool MakeCanonical(const std::string &path, std::string &result)
{
    if (path.empty() || path[0] != '/') return false;

    size_t pos = 0;
    std::vector<std::string> components;
    do {
        while (path.size() > pos && path[pos] == '/') pos++;
        auto next_pos = path.find_first_of('/', pos);
        auto next_component = path.substr(pos, next_pos - pos);
        pos = next_pos;
        if (next_component.empty() || next_component == ".") continue;
        if (next_component == "..") {
            if (!components.empty()) components.pop_back();
        } else {
            components.emplace_back(std::move(next_component));
        }
    } while (pos != std::string::npos);

    if (components.empty()) {
        result = "/";
        return true;
    }

    std::stringstream ss;
    for (const auto &comp : components) ss << "/" << comp;
    result = ss.str();
    return true;
}

bool IsWlcgProfile(const std::string &wlcgVer)
{
    return !TrimCopy(wlcgVer).empty();
}

ScopeProfile DetectProfile(const std::string &wlcgVer)
{
    return IsWlcgProfile(wlcgVer) ? ScopeProfile::WLCG : ScopeProfile::SciTokens;
}

bool ParseScopeClaim(const std::string &scopeClaim,
                     const std::string &wlcgVer,
                     std::vector<ScopeRule> &rules)
{
    return ParseScopeClaimForProfile(scopeClaim, DetectProfile(wlcgVer), rules);
}

bool OperationMatchesCapability(Access_Operation oper, StorageCapability cap)
{
    switch (cap) {
        case StorageCapability::Read:
            return oper == AOP_Read || oper == AOP_Readdir || oper == AOP_Stat;
        case StorageCapability::Create:
            return oper == AOP_Excl_Create || oper == AOP_Mkdir || oper == AOP_Rename
                || oper == AOP_Excl_Insert || oper == AOP_Stat;
        case StorageCapability::Modify:
            return oper == AOP_Create || oper == AOP_Mkdir || oper == AOP_Rename
                || oper == AOP_Insert || oper == AOP_Update || oper == AOP_Chmod
                || oper == AOP_Stat || oper == AOP_Delete;
        case StorageCapability::Stage:
            return oper == AOP_Stage;
        case StorageCapability::Poll:
            return oper == AOP_Poll;
    };
    return false;
}

bool PathMatchesScope(const std::string &scopePath, const std::string &reqPath,
                      Access_Operation oper)
{
    if (is_subdirectory(scopePath, reqPath)) return true;

    if (oper == AOP_Stat || oper == AOP_Mkdir) {
        return is_subdirectory(reqPath, scopePath);
    }
    return false;
}

std::string ApplyBasePath(const std::string &basePath, const std::string &scopePath)
{
    if (basePath.empty()) return scopePath;
    std::string combined;
    if (!MakeCanonical(basePath + scopePath, combined)) return scopePath;
    return combined;
}

std::string ResolveBasePath(const std::string &configBasePath,
                            const std::string &entityBasePath)
{
    std::string entity = TrimCopy(entityBasePath);
    if (!entity.empty()) {
        std::string canonical;
        if (!MakeCanonical(entity, canonical)) return {};
        entity = canonical;
    }
    if (configBasePath.empty()) return entity;
    if (entity.empty()) return configBasePath;
    return ApplyBasePath(configBasePath, entity);
}

bool ParseRestrictedPathsJson(const std::string &json,
                              std::vector<std::string> &paths,
                              std::string &emsg);

bool ResolveBasePaths(const std::string &configBasePath,
                      const std::string &entityBasePath,
                      std::vector<std::string> &basePaths,
                      std::string &emsg)
{
    basePaths.clear();
    emsg.clear();

    std::vector<std::string> entityPaths;
    const std::string raw = TrimCopy(entityBasePath);
    if (!raw.empty() && raw.front() == '[') {
        if (!ParseRestrictedPathsJson(raw, entityPaths, emsg)) {
            emsg = "Invalid base_path entity attribute";
            return false;
        }
    } else if (!raw.empty()) {
        std::string canonical;
        if (!MakeCanonical(raw, canonical)) {
            emsg = "Invalid base_path entity attribute";
            return false;
        }
        entityPaths.push_back(canonical);
    }

    if (entityPaths.empty()) {
        basePaths.push_back(ResolveBasePath(configBasePath, ""));
        return true;
    }
    for (const auto &entity : entityPaths) {
        const std::string effective = ResolveBasePath(configBasePath, entity);
        if (effective.empty()) {
            emsg = "Invalid base_path entity attribute";
            return false;
        }
        basePaths.push_back(effective);
    }
    return true;
}

bool ParseRestrictedPathsJson(const std::string &json,
                              std::vector<std::string> &paths,
                              std::string &emsg)
{
    paths.clear();
    emsg.clear();
    const std::string s = TrimCopy(json);
    if (s.empty()) return true;

    const auto parsed = nlohmann::json::parse(s, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_array()) {
        emsg = "restricted_path must be a JSON array";
        return false;
    }
    for (const auto &el : parsed) {
        if (!el.is_string()) {
            paths.clear();
            emsg = "restricted_path JSON array must contain strings";
            return false;
        }
        std::string canonical;
        if (!MakeCanonical(el.get<std::string>(), canonical)) {
            paths.clear();
            emsg = "restricted_path contains an invalid absolute path";
            return false;
        }
        paths.push_back(std::move(canonical));
    }
    return true;
}

bool RequestMatchesRestrictedPaths(const std::string &reqPath,
                                   const std::vector<std::string> &restrictedPaths)
{
    if (restrictedPaths.empty()) return false;
    for (const auto &restrictedPath : restrictedPaths) {
        if (is_subdirectory(restrictedPath, reqPath)) return true;
    }
    return false;
}

bool AuthorizeOperation(const std::vector<ScopeRule> &rules,
                        const std::string &basePath,
                        const std::string &reqPath,
                        Access_Operation oper)
{
    for (const auto &rule : rules) {
        const auto effectivePath = ApplyBasePath(basePath, rule.path);
        if (!OperationMatchesCapability(oper, rule.capability)) continue;
        if (!PathMatchesScope(effectivePath, reqPath, oper)) continue;
        return true;
    }
    return false;
}

bool AuthorizeOperation(const std::vector<ScopeRule> &rules,
                        const std::vector<std::string> &basePaths,
                        const std::string &reqPath,
                        Access_Operation oper)
{
    if (basePaths.empty())
        return AuthorizeOperation(rules, std::string(), reqPath, oper);
    for (const auto &basePath : basePaths) {
        if (AuthorizeOperation(rules, basePath, reqPath, oper)) return true;
    }
    return false;
}

XrdAccPrivs AddPriv(Access_Operation op, XrdAccPrivs privs)
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
        case AOP_Stage:
            new_privs |= static_cast<int>(XrdAccPriv_Stage);
            break;
        case AOP_Poll:
            new_privs |= static_cast<int>(XrdAccPriv_Poll);
            break;
    };
    return static_cast<XrdAccPrivs>(new_privs);
}

XrdAccPrivs PrivilegesForPath(const std::vector<ScopeRule> &rules,
                              const std::string &basePath,
                              const std::string &reqPath)
{
    static const Access_Operation allOps[] = {
        AOP_Read, AOP_Readdir, AOP_Stat, AOP_Excl_Create, AOP_Mkdir,
        AOP_Rename, AOP_Excl_Insert, AOP_Create, AOP_Insert, AOP_Update,
        AOP_Chmod, AOP_Delete, AOP_Stage, AOP_Poll
    };

    XrdAccPrivs privs = XrdAccPriv_None;
    for (const auto &rule : rules) {
        const auto effectivePath = ApplyBasePath(basePath, rule.path);
        for (const auto checkOp : allOps) {
            if (!OperationMatchesCapability(checkOp, rule.capability)) continue;
            if (!PathMatchesScope(effectivePath, reqPath, checkOp)) continue;
            privs = AddPriv(checkOp, privs);
        }
    }
    return privs;
}

XrdAccPrivs PrivilegesForPath(const std::vector<ScopeRule> &rules,
                              const std::vector<std::string> &basePaths,
                              const std::string &reqPath)
{
    if (basePaths.empty())
        return PrivilegesForPath(rules, std::string(), reqPath);
    XrdAccPrivs privs = XrdAccPriv_None;
    for (const auto &basePath : basePaths)
        privs = static_cast<XrdAccPrivs>(privs | PrivilegesForPath(rules, basePath, reqPath));
    return privs;
}

bool ParseAuthzStrategy(const std::string &text, unsigned &strategy,
                        std::string &emsg)
{
    strategy = 0;
    emsg.clear();
    bool any = false;
    size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size()
            && (text[pos] == ' ' || text[pos] == ',' || text[pos] == '+'
             || text[pos] == '|' || text[pos] == '\t')) {
            ++pos;
        }
        if (pos >= text.size()) break;
        size_t end = pos;
        while (end < text.size()
            && text[end] != ' ' && text[end] != ',' && text[end] != '+'
            && text[end] != '|' && text[end] != '\t') {
            ++end;
        }
        std::string word = text.substr(pos, end - pos);
        pos = end;
        for (char &c : word)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        any = true;
        if (word == "capability") strategy |= AuthzStrategyCapability;
        else if (word == "group") strategy |= AuthzStrategyGroup;
        else if (word == "mapping") strategy |= AuthzStrategyMapping;
        else {
            emsg = "unknown authorization_strategy value: " + word;
            strategy = 0;
            return false;
        }
    }
    if (!any) {
        emsg = "authorization_strategy has no value";
        return false;
    }
    return true;
}

std::string AuthzStrategyText(unsigned strategy)
{
    std::string out;
    if (strategy & AuthzStrategyCapability) out += "capability";
    if (strategy & AuthzStrategyGroup) {
        if (!out.empty()) out += "+";
        out += "group";
    }
    if (strategy & AuthzStrategyMapping) {
        if (!out.empty()) out += "+";
        out += "mapping";
    }
    if (out.empty()) out = "none";
    return out;
}

bool CapabilityGrants(unsigned strategy, bool scopeCovers)
{
    return scopeCovers && (strategy & AuthzStrategyCapability) != 0;
}

namespace {

bool SafeAccountName(const std::string &name)
{
    if (name.empty() || name[0] == '-') return false;
    for (unsigned char c : name) {
        if (!std::isalnum(c) && c != '_' && c != '.' && c != '@' && c != '-')
            return false;
    }
    return true;
}

bool TakeMapString(const nlohmann::json &rule, const char *key, std::string &dest,
                   std::string &emsg)
{
    if (!rule.contains(key)) return true;
    if (!rule.at(key).is_string()) {
        emsg = std::string("name_mapfile ") + key + " must be a string";
        return false;
    }
    dest = rule.at(key).get<std::string>();
    return true;
}

std::string MapPrefix(const std::string &base, const std::string &rulePath)
{
    if (rulePath.empty()) return base;
    if (base.empty()) return rulePath;
    std::string combined = base + rulePath;
    const auto pos = combined.find("//");
    if (pos != std::string::npos) combined.erase(pos + 1, 1);
    std::string canonical;
    if (!MakeCanonical(combined, canonical)) return {};
    return canonical;
}

} // namespace

bool ParseNameMap(const std::string &json, std::vector<NameMapRule> &rules,
                  std::string &emsg)
{
    rules.clear();
    emsg.clear();
    const auto parsed = nlohmann::json::parse(json, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_array()) {
        emsg = "name_mapfile must be a JSON list";
        return false;
    }
    for (const auto &rule : parsed) {
        if (!rule.is_object()) {
            emsg = "name_mapfile must be a list of JSON objects";
            return false;
        }
        if (rule.contains("ignore")) continue;

        NameMapRule out;
        if (!TakeMapString(rule, "result", out.result, emsg)) return false;
        if (out.result.empty()) {
            emsg = "name_mapfile rule has no result";
            return false;
        }
        if (!SafeAccountName(out.result)) {
            emsg = "name_mapfile result is not a safe account name";
            return false;
        }
        if (!TakeMapString(rule, "sub", out.sub, emsg)) return false;
        if (!TakeMapString(rule, "username", out.username, emsg)) return false;
        if (!TakeMapString(rule, "group", out.group, emsg)) return false;
        if (!TakeMapString(rule, "path", out.path, emsg)) return false;
        if (!out.path.empty()) {
            std::string canonical;
            if (!MakeCanonical(out.path, canonical)) {
                emsg = "name_mapfile path is not an absolute path";
                return false;
            }
            out.path = canonical;
        }
        rules.push_back(std::move(out));
    }
    return true;
}

bool LoadNameMapFile(const std::string &path, std::vector<NameMapRule> &rules,
                     std::string &emsg)
{
    rules.clear();
    emsg.clear();
    std::ifstream in(path);
    if (!in) {
        emsg = "name_mapfile cannot be read";
        return false;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    if (!in && !in.eof()) {
        emsg = "name_mapfile cannot be read";
        return false;
    }
    return ParseNameMap(buffer.str(), rules, emsg);
}

std::string MatchNameMap(const std::vector<NameMapRule> &rules,
                         const std::string &sub,
                         const std::string &username,
                         const std::vector<std::string> &groups,
                         const std::string &reqPath,
                         const std::vector<std::string> &basePaths)
{
    const std::vector<std::string> oneEmpty{""};
    const auto &bases = basePaths.empty() ? oneEmpty : basePaths;

    for (const auto &rule : rules) {
        if (!rule.sub.empty() && rule.sub != sub) continue;
        if (!rule.username.empty() && rule.username != username) continue;
        if (!rule.group.empty()) {
            bool found = false;
            for (const auto &group : groups) {
                if (group == rule.group) {
                    found = true;
                    break;
                }
            }
            if (!found) continue;
        }
        for (const auto &base : bases) {
            if (rule.path.empty() && base.empty()) return rule.result;
            const std::string prefix = MapPrefix(base, rule.path);
            if (prefix.empty()) continue;
            if (is_subdirectory(prefix, reqPath)) return rule.result;
        }
    }
    return "";
}

void SplitGroupList(const char *text, std::vector<std::string> &groups)
{
    groups.clear();
    if (!text || !*text) return;
    std::istringstream in(text);
    std::string group;
    while (in >> group) groups.push_back(group);
}

const char *OpenAuthzToken(const char *authz)
{
    if (!authz || !*authz) return nullptr;
    return authz;
}

namespace {

int B64Value(unsigned char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

bool DecodeBase64Url(std::string in, std::string &out)
{
    out.clear();
    if (in.empty()) return false;
    while ((in.size() % 4) != 0) in.push_back('=');
    int val = 0;
    int bits = -8;
    for (unsigned char c : in) {
        if (std::isspace(c)) continue;
        if (c == '=') break;
        const int d = B64Value(c);
        if (d < 0) return false;
        val = (val << 6) | d;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return !out.empty();
}

std::string_view StripBearer(std::string_view in)
{
    while (!in.empty() && std::isspace(static_cast<unsigned char>(in.front())))
        in.remove_prefix(1);
    auto starts = [&](const char *pfx) {
        const size_t n = std::char_traits<char>::length(pfx);
        if (in.size() < n) return false;
        for (size_t i = 0; i < n; ++i) {
            const unsigned char a = static_cast<unsigned char>(in[i]);
            const unsigned char b = static_cast<unsigned char>(pfx[i]);
            if (std::tolower(a) != std::tolower(b)) return false;
        }
        return true;
    };
    if (starts("Bearer%20")) in.remove_prefix(9);
    else if (starts("Bearer ")) in.remove_prefix(7);
    while (!in.empty() && std::isspace(static_cast<unsigned char>(in.front())))
        in.remove_prefix(1);
    while (!in.empty() && std::isspace(static_cast<unsigned char>(in.back())))
        in.remove_suffix(1);
    return in;
}

} // namespace

std::string TokenHeaderType(const char *token)
{
    if (!token || !*token) return {};
    const std::string_view raw = StripBearer(token);
    const size_t dot = raw.find('.');
    if (dot == std::string_view::npos || dot == 0) return {};
    std::string header;
    if (!DecodeBase64Url(std::string(raw.substr(0, dot)), header)) return {};
    const auto parsed = nlohmann::json::parse(header, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) return {};
    const auto it = parsed.find("typ");
    if (it == parsed.end() || !it->is_string()) return {};
    return it->get<std::string>();
}

bool IsEosJwtType(const std::string &typ)
{
    std::string v = TrimCopy(typ);
    for (char &c : v)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const std::string prefix = "application/";
    if (v.compare(0, prefix.size(), prefix) == 0)
        v.erase(0, prefix.size());
    return v == "eos+jwt";
}

MapIdentity IdentityForMap(const std::map<std::string, std::string> *tokenAttrs,
                          const std::string &handshakeName,
                          const char *handshakeGroups,
                          const std::string &handshakeSubject)
{
    MapIdentity id;
    if (!tokenAttrs) {
        id.sub = handshakeSubject;
        id.username = handshakeName;
        SplitGroupList(handshakeGroups, id.groups);
        return id;
    }
    const auto sub = tokenAttrs->find("token.subject");
    if (sub != tokenAttrs->end()) id.sub = sub->second;
    const auto name = tokenAttrs->find("request.name");
    if (name != tokenAttrs->end() && !name->second.empty()) id.username = name->second;
    else id.username = handshakeName;
    const auto groups = tokenAttrs->find("jwt.grps");
    if (groups != tokenAttrs->end()) SplitGroupList(groups->second.c_str(), id.groups);
    return id;
}

} // namespace XrdAccjwtScope
