#include "XrdCks/XrdCksCalccrc32C.hh"

/*
    C++ implementation of CRC-32C checksums based upon
    unattributed library functions.

    This file contains:
        functions implementing the methods of the XrdCksCalc class

    Provided by:
        Anton Schwarz
        University of Heidelberg
        July 26, 2021

    Status:
        Public Domain

*/

/******************************************************************************/
/*                            C o m b i n e   V 1                             */
/******************************************************************************/

const char* XrdCksCalccrc32C::Combine(const char* Cksum, int DLen)
{
   uint32_t crc2;
   memcpy(&crc2, Cksum, sizeof(crc2));

#ifndef Xrd_Big_Endian
    crc2 = ntohl(crc2);
#endif

   C32CResult = XrdOucCRC::Combine32C(C32CResult, crc2, (size_t)DLen);
   return Final();
}

/******************************************************************************/
/*                            C o m b i n e   V 2                             */
/******************************************************************************/

const char* XrdCksCalccrc32C::Combine(const char* Cksum1, const char* Cksum2,
                                      int DLen)
{
   uint32_t crc1, crc2;
   memcpy(&crc1, Cksum1, sizeof(crc1));
   memcpy(&crc2, Cksum2, sizeof(crc2));

#ifndef Xrd_Big_Endian
    crc1 = ntohl(crc1);
    crc2 = ntohl(crc2);
#endif

   TheResult = XrdOucCRC::Combine32C(crc1, crc2, (size_t)DLen);

#ifndef Xrd_Big_Endian
    TheResult = htonl(TheResult);
#endif
    return (char *)&TheResult;
}

/******************************************************************************/
/*                                 F i n a l                                  */
/******************************************************************************/

char *XrdCksCalccrc32C::Final()
{
    TheResult = C32CResult;
#ifndef Xrd_Big_Endian
    TheResult = htonl(TheResult);
#endif
    return (char *)&TheResult;
}

/******************************************************************************/
/*                                   N e w                                    */
/******************************************************************************/

XrdCksCalc *XrdCksCalccrc32C::New()
{ return (XrdCksCalc *)new XrdCksCalccrc32C; }

/******************************************************************************/
/*                                U p d a t e                                 */
/******************************************************************************/

void XrdCksCalccrc32C::Update(const char *Buff, int BLen)
{
    C32CResult = (unsigned int)XrdOucCRC::Calc32C(Buff, BLen, C32CResult);
}
