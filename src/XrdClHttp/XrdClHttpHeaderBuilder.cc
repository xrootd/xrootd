#include "XrdClHttpHeaderBuilder.hh"
#include "XrdClHttpUtil.hh"

#include <XrdCl/XrdClLog.hh>

#include <algorithm>
#include <cctype>
#include <iterator>

using namespace XrdClHttp;

const std::unordered_set<std::string_view> HeaderBuilder::m_forbidden_headers{
    "authorization",
    "connection",
    "content-length",
    "expect",
    "host",
    "keep-alive",
    "proxy-authenticate",
    "proxy-authorization",
    "proxy-connection",
    "te",
    "trailer",
    "transfer-encoding",
    "transferheaderauthorization",
    "upgrade"
};

namespace {

// The characters RFC 7230 permits in a header name.
constexpr std::string_view tchar{
    "!#$%&'*+-.^_`|~"
    "0123456789"
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
};

// The whitespace that may surround a header name or value.
constexpr std::string_view ows{" \t"};

// Returns true if `text` contains `c`.
bool Contains(std::string_view text, char c) {
    return std::find(text.begin(), text.end(), c) != text.end();
}

// Returns `text` with any leading and trailing characters of `strip` removed.
std::string_view Trim(std::string_view text, std::string_view strip) {
    const auto discard = [strip](char c) {return Contains(strip, c);};

    const auto first = std::find_if_not(text.begin(), text.end(), discard);
    const auto last = std::find_if_not(text.rbegin(),
        std::make_reverse_iterator(first), discard).base();

    return text.substr(first - text.begin(), last - first);
}

} // namespace

HeaderBuilder::HeaderBuilder(XrdCl::Log *logger) :
    m_logger(logger)
    {}

bool
HeaderBuilder::IsForbiddenHeader(const std::string &name)
{
    std::string lowered(name.size(), '\0');
    std::transform(name.begin(), name.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    // count rather than contains: this builds under C++17 as well as C++20.
    return m_forbidden_headers.count(lowered) != 0;
}

bool
HeaderBuilder::SameHeaderName(std::string_view lhs, std::string_view rhs)
{
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
        [](unsigned char a, unsigned char b) {
            return std::tolower(a) == std::tolower(b);
        });
}

void
HeaderBuilder::AppendMissing(const HeaderList &extra, HeaderList &headers)
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
HeaderBuilder::Build(const std::string &spec, HeaderList &headers)
{
    // Nothing of a rejected specification reaches the caller, so build the list
    // aside and hand it over only once every entry has been accepted.
    headers.clear();

    // Walk the newline separated entries, stopping at the first unusable one.
    // An empty specification simply yields no headers.
    HeaderList requested;
    const std::string_view all(spec);
    for (auto pos = all.begin(); pos != all.end(); ) {
        const auto eol = std::find(pos, all.end(), '\n');

        // A CRLF separator leaves the CR behind; drop it along with any padding.
        const auto entry = Trim(all.substr(pos - all.begin(), eol - pos), " \t\r");
        pos = eol == all.end() ? eol : std::next(eol);

        // Tolerate blank entries so a trailing newline is not an error.
        if (entry.empty()) {
            continue;
        }

        // The value may itself contain colons, so split on the first one only.
        const auto colon = std::find(entry.begin(), entry.end(), ':');
        if (colon == entry.end()) {
            return false;
        }
        const auto split = static_cast<std::string_view::size_type>(colon - entry.begin());

        // The name must be a non-empty RFC 7230 token.
        const auto name = Trim(entry.substr(0, split), ows);
        if (name.empty() || !std::all_of(name.begin(), name.end(), [](char c) {
                return Contains(tchar, c);
            }))
        {
            return false;
        }
        if (IsForbiddenHeader(std::string(name))) {
            return false;
        }

        // A newline separates entries and so cannot occur in a value, but an
        // embedded carriage return would let one forge part of the request.
        const auto value = Trim(entry.substr(split + 1), ows);
        if (value.empty() || Contains(value, '\r')) {
            return false;
        }

        requested.emplace_back(name, value);
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
    m_logger->Debug(kLogXrdClHttp, "Requested headers %s", names.c_str());

    headers = std::move(requested);
    return true;
}
