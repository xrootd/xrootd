#ifndef XRDCLHTTP_TOKENFILEPARSER_HH
#define XRDCLHTTP_TOKENFILEPARSER_HH

#include "XrdClHttpOps.hh"

#include <string>

namespace XrdClHttp {

// Reads the tokens held by the given token file and appends the corresponding
// 'Authorization' headers to the <src> and/or <dst> header lists.
//
// A JSON token file holds the tokens in the 'src' and/or 'dst' properties,
// any other file holds the <src> token in the first line and the <dst> one
// in the second line. An absent or empty token means the corresponding side
// is given no token at all.
//
// Returns false if the file cannot be opened or holds no token at all, in
// which case no header is appended.
bool ParseTokenFile( const std::string   &token_file,
                     CurlCopyOp::Headers &src_hdrs,
                     CurlCopyOp::Headers &dst_hdrs );

}

#endif // XRDCLHTTP_TOKENFILEPARSER_HH
