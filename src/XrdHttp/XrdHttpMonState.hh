#ifndef XRDHTTPMONSTATE_HH
#define XRDHTTPMONSTATE_HH

// Request lifecycle state used for HTTP monitoring/classification.
//
enum class XrdHttpMonState : int {
  NEW,       // Uninitialised state
  ACTIVE,    // First call to process request
  ERR_NET,   // Network error
  ERR_PROT,  // Filesystem/XRootD error that did not result in a valid HTTP response
             // (typically during a chunked response)
  DONE       // Final state
};

#endif /* XRDHTTPMONSTATE_HH */
