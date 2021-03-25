#include "XrdCks/XrdCksCalccrc32C.hh"
void XrdCksCalccrc32C::Update(const char *Buff, int Blen)
{
    C32CResult = (unsigned int)crc32c(C32CResult, Buff, Blen);
}
const char *XrdCksCalccrc32C::Type(int &csSz)
{
    csSz = sizeof(TheResult);
    return "crc32c";
}