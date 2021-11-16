#ifndef XRDOUCSHA3_HH
#define XRDOUCSHA3_HH
/******************************************************************************/
/*                                                                            */
/*                         X r d O u c S H A 3 . h h                          */
/*                                                                            */
/* The MIT License (MIT)                                                      */
/*                                                                            */
/* Copyright (c) 2015 Markku-Juhani O. Saarinen                               */
/* Contact:      19-Nov-11  Markku-Juhani O. Saarinen <mjos@iki.fi>           */
/* Reprository:  https://github.com/mjosaarinen/tiny_sha3.git                 */
/* Original:     tiny_sha3/sha3.h                                             */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/*                                                                            */
/* The above copyright notice and this permission notice shall be included    */
/* in all copies or substantial portions of the Software.                     */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    */
/* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    */
/* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        */
/* DEALINGS IN THE SOFTWARE.                                                  */
/******************************************************************************/

#include <stddef.h>
#include <cstdint>

class XrdOucSHA3
{
public:

//-----------------------------------------------------------------------------
//! SHA3 state context used by all methods (OpenSSL - like interface)
//-----------------------------------------------------------------------------

typedef struct {
    union {                                 //!< state:
        uint8_t  b[200];                    //!< 8-bit bytes
        uint64_t q[25];                     //!< 64-bit words
    } st;
    int pt, rsiz, mdlen, xof;               //!< these don't overflow
} sha3_ctx_t;

//-----------------------------------------------------------------------------
//! SHA3 digest lengths (bits to bytes).
//-----------------------------------------------------------------------------

enum MDLen {SHA3_128 = 16,
            SHA3_224 = 28,
            SHA3_256 = 32,
            SHA3_384 = 48,
            SHA3_512 = 64
           };

//-----------------------------------------------------------------------------
//! Compute a sha3 hash (md) of given byte length from "in" (one time call).
//!
//! @param  in      Pointer to input data.
//! @param  inlen   Length of data in bytes.
//! @param  md      Pointer to mbuffer of size SHA3_xxx to receive result.
//! @param  mdlen   Message digest byte length (one of the listed enums).
//!
//! @return Pointer to md.
//-----------------------------------------------------------------------------

static void *Calc(const void *in, size_t inlen, void *md, MDLen mdlen);

//-----------------------------------------------------------------------------
//! Initialize context in prepration for computing SHA3 checksum.
//!
//! @param  c       Pointer to context.
//! @param  mdlen   Message digest byte length (one of the listed enums).
//-----------------------------------------------------------------------------

static void Init(sha3_ctx_t *c, MDLen mdlen);

//-----------------------------------------------------------------------------
//! Update digest with data.
//!
//! @param  c       Pointer to context.
//! @param  data    Pointer to data.
//! @param  len     Length of data in bytes.
//-----------------------------------------------------------------------------

static void Update(sha3_ctx_t *c, const void *data, size_t len);

//-----------------------------------------------------------------------------
//! Return final message digest.
//!
//! @param  c       Pointer to context.
//! @param  md      Pointer to buffer of size SHA3_xxx to receive result.
//-----------------------------------------------------------------------------

static void Final(sha3_ctx_t *c, void *md);

//-----------------------------------------------------------------------------
//! Initialize context to compute an extensible hash using a SHA3_128 digest.
//!
//! @param  c       Pointer to context.
//-----------------------------------------------------------------------------

static void SHAKE128_Init(sha3_ctx_t *c) {Init(c, SHA3_128);}

//-----------------------------------------------------------------------------
//! Initialize context to compute an extensible hash using a SHA3_256 digest.
//!
//! @param  c       Pointer to context.
//-----------------------------------------------------------------------------

static void SHAKE256_Init(sha3_ctx_t *c) {Init(c, SHA3_256);}

//-----------------------------------------------------------------------------
//! Update shake digest with data.
//!
//! @param  c       Pointer to context.
//! @param  data    Pointer to data.
//! @param  len     Length of data in bytes.
//-----------------------------------------------------------------------------

static void SHAKE_Update(sha3_ctx_t *c, const void *data, size_t len)
                        {Update(c, data, len);}

//-----------------------------------------------------------------------------
//! Return final message digest of desired length. This function may be called
//! iteratively to get as many bits as needed. Bits beyound MDLen form a
//! pseudo-random sequence (i.e. are repeatable with the same input).
//!
//! @param  c       Pointer to context.
//! @param  out     Pointer to buffer of size len to receive result.
//! @param  len     The number of digest bytes to return.
//-----------------------------------------------------------------------------

static void SHAKE_Out(sha3_ctx_t *c, void *out, size_t len);

     XrdOucSHA3() {}
    ~XrdOucSHA3() {}

private:

// Compression function.
static void shake_xof(sha3_ctx_t *c);
static void sha3_keccakf(uint64_t st[25]);
};
#endif

