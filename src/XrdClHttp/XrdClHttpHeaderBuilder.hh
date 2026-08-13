#ifndef XRDCLHTTP_HEADERBUILDER_HH
#define XRDCLHTTP_HEADERBUILDER_HH

#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace XrdCl {

class Log;

}

namespace XrdClHttp {

// Builds the headers a client asks a request to carry.
//
// The request is described by the HttpHeaders environment setting, which xrdcp
// fills in from its --header option.  This class is the only place that setting
// is interpreted.
class HeaderBuilder final {
public:
    using HeaderList = std::vector<std::pair<std::string, std::string>>;

    // `logger` receives the name of each header the builder returns.
    explicit HeaderBuilder(XrdCl::Log *logger);

    // Returns true if the named header may not be injected by the client.
    [[nodiscard]] static bool IsForbiddenHeader(const std::string &name);

    // Returns true if the two header names are the same, ignoring case.
    [[nodiscard]] static bool SameHeaderName(std::string_view lhs, std::string_view rhs);

    // Build into `headers` the headers `spec` asks for, replacing its contents.
    //
    // `spec` is a newline separated list of "<name>: <value>" entries; callers
    // pass the user's input through verbatim.
    //
    // Returns false if any entry is malformed or names a header that may not be
    // overridden, in which case the request must not be sent -- quietly dropping
    // a header changes what the request asks for.  A rejected specification
    // leaves `headers` empty rather than half built.
    [[nodiscard]] bool Build(const std::string &spec, HeaderList &headers);

    // Append to `headers` each entry of `extra` whose name `headers` does not
    // already carry.
    //
    // A request keeps the headers it built for itself: a requested Range, Depth
    // or Want-Digest would corrupt it.
    static void AppendMissing(const HeaderList &extra, HeaderList &headers);

private:

    XrdCl::Log *m_logger;

    // Headers that may not be injected by the client.
    //
    // These are the RFC 7230 hop-by-hop and message framing headers, plus Host
    // and the two credential headers.  Overriding them permits request smuggling
    // (Content-Length, Transfer-Encoding, TE, Trailer), cache poisoning and
    // routing confusion (Host), leaking a credential to an endpoint the user did
    // not name (Authorization, TransferHeaderAuthorization, Proxy-*), or protocol
    // switch attempts (Upgrade, Connection, Keep-Alive).  Several are also owned
    // by libcurl or by the header callout and overriding them would corrupt the
    // request framing -- Content-Length in particular is set from
    // CURLOPT_INFILESIZE_LARGE.
    //
    // Held in lower case; IsForbiddenHeader folds the name before looking it up.
    static const std::unordered_set<std::string_view> m_forbidden_headers;
};

}

#endif // XRDCLHTTP_HEADERBUILDER_HH
