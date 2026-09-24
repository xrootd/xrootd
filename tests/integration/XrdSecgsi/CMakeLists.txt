add_executable(xrd-gsi-oracle gsi_oracle.cc)

target_link_libraries(xrd-gsi-oracle XrdCrypto XrdUtils OpenSSL::Crypto)
