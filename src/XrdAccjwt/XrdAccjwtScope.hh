#ifndef XRDACCJWTSCOPE_HH
#define XRDACCJWTSCOPE_HH
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

#include "XrdAcc/XrdAccAuthorize.hh"
#include "XrdAcc/XrdAccPrivs.hh"

#include <map>
#include <string>
#include <vector>

namespace XrdAccjwtScope {

enum class StorageCapability {
    Read,
    Create,
    Modify,
    Stage,
    Poll
};

enum class ScopeProfile {
    WLCG,
    SciTokens
};

struct ScopeRule {
    StorageCapability capability;
    std::string path;
};

bool MakeCanonical(const std::string &path, std::string &result);

bool IsWlcgProfile(const std::string &wlcgVer);

ScopeProfile DetectProfile(const std::string &wlcgVer);

bool ParseScopeClaim(const std::string &scopeClaim,
                     const std::string &wlcgVer,
                     std::vector<ScopeRule> &rules);

bool OperationMatchesCapability(Access_Operation oper, StorageCapability cap);

bool PathMatchesScope(const std::string &scopePath, const std::string &reqPath,
                      Access_Operation oper);

std::string ApplyBasePath(const std::string &basePath, const std::string &scopePath);

std::string ResolveBasePath(const std::string &configBasePath,
                            const std::string &entityBasePath);

// entityBasePath is one absolute path, or a JSON array of absolute paths
// (sec.protocol jwt writes the array when a SciTokens issuer lists several).
// Each entry is prefixed with configBasePath. An empty attribute yields one
// prefix, which is configBasePath or "" when that is empty too.
bool ResolveBasePaths(const std::string &configBasePath,
                      const std::string &entityBasePath,
                      std::vector<std::string> &basePaths,
                      std::string &emsg);

bool ParseRestrictedPathsJson(const std::string &json,
                              std::vector<std::string> &paths,
                              std::string &emsg);

bool RequestMatchesRestrictedPaths(const std::string &reqPath,
                                   const std::vector<std::string> &restrictedPaths);

bool AuthorizeOperation(const std::vector<ScopeRule> &rules,
                        const std::string &basePath,
                        const std::string &reqPath,
                        Access_Operation oper);

// True when any prefix in basePaths authorizes the operation. An empty list
// is the same as a single empty prefix.
bool AuthorizeOperation(const std::vector<ScopeRule> &rules,
                        const std::vector<std::string> &basePaths,
                        const std::string &reqPath,
                        Access_Operation oper);

XrdAccPrivs PrivilegesForPath(const std::vector<ScopeRule> &rules,
                              const std::string &basePath,
                              const std::string &reqPath);

XrdAccPrivs PrivilegesForPath(const std::vector<ScopeRule> &rules,
                              const std::vector<std::string> &basePaths,
                              const std::string &reqPath);

XrdAccPrivs AddPriv(Access_Operation op, XrdAccPrivs privs);

// SciTokens authorization_strategy. The default is all three. Only
// capability grants an operation. group and mapping do not.
constexpr unsigned AuthzStrategyCapability = 1u;
constexpr unsigned AuthzStrategyGroup      = 2u;
constexpr unsigned AuthzStrategyMapping    = 4u;
constexpr unsigned AuthzStrategyDefault    =
    AuthzStrategyCapability | AuthzStrategyGroup | AuthzStrategyMapping;

// Words separated by space, comma, '+', or '|'. An empty string is rejected.
bool ParseAuthzStrategy(const std::string &text, unsigned &strategy,
                        std::string &emsg);

// capability+group+mapping, or "none" when no bit is set.
std::string AuthzStrategyText(unsigned strategy);

// A storage scope grants only when the capability bit is set.
bool CapabilityGrants(unsigned strategy, bool scopeCovers);

// One SciTokens name_mapfile rule. Empty sub, username, or group means that
// field is not required. path is the prefix from the file, before base_path
// is prepended. result is the account published as request.name.
struct NameMapRule {
    std::string sub;
    std::string username;
    std::string group;
    std::string path;
    std::string result;
};

// JSON list of rule objects. The first match wins at lookup time.
bool ParseNameMap(const std::string &json, std::vector<NameMapRule> &rules,
                  std::string &emsg);

bool LoadNameMapFile(const std::string &path, std::vector<NameMapRule> &rules,
                     std::string &emsg);

// Account from the first matching rule, or empty when none match. An empty
// result means the caller leaves request.name as the handshake set it.
// Each issuer base path is prepended to the rule path, as SciTokens does.
std::string MatchNameMap(const std::vector<NameMapRule> &rules,
                         const std::string &sub,
                         const std::string &username,
                         const std::vector<std::string> &groups,
                         const std::string &reqPath,
                         const std::vector<std::string> &basePaths);

// Who a name_mapfile rule is matched against. A request token supplies sub,
// its own account, and jwt.grps. With no request token, sub and the account
// are the handshake values. username falls back to the handshake account
// when the token did not set request.name.
struct MapIdentity {
    std::string sub;
    std::string username;
    std::vector<std::string> groups;
};

MapIdentity IdentityForMap(const std::map<std::string, std::string> *tokenAttrs,
                          const std::string &handshakeName,
                          const char *handshakeGroups,
                          const std::string &handshakeSubject);

// Non-empty authz CGI value from an xroot open, or nullptr.
const char *OpenAuthzToken(const char *authz);

// JOSE typ from the unverified header. Empty when the value is not a JWT.
// A leading "Bearer" scheme is ignored, as it is during validation.
std::string TokenHeaderType(const char *token);

// typ eos+jwt, or application/eos+jwt. Media type comparison is case-insensitive.
bool IsEosJwtType(const std::string &typ);

} // namespace XrdAccjwtScope

#endif
