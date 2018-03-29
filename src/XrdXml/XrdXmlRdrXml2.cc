/******************************************************************************/
/*                                                                            */
/*                      X r d X m l R d r X m l 2 . c c                       */
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

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include <libxml/xmlreader.h>

#include "XrdXml/XrdXmlRdrXml2.hh"

/******************************************************************************/
/*                     L o c a l   D e f i n i t i o n s                      */
/******************************************************************************/
  
namespace
{
// Develop a sane enum structure of xml node types
//
enum XmlNodeType {ntNone    = XML_READER_TYPE_NONE,
                  ntElmBeg  = XML_READER_TYPE_ELEMENT,
                  ntAttr    = XML_READER_TYPE_ATTRIBUTE,
                  ntText    = XML_READER_TYPE_TEXT,
                  ntCData   = XML_READER_TYPE_CDATA,
                  ntEntRef  = XML_READER_TYPE_ENTITY_REFERENCE,
                  ntEntBeg  = XML_READER_TYPE_ENTITY,
                  ntPI      = XML_READER_TYPE_PROCESSING_INSTRUCTION,
                  ntCmt     = XML_READER_TYPE_COMMENT,
                  ntDoc     = XML_READER_TYPE_DOCUMENT,
                  ntDTD     = XML_READER_TYPE_DOCUMENT_TYPE,
                  ntDFrag   = XML_READER_TYPE_DOCUMENT_FRAGMENT,
                  ntNote    = XML_READER_TYPE_NOTATION,
                  ntWSpace  = XML_READER_TYPE_WHITESPACE,
                  ntWSpSig  = XML_READER_TYPE_SIGNIFICANT_WHITESPACE,
                  ntElmEnd  = XML_READER_TYPE_END_ELEMENT,
                  ntEntEnd  = XML_READER_TYPE_END_ENTITY,
                  ntXMLDcl  = XML_READER_TYPE_XML_DECLARATION
                 };

/******************************************************************************/
/*                           X m l N o d e N a m e                            */
/******************************************************************************/
  
const char *NodeName(int ntype)
{
   switch(ntype)
         {case ntNone:    return "isNode  "; break;
          case ntElmBeg:  return "isElmBeg"; break;
          case ntAttr:    return "isAttr  "; break;
          case ntText:    return "isText  "; break;
          case ntCData:   return "isCData "; break;
          case ntEntRef:  return "isEntRef"; break;
          case ntEntBeg:  return "isEntBeg"; break;
          case ntPI:      return "isPI    "; break;
          case ntCmt:     return "isCmt   "; break;
          case ntDoc:     return "isDoc   "; break;
          case ntDTD:     return "isDTD   "; break;
          case ntDFrag:   return "isDFrag "; break;
          case ntWSpace:  return "isWSpace"; break;
          case ntWSpSig:  return "isWSpSig"; break;
          case ntNote:    return "isNote  "; break;
          case ntElmEnd:  return "isElmEnd"; break;
          case ntEntEnd:  return "isEntEnd"; break;
          case ntXMLDcl:  return "isXMLDcl"; break;
          default: break;
         };
   return "???";
}
}

/******************************************************************************/
/*                        C o n s t r c u t o r   # 1                         */
/******************************************************************************/
  
XrdXmlRdrXml2::XrdXmlRdrXml2(bool &aOK, const char *fname, const char *enc)
{
// Initialize the standard values
//
   encType = (enc ? strdup(enc) : 0);
   eCode   = 0;
  *eText   = 0;
   doDup   = true;    // We always duplicate memory to avoid allocator issues
   debug   = getenv("XrdXmlDEBUG") != 0;

// Get a file reader
//
   if (!(reader = xmlNewTextReaderFilename(fname)))
      {if ((eCode = errno)) 
	  {size_t size = sizeof(eText) - 1;
           strncpy(eText, strerror(errno), size);
	   eText[size] = '\0';
	  }
          else strcpy(eText, "Unknown error opening input file");
       aOK = false;
      } else aOK = true;
}

/******************************************************************************/
/*                            D e s t r u c t o r                             */
/******************************************************************************/

XrdXmlRdrXml2::~XrdXmlRdrXml2()
{

// Tear down the reader
//
   xmlFreeTextReader(reader); reader = 0;
}
  
/******************************************************************************/
/* Private:                        D e b u g                                  */
/******************************************************************************/

void XrdXmlRdrXml2::Debug(const char *hdr, const char *want, char *have,
                          const char *scope, int nType)
{
   char buff[512];

// Format the message
//
   snprintf(buff,sizeof(buff),"%s %s depth: %d scope: %s want: %s have: %s\n",
            hdr,NodeName(nType),xmlTextReaderDepth(reader),scope,want,have);
   std::cerr <<buff <<std::flush;
}
  
/******************************************************************************/
/*                                  F r e e                                   */
/******************************************************************************/

void XrdXmlRdrXml2::Free(void *strP) {xmlFree(strP);}
  
/******************************************************************************/
/*                         G e t A t t r i b u t e s                          */
/******************************************************************************/
  
bool XrdXmlRdrXml2::GetAttributes(const char **aname, char **aval)
{
   char *name, *value;
   int   i;
   bool  found = false;

// If we are not at the begining of an element, this is a sequence error
//
   if (xmlTextReaderNodeType(reader) != ntElmBeg)
      {snprintf(eText, sizeof(eText),
                "Illegal position seeking attribute %s",aname[0]);
       eCode = EILSEQ;
       return false;
      }

// Find the attribute
//
   while(xmlTextReaderMoveToNextAttribute(reader))
        {if ((name = GetName()))
            {i = 0;
             while(aname[i] && strcmp(name, aname[i])) i++;
             xmlFree(name);
             if (aname[i])
                {if (!(value = (char *)xmlTextReaderValue(reader))) continue;
                 found = true;
                 if (doDup)
                    {if (aval[i]) free(aval[i]);
                     aval[i] = strdup(value);
                     xmlFree(value);
                    } else {
                     if (aval[i]) xmlFree(aval[i]);
                     aval[i] = value;
                    }
                }
            }
        }

// All done
//
   return found;
}
  
/******************************************************************************/
/*                            G e t E l e m e n t                             */
/******************************************************************************/
  
int XrdXmlRdrXml2::GetElement(const char **ename, bool reqd)
{
   char *name = 0;
   int i, nType;

// Scan over to the wanted element
//
   while(xmlTextReaderRead(reader) == 1)
        {nType = xmlTextReaderNodeType(reader);
         if (nType == ntWSpSig || !(name = GetName())) continue;

              if (nType == ntElmEnd)
                 {if (debug) Debug("getelem:",ename[1],name,ename[0],nType);
                  if (!strcmp(name, ename[0])) break;
                 }
         else if (nType == ntElmBeg)
                 {i = 1;
                  while(ename[i] && strcmp(name, ename[i])) i++;
                  if (ename[i])
                     {if (debug) Debug("getelem:",ename[i],name,ename[0],nType);
                      xmlFree(name);
                      return i;
                     }
                  if (debug) Debug("getelem:",ename[1],name,ename[0],nType);
                  xmlFree(name);
                 }
        }

// Free any allocate storage
//
   if (name) xmlFree(name);

// This is an error if this element was required
//
   if (reqd)
      {snprintf(eText,sizeof(eText),"Required element '%s' not found in %s",
                (ename[1] ? ename[1] : "???"), ename[0]);
       eCode = ESRCH;
      }
   return 0;
}

/******************************************************************************/
/* Private:                      G e t N a m e                                */
/******************************************************************************/

char *XrdXmlRdrXml2::GetName()
{
    return (char *)xmlTextReaderName(reader);
}
  
/******************************************************************************/
/*                               G e t T e x t                                */
/******************************************************************************/

char *XrdXmlRdrXml2::GetText(const char *ename, bool reqd)
{
   char *sP, *value = 0;

// Get next element and make sure it exists and is text
//
   if (xmlTextReaderRead(reader)     == 1
   &&  xmlTextReaderNodeType(reader) == ntText)
      {if ((value = (char *)xmlTextReaderValue(reader)) && !(*value))
          {xmlFree(value); value = 0;}
      }

// We did not find a value. If not required return.
//
   if (value || !reqd)
      {if (!doDup || !value) return value;
       sP = strdup(value);
       xmlFree(value);
       return sP;
      }

// Create error message
//
   snprintf(eText,sizeof(eText),"Required %s tag text value not found",ename);
   eCode = ENOMSG;
   return 0;
}

/******************************************************************************/
/*                                  I n i t                                   */
/******************************************************************************/
  
bool XrdXmlRdrXml2::Init() {xmlInitParser(); return true;}
