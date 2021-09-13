/******************************************************************************/
/*                                                                            */
/*                      X r d X m l R d r T i n y . c c                       */
/*                                                                            */
/* (c) 2015 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* You should have received a copy of the GNU Lesser General Public License   */
/* along with XRootD in a file called COPYING.LESSER (LGPL license) and file  */
/* COPYING (GPL license).  If not, see <http://www.gnu.org/licenses/>.        */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>

#include "XrdSys/XrdSysE2T.hh"
#include "tinyxml.h"
#include "XrdXml/XrdXmlRdrTiny.hh"

/******************************************************************************/
/*                     L o c a l   D e f i n i t i o n s                      */
/******************************************************************************/

namespace
{
// Develop a sane enum structure of xml node types
//
enum XmlNodeType {ntNone    = TiXmlNode::TINYXML_UNKNOWN,
                  ntElmBeg  = TiXmlNode::TINYXML_ELEMENT,
                  ntElmEnd  = -1,
                  ntText    = TiXmlNode::TINYXML_TEXT,
                  ntCmt     = TiXmlNode::TINYXML_COMMENT,
                  ntDoc     = TiXmlNode::TINYXML_DOCUMENT,
                  ntXMLDcl  = TiXmlNode::TINYXML_DECLARATION
                 };

/******************************************************************************/
/*                           X m l N o d e N a m e                            */
/******************************************************************************/
  
const char *NodeName(int ntype)
{
   switch(ntype)
         {case ntNone:    return "isNode  "; break;
          case ntElmBeg:  return "isElmBeg"; break;
          case ntText:    return "isText  "; break;
          case ntCmt:     return "isCmt   "; break;
          case ntDoc:     return "isDoc   "; break;
          case ntElmEnd:  return "isElmEnd"; break;
          case ntXMLDcl:  return "isXMLDcl"; break;
          default: break;
         };
   return "???";
}
}

/******************************************************************************/
/*                        C o n s t r c u t o r   # 1                         */
/******************************************************************************/
  
XrdXmlRdrTiny::XrdXmlRdrTiny(bool &aOK, const char *fname, const char *enc) : reader(0) // make sure the pointer is nil initialized otherwise if stat fails the destructor segfaults
{
   struct stat Stat;
   const char *etext;

// Initialize the standard values
//
   curNode = 0;
   curElem = 0;
   elmNode = 0;
   eCode   = 0;
  *eText   = 0;
   debug   = getenv("XrdXmlDEBUG") != 0;

// Make sure this file exists
//
   if (stat(fname, &Stat))
      {eCode = errno;
       snprintf(eText,sizeof(eText),"%s opening %s", XrdSysE2T(errno), fname);
       aOK = false;
       return;
      }

// Get a file reader
//
   reader = new TiXmlDocument(fname);
   if (reader->LoadFile())
      {curNode = (TiXmlNode *)reader;
       curElem = 0;
       elmNode = curNode;
       aOK = true;
      } else {
       if (!(etext = reader->ErrorDesc()) || *etext)
          {if ((eCode = errno)) etext = XrdSysE2T(errno);
              else etext =  "Unknown error";
          }
       snprintf(eText,sizeof(eText),"%s opening %s", etext, fname);
       eCode = EINVAL;
       aOK = false;
      }
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdXmlRdrTiny::~XrdXmlRdrTiny()
{

// Tear down the reader
//
   if (reader)
   {
     delete reader;
     reader = 0;
   }
}
  
/******************************************************************************/
/* Private:                        D e b u g                                  */
/******************************************************************************/

void XrdXmlRdrTiny::Debug(const char *hdr, const char *want, const char *have,
                          const char *scope, int nType)
{
   char buff[512];

// Format the message
//
   snprintf(buff,sizeof(buff),"%s %s scope: %s want: %s have: %s\n",
            hdr,NodeName(nType),scope,want,have);
   std::cerr <<buff <<std::flush;
}
  
/******************************************************************************/
/*                         G e t A t t r i b u t e s                          */
/******************************************************************************/
  
bool XrdXmlRdrTiny::GetAttributes(const char **aname, char **aval)
{
   const char *value;
   int   i = 0;
   bool  found = false;

// If we are not at the begining of an element, this is a sequence error
//
   if (!curElem)
      {snprintf(eText, sizeof(eText),
                "Element not fetched when seeking attribute %s",aname[0]);
       eCode = EILSEQ;
       return false;
      }

// Find all the requested attributes
//
   while(aname[i])
        {if ((value = curElem->Attribute(aname[i])))
            {if (aval[i]) free(aval[i]);
             aval[i] = strdup(value);
             found = true;
            }
         i++;
        }

// All done
//
   return found;
}
  
/******************************************************************************/
/*                            G e t E l e m e n t                             */
/******************************************************************************/
  
int XrdXmlRdrTiny::GetElement(const char **ename, bool reqd)
{
   TiXmlNode *theChild;
   const char *name = (curNode ? curNode->Value() : 0);
   int i;

// If we are positioned either at the current node or the last node we returned
// Complain if that is not the case.
//
   if (*ename[0])
      {if (name && strcmp(name, ename[0]))
          {if (curElem && !strcmp(elmNode->Value(),ename[0])) curNode = elmNode;
              else {snprintf(eText, sizeof(eText),
                    "Current context '%s' does not match stated scope '%s'",
                     (name ? name : ""), ename[0]);
                    eCode = EILSEQ;
                    return false;
                   }
          }
      }

// Sequence to the next node at appropriate level.
//
    if (curNode == elmNode ) theChild = curNode->FirstChild();
       else if (elmNode)     theChild = elmNode->NextSibling();
               else          theChild = curNode->NextSibling();


// Scan over to the first wanted element
//
   while(theChild)
        {if ((name = theChild->Value()) && theChild->Type() == ntElmBeg)
            {i = 1;
             while(ename[i] && strcmp(name, ename[i])) i++;
             if (ename[i])
                {if (debug) Debug("getelem:",ename[i],name,ename[0],ntElmBeg);
                 curElem = theChild->ToElement();
                 elmNode = theChild;
                 return i;
                }
            }
          theChild = theChild->NextSibling();
         }

// We didn't find any wanted tag here in this scope. Transition to the element's
// parent we finished the tag
//
   if (debug) Debug("getelem:",ename[1],ename[0],ename[0],ntElmEnd);
   elmNode = curNode;
   curNode = curNode->Parent();
   curElem = 0;
   return 0;

// This is an error if this element was required
//
   if (reqd)
      {snprintf(eText,sizeof(eText),"Required element '%s' not found in '%s'",
                (ename[1] ? ename[1] : "???"), ename[0]);
       eCode = ESRCH;
      }
   return 0;
}
  
/******************************************************************************/
/*                               G e t T e x t                                */
/******************************************************************************/

char *XrdXmlRdrTiny::GetText(const char *ename, bool reqd)
{
   const char *value;
   char *sP;

// If we are not at the begining of an element, this is a sequence error
//
   if (!curElem)
      {snprintf(eText, sizeof(eText),
                "Illegal position seeking text for tag %s",ename);
       eCode = EILSEQ;
       return 0;
      }

// Get the text associated with element (simple text only)
//
   value = curElem->GetText();

// We did not find a value. If not required return.
//
   if (value || !reqd)
      {if (!value) return 0;
       sP = strdup(value);
       return sP;
      }

// Create error message
//
   snprintf(eText, sizeof(eText), "Required %s tag value not found", ename);
   eCode = ENOMSG;
   return 0;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
bool XrdXmlRdrTiny::Init() {return true;}
