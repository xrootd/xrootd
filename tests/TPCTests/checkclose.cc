#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using close_fn = int (*)(int);

static void write_message(int fd, const char* buf, size_t len)
{
    // Deliberately ignore write()'s return value.
    [[maybe_unused]] ssize_t rc = write(fd, buf, len);
}

extern "C" int close(int fd)
{
    static close_fn real_close =
        reinterpret_cast<close_fn>(dlsym(RTLD_NEXT, "close"));
    static int enable = 0;

    if (!real_close) {
        const char msg[] =
            "LD_PRELOAD close wrapper: dlsym(close) failed\n";
        write_message(2, msg, sizeof(msg) - 1);
        abort();
    }

    int rc = real_close(fd);
    int saved_errno = errno;

    if (enable && rc == -1 && saved_errno == EBADF) {
        char buf[128];
        int n = snprintf(buf, sizeof(buf),
                         "LD_PRELOAD close wrapper: EBADF on fd %d\n", fd);
        if (n > 0)
            write_message(2, buf, static_cast<size_t>(n));

        abort();
    }

    if (!enable && fd == 255) {
        // xrootd closes first 256 descriptors at startup, after forking
        enable = 1;
    }

    errno = saved_errno;
    return rc;
}
