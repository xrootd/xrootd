/******************************************************************************/
/*                                                                            */
/*                     X r d X m l M e t a L i n k . h h                      */
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
#include <cstdio>
#include <unistd.h>
#include <limits.h>

#include "XrdSys/XrdSysAtomics.hh"
#include "XrdSys/XrdSysE2T.hh"
#include "XrdSys/XrdSysFD.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdXml/XrdXmlMetaLink.hh"

/******************************************************************************/
/*                     L o c a l   D e f i n i t i o n s                      */
/******************************************************************************/

#define SizeOfVec(x) sizeof(x)/sizeof(x[0])

namespace
{
char         tmpPath[40];

unsigned int GenTmpPath()
{
// The below will not generate a result more than 31 characters.
//
     snprintf(tmpPath, sizeof(tmpPath), "/tmp/.MetaLink%8x.%d.",
                       static_cast<int>(time(0)), static_cast<int>(getpid()));
     return 0;
}

XrdSysMutex xMutex;

unsigned int seqNo = GenTmpPath();
}

/******************************************************************************/
/*                         L o c a l   C l a s s e s                          */
/******************************************************************************/

namespace
{
class CleanUp
{
public:

XrdXmlReader **delRDR;
char          *delTFN;

              CleanUp() : delRDR(0), delTFN(0) {}
             ~CleanUp() {if (delRDR) {delete *delRDR; *delRDR = 0;}
                         if (delTFN) unlink(delTFN);
                        }
};

class vecMon
{
public:

      vecMon(char **vec, int vecn)
            : theVec(vec), vecNum(vecn) {}
     ~vecMon() {if (theVec)
                   for (int i = 0; i < vecNum; i++)
                       if (theVec[i]) free(theVec[i]);
               }
private:
char        **theVec;
int           vecNum;
};
}
  
/******************************************************************************/
/*                               C o n v e r t                                */
/******************************************************************************/
  
XrdOucFileInfo *XrdXmlMetaLink::Convert(const char *fname, int blen)
{
   static const char *mlV3NS = "http://www.metalinker.org/";
   static const char *mlV4NS = "urn:ietf:params:xml:ns:metalink";
   static const char *mlV3[] = {"metalink", "files", 0};
   static const char *mTag[] = {"", "metalink", 0};
   static const char *mAtr[] = {"xmlns", 0};
          const char *scope  = "metalink";
                char *mVal[] = {0};
   CleanUp onReturn;
   XrdOucFileInfo *fP;
   const char *gLFN;
   char *colon, gHdr[272];
   bool chkG;

// If we are converting a buffer, then generate the file
//
   if (blen > 0)
      {if (!PutFile(fname, blen)) return 0;
       onReturn.delTFN = tmpFn;
       fname = tmpFn;
      }

// Check if we should add a global file entry
//
   if (rdHost && (rdProt || (prots && (colon = index(prots,':')))))
      {if (!rdProt) {rdProt = prots; *(colon+1) = 0;}
          else colon = 0;
       snprintf(gHdr, sizeof(gHdr), "%s//%s/", rdProt, rdHost);
       if (colon) *(colon+1) = ':';
       chkG = true;
      } else chkG = false;

// Get a file reader
//
   if (!(reader = XrdXmlReader::GetReader(fname, encType)))
      {eCode =  errno;
       snprintf(eText, sizeof(eText), "%s trying to read %s",
               (errno ? XrdSysE2T(errno) : "Unknown error"), fname);
       return 0;
      }

// Make sure we delete the reader should we return
//
   onReturn.delRDR = &reader;

// We must find the metalink tag
//
   if (!reader->GetElement(mTag, true))
      {GetRdrError("looking for 'metalink' tag");
       return 0;
      }

// The input can be in metalink 3 or metalink 4 format. The metalink tag will
// tell us which one it is. It better be in the document with the xmlns attribute
//
   if (!reader->GetAttributes(mAtr, mVal))
      {strcpy(eText, "Required metalink tag attribute 'xmlns' not found");
       eCode = ENOMSG;
       return 0;
      }

// The namespace tells us what format we are using here. For v3 formt we must
// alignh ourselves on the "files" tag. There can only be one of those present.
//
        if (!strcmp(mVal[0], mlV3NS))
           {if (!reader->GetElement(mlV3, true))
               GetRdrError("looking for 'files' tag");
            scope = "files";
           }
   else if ( strcmp((const char *)mVal[0], mlV4NS))
           {strcpy(eText, "Metalink format not supported");
            eCode = EPFNOSUPPORT;
           }

// Check if can continue
//
   free(mVal[0]);
   if (eCode) return 0;

// Get one or more files
//
   currFile = 0; fileCnt = 0; noUrl = true;
   do{if (!GetFile(scope)) break;
      currFile = new XrdOucFileInfo;
      if (GetFileInfo("file"))
         {if (lastFile) lastFile ->nextFile = currFile;
             else       fileList = currFile;
          lastFile = currFile;
          if (chkG && (gLFN = currFile->GetLfn()))
             {char lfnBuff[2048];
              snprintf(lfnBuff, sizeof(lfnBuff), "%s%s", gHdr, gLFN);
              currFile->AddUrl(lfnBuff, 0, INT_MAX);
              currFile->AddProtocol(rdProt);
             }
          currFile = 0;
          fileCnt++; noUrl = true;
         }
     } while(doAll);

// The loop ends when we cannot find a file tag. So, the current file is invalid
//
   if (currFile) {delete currFile; currFile = 0;}

// Check if we have any files at all
//
   if (!fileCnt)
      {strcpy(eText, "No applicable urls specified for the file entry");
       eCode = EDESTADDRREQ;
      }

// If this is an all call then return to execute the postantem
//
   fP = fileList; lastFile = fileList = 0;
   if (doAll) return fP;

// Check if we have clean status. If not, undo all we have and return failure
//
   if (!eCode) return fP;
   if (fP) delete fP;
   return 0;
}

/******************************************************************************/
/*                            C o n v e r t A l l                             */
/******************************************************************************/
  
XrdOucFileInfo **XrdXmlMetaLink::ConvertAll(const char *fname, int &count,
                                                               int  blen)
{
   CleanUp onReturn;
   XrdOucFileInfo *fP, **fvP;

// Indicate this is a call from here
//
   doAll = true;
   count = 0;

// If we are converting a buffer, then generate the file
//
   if (blen > 0)
      {if (!PutFile(fname, blen)) return 0;
       onReturn.delTFN = tmpFn;
       fname = tmpFn;
      }

// Perform the conversion
//
   if (!(fP = Convert(fname))) return 0;

// Check if we have clean status, if not return nothing
//
   if (eCode)
      {XrdOucFileInfo *fnP = fP->nextFile;
       while((fP = fnP))
            {fnP = fP->nextFile;
             delete fP;
            }
       return 0;
      }

// Return a vector of the file info objects
//
   fvP = new XrdOucFileInfo* [fileCnt];
   for (int i = 0; i < fileCnt; i++) {fvP[i] = fP; fP = fP->nextFile;}
   count = fileCnt;
   return fvP;
}

/******************************************************************************/
/*                             D e l e t e A l l                              */
/******************************************************************************/

void XrdXmlMetaLink::DeleteAll(XrdOucFileInfo ** vecp, int vecn)
{
// Delete each object in the vector
//
   for (int i = 0; i < vecn; i++)
     delete vecp[i];

// Now delete the vector
//
   delete []vecp;
}

/******************************************************************************/
/* Private:                      G e t F i l e                                */
/******************************************************************************/
  
bool XrdXmlMetaLink::GetFile(const char *scope)
{
   const char *fileElem[] = {scope, "file", 0};
   const char *etext;
   bool needFile = fileCnt == 0;

// We align on "file" this is true at this point regardless of version.
//
   if (!reader->GetElement(fileElem, needFile))
      {if ((etext = reader->GetError(eCode)))
	  {size_t len = strlen(etext);
           if(len > sizeof(eText)-1) len=sizeof(eText)-1;
           memcpy(eText, etext, len);
	   eText[len]=0;
	  }
       return false;
      }

// We are now aligned on a file tag
//
   return true;
}
  
/******************************************************************************/
/* Private:                  G e t F i l e I n f o                            */
/******************************************************************************/
  
bool XrdXmlMetaLink::GetFileInfo(const char *scope)
{
   static const char *fileScope = "file";
   const char *fsubElem[] = {scope, "url", "hash", "size",
                             "verification", "resources", "glfn", 0};
   int ePos;

   if(strncmp(scope, fileScope, 4) == 0) GetName();

// Process the elements in he file section. Both formats have the same tags,
// though not the same attributes. We will take care of the differences later.
//
   while((ePos = reader->GetElement(fsubElem)))
        switch(ePos)
              {case 1:  if (!GetUrl())  return false;
                        break;
               case 2:  if (!GetHash()) return false;
                        break;
               case 3:  if (!GetSize()) return false;
                        break;
               case 4:  GetFileInfo("verification");
                        if (eCode)      return false;
                        break;
               case 5:  GetFileInfo("resources");
                        if (eCode)      return false;
                        break;
               case 6:  if (!GetGLfn()) return false;
                        break;
               default: break;
              }

// Return success if we had at least one url
//
   return !noUrl;
}

/******************************************************************************/
/* Private:                      G e t G L f n                                */
/******************************************************************************/

bool XrdXmlMetaLink::GetGLfn()
{
   static const char *gAttr[] = {"name", 0};
                char *gAVal[] = {0};
   vecMon monVec(gAVal, SizeOfVec(gAVal));

// Get the name
//
   if (!reader->GetAttributes(gAttr, gAVal))
      {strcpy(eText, "Required glfn tag name attribute not found");
       eCode = ENOMSG;
       return false;
      }

// Add the the glfn
//
   currFile->AddLfn(gAVal[0]);

// All done
//
   return true;
}

/******************************************************************************/
/* Private:                      G e t H a s h                                */
/******************************************************************************/

bool XrdXmlMetaLink::GetHash()
{
   static const char *hAttr[] = {"type", 0};
                char *hAVal[] = {0};
   vecMon monVec(hAVal, SizeOfVec(hAVal));
   char  *value;

// Get the hash type
//
   if (!reader->GetAttributes(hAttr, hAVal))
      {strcpy(eText, "Required hash tag type attribute not found");
       eCode = ENOMSG;
       return false;
      }

// Now get the hash value
//
   if (!(value = reader->GetText("hash", true))) return false;

// Add a new digest
//
   currFile->AddDigest(hAVal[0], value);

// All done
//
   free(value);
   return true;
}
  
/******************************************************************************/
/*                           G e t R d r E r r o r                            */
/******************************************************************************/

void XrdXmlMetaLink::GetRdrError(const char *why)
{
   const char *etext = reader->GetError(eCode);

   if (etext) 
      {size_t len = strlen(etext);
       if(len > sizeof(eText)-1) len = sizeof(eText)-1;
       memcpy(eText, etext, len);
       eText[len]=0;
      }
      else {snprintf(eText, sizeof(eText), "End of xml while %s", why);
            eCode = EIDRM;
           }
}
  
/******************************************************************************/
/* Private:                      G e t S i z e                                */
/******************************************************************************/

bool XrdXmlMetaLink::GetSize()
{
   char *eP, *value;
   long long fsz;

// Now get the size value
//
   if (!(value = reader->GetText("size", true))) return false;
  
// Convert size, it must convert clean and be non-negatie
//
   fsz = strtoll(value, &eP, 10);
   if (fsz < 0 || *eP != 0)
      {snprintf(eText,sizeof(eText), "Size tag value '%s' is invalid", value);
       eCode = EINVAL;
       free(value);
       return false;
      }

// Set the size and return
//
   currFile->SetSize(fsz);
   free(value);
   return true;
}

/******************************************************************************/
/* Private:                       G e t U r l                                 */
/******************************************************************************/

bool XrdXmlMetaLink::GetUrl()
{
   static const char *uAttr[] = {"location", "priority", "preference", 0};
                char *uAVal[] = {0, 0, 0};
   vecMon monVec(uAVal, SizeOfVec(uAVal));
   char *value;
   int prty = 0;
  
// Get the optional attributes
//
   reader->GetAttributes(uAttr, uAVal);

// Now get the url value. There might be one, that is valid and we ignore it.
//
   if (!(value = reader->GetText("url"))) return true;

// Check if we need to screen url protocols
//
   if (!UrlOK(value))
      {free(value);
       return true;
      }

// Process priority or preference (we ignore errors here)
//
   if (uAVal[1]) prty = atoi(uAVal[1]);
      else if (uAVal[2])
              {prty = 100 - atoi(uAVal[2]);
               if (prty < 0) prty = 0;
              }

// Add the url to the flle
//
   currFile->AddUrl(value, uAVal[0], prty);
   free(value);

// All done
//
   noUrl = false;
   return true;
}

/******************************************************************************/
/* Private:                       G e t N a m e                               */
/******************************************************************************/

void XrdXmlMetaLink::GetName()
{
  static const char *mAtr[] = {"name", 0};
  char *mVal[] = {0};
  reader->GetAttributes(mAtr, mVal);
  currFile->AddFileName(mVal[0]);
  free(mVal[0]);
}

/******************************************************************************/
/* Private:                      P u t F i l e                                */
/******************************************************************************/
  
bool XrdXmlMetaLink::PutFile(const char *buff, int blen)
{
   static const int oFlags = O_EXCL | O_CREAT | O_TRUNC | O_WRONLY;
   const char *what = "opening";
   unsigned int fSeq;
   int fd;

// Get a unique sequence number
//
   AtomicBeg(xMutex);
   fSeq = AtomicInc(seqNo);
   AtomicEnd(xMutex);

// Generate a unique filepath. Unfortunately, mktemp is unsafe and mkstemp may
// leak a file descriptor. So, we roll our own using above sequence number.
// Note that the target buffer is 64 characters which is suffcient for us.
//
   snprintf(tmpFn, sizeof(tmpFn), "%s%u", tmpPath, fSeq);

// Open the file for output, write out the buffer, and close the file
//
   if ((fd = XrdSysFD_Open(tmpFn, oFlags, S_IRUSR|S_IWUSR)) > 0)
      {what = "writing";
       if (write(fd, buff, blen) == blen)
          {what = "closing";
           if (!close(fd)) return true;
          }
      }

// We failed
//
   eCode = errno;
   snprintf(eText, sizeof(eText), "%s %s %s", XrdSysE2T(eCode), what, tmpFn);
   unlink(tmpFn);
   return false;
}
  
/******************************************************************************/
/* Private:                        U r l O K                                  */
/******************************************************************************/

bool XrdXmlMetaLink::UrlOK(char *url)
{
   char *colon, pBuff[16];
   int n;
  
// Find the colon and get the length of the protocol
//
   if (!(colon = index(url, ':'))) return false;
   n = colon - url + 1;
   if (n >= (int)sizeof(pBuff)) return false;
   strncpy(pBuff, url, n);
   pBuff[n] = 0;

// Add this protocol to the list we found
//
   currFile->AddProtocol(pBuff);

// Return whether or not this os one of the acceptable protocols
//
   if (prots) return (strstr(prots, pBuff) != 0);
   return true;
}
