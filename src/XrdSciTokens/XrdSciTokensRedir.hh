#ifndef __XrdSciTokensRedir_hh__
#define __XrdSciTokensRedir_hh__

/******************************************************************************/
/*                                                                            */
/*                 X r d S c i T o k e n s R e d i r . h h                    */
/*                                                                            */
/******************************************************************************/

//-----------------------------------------------------------------------------
//! This class defines the XrdAccSciTokensRedir API to generate a redirect token
//! associated with a token. It requires that the SciTokens authorization plugin
//! be loaded and initialized. Upon successful loading and initialization the
//! symbol "SciTokensAcls" will contain the address of an instance of this class.
//! The inspiration for this approach was taken the XrdSciTokensHelper.hh
//-----------------------------------------------------------------------------

#include <string>

class XrdSciTokensRedir
{
public:

//-----------------------------------------------------------------------------
//! Alter a given URL to embed a redirect token.
//!
//! This will scan the URL for an embedded authorization token and, if it's a
//! valid SciToken, replaces it with a "redirect token" that is equivalent to
//! the SciToken intersected with the path in the URL.
//!
//! @param   url - Pointer to the URL to modify.
//!
//! @result  Returns the modified URL; an empty string is given if an error occurs.
//!          If no modification was done, it returns the original URL.
//-----------------------------------------------------------------------------

virtual std::string Redirect(const char *url) = 0;

//-----------------------------------------------------------------------------
//! Constructor and Destructor.
//-----------------------------------------------------------------------------

         XrdSciTokensRedir() {}
virtual ~XrdSciTokensRedir() {}
};

#endif
