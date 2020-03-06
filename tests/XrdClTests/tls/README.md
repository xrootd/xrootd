The test requires a SSL certificate and private key. These can be generated
using the 'openssl' program using these steps:

1. Generate the private key, this is what we normally keep secret:
```console
    openssl genrsa -des3 -passout pass:x -out server.pass.key 2048
    openssl rsa -passin pass:x -in server.pass.key -out server.key
    rm -f server.pass.key
```
2. Next generate the CSR.  We can leave the password empty when prompted
   (because this is self-sign):
```console
    openssl req -new -key server.key -out server.csr
```
3. Next generate the self signed certificate:
```console
    openssl x509 -req -sha256 -days 365 -in server.csr -signkey server.key -out server.crt
    rm -f server.csr
```
