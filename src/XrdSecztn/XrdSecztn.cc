// Copyright (c) 2015 Erwin Jansen
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// Note: the code in the anonymous namespace came from Erwin Jansen but was
// heavily edited to solve this particular problem. For more info see:
// https://github.com/pokowaka/jwt-cpp

#include <alloca.h>
#include <cstdint>
#include <cstring>

#define WHITESPACE 64
#define EQUALS 65
#define INVALID 66

/******************************************************************************/
/*                       L o c a l   F u n c t i o n s                        */
/******************************************************************************/
  
namespace
{
    const char b64Table[] = {
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 62, 66, 62, 66, 63, 52, 53, 54, 55, 56, 57,
        58, 59, 60, 61, 66, 66, 66, 66, 66, 66, 66, 0,  1,  2,  3,  4,  5,  6,
        7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
        25, 66, 66, 66, 66, 63, 66, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
        37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66, 66,
        66, 66, 66, 66};

/******************************************************************************/
/*                     D e c o d e B y t e s N e e d e d                      */
/******************************************************************************/
  
  /**
   * Gets the number of bytes needed to decode a base64 encoded string of the
   * given size
   * @param num_decode length of the string you wish to decode
   * @return the number of bytes encoded in this string
   */
  size_t DecodeBytesNeeded(size_t num_decode) {
    return 3 + (num_decode / 4) * 3;
  }

/******************************************************************************/
/*                             D e c o d e U r l                              */
/******************************************************************************/
  
int DecodeUrl(const char *decode, size_t num_decode, char *out, size_t &num_out)
{
  // No integer overflows please.
  if ((decode + num_decode) < decode || (out + num_out) < out)
    return 1;

  if (num_out < DecodeBytesNeeded(num_decode))
    return 1;

  const char *end = decode + num_decode;
  const char *out_start = out;
  char iter = 0;
  uint32_t buf = 0;
  uint8_t ch;
  char c;

  while (decode < end) {
    ch = *decode++;
    c = b64Table[ch];

    switch (c) {
    case INVALID:
      return 1; // invalid input, return error
    default:
      buf = buf << 6 | c;
      iter++; // increment the number of iteration
      // If the buffer is full, split it into bytes
      if (iter == 4) {
        *(out++) = (buf >> 16) & 0xff;
        *(out++) = (buf >> 8) & 0xff;
        *(out++) = buf & 0xff;
        buf = 0;
        iter = 0;
      }
    }
  }

  if (iter == 3) {
    *(out++) = (buf >> 10) & 0xff;
    *(out++) = (buf >> 2) & 0xff;
  } else {
    if (iter == 2) {
      *(out++) = (buf >> 4) & 0xff;
    }
  }

  num_out = (out - out_start); // modify to reflect the actual output size
  return 0;
}
}

/******************************************************************************/
/*                      X r d S e c z t n : : i s J W T                       */
/******************************************************************************/
  
namespace XrdSecztn
{
bool isJWT(const char *b64data)
{
   size_t inBytes, outBytes;
   const char *dot;
   char *key, *outData, inData[1024];

// Skip over the header should it exist (sommetime it does sometimes not)
//
   if (!strncmp(b64data, "Bearer%20", 9)) b64data += 9;

// We are only interested in the header which must appear first and be
// separated by a dot from subsequent tokens. If it does not have the
// dot then we assume it's not returnable. Otherwise truncate it at the dot.
//
   if (!(dot = index(b64data, '.'))) return false;

// Copy out the token segment we wish to check. The JWT header can never be
// more than 1K long and that's being way generous.
//
   inBytes = dot - b64data;
   if (inBytes >= (int)sizeof(inData)) return false;
   memcpy(inData, b64data, inBytes);
   inData[inBytes] = 0;

// Allocate a buffer large enough to hold the result. Get it from the stack.
//
   outBytes = DecodeBytesNeeded(inBytes);
   outData  = (char *)alloca(outBytes);

// If we can't decode what we have then indicate this is not returnable
//
   if (DecodeUrl(inData, inBytes, outData, outBytes)) return false;

// The json object must start/end with a brace and must contain the key:value
// of '"typ":"JWT"', other elements may change but not this one.
//
   if (outBytes <= 0 || *outData != '{' || outData[outBytes-1] != '}')
      return false;

// Search for the key
//
   if (!(key = strstr(outData, "\"typ\""))) return false;

// Subsequently there should be a colon or spaces but nothing more
//
   key += 5;
   while(*key == ' ') key++;
   if (*key != ':') return false;

// There may be more spaces but anything else must be the expected value
//
   key++;
   while(*key == ' ') key++;
   return strncmp(key, "\"JWT\"", 5) == 0;
}
}
