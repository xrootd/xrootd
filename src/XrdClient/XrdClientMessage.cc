//////////////////////////////////////////////////////////////////////////
//                                                                      //
// XrdMessage                                                           // 
//                                                                      //
// Author: Fabrizio Furano (INFN Padova, 2004)                          //
// Adapted from TXNetFile (root.cern.ch) originally done by             //
//  Alvise Dorigo, Fabrizio Furano                                      //
//          INFN Padova, 2003                                           //
//                                                                      //
// A message coming from a physical connection. I.e. a server response  //
//  or some kind of error                                               //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "XrdMessage.hh"
#include "XrdXProtocol.hh"
#include "XrdDebug.hh"
#include "XrdPhyConnection.hh"

#include <stdlib.h> // for malloc
#include <string.h> // for memcpy


//__________________________________________________________________________
XrdMessage::XrdMessage(struct ServerResponseHeader header)
{
   // Constructor

   fStatusCode = kXrdMSC_ok;
   memcpy((void *)&fHdr, (const void*)&header, sizeof(ServerResponseHeader));
   fData = 0;
   fMarshalled = false;
   if (!CreateData()) {
      Error("XrdMessage", 
            "Error allocating " << fHdr.dlen << " bytes.");
      fAllocated = false;
   } else 
      fAllocated = true;
}

//__________________________________________________________________________
XrdMessage::XrdMessage()
{
   // Default constructor

   memset(&fHdr, 0, sizeof(fHdr));
   fStatusCode = kXrdMSC_ok;
   fData = 0;
   fMarshalled = false;
   fAllocated = false;
}

//__________________________________________________________________________
XrdMessage::~XrdMessage()
{
   // Destructor

   if (fData)
      free(fData);
}

//__________________________________________________________________________
void *XrdMessage::DonateData()
{
   // Unlink the owned data in order to pass them elsewhere

   void *res = fData;
   fData = 0;
   fAllocated = false;
  
   return (res);
}

//__________________________________________________________________________
bool XrdMessage::CreateData()
{
   // Allocate data

   if (!fAllocated) {
      if (fHdr.dlen) {
         fData = malloc(fHdr.dlen+1);
         if (!fData) {
            Error("XrdMessage::CreateData","Fatal ERROR *** malloc failed."
                  " Probable system resources exhausted.");
            abort();
         }
         char *tmpPtr = (char *)fData;
         memset((void*)(tmpPtr+fHdr.dlen), 0, 1);
      }
      if (!fData)
         return FALSE;
      else
         return TRUE;
   } else
      return TRUE;
}

//__________________________________________________________________________
void XrdMessage::Marshall()
{
   // Marshall, i.e. put in network byte order

   if (!fMarshalled) {
      ServerResponseHeader2NetFmt(&fHdr);
      fMarshalled = TRUE;
   }
}

//__________________________________________________________________________
void XrdMessage::Unmarshall()
{
   // Unmarshall, i.e. from network byte to normal order

   if (fMarshalled) {
      clientUnmarshall(&fHdr);
      fMarshalled = FALSE;
   }
}

//__________________________________________________________________________
int XrdMessage::ReadRaw(XrdPhyConnection *phy)
{
   // Given a physical connection, we completely build the content
   // of the message, reading it from the socket of a phyconn

   int readres;
   int readLen = sizeof(ServerResponseHeader);

   Info(XrdDebug::kDUMPDEBUG,
	"XrdMessage::ReadRaw",
	"Reading header (" << readLen << " bytes) from socket.");
  
   readres = phy->ReadRaw((void *)&fHdr, readLen);

   if (readres) {

      if (readres == TXSOCK_ERR_TIMEOUT)
         SetStatusCode(kXrdMSC_timeout);
      else {
         Error("XrdMessage::ReadRaw",
	       "Error reading " << readLen << " bytes.");
         SetStatusCode(kXrdMSC_readerr);

      }
      memset(&fHdr, 0, sizeof(fHdr));
   }

   // the data arrive marshalled from the server (i.e. network byte order)
   SetMarshalled(TRUE);
   Unmarshall();

   if (fHdr.dlen) {

      Info(XrdDebug::kDUMPDEBUG,
	   "XrdMessage::ReadRaw",
	   "Reading data (" << fHdr.dlen << " bytes) from socket.");

      CreateData();
      if (phy->ReadRaw(fData, fHdr.dlen)) {
         Error("XrdMessage::ReadRaw", "Error reading " << fHdr.dlen << " bytes.");
         SetStatusCode(kXrdMSC_readerr);
      }
   }
   return 1;
}

//___________________________________________________________________________
void XrdMessage::Int2CharStreamid(kXR_char *charstreamid, short intstreamid)
{
   // Converts a streamid given as an integer to its representation
   // suitable for the streamid inside the messages (i.e. ascii)

   memcpy(charstreamid, &intstreamid, sizeof(intstreamid));
}

//___________________________________________________________________________
short XrdMessage::CharStreamid2Int(kXR_char *charstreamid)
{
   // Converts a streamid given as an integer to its representation
   // suitable for the streamid inside the messages (i.e. ascii)

   int res = *((short *)charstreamid);

   return res;
}
