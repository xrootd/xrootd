

///
// Create a sample token useful for unit tests
///

#include <scitokens/scitokens.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>


ssize_t fullRead(int fd, void *ptr, size_t nbytes) {
    ssize_t nleft, nread;

    nleft = nbytes;
    while (nleft > 0) {
REISSUE_READ:
        nread = read(fd, ptr, nleft);
        if (nread < 0) {
            if (errno == EINTR) {
                goto REISSUE_READ;
            }

            return -1;
        } else if (nread == 0) {
            break;
        }

        nleft -= nread;
        ptr = ((char *)ptr) + nread;
    }

    return (nbytes - nleft);
}


bool readShortFile(const std::string &fileName, std::string &contents) {
    int fd = open(fileName.c_str(), O_RDONLY, 0600);

    if (fd < 0) {
        std::cerr << "Failed to open " << fileName << ": " << strerror(errno) << std::endl;
        return false;
    }

    struct stat statbuf;
    int rv = fstat(fd, &statbuf);
    if (rv < 0) {
        std::cerr << "Failed to fstat " << fileName << ": " << strerror(errno) << std::endl;
        return false;
    }
    unsigned long fileSize = statbuf.st_size;
    if (fileSize > 1024*1024) {
        std::cerr << "File " << fileName << " too large for reading to memory" << std::endl;
        return false;
    }

    std::unique_ptr<char, decltype(&std::free)> rawBuffer((char*)malloc(fileSize + 1), &std::free);
    if (!rawBuffer) {
        std::cerr << "Failed to allocate memory buffer" << std::endl;
        return false;
    }
    unsigned long totalRead = fullRead(fd, rawBuffer.get(), fileSize);
    if (totalRead != fileSize) {
        std::cerr << "Failed to fully read file " << fileName << ": " << strerror(errno) << std::endl;
        close(fd);
        return false;
    }
    close(fd);
    contents.assign(rawBuffer.get(), fileSize);

    return true;
}

int main(int argc, char *argv[]) {
    if (argc < 6 || argc > 7) {
        std::cerr << "Usage: " << argv[0] << " issuer.pem issuer.key kid iss prefix [lifetime]" << std::endl;
        return 1;
    }

    std::string pubkey, privkey;
    if (!readShortFile(argv[1], pubkey)) {
        return 2;
    }
    if (!readShortFile(argv[2], privkey)) {
        return 3;
    }

    using KeyPtr = std::unique_ptr<void, decltype(&scitoken_key_destroy)>;
    using TokenPtr = std::unique_ptr<void, decltype(&scitoken_destroy)>;

    char *err_msg = nullptr;
    KeyPtr key(scitoken_key_create(argv[3], "ES256", pubkey.c_str(), privkey.c_str(), &err_msg), scitoken_key_destroy);
    if (!key) {
        std::cerr << err_msg << std::endl;
        return 4;
    }

    TokenPtr token(scitoken_create(key.get()), scitoken_destroy);
    if (!token) {
        std::cerr << err_msg << std::endl;
        return 5;
    }

    auto rv = scitoken_set_claim_string(token.get(), "iss", argv[4], &err_msg);
    if (rv) {
        std::cerr << err_msg << std::endl;
        return 6;
    }

    rv = scitoken_set_claim_string(token.get(), "scope", argv[5], &err_msg);
    if (rv) {
        std::cerr << err_msg << std::endl;
        return 7;
    }

    rv = scitoken_set_claim_string(token.get(), "sub", "test", &err_msg);
    if (rv) {
        std::cerr << err_msg << std::endl;
        return 8;
    }

    // Parse lifetime if provided, otherwise use default
    int lifetime = 60;
    if (argc == 7) {
        try {
            lifetime = std::stoi(argv[6]);
            if (lifetime <= 0) {
                throw std::invalid_argument("Lifetime must be positive");
            }
        } catch (const std::exception &e) {
            std::cerr << "Invalid lifetime value: " << argv[6] << std::endl;
            return 10;
        }
    }

    scitoken_set_lifetime(token.get(), lifetime);
    scitoken_set_serialize_profile(token.get(), SciTokenProfile::WLCG_1_0);

    char *token_value;
    rv = scitoken_serialize(token.get(), &token_value, &err_msg);
    if (rv) {
        std::cerr << err_msg << std::endl;
        return 9;
    }
    printf("%s\n", token_value);
    free(token_value);
    return 0;
}
