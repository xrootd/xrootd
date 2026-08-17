#include "XrdClHttpHeaderBuilder.hh"
#include "XrdClHttpUtil.hh"

#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClDefaultEnv.hh>

#include <algorithm>
#include <array>
#include <iterator>

namespace {

using namespace std::string_view_literals;

// Headers the client must not inject: the RFC 7230 hop-by-hop and message
// framing headers, and Host.  Overriding one permits request smuggling, cache
// poisoning, or corrupt framing.
//
// Held in lower case and without the TransferHeader prefix, the form
// IsForbiddenHeader reduces a name to before it looks the name up.
constexpr std::array forbidden_headers{
    "connection"sv,
    "content-length"sv,
    "expect"sv,
    "host"sv,
    "keep-alive"sv,
    "proxy-authenticate"sv,
    "proxy-authorization"sv,
    "proxy-connection"sv,
    "te"sv,
    "trailer"sv,
    "transfer-encoding"sv,
    "upgrade"sv
};

// The prefix that aims a header at the far server of a third party copy.
constexpr std::string_view transfer_header_prefix{"transferheader"};

// The characters RFC 7230 permits in a header name.
constexpr std::string_view tchar{
    "!#$%&'*+-.^_`|~"
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
};

// The whitespace that may surround a header name or value.
constexpr std::string_view ows{" \t"};

// Returns `text` with any leading and trailing characters of `strip` removed.
std::string_view trim(const std::string_view text, const std::string_view strip)
{
    const auto discard = [strip](const char c) {
        return strip.find(c) != std::string_view::npos;
    };

    const auto first = std::find_if_not(text.begin(), text.end(), discard);
    const auto last = std::find_if_not(text.rbegin(),
        std::make_reverse_iterator(first), discard).base();

    return text.substr(first - text.begin(), last - first);
}

} // namespace

namespace XrdClHttp {
namespace HeaderBuilder {

bool
IsForbiddenHeader(const std::string_view name)
{
    std::string lowered(name.size(), '\0');
    std::transform(name.begin(), name.end(), lowered.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    // A forbidden header stays forbidden when the TransferHeader prefix aims it
    // at the far server of a third party copy, however often the prefix repeats.
    std::string_view bare(lowered);
    while (bare.substr(0, transfer_header_prefix.size()) == transfer_header_prefix) {
        bare.remove_prefix(transfer_header_prefix.size());
    }

    return std::find(forbidden_headers.begin(), forbidden_headers.end(), bare)
        != forbidden_headers.end();
}

bool
SameHeaderName(const std::string_view lhs, const std::string_view rhs)
{
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
        [](const unsigned char a, const unsigned char b) {
            return std::tolower(a) == std::tolower(b);
        });
}

void
AppendMissing(const HeaderList &extra, HeaderList &headers)
{
    for (const auto &header : extra) {
        const auto present = std::any_of(headers.cbegin(), headers.cend(),
            [&header](const auto &existing) {
                return SameHeaderName(existing.first, header.first);
            });
        if (!present) {
            headers.emplace_back(header);
        }
    }
}

bool
Build(const std::string_view spec, HeaderList &headers)
{
    XrdCl::Log *const log = XrdCl::DefaultEnv::GetLog();

    // Walk the newline separated entries, reporting every unusable one so a user
    // who wrote several mistakes sees them all at once.
    // An empty specification simply yields no headers.
    HeaderList requested;
    bool usable = true;
    for (auto pos = spec.begin(); pos != spec.end(); ) {
        const auto eol = std::find(pos, spec.end(), '\n');

        // A CRLF separator leaves the CR behind; drop it along with any padding.
        const auto entry = trim(spec.substr(pos - spec.begin(), eol - pos), " \t\r");
        pos = (eol == spec.end() ? eol : std::next(eol));

        // Tolerate blank entries so a trailing newline is not an error.
        if (entry.empty()) {
            continue;
        }

        // The value may itself contain colons, so split on the first one only.
        // Without a colon the entry carries no value, so the log can show the
        // whole entry without showing a value the user keeps private.
        const auto colon = entry.find(':');
        if (colon == std::string_view::npos) {
            log->Error(kLogXrdClHttp, "Requested header %s holds no colon;"
                " each header must read \"<name>: <value>\"",
                std::string(entry).c_str());
            usable = false;
            continue;
        }

        // The name must be a non-empty RFC 7230 token.
        const std::string name(trim(entry.substr(0, colon), ows));
        if (name.empty()) {
            log->Error(kLogXrdClHttp, "Requested header holds no name before"
                " its colon");
            usable = false;
            continue;
        }
        if (!std::all_of(name.begin(), name.end(), [](const char c) {
                return tchar.find(c) != std::string_view::npos;
            }))
        {
            log->Error(kLogXrdClHttp, "Requested header %s holds a name that"
                " is not an HTTP token", name.c_str());
            usable = false;
            continue;
        }
        if (IsForbiddenHeader(name)) {
            log->Error(kLogXrdClHttp, "Requested header %s must not be set by"
                " the client", name.c_str());
            usable = false;
            continue;
        }

        // A newline separates entries and so cannot occur in a value, but an
        // embedded carriage return would let one forge part of the request.
        const auto value = trim(entry.substr(colon + 1), ows);
        if (value.empty()) {
            log->Error(kLogXrdClHttp, "Requested header %s holds no value",
                name.c_str());
            usable = false;
            continue;
        }
        if (value.find('\r') != std::string_view::npos) {
            log->Error(kLogXrdClHttp, "Requested header %s holds a carriage"
                " return in its value", name.c_str());
            usable = false;
            continue;
        }

        requested.emplace_back(name, value);
    }

    if (!usable) {
        return false;
    }

    if (requested.empty()) {
        return true;
    }

    // Name the headers but never their values, which the user fills with whatever
    // the endpoint understands.
    std::string names;
    for (const auto &header : requested) {
        if (!names.empty()) names += ", ";
        names += header.first;
    }
    log->Debug(kLogXrdClHttp, "Requested headers %s", names.c_str());

    headers = std::move(requested);
    return true;
}

} // namespace HeaderBuilder

} // namespace XrdClHttp
