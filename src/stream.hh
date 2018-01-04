
/**
 * The "stream" interface is a simple abstraction of a file handle.
 *
 * The abstraction layer is necessary to do the necessary buffering
 * of multi-stream writes where the underlying filesystem only
 * supports single-stream writes.
 */

#include <memory>

struct stat;

class XrdSfsFile;

namespace TPC {
class Stream {
public:
    Stream(std::unique_ptr<XrdSfsFile> fh)
        : m_fh(std::move(fh))
    {}

    ~Stream();

    int Stat(struct stat *);

    int Read(off_t offset, char *buffer, size_t size);

    int Write(off_t offset, const char *buffer, size_t size);

private:
    std::unique_ptr<XrdSfsFile> m_fh;
};
}
