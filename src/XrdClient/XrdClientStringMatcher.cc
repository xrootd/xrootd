//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdClientStringMatcher                                               //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
//                                                                      //
// Simple class used to match against 'regular expressions formed       //
//  as follows:                                                         //
//   - a sequence of single exprs separated by |                        //
//   - each single expr can have a * char as a wildcard                 //
//     but only in its begin or in its end                              //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//       $Id$

#include "XrdClient/XrdClientStringMatcher.hh"
#include "XrdClient/XrdClientConst.hh"

#include <iostream.h>


XrdClientStringMatcher::XrdClientStringMatcher(char *expr) {

   exp = strdup(expr);

}

XrdClientStringMatcher::~XrdClientStringMatcher() {
   free(exp);
}


bool XrdClientStringMatcher::SingleMatches(char *expr, char *str) {  
   char *plainexp;
   unsigned int exprlen, plainexplen;
   bool starbeg, starend;

   starbeg = FALSE;
   starend = FALSE;

   exprlen = strlen(expr);

   if (exprlen > 0) {
      starbeg = (expr[0] == '*');
      starend = ((expr[exprlen-1] == '*') && (exprlen > 1));
   }

   // Build plainexp by stripping the initial  *
   if (starbeg)
      plainexp = strdup(expr+1);
   else
      plainexp = strdup(expr);

   // Now strip the trailing * if there is one
   if (starend)
      plainexp[strlen(plainexp)-1] = '\0';

   plainexplen = strlen(plainexp);


   char *p = strstr(str, plainexp);
   if (!p) {
      free(plainexp);
      return FALSE;
   }

   if (starbeg && starend) {
      free(plainexp);
      return TRUE;
   }

   unsigned int pos = (int)(p - str);

   if (!starbeg && !starend) {
      free(plainexp);
      return ( (pos == 0) && (plainexplen == strlen(str)) );
   }

   unsigned int l = strlen(str);

   // If exp does not start with a * then match the start of str
   if (!starbeg && !pos) {
       free(plainexp);
       return TRUE;
   }

   // If exp does not end with a * then match the end of str
   if (!starend && (pos == (l - plainexplen))) {
       free(plainexp);
       return TRUE;
   }


   free(plainexp);
   return FALSE;

}


bool XrdClientStringMatcher::Matches(char *str) {
   char *p1, *p2;

   p1 = exp;
   p2 = strchr(exp, '|');

   while (TRUE) {

      if (p2)
	 *p2 = '\0';

      if (SingleMatches(p1, str)) {
	 if (p2)
	    *p2 = '|';

	 return TRUE;
      }

      if (p2)
	 *p2 = '|';


      if (p2) {
	 p1 = p2+1;
	 p2 = strchr(p1, '|');
      }
      else break;

   }

  
   return FALSE;
}

