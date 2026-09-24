//------------------------------------------------------------------------------
// Wire level test for the GSI random tag signing oracle.
//
// The program speaks the xroot protocol by hand, with no client credentials,
// and reproduces the first round trip of a GSI handshake. It advertises
// version 10600, so it also acts as an old client and doubles as an
// old-client to new-server interoperability test.
//
// Test A: send a 51 byte SHA-256 DigestInfo as the random tag and check that
//         the server does not return a signature over it. A fixed server
//         refuses to sign a malformed random tag.
// Test B: send a well formed 8 byte random tag and check that the server signs
//         it, and that the raw signature recovers the tag under the host
//         public key. This proves an old client still authenticates a new
//         server.
//
// Usage: xrd-gsi-oracle <host> <port> <host-cert.pem>
// Exit code 0 means the server is safe, 1 means a check failed, 2 means the
// test could not run.
//------------------------------------------------------------------------------

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/sha.h>

#include "XProtocol/XProtocol.hh"
#include "XrdCrypto/XrdCryptoFactory.hh"
#include "XrdCrypto/XrdCryptoRSA.hh"
#include "XrdCrypto/XrdCryptoX509.hh"
#include "XrdSut/XrdSutAux.hh"
#include "XrdSut/XrdSutBucket.hh"
#include "XrdSut/XrdSutBuffer.hh"

// First client GSI step, see XrdSecProtocolgsi.hh (kept local to avoid pulling
// in the whole protocol header)
static const int kXGC_certreq = 1000;

// GSI protocol version this old client claims to run
static const int kOldVersion = 10600;

//______________________________________________________________________________
static int connectTo(const char *host, const char *port)
{
   // Return a connected socket, or -1 on error

   struct addrinfo hints;
   memset(&hints, 0, sizeof(hints));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;

   struct addrinfo *res = 0;
   if (getaddrinfo(host, port, &hints, &res) != 0) return -1;

   int fd = -1;
   for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
      fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (fd < 0) continue;
      if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
      close(fd);
      fd = -1;
   }
   freeaddrinfo(res);
   return fd;
}

//______________________________________________________________________________
static bool sendAll(int fd, const void *buf, size_t len)
{
   const char *p = (const char *)buf;
   while (len > 0) {
      ssize_t n = write(fd, p, len);
      if (n <= 0) return false;
      p += n;
      len -= n;
   }
   return true;
}

//______________________________________________________________________________
static bool recvAll(int fd, void *buf, size_t len)
{
   char *p = (char *)buf;
   while (len > 0) {
      ssize_t n = read(fd, p, len);
      if (n <= 0) return false;
      p += n;
      len -= n;
   }
   return true;
}

//______________________________________________________________________________
static bool recvResponse(int fd, int &status, std::string &body)
{
   // Read a ServerResponseHeader (streamid[2], status, dlen) and the body

   ServerResponseHeader hdr;
   if (!recvAll(fd, &hdr, sizeof(hdr))) return false;
   status = ntohs(hdr.status);
   int dlen = ntohl(hdr.dlen);
   body.clear();
   if (dlen > 0) {
      body.resize(dlen);
      if (!recvAll(fd, &body[0], dlen)) return false;
   }
   return true;
}

//______________________________________________________________________________
static bool doHandshake(int fd)
{
   uint32_t init[5] = {0, 0, 0, htonl(4), htonl(2012)};
   if (!sendAll(fd, init, sizeof(init))) return false;
   char reply[16];
   return recvAll(fd, reply, sizeof(reply));  // 8 byte header + 8 byte body
}

//______________________________________________________________________________
static bool doLogin(int fd, std::string &token)
{
   // Build a 24 byte ClientLoginRequest with no data
   ClientLoginRequest req;
   memset(&req, 0, sizeof(req));
   req.requestid = htons(kXR_login);
   memcpy(req.username, "poc\0\0\0\0", 8);

   if (!sendAll(fd, &req, sizeof(req))) return false;

   int status = 0;
   // The reply carries a session id and then the security token. A server
   // that requires authentication returns kXR_ok with the token, some
   // versions use kXR_authmore. Accept both.
   if (!recvResponse(fd, status, token)) return false;
   return status == kXR_ok || status == kXR_authmore;
}

//______________________________________________________________________________
static std::string caHashFromToken(const std::string &token)
{
   // The token looks like "&P=gsi,v:10600,c:ssl,ca:<hash>.0|<hash>.0"
   size_t i = token.find("ca:");
   if (i == std::string::npos) return "";
   std::string rest = token.substr(i + 3);
   size_t j = rest.find_first_of(",|");
   if (j != std::string::npos) rest = rest.substr(0, j);
   size_t z = rest.find('\0');
   if (z != std::string::npos) rest = rest.substr(0, z);
   return rest;
}

//______________________________________________________________________________
static bool sendCertreq(int fd, const std::string &cahash,
                        const char *rtag, int rtaglen)
{
   // Inner main buffer: one kXRS_rtag bucket
   XrdSutBuffer main("gsi", "");
   {
      char *copy = new char[rtaglen];
      memcpy(copy, rtag, rtaglen);
      main.AddBucket(copy, rtaglen, kXRS_rtag);  // adopts copy
   }
   char *mser = 0;
   int mlen = main.Serialized(&mser);
   if (mlen < 0) return false;

   // Outer global buffer for kXGC_certreq
   XrdSutBuffer out("gsi", "");
   out.SetStep(kXGC_certreq);
   out.MarshalBucket(kXRS_version, (kXR_int32)kOldVersion);
   out.AddBucket(XrdOucString("ssl"), kXRS_cryptomod);
   out.AddBucket(XrdOucString(cahash.c_str()), kXRS_issuer_hash);
   out.MarshalBucket(kXRS_clnt_opts, (kXR_int32)0);
   out.AddBucket(mser, mlen, kXRS_main);  // adopts mser

   char *pser = 0;
   int plen = out.Serialized(&pser);
   if (plen < 0) return false;

   // ClientAuthRequest header with credtype "gsi"
   ClientAuthRequest req;
   memset(&req, 0, sizeof(req));
   req.requestid = htons(kXR_auth);
   memcpy(req.credtype, "gsi\0", 4);
   req.dlen = htonl(plen);

   bool ok = sendAll(fd, &req, sizeof(req)) && sendAll(fd, pser, plen);
   delete[] pser;
   return ok;
}

//______________________________________________________________________________
static std::string extractSignedRtag(int status, const std::string &body)
{
   // Return the signed rtag bytes, or an empty string if absent

   if (status != kXR_authmore) return "";

   XrdSutBuffer outer(body.data(), (kXR_int32)body.size());
   XrdSutBucket *bmain = outer.GetBucket(kXRS_main);
   if (!bmain) return "";

   XrdSutBuffer inner(bmain->buffer, bmain->size);
   XrdSutBucket *bsig = inner.GetBucket(kXRS_signed_rtag);
   if (!bsig) return "";

   return std::string(bsig->buffer, bsig->size);
}

//______________________________________________________________________________
static std::string recoverSignature(XrdCryptoRSA *pub, const std::string &sig)
{
   // Recover the raw bytes a PKCS#1 v1.5 signature was made over, using the
   // public key. Return them, or an empty string on error.

   char *copy = new char[sig.size()];
   memcpy(copy, sig.data(), sig.size());
   XrdSutBucket bck(copy, (int)sig.size(), 0);  // adopts copy

   if (pub->DecryptPublic(bck) <= 0) return "";
   return std::string(bck.buffer, bck.size);
}

//______________________________________________________________________________
static std::string digestInfoSha256(const std::string &message)
{
   // 19 byte SHA-256 DigestInfo prefix followed by the 32 byte hash
   static const unsigned char prefix[19] = {
      0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
      0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20};

   unsigned char md[SHA256_DIGEST_LENGTH];
   SHA256((const unsigned char *)message.data(), message.size(), md);

   std::string di((const char *)prefix, sizeof(prefix));
   di.append((const char *)md, sizeof(md));
   return di;
}

//______________________________________________________________________________
static bool oneRound(const char *host, const char *port,
                     const std::string &cahash, const char *rtag,
                     int rtaglen, std::string &sig)
{
   int fd = connectTo(host, port);
   if (fd < 0) return false;

   std::string token, body;
   int status = 0;
   bool ok = doHandshake(fd) && doLogin(fd, token);
   std::string ca = cahash.empty() ? caHashFromToken(token) : cahash;
   ok = ok && !ca.empty() && sendCertreq(fd, ca, rtag, rtaglen) &&
        recvResponse(fd, status, body);
   close(fd);

   if (!ok) return false;
   sig = extractSignedRtag(status, body);
   return true;
}

//______________________________________________________________________________
int main(int argc, char **argv)
{
   if (argc != 4) {
      fprintf(stderr, "usage: %s <host> <port> <host-cert.pem>\n", argv[0]);
      return 2;
   }
   const char *host = argv[1];
   const char *port = argv[2];
   const char *certfile = argv[3];

   XrdCryptoFactory *cf = XrdCryptoFactory::GetCryptoFactory("ssl");
   if (!cf) {
      fprintf(stderr, "cannot load the ssl crypto factory\n");
      return 2;
   }
   XrdCryptoX509 *cert = cf->X509(certfile);
   if (!cert || !cert->PKI()) {
      fprintf(stderr, "cannot load the host public key from %s\n", certfile);
      return 2;
   }
   XrdCryptoRSA *pub = cert->PKI();

   const std::string message = "forgery-test";
   const std::string digestinfo = digestInfoSha256(message);

   bool ok = true;

   // Test A: the DigestInfo signing oracle. A round trip may not even leave a
   // CA hash from the token, so parse it inside oneRound.
   std::string sig;
   if (!oneRound(host, port, "", digestinfo.data(), (int)digestinfo.size(),
                 sig)) {
      fprintf(stderr, "Test A: could not complete the handshake\n");
      return 2;
   }
   if (sig.empty()) {
      printf("Test A: server refused to sign the DigestInfo - OK\n");
   } else {
      std::string recovered = recoverSignature(pub, sig);
      if (recovered == digestinfo) {
         printf("Test A: FAIL - server signed the DigestInfo for '%s' "
                "(oracle open)\n", message.c_str());
         ok = false;
      } else {
         printf("Test A: server returned a signature that is not over the "
                "forged DigestInfo - OK\n");
      }
   }

   // Test B: a well formed random tag, old client interoperability
   const char rtag[8] = {'A', 'b', '3', '.', '/', 'x', 'Y', 'z'};
   sig.clear();
   if (!oneRound(host, port, "", rtag, (int)sizeof(rtag), sig)) {
      fprintf(stderr, "Test B: could not complete the handshake\n");
      return 2;
   }
   if (sig.empty()) {
      printf("Test B: FAIL - server did not sign a valid random tag, "
             "an old client would not authenticate\n");
      ok = false;
   } else {
      std::string recovered = recoverSignature(pub, sig);
      if (recovered == std::string(rtag, sizeof(rtag))) {
         printf("Test B: server signed the random tag and it recovers to the "
                "sent value - old client interoperability OK\n");
      } else {
         printf("Test B: FAIL - recovered value does not match the sent tag\n");
         ok = false;
      }
   }

   delete cert;
   return ok ? 0 : 1;
}
