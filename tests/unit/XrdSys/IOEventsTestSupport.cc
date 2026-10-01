#include "XrdSys/XrdSysError.hh"

// IOEvents tests do not install an error logger; satisfy the thread wrapper's
// optional startup trace without pulling the complete XrdUtils library in.
void XrdSysError::Emsg(const char *, const char *, const char *, const char *) {}
