In the case XrdHttp has been configured, it is important that the configuration contains at least one digest algorithm registered in the IANA database: https://www.iana.org/assignments/http-dig-alg/http-dig-alg.xhtml
Otherwise the user will get a 403 error for each checksum request they will do.

Here is a table summarizing the IANA-compliant checksum names:

| Digest algorithm configured | HTTP digest | Will be base64 encoded |
|-----------------------------|-------------|------------------------|
| md5                         | md5         | true                   |
| adler32                     | adler32     | false                  |
| sha1                        | sha         | true                   |
| sha256                      | sha-256     | true                   |
| sha512                      | sha-512     | true                   |
| cksum                       | UNIXcksum   | false                  |
| crc32c                      | crc32c      | true                   |