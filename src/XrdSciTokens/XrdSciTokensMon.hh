#ifndef __XrdSciTokensMon_hh__
#define __XrdSciTokensMon_hh__
/******************************************************************************/
/*                                                                            */
/*                    X r d S c o T o k e n s M o n . h h                     */
/*                                                                            */
/******************************************************************************/

#include <string>

#include "XrdAcc/XrdAccAuthorize.hh"

class XrdSciTokensMon
{
public:

bool Mon_isIO(const Access_Operation oper)
             {return oper == AOP_Read   || oper == AOP_Update 
                  || oper == AOP_Create || oper == AOP_Excl_Create;       
             }

void Mon_Report(const XrdSecEntity& Entity, const std::string& subject,
                                            const std::string& username);
  
     XrdSciTokensMon() {}
    ~XrdSciTokensMon() {}
};
#endif
