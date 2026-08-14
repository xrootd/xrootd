#ifndef XRDCLHTTP_HEADERBUILDER_HH
#define XRDCLHTTP_HEADERBUILDER_HH

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace XrdClHttp {

// Builds the headers a client asks a request to carry.  The request is
// described by the HttpHeaders environment setting, which xrdcp fills in from
// its --header option.  These functions are the only place it is interpreted.
namespace HeaderBuilder {

using HeaderList = std::vector<std::pair<std::string, std::string>>;

// Returns true if the named header must not be injected by the client.  A
// forbidden name stays forbidden under the TransferHeader prefix, which aims a
// header at the far server of a third party copy.
[[nodiscard]] bool IsForbiddenHeader(std::string_view name);

// Returns true if the two header names are the same, ignoring case.
[[nodiscard]] bool SameHeaderName(std::string_view lhs, std::string_view rhs);

// Build into `headers` the headers `spec` asks for, replacing its contents.
// `spec` is a newline separated list of "<name>: <value>" entries; callers pass
// the user's input through verbatim.
//
// Returns false if any entry is malformed or names a forbidden header, in which
// case the request must not be sent -- a dropped header changes what the
// request asks for.  A rejected specification leaves `headers` empty.
[[nodiscard]] bool Build(std::string_view spec, HeaderList &headers);

// Append to `headers` each entry of `extra` whose name `headers` does not
// already carry.  A request keeps the headers it built for itself: a requested
// Range, Depth or Want-Digest would corrupt it.
void AppendMissing(const HeaderList &extra, HeaderList &headers);

} // namespace HeaderBuilder

} // namespace XrdClHttp

#endif // XRDCLHTTP_HEADERBUILDER_HH
