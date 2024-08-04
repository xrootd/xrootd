/******************************************************************************/
/*                                                                            */
/*                    X r d S c i T o k e n s M o n . c c                     */
/*                                                                            */
/******************************************************************************/
  
#include "XrdSciTokens/XrdSciTokensMon.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSec/XrdSecMonitor.hh"

/******************************************************************************/
/*                                R e p o r t                                 */
/******************************************************************************/

void XrdSciTokensMon::Mon_Report(const XrdSecEntity& Entity,
                                 const std::string&  subject,
                                 const std::string&  username)
{
// Create record
//
   if (Entity.secMon)
      {char buff[2048];
       snprintf(buff, sizeof(buff),
                "s=%s&n=%s&o=%s&r=%s&g=%.1024s",
                subject.c_str(),username.c_str(),
                (Entity.vorg ? Entity.vorg : ""),
                (Entity.role ? Entity.role : ""),
                (Entity.grps ? Entity.grps : ""));
        Entity.secMon->Report(XrdSecMonitor::TokenInfo, buff);
      }
}
