//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientStringMatcher                                               //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// Simple class used to match against 'regular expressions formed       //
//  as follows:                                                         //
//   - a sequence of single exprs separated by |                        //
//   - each single expr can have a * character as a wildcard            //
//     but only in its begin or in its end                              //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include <string.h>
#include <stdlib.h>

class XrdClientStringMatcher {
 private:
   // The expr to match
   char *exp;

   bool SingleMatches(const char *expr, const char *str);

 public:
   XrdClientStringMatcher(const char *expr);
   ~XrdClientStringMatcher();

   bool Matches(const char *str);

};
