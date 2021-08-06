#ifndef __XRDOUCURI__HH__
#define __XRDOUCURI__HH__
/******************************************************************************/
/*                                                                            */
/*                          X r d O u c U r i . h h                           */
/*                                                                            */
/******************************************************************************/
  
/*
This software is Copyright (c) 2016 by David Farrell.
https://github.com/dnmfarrell/URI-Encode-C

This is free software, licensed under:

  The (two-clause) FreeBSD License

The FreeBSD License

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the
     distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

class XrdOucUri
{
public:

//------------------------------------------------------------------------------
//! Decode a url encoded string
//!
//! @param  src    Pointer to the source string to decode
//! @param  len    The length of src (i.e. strlen(src)).
//! @param  dst    Pointer to the buffer where to put the result. The buffer
//!                should be at least as long as the src, including null byte.
//!
//! @return The index of the null byte (i.e. end of string) in dst.
//------------------------------------------------------------------------------

static int Decode( const char *src, int len, char  *dst);

//------------------------------------------------------------------------------
//! Encode an ASCII text string
//!
//! @param  src    Pointer to the source string to encode
//! @param  len    The length of src (i.e. strlen(src)).
//! @param  dst    Pointer to where the allocated buffer pointer should be
//!                placed. This buffer holds the result and must be released
//!                using free() when no longer needed.
//!
//! @return The index of the null byte (i.e. end of string) in dst.
//------------------------------------------------------------------------------

static int Encode( const char *src, int len, char **dst);

//------------------------------------------------------------------------------
//! Encode an ASCII text string
//!
//! @param  src    Pointer to the source string to encode
//! @param  len    The length of src (i.e. strlen(src)).
//! @param  dst    Pointer to the buffer where to put the result. The buffer
//!                should be long enough to hold the result. Use the Encoded()
//!                method to determine how many bytes will be needed.
//!
//! @return The index of the null byte (i.e. end of string) in dst.
//------------------------------------------------------------------------------

static int Encode( const char *src, int len, char  *dst);

//------------------------------------------------------------------------------
//! Calculate the number of bytes an encoded string will occupy.
//!
//! @param  src    Pointer to the source string to encode
//! @param  len    The length of src (i.e. strlen(src)).
//!
//! @return The number of bytes needed, including the ending null byte.
//------------------------------------------------------------------------------

static int Encoded(const char *src, int len);

       XrdOucUri() {}
      ~XrdOucUri() {}
};
#endif
