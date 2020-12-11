
#include <string>
#include <vector>

class XrdSciTokensHelper
{
public:

//-----------------------------------------------------------------------------
//! Initialize SciTokens plugin via authentication plugin path.
//!
//! @param   lp    - Pointer to the error logging object.
//! @param   cfn   - Pointer to the configuration file used by xrootd.
//! @param   parm  - Pointer to the plugin library parameters.
//! @param   accP  - Pointer to the authorization object, but should be null 
//!                  when initialized with this function.
//!
//! @result  Pointer to an instance of this object upon success, nil otherwise.
//-----------------------------------------------------------------------------

static XrdSciTokensHelper *InitViaZTN(XrdSysLogger *lp,
                                      const char   *cfn,
                                      const char   *parm,
                                      XrdAccAuthorize *accP = 0
                                      );

//-----------------------------------------------------------------------------
//! Get the list of valid issuers.
//!
//! @result  A vector of valid issuers.  The list of issuers never changes.
//!          Only a reconfig of the scitokens plugin could cause the issuer
//!          list to change, which right now only happens in plugin 
//!          initialization.
//-----------------------------------------------------------------------------

struct   ValidIssuer
        {std::string issuer_name;
         std::string issuer_url;
        };
typedef std::vector<ValidIssuer> Issuers;

virtual  Issuers IssuerList() = 0;

//-----------------------------------------------------------------------------
//! Validate a scitoken.
//!
//! @param   token - Pointer to the token to validate.
//! @param   emsg  - Reference to a string to hold the reason for rejection
//!
//! @result  Return true if the token is valid; false otherwise with emsg set.
//-----------------------------------------------------------------------------

virtual  bool    Validate(const char *token, std::string &emsg) = 0;

//-----------------------------------------------------------------------------
//! Constructor and Destructor.
//-----------------------------------------------------------------------------

         XrdSciTokensHelper() {}
virtual ~XrdSciTokensHelper() {}
};
