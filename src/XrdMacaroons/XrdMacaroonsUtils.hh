#ifndef __XRD_MACAROONS_UTILS_HH__
#define __XRD_MACAROONS_UTILS_HH__

#include <string>
#include <sys/types.h>

namespace Macaroons {

// Parse an ISO 8601 duration of the form "PT<n>H<n>M<n>S" into a number of
// seconds.  Returns -1 if the input is not a valid duration.  The components
// are optional, but at least the "PT" prefix is required; "PT" on its own
// parses to a duration of zero seconds.
ssize_t determine_validity(const std::string &input);

} // namespace Macaroons

#endif // __XRD_MACAROONS_UTILS_HH__
