/*-----------------------------------------------------------------------------
Copyright(c) 2010 - 2018 ViSUS L.L.C.,
Scientific Computing and Imaging Institute of the University of Utah

ViSUS L.L.C., 50 W.Broadway, Ste. 300, 84101 - 2044 Salt Lake City, UT
University of Utah, 72 S Central Campus Dr, Room 3750, 84112 Salt Lake City, UT

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

For additional information about this project contact : pascucci@acm.org
For support : support@visus.net
-----------------------------------------------------------------------------*/

#include "StringUtils.h"
#include "Time.h"
#include "HeapMemory.h"

#include <iostream>
#include <iomanip>
#include <cctype>

#include "cryptlite/sha1.h"
#include "cryptlite/sha256.h"
#include "cryptlite/hmac.h"

namespace Visus {

///////////////////////////////////////////////////////////////////////////
bool cbool(String s) 
{
  s = StringUtils::toLower(StringUtils::trim(s));
  if (s.empty())
    return false;
  else if (s == "true")
    return true;
  else if (s == "false")
    return false;
  else
    return std::stoi(s)?true:false;
}


///////////////////////////////////////////////////////////////////////////
String StringUtils::onlyAlNum(String value)
{
  for (int I = 0; I < (int)value.size(); I++)
  {
    if (!std::isalnum(value[I]))
      value[I] = '_';
  }
  return value;
}

///////////////////////////////////////////////////////////////////////////
static char CharToUpper(char c) 
{return std::toupper(c);}

static char CharToLower(char c) 
{return std::tolower(c);}


String StringUtils::toLower(String ret) 
{std::transform(ret.begin(),ret.end(),ret.begin(),CharToLower);return ret;}

String StringUtils::toUpper(String ret) 
{std::transform(ret.begin(),ret.end(),ret.begin(),CharToUpper);return ret;}

///////////////////////////////////////////////////////////////////////////
std::vector<String> StringUtils::split(String source,String separator,bool bPurgeEmptyItems) 
{
  std::vector<String> ret;
  int m=(int)separator.size();
  for (int j;(j=(int)source.find(separator))>=0;source=source.substr(j+m))
  {
    String item=source.substr(0,j);
    if (!bPurgeEmptyItems || !item.empty()) ret.push_back(item);
  }
  if (!bPurgeEmptyItems || !source.empty()) ret.push_back(source);
  return ret;
}

///////////////////////////////////////////////////////////////////////////
String StringUtils::join(std::vector<String> v,String separator,String prefix,String suffix)
{
  int N=(int)v.size();
  std::ostringstream out;
  out<<prefix;
  for (int I=0;I<N;I++)
  {
    if (I) out<<separator;
    out<<v[I];
  }
  out<<suffix;
  return out.str();
}


///////////////////////////////////////////////////////////////////////////
std::vector<String> StringUtils::getLines(const String& s)
{
  std::vector<String> lines;
  String line;

  int N=(int)s.size();
  for (int I=0;I<N;I++)
  {
    char ch=s[I];
    if (ch=='\r')
    {
      lines.push_back(line);line="";
      if (I<(N-1) && s[I+1]=='\n') I++;
    }
    else if (ch=='\n')
    {
      lines.push_back(line);line="";
      if (I<(N-1) && s[I+1]=='\r') I++;
    }
    else
    {
      line.push_back(ch);
    }
  }

  if (!line.empty())
    lines.push_back(line);

  return lines;
}

///////////////////////////////////////////////////////////////////////////
std::vector<String> StringUtils::getNonEmptyLines(const String& s) 
{
  std::vector<String> lines=getLines(s);
  std::vector<String> ret;
  for (int I=0;I<(int)lines.size();I++)
  {
    if (!StringUtils::trim(lines[I]).empty()) 
      ret.push_back(lines[I]);
  }
  return ret;
}



///////////////////////////////////////////////////////////////////////////
std::vector<String> StringUtils::getLinesAndPurgeComments(String source,String commentString) 
{
  std::vector<String> lines=StringUtils::getNonEmptyLines(source),ret;
  ret.reserve(lines.size());
  for (int I=0;I<(int)lines.size();I++)
  {
    String L=StringUtils::trim(lines[I]);
    if (!StringUtils::startsWith(L,commentString)) 
      ret.push_back(L);
  }
  return ret;
}


///////////////////////////////////////////////////////////////////////////
String StringUtils::base64Encode(const String& input)
{
  auto temp=HeapMemory::createUnmanaged((unsigned char*)input.c_str(),input.size());
  return temp->base64Encode();
}

///////////////////////////////////////////////////////////////////////////
String StringUtils::base64Decode(const String& input)
{
  auto tmp=HeapMemory::base64Decode(input);
  if (!tmp) {VisusAssert(false);return "";}
  return String((const char*)tmp->c_ptr(),(size_t)tmp->c_size());
}


///////////////////////////////////////////////////////////////////////////
String StringUtils::hmac_sha256(String input, String key)
{
  Uint8 buffer[cryptlite::sha256::HASH_SIZE];
  cryptlite::hmac<cryptlite::sha256>::calc(input, key, buffer);
  return String((char*)buffer, cryptlite::sha256::HASH_SIZE);
}

///////////////////////////////////////////////////////////////////////////
String StringUtils::hmac_sha1(String input, String key)
{
  Uint8 buffer[cryptlite::sha1::HASH_SIZE];
  cryptlite::hmac<cryptlite::sha1>::calc(input, key, buffer);
  return String((char*)buffer, cryptlite::sha1::HASH_SIZE);
}


////////////////////////////////////////////////////////////////////////
class MD5
{
private:

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

  typedef unsigned int size_type; // must be 32bit

  typedef Uint8  uint1; //  8bit
  typedef Uint32 uint4;  // 32bit
  enum { blocksize = 64 }; // VC6 won't eat a const static int here

  bool finalized;
  uint1 buffer[blocksize]; // bytes that didn't fit in last 64 byte chunk
  uint4 count[2];   // 64bit counter for number of bits (lo, hi)
  uint4 state[4];   // digest so far
  uint1 digest[16]; // the result

  //F
  inline uint4 F(uint4 x, uint4 y, uint4 z) {
    return (x&y) | (~x&z);
  }

  //G
  inline uint4 G(uint4 x, uint4 y, uint4 z) {
    return (x&z) | (y&~z);
  }

  //H
  inline uint4 H(uint4 x, uint4 y, uint4 z) {
    return x^y^z;
  }

  //I
  inline uint4 I(uint4 x, uint4 y, uint4 z) {
    return y ^ (x | ~z);
  }

  // rotate_left rotates x left n bits.
  inline uint4 rotate_left(uint4 x, int n) {
    return (x << n) | (x >> (32 - n));
  }

  // FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
  // Rotation is separate from addition to prevent recomputation.
  inline void FF(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
    a = rotate_left(a + F(b, c, d) + x + ac, s) + b;
  }

  inline void GG(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
    a = rotate_left(a + G(b, c, d) + x + ac, s) + b;
  }

  inline void HH(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
    a = rotate_left(a + H(b, c, d) + x + ac, s) + b;
  }

  inline void II(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
    a = rotate_left(a + I(b, c, d) + x + ac, s) + b;
  }

  //constructor
  MD5()
  {
    init();
  }

  //init
  void init()
  {
    finalized = false;

    count[0] = 0;
    count[1] = 0;

    // load magic initialization constants.
    state[0] = 0x67452301;
    state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;
    state[3] = 0x10325476;
  }

  //decode
  void decode(uint4 output[], const uint1 input[], size_type len)
  {
    for (unsigned int i = 0, j = 0; j < len; i++, j += 4)
      output[i] = ((uint4)input[j]) | (((uint4)input[j + 1]) << 8) |
      (((uint4)input[j + 2]) << 16) | (((uint4)input[j + 3]) << 24);
  }

  //encode
  void encode(uint1 output[], const uint4 input[], size_type len)
  {
    for (size_type i = 0, j = 0; j < len; i++, j += 4) {
      output[j] = input[i] & 0xff;
      output[j + 1] = (input[i] >> 8) & 0xff;
      output[j + 2] = (input[i] >> 16) & 0xff;
      output[j + 3] = (input[i] >> 24) & 0xff;
    }
  }

  //transform
  void transform(const uint1 block[blocksize])
  {
    uint4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    decode(x, block, blocksize);

    /* Round 1 */
    FF(a, b, c, d, x[0], S11, 0xd76aa478); /* 1 */
    FF(d, a, b, c, x[1], S12, 0xe8c7b756); /* 2 */
    FF(c, d, a, b, x[2], S13, 0x242070db); /* 3 */
    FF(b, c, d, a, x[3], S14, 0xc1bdceee); /* 4 */
    FF(a, b, c, d, x[4], S11, 0xf57c0faf); /* 5 */
    FF(d, a, b, c, x[5], S12, 0x4787c62a); /* 6 */
    FF(c, d, a, b, x[6], S13, 0xa8304613); /* 7 */
    FF(b, c, d, a, x[7], S14, 0xfd469501); /* 8 */
    FF(a, b, c, d, x[8], S11, 0x698098d8); /* 9 */
    FF(d, a, b, c, x[9], S12, 0x8b44f7af); /* 10 */
    FF(c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
    FF(b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
    FF(a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
    FF(d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
    FF(c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
    FF(b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

                                            /* Round 2 */
    GG(a, b, c, d, x[1], S21, 0xf61e2562); /* 17 */
    GG(d, a, b, c, x[6], S22, 0xc040b340); /* 18 */
    GG(c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
    GG(b, c, d, a, x[0], S24, 0xe9b6c7aa); /* 20 */
    GG(a, b, c, d, x[5], S21, 0xd62f105d); /* 21 */
    GG(d, a, b, c, x[10], S22, 0x2441453); /* 22 */
    GG(c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
    GG(b, c, d, a, x[4], S24, 0xe7d3fbc8); /* 24 */
    GG(a, b, c, d, x[9], S21, 0x21e1cde6); /* 25 */
    GG(d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
    GG(c, d, a, b, x[3], S23, 0xf4d50d87); /* 27 */
    GG(b, c, d, a, x[8], S24, 0x455a14ed); /* 28 */
    GG(a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
    GG(d, a, b, c, x[2], S22, 0xfcefa3f8); /* 30 */
    GG(c, d, a, b, x[7], S23, 0x676f02d9); /* 31 */
    GG(b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

                                            /* Round 3 */
    HH(a, b, c, d, x[5], S31, 0xfffa3942); /* 33 */
    HH(d, a, b, c, x[8], S32, 0x8771f681); /* 34 */
    HH(c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
    HH(b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
    HH(a, b, c, d, x[1], S31, 0xa4beea44); /* 37 */
    HH(d, a, b, c, x[4], S32, 0x4bdecfa9); /* 38 */
    HH(c, d, a, b, x[7], S33, 0xf6bb4b60); /* 39 */
    HH(b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
    HH(a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
    HH(d, a, b, c, x[0], S32, 0xeaa127fa); /* 42 */
    HH(c, d, a, b, x[3], S33, 0xd4ef3085); /* 43 */
    HH(b, c, d, a, x[6], S34, 0x4881d05); /* 44 */
    HH(a, b, c, d, x[9], S31, 0xd9d4d039); /* 45 */
    HH(d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
    HH(c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
    HH(b, c, d, a, x[2], S34, 0xc4ac5665); /* 48 */

                                            /* Round 4 */
    II(a, b, c, d, x[0], S41, 0xf4292244); /* 49 */
    II(d, a, b, c, x[7], S42, 0x432aff97); /* 50 */
    II(c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
    II(b, c, d, a, x[5], S44, 0xfc93a039); /* 52 */
    II(a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
    II(d, a, b, c, x[3], S42, 0x8f0ccc92); /* 54 */
    II(c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
    II(b, c, d, a, x[1], S44, 0x85845dd1); /* 56 */
    II(a, b, c, d, x[8], S41, 0x6fa87e4f); /* 57 */
    II(d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
    II(c, d, a, b, x[6], S43, 0xa3014314); /* 59 */
    II(b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
    II(a, b, c, d, x[4], S41, 0xf7537e82); /* 61 */
    II(d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
    II(c, d, a, b, x[2], S43, 0x2ad7d2bb); /* 63 */
    II(b, c, d, a, x[9], S44, 0xeb86d391); /* 64 */

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    // Zeroize sensitive information.
    memset(x, 0, sizeof x);
  }

  //update
  void update(const unsigned char input[], size_type length)
  {
    // compute number of bytes mod 64
    size_type index = count[0] / 8 % blocksize;

    // Update number of bits
    if ((count[0] += (length << 3)) < (length << 3))
      count[1]++;
    count[1] += (length >> 29);

    // number of bytes we need to fill in buffer
    size_type firstpart = 64 - index;

    size_type i;

    // transform as many times as possible.
    if (length >= firstpart)
    {
      // fill buffer first, transform
      memcpy(&buffer[index], input, firstpart);
      transform(buffer);

      // transform chunks of blocksize (64 bytes)
      for (i = firstpart; i + blocksize <= length; i += blocksize)
        transform(&input[i]);

      index = 0;
    }
    else
      i = 0;

    // buffer remaining input
    memcpy(&buffer[index], &input[i], length - i);
  }

  //update
  void update(const char input[], size_type length)
  {
    update((const unsigned char*)input, length);
  }

  //finalize
  MD5& finalize()
  {
    static unsigned char padding[64] = {
      0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    if (!finalized) {
      // Save number of bits
      unsigned char bits[8];
      encode(bits, count, 8);

      // pad out to 56 mod 64.
      size_type index = count[0] / 8 % 64;
      size_type padLen = (index < 56) ? (56 - index) : (120 - index);
      update(padding, padLen);

      // Append length (before padding)
      update(bits, 8);

      // Store state in digest
      encode(digest, state, 16);

      // Zeroize sensitive information.
      memset(buffer, 0, sizeof buffer);
      memset(count, 0, sizeof count);

      finalized = true;
    }

    return *this;
  }

public:

  //compute
  static String compute(const String& s)
  {
    MD5 tmp;
    tmp.init();
    tmp.update(s.c_str(), (int)s.length());
    tmp.finalize();
    return String((char*)tmp.digest, 16);

  }
};


String StringUtils::md5(const String& input)
{
  return MD5::compute(input);
}

///////////////////////////////////////////////////////////////////////////
String StringUtils::encodeForFilename(String value)
{
  String ret;
  for (int I=0;I<(int)value.size();I++)
  {
    if (std::isalnum(value[I]) || value[I]=='_')
      ret.push_back(value[I]);
  }
  return ret;
}

///////////////////////////////////////////////////////////////////////////
String StringUtils::getDateTimeForFilename()
{
  return Time::now().getFormattedLocalTime();
}




//////////////////////////////////////////////////////////////
Int64 StringUtils::getByteSizeFromString(String value)
{
  const Int64 GB=1024*1024*1024;
  const Int64 MB=1024*1024;
  const Int64 KB=1024;

  value=StringUtils::toLower(StringUtils::trim(value));

  if (value=="-1") 
    return (Int64)-1;

  Int64 multiply=1;
  int len=(int)value.length();
  for (int I=0;I<len;I++)
  {
    if (value[I]=='.' || std::isdigit(value[I]))
      continue;

    String unit=value.substr(I);
    value=value.substr(0,I);
    if      (unit=="gb") multiply=GB;
    else if (unit=="mb") multiply=MB;
    else if (unit=="kb") multiply=KB;
    break;
  }

  double size=0;
  std::istringstream in(value);
  in>>size;

  return (Int64)(size*multiply);
}

//////////////////////////////////////////////////////////////
String StringUtils::getStringFromByteSize(Int64 size)
{
  const Int64 GB=1024*1024*1024;
  const Int64 MB=1024*1024;
  const Int64 KB=1024;

  if (size==(Int64)-1)  
    return cstring(-1);

  if (size>=GB)         
    return concatenate(convertDoubleToString(size / (double)GB, 1),"GB");

  if (size>=MB)         
    return concatenate(convertDoubleToString(size / (double)MB, 1),"MB");

  if (size>=KB)         
    return concatenate(convertDoubleToString(size / (double)KB, 1), "KB");
  
  return cstring(size);
}


//////////////////////////////////////////////////////////////////////////
static const char ESCAPE_CHARS[256] =
{
  /*      0 1 2 3  4 5 6 7  8 9 A B  C D E F */
  /* 0 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  /* 1 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  /* 2 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  /* 3 */ 1,1,1,1, 1,1,1,1, 1,1,0,0, 0,0,0,0,
      
  /* 4 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
  /* 5 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,
  /* 6 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
  /* 7 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,
      
  /* 8 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  /* 9 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  /* A */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  /* B */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
      
  /* C */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  /* D */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  /* E */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  /* F */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};

String StringUtils::addEscapeChars(String src)
{
  // Only alphanum is safe.
  const char DEC2HEX[16 + 1] = "0123456789ABCDEF";
  const char * pSrc = (const char *)src.c_str();
  const int SRC_LEN = (int)src.size();

  HeapMemory pStart;
  bool bOk = pStart.resize(SRC_LEN * 3, __FILE__, __LINE__);
  VisusAssert(bOk);

  char * pEnd = (char*)pStart.c_ptr();
  const char * const SRC_END = pSrc + SRC_LEN;

  for (; pSrc < SRC_END; ++pSrc)
  {
    if (ESCAPE_CHARS[(int)*pSrc])
    {
      *pEnd++ = *pSrc;
    }
    else
    {
      // escape this char
      *pEnd++ = '%';
      *pEnd++ = DEC2HEX[*pSrc >> 4];
      *pEnd++ = DEC2HEX[*pSrc & 0x0F];
    }
  }

  String ret((char*)pStart.c_ptr(), pEnd);
  return ret;
}



////////////////////////////////////////////////////////////////////////////////////
static const char HEX2DEC[256] = 
{
  /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
  /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
      
  /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
      
  /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
      
  /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
  /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};

String StringUtils::removeEscapeChars(String src)
{
  const char * pSrc = src.c_str();
  const int SRC_LEN = (int)src.size();
  const char * const SRC_END = pSrc + SRC_LEN;
  const char * const SRC_LAST_DEC = SRC_END - 2;   // last decodable '%' 

  HeapMemory pStart;
  bool bOk=pStart.resize(SRC_LEN,__FILE__,__LINE__);
  VisusAssert(bOk);

    char * pEnd = (char*)pStart.c_ptr();

    while (pSrc < SRC_LAST_DEC)
  {
    if (*pSrc == '%')
        {
            char dec1, dec2;
            if (-1 != (dec1 = HEX2DEC[(int)*(pSrc + 1)])
                && -1 != (dec2 = HEX2DEC[(int)*(pSrc + 2)]))
            {
                *pEnd++ = (dec1 << 4) + dec2;
                pSrc += 3;
                continue;
            }
        }

        *pEnd++ = *pSrc++;
  }

    // the last 2- chars
    while (pSrc < SRC_END)
        *pEnd++ = *pSrc++;

  String ret((char*)pStart.c_ptr(),pEnd);
  return ret;
}


////////////////////////////////////////////////////
ParseStringParams::ParseStringParams(String with_params,String question_sep,String and_sep,String equal_sep)
{
  this->source=with_params;

  int question_index=StringUtils::find(with_params,question_sep);
  if (question_index>=0)
  {
    std::vector<String> v=StringUtils::split(with_params.substr(question_index+1),and_sep);
    this->without_params=StringUtils::trim(with_params.substr(0,question_index));
    for (int i=0;i<(int)v.size();i++)
    {
      String key,value;
      int equal_index=(int)v[i].find(equal_sep);
      if (equal_index>=0)
      {
        key  =v[i].substr(0,equal_index);
        value=v[i].substr(equal_index+1);
      }
      else
      {
        key  =v[i];
        value="";
      }

      key  = StringUtils::trim(key);
      value= StringUtils::removeEscapeChars(value);
      if (!key.empty()) 
        this->params.setValue(key,value);
    }
  }
  else
  {
    this->without_params=with_params;
  }
}



} //namespace Visus

