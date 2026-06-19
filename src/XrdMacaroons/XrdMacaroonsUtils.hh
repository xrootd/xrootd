#ifndef __XRD_MACAROONS_UTILS_HH__
#define __XRD_MACAROONS_UTILS_HH__

#include <string>
#include <sys/types.h>

namespace Macaroons {

// 'Normalize' the macaroon path.  This only takes care of double slashes
// but, as is common in XRootD, it doesn't treat these as a hierarchy.
// For example, these result in the same path:
//
//   /foo/bar -> /foo/bar
//   //foo////bar -> /foo/bar
//
// These are all distinct:
//
//   /foo/bar  -> /foo/bar
//   /foo/bar/ -> /foo/bar/
//   /foo/baz//../bar -> /foo/baz/../bar
//
std::string NormalizeSlashes(const std::string &input);

// Parse an ISO 8601 duration of the form "PT<n>H<n>M<n>S" into a number of
// seconds.  Returns -1 if the input is not a valid duration.  The components
// are optional, but at least the "PT" prefix is required; "PT" on its own
// parses to a duration of zero seconds.
ssize_t determine_validity(const std::string &input);

} // namespace Macaroons

#endif // __XRD_MACAROONS_UTILS_HH__
