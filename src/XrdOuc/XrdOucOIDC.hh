/******************************************************************************/
/*                                                                            */
/*                         X r d O u c O I D C . h h                          */
/*                                                                            */
/* (c) 2026 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/******************************************************************************/

#ifndef XRDOUCOIDC_HH
#define XRDOUCOIDC_HH

#include <cstdint>
#include <map>
#include <string>

class XrdOucErrInfo;
class XrdSysLogger;

namespace XrdOucOIDC
{

//------------------------------------------------------------------------------
//! Initialize OIDC JWT validation using the same options as sec.protocol oidc.
//! @return true on success; false with emsg set on failure.
//------------------------------------------------------------------------------
bool Init(XrdSysLogger *logger, const char *parms, std::string &emsg);

//------------------------------------------------------------------------------
//! sec.protocol-compatible initializer. Returns a malloc'd protocol parameter
//! string on success, or nullptr on failure (details in erp).
//------------------------------------------------------------------------------
char *InitSecProtocol(const char *parms, XrdOucErrInfo *erp);

//------------------------------------------------------------------------------
//! True after a successful Init with at least one issuer configured.
//------------------------------------------------------------------------------
bool IsConfigured();

//------------------------------------------------------------------------------
//! Strip whitespace and an optional Bearer prefix from a token buffer.
//! When maxLen >= 0 the input is treated as length-bounded untrusted data.
//------------------------------------------------------------------------------
const char *StripToken(const char *bTok, int &sz, int maxLen = -1);

//------------------------------------------------------------------------------
//! Validate a JWT and return the mapped local username in identity.
//! Uses the shared validated-token cache when enabled.
//------------------------------------------------------------------------------
bool ValidateToken(const char *rawTok, std::string &identity, std::string &emsg,
                   uint64_t *expTime = nullptr,
                   std::map<std::string, std::string> *entityAttrs = nullptr);

} // namespace XrdOucOIDC

#endif
