#include "XrdCks/XrdCksCalccrc32C.hh"

void XrdCksCalccrc32C::Update(const char *Buff, int BLen)
{
    C32CResult = (unsigned int)XrdOucCRC::Calc32C(Buff, BLen, C32CResult);
}

const char *XrdCksCalccrc32C::Type(int &csSz)
{
    csSz = sizeof(TheResult);
    return "crc32c";
}

XrdCksCalc *XrdCksCalccrc32C::New() { return (XrdCksCalc *)new XrdCksCalccrc32C; }

void XrdCksCalccrc32C::Init()
{
    C32CResult = C32C_XINIT;
}

char *XrdCksCalccrc32C::Final()
{
    TheResult = C32CResult;
#ifndef Xrd_Big_Endian
    TheResult = htonl(TheResult);
#endif
    return (char *)&TheResult;
}

XrdCksCalccrc32C::XrdCksCalccrc32C() { Init(); }

XrdCksCalccrc32C::~XrdCksCalccrc32C() {};