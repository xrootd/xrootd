/******************************************************************************/
/*                                                                            */
/*                          X r d S e c J W T . h h                           */
/*                                                                            */
/* (c) 2026 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/******************************************************************************/

#ifndef XRDSECJWT_HH
#define XRDSECJWT_HH

#include <cstdint>
#include <map>
#include <string>
#include <string_view>

class XrdOucErrInfo;
class XrdSysLogger;

namespace XrdSecJWT
{

//------------------------------------------------------------------------------
//! Initialize JWT validation using the same options as sec.protocol jwt.
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
//! Configured maximum token size in bytes (-maxsz), as applied by the server.
//------------------------------------------------------------------------------
int MaxTokenSize();

//------------------------------------------------------------------------------
//! Strip whitespace and an optional (case-insensitive) Bearer prefix.
//! The view is the exact input; callers bound untrusted data before calling.
//! Returns an empty view when no token remains.
//------------------------------------------------------------------------------
std::string_view StripToken(std::string_view token);

//------------------------------------------------------------------------------
//! Validate a JWT and return the mapped local username in identity.
//! Uses the shared validated-token cache when enabled.
//------------------------------------------------------------------------------
bool ValidateToken(std::string_view rawTok, std::string &identity, std::string &emsg,
                   uint64_t *expTime = nullptr,
                   std::map<std::string, std::string> *entityAttrs = nullptr);

} // namespace XrdSecJWT

#endif
